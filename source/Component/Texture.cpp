#include "Texture.hpp"
#include "System/TextureSystem.hpp"

#include "Utility/Logger.hpp"
#include "Utility/File.hpp"

#include "imgui.h"

namespace Data
{
	OpenGL::TextureFormat format_from_channels(const uint8_t p_channels)
	{
		switch (p_channels)
		{
			case 1: return OpenGL::TextureFormat::R;
			case 2: return OpenGL::TextureFormat::RG;
			case 3: return OpenGL::TextureFormat::RGB;
			case 4: return OpenGL::TextureFormat::RGBA;
			default: throw std::runtime_error("Invalid number of channels for texture format.");
		}
	}

	Texture::Texture(const std::filesystem::path& p_filepath) noexcept
		: m_image_ref{Utility::File::s_image_files.get_or_create([&p_filepath](const Utility::Image& p_image){ return p_image.m_filepath == p_filepath; }, p_filepath)}
		, m_GL_texture{
		                   m_image_ref->resolution(),
		                   OpenGL::TextureMagFunc::Linear,
		                   OpenGL::WrappingMode::Repeat,
		                   OpenGL::TextureInternalFormat::RGB32F,
		                   format_from_channels(m_image_ref->m_number_of_channels),
		                   OpenGL::TextureDataType::UNSIGNED_BYTE,
		                   true,
		                   m_image_ref->get_data()}
	{
		LOG("Data::Texture '{}' loaded", m_image_ref->m_filepath.string());
	}
}

namespace Component
{
	Texture::Texture() noexcept
		: m_diffuse{}
		, m_specular{}
		, m_shininess{32.f}
		, m_colour{glm::vec4(1.f)}
	{}
	Texture::Texture(const TextureRef& m_diffuse) noexcept
		: m_diffuse{m_diffuse}
		, m_specular{}
		, m_shininess{32.f}
		, m_colour{glm::vec4(1.f)}
	{}
	Texture::Texture(const glm::vec4& p_colour) noexcept
		: m_diffuse{}
		, m_specular{}
		, m_shininess{32.f}
		, m_colour{p_colour}
	{}

	void Texture::draw_UI(System::TextureSystem& p_texture_system)
	{
		if (ImGui::TreeNode("Texture"))
		{
			const std::string currentDiffuse  = m_diffuse ? m_diffuse->m_image_ref->name() : "None";
			const std::string currentSpecular = m_specular ? m_specular->m_image_ref->name() : "None";

			static size_t selected;
			auto& availableTextures = p_texture_system.m_available_textures;
			std::vector<std::string> availableTextureNames;
			for (const auto& path : availableTextures)
				availableTextureNames.push_back(path.stem().string());

			if (ImGui::ComboContainer("Diffuse Texture", currentDiffuse.c_str(), availableTextureNames, selected))
				m_diffuse = p_texture_system.getTexture(availableTextures[selected]);
			if (ImGui::ComboContainer("Specular Texture", currentSpecular.c_str(), availableTextureNames, selected))
				m_specular = p_texture_system.getTexture(availableTextures[selected]);
			ImGui::Slider("Shininess", m_shininess, 1.f, 512.f, "%.1f");

			ImGui::ColorEdit4("Colour", &m_colour[0]);
			ImGui::SameLine();
			ImGui::Text("Used if no textures are specified.");

			ImGui::TreePop();
		}
	}
} // namespace Component