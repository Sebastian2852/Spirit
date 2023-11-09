#pragma once

#include "Component/Mesh.hpp"
#include "OpenGL/GLState.hpp"
#include "Utility/Utility.hpp"

#include <vector>
#include <numbers>
#include <utility>
#include <type_traits>

namespace Utility
{
	template <typename VertexType = Data::Vertex, OpenGL::PrimitiveMode primitive_mode = OpenGL::PrimitiveMode::Triangles>
	requires Data::is_valid_mesh_vert<VertexType>
	class MeshBuilder
	{
		std::vector<VertexType> data;
		glm::vec4 current_colour;

	public:
		MeshBuilder() noexcept
			: data{}
			, current_colour{glm::vec4{1.f}}
		{}

		template <typename Vertex>
		void add_vertex(Vertex&& v)
		{
			static_assert(primitive_mode == OpenGL::PrimitiveMode::Points, "add_vertex requires MeshBuilder PrimitiveMode to be Points.");
			static_assert(std::is_same_v<std::decay_t<Vertex>, VertexType>, "Vertex type must match the MeshBuilder VertexType.");
			static_assert(!Data::has_normal_member<Vertex>, "add_vertex doesnt support normal data. Remove normal from Vertex.");
			static_assert(!Data::has_UV_member<Vertex>, "add_vertex doesnt support UV data. Remove UV from VertexType.");

			if constexpr (Data::has_colour_member<Vertex>)
				v.colour = current_colour;

			data.emplace_back(std::forward<Vertex>(v));
		}
		template <typename Vertex>
		void add_line(Vertex&& v1, Vertex&& v2)
		{
			static_assert(primitive_mode == OpenGL::PrimitiveMode::Lines, "add_line requires MeshBuilder PrimitiveMode to be Lines.");

			if constexpr (std::is_same_v<std::decay_t<Vertex>, glm::vec3>)
			{// vec3 overload, apply the position to the vertices and call recursively with them.
				VertexType v1_t;
				v1_t.position = v1;
				VertexType v2_t;
				v2_t.position = v2;
				add_line(std::forward<VertexType>(v1_t), std::forward<VertexType>(v2_t));
			}
			else
			{
				static_assert(std::is_same_v<std::decay_t<Vertex>, VertexType>, "Vertex type must match the MeshBuilder VertexType.");
				static_assert(!Data::has_normal_member<Vertex>, "add_line doesnt support normal data. Remove the normal from VertexType.");
				static_assert(!Data::has_UV_member<Vertex>, "add_line doesnt support UV data. Remove UV from VertexType.");

				if constexpr (Data::has_colour_member<Vertex>)
				{
					v1.colour = current_colour;
					v2.colour = current_colour;
				}
				data.emplace_back(std::forward<Vertex>(v1));
				data.emplace_back(std::forward<Vertex>(v2));
			}
		}
		// Add a triangle to the mesh.
		// Calculates the normal from the vertices. If the vertices already have a normal, use the other overload.
		template <typename Vertex>
		void add_triangle(Vertex&& v1, Vertex&& v2, Vertex&& v3)
		{
			static_assert(std::is_same_v<std::decay_t<Vertex>, VertexType>, "Vertex type must match the MeshBuilder VertexType.");
			static_assert(primitive_mode == OpenGL::PrimitiveMode::Triangles, "add_triangle requires MeshBuilder PrimitiveMode to be Triangles.");

			if constexpr (Data::has_normal_member<Vertex>)
			{
				const auto edge1       = v2.position - v1.position;
				const auto edge2       = v3.position - v1.position;
				const auto calc_normal = glm::cross(edge1, edge2);
				add_triangle(std::forward<Vertex>(v1), std::forward<Vertex>(v2), std::forward<Vertex>(v3), calc_normal);
			}
			else
			{
				if constexpr (Data::has_colour_member<Vertex>)
				{
					v1.colour = current_colour;
					v2.colour = current_colour;
					v3.colour = current_colour;
				}

				data.emplace_back(std::forward<Vertex>(v1));
				data.emplace_back(std::forward<Vertex>(v2));
				data.emplace_back(std::forward<Vertex>(v3));
			}
		}
		// Add a triangle to the mesh.
		// Uses the provided normal. If the vertices dont have a normal, use the other overload.
		template <typename Vertex>
		void add_triangle(Vertex&& v1, Vertex&& v2, Vertex&& v3, const glm::vec3& normal)
		{
			static_assert(std::is_same_v<std::decay_t<Vertex>, VertexType>, "Vertex type must match the MeshBuilder VertexType.");
			static_assert(primitive_mode == OpenGL::PrimitiveMode::Triangles, "add_triangle requires MeshBuilder PrimitiveMode to be Triangles.");
			static_assert(Data::has_normal_member<Vertex>, "VertexType must have a normal member. Call non-normal overload of add_triangle or remove normal data from VertexType.");

			if constexpr (Data::has_colour_member<Vertex>)
			{
				v1.colour = current_colour;
				v2.colour = current_colour;
				v3.colour = current_colour;
			}

			v1.normal = normal;
			v2.normal = normal;
			v3.normal = normal;

			data.emplace_back(std::forward<Vertex>(v1));
			data.emplace_back(std::forward<Vertex>(v2));
			data.emplace_back(std::forward<Vertex>(v3));
		}




