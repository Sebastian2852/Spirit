#include "ParticleRenderer.hpp"
#include "GLState.hpp"
#include "DrawCall.hpp"

#include "System/SceneSystem.hpp"

#include "glm/gtx/norm.hpp"

#include <random>

namespace OpenGL
{
	ParticleRenderer::ParticleRenderer()
		: m_particle_shader{"particle"}
		, m_quad_VAO{}
		, m_quad_VBO{{OpenGL::BufferStorageFlag::DynamicStorageBit}} // TODO: Can we use non-dynamic storage for this?
		, m_quad_EBO{{OpenGL::BufferStorageFlag::DynamicStorageBit}} // TODO: Can we use non-dynamic storage for this?
		, m_particle_buffer{{OpenGL::BufferStorageFlag::DynamicStorageBit}}
	{
		// Prepare the quad VAO for rendering.
		m_quad_VBO.upload_data(quad_vertices);
		m_quad_EBO.upload_data(quad_indices);

		constexpr GLuint vertex_buffer_binding_point = 0;
		m_quad_VAO.set_vertex_attrib_pointers(PrimitiveMode::Triangles, {
			VertexAttributeMeta{m_particle_shader.get_attribute_index("VertexPosition"), 3, BufferDataType::Float, 0, vertex_buffer_binding_point, false},
			VertexAttributeMeta{m_particle_shader.get_attribute_index("VertexTexCoord"), 2, BufferDataType::Float, 3 * sizeof(float), vertex_buffer_binding_point, false}
		});
		m_quad_VAO.attach_buffer(m_quad_VBO, 0, vertex_buffer_binding_point);
		m_quad_VAO.attach_element_buffer(m_quad_EBO);
	}

	void ParticleRenderer::update(const DeltaTime& p_delta_time, System::Scene& p_scene, const glm::vec3& p_camera_position, const Buffer& p_view_properties, const FBO& p_target_FBO)
	{
		p_scene.m_entities.foreach([&](Component::ParticleEmitter& p_emitter)
		{
			constexpr auto zero_seconds = std::chrono::seconds(0);

			{// Spawning new particles every spawn_period
				p_emitter.time_to_next_spawn -= p_delta_time;
				if (p_emitter.time_to_next_spawn <= zero_seconds)
				{
					p_emitter.time_to_next_spawn = p_emitter.spawn_period; // Reset time.

					auto current_particle_count = static_cast<unsigned int>(p_emitter.particles.size()); // TODO: Casting down, check size_t fits into the uint
					if (current_particle_count < p_emitter.max_particle_count)
					{
						auto remaining_size     = p_emitter.max_particle_count - current_particle_count;
						auto new_particle_count = std::min(remaining_size, p_emitter.spawn_count);

						if (new_particle_count > 0)
						{// Create random number generators for each component
							ASSERT_THROW(p_emitter.emit_velocity_min.x < p_emitter.emit_velocity_max.x
								&& p_emitter.emit_velocity_min.y < p_emitter.emit_velocity_max.y
								&& p_emitter.emit_velocity_min.z < p_emitter.emit_velocity_max.z, "ParticleEmitter min not smaller than max");

							auto rd           = std::random_device();
							auto gen          = std::mt19937(rd());
							auto distribution = std::uniform_real_distribution<float>(0.f, 1.f);

							for (unsigned int i = 0; i < new_particle_count; i++)
							{
								auto vel = glm::vec4{ // Scale distribution(gen) from [0 - 1] to [min - max]
									p_emitter.emit_velocity_min.x + (distribution(gen) * (p_emitter.emit_velocity_max.x - p_emitter.emit_velocity_min.x)),
									p_emitter.emit_velocity_min.y + (distribution(gen) * (p_emitter.emit_velocity_max.y - p_emitter.emit_velocity_min.y)),
									p_emitter.emit_velocity_min.z + (distribution(gen) * (p_emitter.emit_velocity_max.z - p_emitter.emit_velocity_min.z)),
									1.f
								};
								p_emitter.particles.push_back({glm::vec4(p_emitter.emit_position, 1.f), vel, p_emitter.lifetime});
							}
						}
					}
				}
			}
			{// Update particle lifetimes and positions.
				// When the lifetime of a particle is below zero, remove it from the vector.
				// Otherwise, integrate its position by the velocity.
				for (auto it = p_emitter.particles.begin(); it != p_emitter.particles.end();)
				{
					it->lifetime -= p_delta_time;

					if (it->lifetime <= zero_seconds)
						it = p_emitter.particles.erase(it);
					else
					{
						it->position += (it->velocity * p_delta_time.count());
						++it;
					}
				}
			}
			if (p_emitter.sort_by_distance_to_camera)
			{
				const auto camera_position = glm::vec4(p_camera_position, 0.f);
				for (auto& particle : p_emitter.particles)
					particle.distance_to_camera = glm::distance2(camera_position, particle.position);

				std::sort(p_emitter.particles.begin(), p_emitter.particles.end(), [](const auto& lhs, const auto& rhs)
				{
					return lhs.distance_to_camera > rhs.distance_to_camera;
				});
			}

			{// Upload the particle data to the SSBO in particle_shader
				// TODO: set these by querying the SSBO using GL introspection.
				GLsizeiptr particle_count_offset  = 0;
				GLint particle_count_size         = 16; // Size of the uint number_of_particles variable.
				GLint particle_array_start_offset = 16; // Starts after number_of_particles + padding.
				GLint particle_position_offset    = 0;  // Offset from the start of an index to the position var.
				GLint particle_velocity_offset    = 16; // Offset from the start of an index to the velocity var.
				GLintptr particle_stride          = 32; // Size of one particle

				auto particle_count = p_emitter.particles.size();
				const GLsizeiptr required_size = particle_count_size + particle_count * particle_stride;

				// Resize the buffer to accomodate at least the directional_light_count
				if (required_size > m_particle_buffer.size())
				{
					LOG("[OPENGL][PARTICLE RENDERER] ParticleEmitter particle count changed ({}), resized the particles buffer to {}B", particle_count, required_size);
					auto grow_size = required_size - m_particle_buffer.size();
					m_particle_buffer.resize(required_size);

					if (particle_count_offset > particle_array_start_offset)
					{ // The var is after the variable sized array, update its offset by the growth of the variable-sized-array.
						particle_count_offset += grow_size;
					}
				}

				// Set the count
				auto uint_particle_count = static_cast<unsigned int>(particle_count);
				m_particle_buffer.buffer_sub_data(particle_count_offset, uint_particle_count);

				for (size_t i = 0; i < p_emitter.particles.size(); i++)
				{
					m_particle_buffer.buffer_sub_data(particle_array_start_offset + particle_position_offset + (particle_stride * i), p_emitter.particles[i].position);
					m_particle_buffer.buffer_sub_data(particle_array_start_offset + particle_velocity_offset + (particle_stride * i), p_emitter.particles[i].velocity);
				}
			}
			{ // Draw the particles
				// Ensure the p_emitter.particles vector fits inside GLSizei
				ASSERT_THROW(p_emitter.particles.size() <= std::numeric_limits<GLsizei>::max(), "ParticleEmitter particle count too large for draw_elements_instanced");

				DrawCall dc;
				dc.set_texture("diffuse", p_emitter.diffuse->m_GL_texture);
				dc.set_SSBO("ParticlesBuffer", m_particle_buffer);
				dc.set_UBO("ViewProperties", p_view_properties);
				dc.submit_instanced(m_particle_shader, m_quad_VAO, p_target_FBO, static_cast<GLsizei>(p_emitter.particles.size()));
			}
		});
	}
} // namespace OpenGL