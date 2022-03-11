#pragma once

#include "Mesh.hpp"
#include "Texture.hpp"

#include "Utility.hpp"

#include "glm/vec3.hpp"	// vec3, bvec3, dvec3, ivec3 and uvec3

#include "optional"
#include "array"

enum class DrawMode
{
	Fill,
	Wireframe
};


enum class DrawStyle : size_t
{
	Default = 0,
	Textured = 1,
	UniformColour = 2,

	Count
};
// Allows iterating over enum class DrawStyle
static const std::array<std::string, util::toIndex(DrawStyle::Count)> drawStyles { "Default", "Textured", "Uniform Colour" };
static std::string convert(const DrawStyle& pDrawStyle)
{
	return drawStyles[util::toIndex(pDrawStyle)];
}

// A request to execute a specific draw using a GraphicsAPI.
struct DrawCall
{
	MeshID 					 mMesh;
	std::optional<TextureID> mTexture;

	DrawMode 	mDrawMode  	= DrawMode::Fill;
	DrawStyle	mDrawStyle  = DrawStyle::Default;

	glm::vec3 	mPosition 	= glm::vec3(0.0f);
	glm::vec3 	mRotation	= glm::vec3(0.0f);
	glm::vec3 	mScale		= glm::vec3(1.0f);
};