		void add_circle(const glm::vec3& center, float radius, size_t segments, const glm::vec3& normal = {0.f, 1.f, 0.f})
		{
			if constexpr (primitive_mode == OpenGL::PrimitiveMode::Triangles)
			{
				const auto points_and_UVs = get_circle_points(center, radius, segments, normal);

				for (auto i = 0; i < segments; ++i)
				{
					VertexType v1;
					VertexType v2;
					VertexType v3;

					v1.position = points_and_UVs[(i + 1) % segments].first;
					v2.position = center;
					v3.position = points_and_UVs[i].first;

					if constexpr (Data::has_UV_member<VertexType>)
					{
						v1.uv = glm::vec2(0.5f) - (points_and_UVs[(i + 1) % segments].second * glm::vec2(0.5f, -0.5f));
						v2.uv = glm::vec2{0.5f, 0.5f};
						v3.uv = glm::vec2(0.5f) - (points_and_UVs[i].second * glm::vec2(0.5f, -0.5f));
					}

					if constexpr (Data::has_normal_member<VertexType>)
						add_triangle(v1, v2, v3, normal);
					else
						add_triangle(v1, v2, v3);
				}
			}
			else
				[]<bool flag=false>(){ static_assert(flag, "Not implemented add_circle for this primitive_mode."); }(); // #CPP23 P2593R0 swap for static_assert(false)
		}
		void add_quad(const glm::vec3& top_left, const glm::vec3& top_right, const glm::vec3& bottom_left, const glm::vec3& bottom_right)
		{
			if constexpr (primitive_mode == OpenGL::PrimitiveMode::Triangles)
			{
				VertexType top_left_v;
				VertexType bottom_left_v;
				VertexType bottom_right_v;
				VertexType top_right_v;

				top_left_v.position     = top_left;
				bottom_left_v.position  = bottom_left;
				bottom_right_v.position = bottom_right;
				top_right_v.position    = top_right;

				if constexpr (Data::has_UV_member<VertexType>)
				{
					top_left_v.uv     = glm::vec2(0.f, 1.f);
					bottom_left_v.uv  = glm::vec2(0.f, 0.f);
					bottom_right_v.uv = glm::vec2(1.f, 0.f);
					top_right_v.uv    = glm::vec2(1.f, 1.f);
				}

				if constexpr (Data::has_normal_member<VertexType>)
				{
					const auto normal = glm::normalize(glm::cross(bottom_left - top_left, top_right - top_left));
					add_triangle(top_left_v, bottom_left_v, bottom_right_v, normal);
					add_triangle(top_left_v, bottom_right_v, top_right_v, normal);
				}
				else
				{
					add_triangle(top_left_v, bottom_left_v, bottom_right_v);
					add_triangle(top_left_v, bottom_right_v, top_right_v);
				}
			}
			else
				[]<bool flag=false>(){ static_assert(flag, "Not implemented add_quad for this primitive_mode."); }(); // #CPP23 P2593R0 swap for static_assert(false)
		}
		void add_cone(const glm::vec3& base, const glm::vec3& top, float radius, size_t segments)
		{
			if constexpr (primitive_mode == OpenGL::PrimitiveMode::Triangles)
			{
				const auto top_to_base    = glm::normalize(base - top);
				const auto points_and_UVs = get_circle_points(base, radius, segments, top_to_base);

				for (auto i = 0; i < segments; ++i)
				{
					VertexType v1;
					VertexType v2;
					VertexType v3;

					v1.position = points_and_UVs[i].first;
					v2.position = top;
					v3.position = points_and_UVs[(i + 1) % segments].first;

					if constexpr (Data::has_UV_member<VertexType>)
					{
						v1.uv = glm::vec2(0.5f) - (points_and_UVs[i].second * glm::vec2(0.5f, -0.5f));
						v2.uv = glm::vec2{0.5f, 0.5f};
						v3.uv = glm::vec2(0.5f) - (points_and_UVs[(i + 1) % segments].second * glm::vec2(0.5f, -0.5f));
					}

					add_triangle(v1, v2, v3);
				}

				add_circle(base, radius, segments, top_to_base);
			}
			else
				[]<bool flag=false>(){ static_assert(flag, "Not implemented add_cone for this primitive_mode."); }(); // #CPP23 P2593R0 swap for static_assert(false)
		}
		void add_cylinder(const glm::vec3& base, const glm::vec3& top, float radius, size_t segments)
		{
			if constexpr (primitive_mode == OpenGL::PrimitiveMode::Triangles)
			{
				const auto base_to_top                = top - base;
				const auto base_to_top_dir            = glm::normalize(base_to_top);
				const auto top_to_base_dir            = -base_to_top_dir;
				const auto base_circle_points_and_UVs = get_circle_points(base, radius, segments, top_to_base_dir);

				VertexType base_vertex_center;
				VertexType top_vertex_center;
				base_vertex_center.position = base;
				top_vertex_center.position = top;

				if constexpr (Data::has_UV_member<VertexType>)
				{
					base_vertex_center.uv = glm::vec2{0.5f, 0.5f};
					top_vertex_center.uv  = glm::vec2{0.5f, 0.5f};
				}
				if constexpr (Data::has_normal_member<VertexType>)
				{
					base_vertex_center.normal = top_to_base_dir;
					top_vertex_center.normal  = base_to_top_dir;
				}

				for (auto i = 0; i < segments; ++i)
				{
					VertexType base_vertex_1;
					VertexType base_vertex_2;
					VertexType top_vertex_1;
					VertexType top_vertex_2;

					base_vertex_1.position = base_circle_points_and_UVs[i].first;
					base_vertex_2.position = base_circle_points_and_UVs[(i + 1) % segments].first;
					top_vertex_1.position  = base_circle_points_and_UVs[i].first + base_to_top;
					top_vertex_2.position  = base_circle_points_and_UVs[(i + 1) % segments].first + base_to_top;

					if constexpr (Data::has_UV_member<VertexType>)
					{
						base_vertex_1.uv = glm::vec2(0.5f) - (base_circle_points_and_UVs[i].second * glm::vec2(0.5f, -0.5f));
						base_vertex_2.uv = glm::vec2{0.5f, 0.5f};
						top_vertex_1.uv  = glm::vec2{0.5f, 0.5f};
						top_vertex_2.uv  = glm::vec2{0.5f, 0.5f};
					}

					{// Add the triangles for the side of the cylinder. Go in triangle pairs forming quads along the side.
						add_triangle(base_vertex_1, top_vertex_1, base_vertex_2);
						add_triangle(base_vertex_2, top_vertex_1, top_vertex_2);
					}
					{// Add the triangles for the circles at the base and top of the cylinder.
					 // Though identical to MeshBuilder::add_circle, we dont want to call get_circle_points more than once so we reuse the base_circle_points.
					 	if constexpr (Data::has_normal_member<VertexType>)
						{
							add_triangle(base_vertex_2, base_vertex_center, base_vertex_1, top_to_base_dir);
							add_triangle(top_vertex_1, top_vertex_center, top_vertex_2, base_to_top_dir);
						}
						else
						{
							add_triangle(base_vertex_2, base_vertex_center, base_vertex_1);
							add_triangle(top_vertex_1, top_vertex_center, top_vertex_2);
						}
					}
				}
			}
			else
				[]<bool flag=false>(){ static_assert(flag, "Not implemented add_cylinder for this primitive_mode."); }(); // #CPP23 P2593R0 swap for static_assert(false)
		}
		void add_arrow(const glm::vec3& base, const glm::vec3& top, size_t segments = 16)
		{
			const auto base_to_top        = top - base;
			const auto direction          = glm::normalize(base_to_top);
			const auto length             = glm::length(base_to_top);
			const auto base_radius        = length * 0.05f;
			const auto cone_radius        = length * 0.2f;
			const auto body_to_head_ratio = 0.75f;
			const auto body_top           = base + direction * length * body_to_head_ratio;

			add_cylinder(base, body_top, base_radius, segments);
			add_cone(body_top, top, cone_radius, segments);
		}
		void reserve(size_t size)
		{
			data.reserve(size);
		}
		void set_colour(const glm::vec4& colour)
		{
			static_assert(Data::has_colour_member<VertexType>, "VertexType must have a colour member.");
			current_colour = colour;
		}
		void set_colour(const glm::vec3& colour)
		{
			static_assert(Data::has_colour_member<VertexType>, "VertexType must have a colour member.");
			current_colour = glm::vec4(colour, 1.f);
		}
		[[NODISCARD]] Data::NewMesh get_mesh()
		{
			return Data::NewMesh{data, primitive_mode};
		}

	private:

		// Get the points and UVs for a circle.
		//@param center Center of the circle.
		//@param radius Radius of the circle.
		//@param segments Number of segments to use to make the circle.
		//@param normal Direction the circle is facing. Decides the winding order.
		//@return A vector of pairs of points and UVs.
		[[nodiscard]] static std::vector<std::pair<glm::vec3, glm::vec2>> get_circle_points(const glm::vec3& center, float radius, size_t segments, const glm::vec3& normal = {0.f, 1.f, 0.f})
		{
			std::vector<std::pair<glm::vec3, glm::vec2>> points_and_UVs;
			points_and_UVs.reserve(segments);
			const auto rotation = Utility::getRotation(glm::vec3{0.f, 1.f, 0.f}, normal); // Get the rotation quaternion to rotate the circle to the correct orientation.
			const float angle_step = 2.0f * std::numbers::pi_v<float> / segments;

			for (auto i = 0; i < segments; ++i)
			{
				const auto angle = (float)i * angle_step;
				const auto point = center + (rotation * glm::vec3(radius * glm::sin(angle), 0.f, radius * glm::cos(angle)));
				const auto uv    = glm::vec2{glm::sin(angle), glm::cos(angle)};
				points_and_UVs.emplace_back(std::make_pair(point, uv));
			}

			return points_and_UVs;
		}
	};
}