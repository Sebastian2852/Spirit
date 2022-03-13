#define IMGUI_USER_CONFIG "ImGuiConfig.hpp"
#include "imgui.h"

#include "Renderer.hpp"
#include "Logger.hpp"
#include "OpenGLAPI.hpp"

Renderer::Renderer()
: mMeshManager()
, mTextureManager()
, mLightManager()
, mOpenGLAPI(new OpenGLAPI(mMeshManager, mTextureManager, mLightManager))
, mCamera(glm::vec3(0.0f, 0.0f, 7.0f)
, std::bind(&GraphicsAPI::setView, mOpenGLAPI, std::placeholders::_1)
, std::bind(&GraphicsAPI::setViewPosition, mOpenGLAPI, std::placeholders::_1)
)
{
	lightPosition.mScale 		= glm::vec3(0.1f);
	lightPosition.mMesh 		= mMeshManager.getMeshID("3DCube");
	lightPosition.mColour		= glm::vec3(1.f);
	lightPosition.mDrawStyle 	= DrawStyle::UniformColour;

	{
		DrawCall& drawCall		= mDrawCalls.Create(ECS::CreateEntity());
		drawCall.mScale 		= glm::vec3(0.25f);
		drawCall.mPosition 		= glm::vec3(-0.75f, 0.75f, 0.0f);
		drawCall.mMesh 			= mMeshManager.getMeshID("2DSquare");
		//drawCall.mTexture		= mOpenGLAPI->getTextureID("tiles");
	}
	{
		DrawCall& drawCall		= mDrawCalls.Create(ECS::CreateEntity());
		drawCall.mScale 		= glm::vec3(0.25f);
		drawCall.mPosition 		= glm::vec3(0.0f, 0.75f, 0.0f);
		drawCall.mMesh 			= mMeshManager.getMeshID("2DSquare");
		drawCall.mDrawMode 		= DrawMode::Wireframe;
	}
	{
		DrawCall& drawCall 		= mDrawCalls.Create(ECS::CreateEntity());
		drawCall.mPosition 		= glm::vec3(1.f, 0.f, 0.f);
		drawCall.mMesh 			= mMeshManager.getMeshID("3DCube");
		drawCall.mTexture 		= mTextureManager.getTextureID("marcy");
	}
	{
		DrawCall& drawCall 		= mDrawCalls.Create(ECS::CreateEntity());
		drawCall.mScale 		= glm::vec3(0.25f);
		drawCall.mPosition 		= glm::vec3(-0.75f, -0.75f, 0.0f);
		drawCall.mMesh 			= mMeshManager.getMeshID("2DTriangle");
		//drawCall.mTexture	= mOpenGLAPI->getTextureID("tiles");
	}
	{
		DrawCall& drawCall 		= mDrawCalls.Create(ECS::CreateEntity());
		drawCall.mScale 		= glm::vec3(0.25f, 0.5f, 0.25f);
		drawCall.mPosition 		= glm::vec3(0.0f, -0.75f, 0.0f);
		drawCall.mMesh 			= mMeshManager.getMeshID("2DTriangle");
	}
	{
		DrawCall& drawCall 		= mDrawCalls.Create(ECS::CreateEntity());
		drawCall.mScale 		= glm::vec3(0.25f);
		drawCall.mPosition 		= glm::vec3(0.75f, -0.75f, 0.0f);
		drawCall.mMesh 			= mMeshManager.getMeshID("2DTriangle");
	}
}

Renderer::~Renderer()
{
	delete mOpenGLAPI;
}

void Renderer::onFrameStart()
{
	mOpenGLAPI->onFrameStart();

	if (ImGui::Begin("Render options", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Checkbox("Render light positions", &mLightManager.mRenderLightPositions);
	}
	ImGui::End();

	static ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize;
	if (ImGui::Begin("ImGui options", nullptr, window_flags))
	{
		ImGuiIO &io = ImGui::GetIO();
		ImGui::SliderFloat("FontGlobalScale", &io.FontGlobalScale, 0.1f, 5.f);
		ImGui::SliderFloat2("DisplaySize", &io.DisplaySize.x, 1.f, 3840.f);
		ImGui::Checkbox("FontAllowUserScaling", &io.FontAllowUserScaling);
		ImGui::Checkbox("ConfigDockingWithShift", &io.ConfigDockingWithShift);
		ImGui::SliderFloat("Mainviewport DpiScale", &ImGui::GetMainViewport()->DpiScale, 1.f, 300.f);

		if (ImGui::TreeNode("Window options"))
		{
			ImGui::Text("These options only affect the parent 'ImGui options' window");
			struct ImGuiWindowOption
			{
				const char *displayName;
				const int ImGuiFlag;
				bool set;
			};
			static std::array<ImGuiWindowOption, 24> ImGuiWindowOptions =
				{{{"NoTitleBar", ImGuiWindowFlags_NoTitleBar, false},
				  {"NoResize", ImGuiWindowFlags_NoResize, false},
				  {"NoMove", ImGuiWindowFlags_NoMove, false},
				  {"NoScrollbar", ImGuiWindowFlags_NoScrollbar, false},
				  {"NoScrollWithMouse", ImGuiWindowFlags_NoScrollWithMouse, false},
				  {"NoCollapse  ", ImGuiWindowFlags_NoCollapse, false},
				  {"AlwaysAutoResize", ImGuiWindowFlags_AlwaysAutoResize, false},
				  {"NoBackground", ImGuiWindowFlags_NoBackground, false},
				  {"NoSavedSettings", ImGuiWindowFlags_NoSavedSettings, false},
				  {"NoMouseInputs", ImGuiWindowFlags_NoMouseInputs, false},
				  {"MenuBar", ImGuiWindowFlags_MenuBar, false},
				  {"HorizontalScrollbar", ImGuiWindowFlags_HorizontalScrollbar, false},
				  {"NoFocusOnAppearing", ImGuiWindowFlags_NoFocusOnAppearing, false},
				  {"NoBringToFrontOnFocus", ImGuiWindowFlags_NoBringToFrontOnFocus, false},
				  {"AlwaysVerticalScrollbar", ImGuiWindowFlags_AlwaysVerticalScrollbar, false},
				  {"AlwaysHorizontalScrollbar", ImGuiWindowFlags_AlwaysHorizontalScrollbar, false},
				  {"AlwaysUseWindowPadding", ImGuiWindowFlags_AlwaysUseWindowPadding, false},
				  {"NoNavInputs", ImGuiWindowFlags_NoNavInputs, false},
				  {"NoNavFocus", ImGuiWindowFlags_NoNavFocus, false},
				  {"UnsavedDocument", ImGuiWindowFlags_UnsavedDocument, false},
				  {"NoDocking", ImGuiWindowFlags_NoDocking, false},
				  {"NoNav", ImGuiWindowFlags_NoNav, false},
				  {"NoDecoration", ImGuiWindowFlags_NoDecoration, false},
				  {"NoInputs", ImGuiWindowFlags_NoInputs, false}}};

			for (int i = 0; i < ImGuiWindowOptions.size(); i++)
			{

				ImGuiWindowOptions[i].set = ((window_flags & ImGuiWindowOptions[i].ImGuiFlag) == ImGuiWindowOptions[i].ImGuiFlag);

				if (ImGui::Checkbox(ImGuiWindowOptions[i].displayName, &ImGuiWindowOptions[i].set))
				{
					if (ImGuiWindowOptions[i].set)
					{
						// Disabling NoMouseInputs as it results in being locked out of ImGui navigation
						if (((ImGuiWindowOptions[i].ImGuiFlag) & ImGuiWindowFlags_NoMouseInputs) == ImGuiWindowFlags_NoMouseInputs)
						{
							ImGuiWindowOptions[i].set = false;
							continue;
						}

						window_flags |= ImGuiWindowOptions[i].ImGuiFlag;
					}
					else
						window_flags &= ~ImGuiWindowOptions[i].ImGuiFlag;
				}
				if (ImGuiWindowOptions[i].ImGuiFlag == ImGuiWindowFlags_NoNav
				|| ImGuiWindowOptions[i].ImGuiFlag == ImGuiWindowFlags_NoDecoration
				|| ImGuiWindowOptions[i].ImGuiFlag == ImGuiWindowFlags_NoInputs)
				{
					ImGui::SameLine();
					ImGui::Text(" (group action)");
				}
			}
			ImGui::TreePop();
		}
	}
	ImGui::End();

	if (ImGui::Begin("Entity draw options"))
	{
		size_t count = 0;

		mDrawCalls.ModifyForEach([&](DrawCall& pDrawCall)
		{
			count++;
			const std::string title = "Draw call option " + std::to_string(count);

			if(ImGui::TreeNode(title.c_str()))
			{
				ImGui::SliderFloat3("Position", &pDrawCall.mPosition.x, -1.f, 1.f);
				ImGui::SliderFloat3("Rotation", &pDrawCall.mRotation.x, -90.f, 90.f);
				ImGui::SliderFloat3("Scale",  &pDrawCall.mScale.x, 0.1f, 1.5f);

				{ // Draw mode selection
					if (ImGui::BeginCombo("Draw Mode", convert(pDrawCall.mDrawMode).c_str(), ImGuiComboFlags()))
					{
						for (size_t i = 0; i < drawModes.size(); i++)
						{
							if (ImGui::Selectable(drawModes[i].c_str()))
								pDrawCall.mDrawMode = static_cast<DrawMode>(i);
						}
						ImGui::EndCombo();
					}
				}

				{ // Draw style selection
					if (ImGui::BeginCombo("Draw Style", convert(pDrawCall.mDrawStyle).c_str(), ImGuiComboFlags()))
					{
						for (size_t i = 0; i < drawStyles.size(); i++)
						{
							if (ImGui::Selectable(drawStyles[i].c_str()))
								pDrawCall.mDrawStyle = static_cast<DrawStyle>(i);
						}
						ImGui::EndCombo();
					}
				}

				switch (pDrawCall.mDrawStyle)
				{
				case DrawStyle::Textured:
				{
					const std::string currentTexture = pDrawCall.mTexture.has_value() ? mTextureManager.getTextureName(pDrawCall.mTexture.value()) : "No texture set";
					if (ImGui::BeginCombo("Texture", currentTexture.c_str(), ImGuiComboFlags()))
					{
						mTextureManager.ForEach([&](const Texture &texture)
							{
								if (ImGui::Selectable(texture.mName.c_str()))
								{
									pDrawCall.mTexture = texture.mID;
									pDrawCall.mDrawStyle = DrawStyle::Textured;
								}
							});
						ImGui::EndCombo();
					}
				}
				break;
				case DrawStyle::UniformColour:
				{
					if (!pDrawCall.mColour.has_value())
						pDrawCall.mColour = glm::vec3(1.f, 1.f, 1.f);

					ImGui::ColorEdit3("color",  &pDrawCall.mColour.value().x);
				}
				default:
					break;
				}


				ImGui::TreePop();
			}
		});
	}
	ImGui::End();

	mLightManager.outputImGui();
}

void Renderer::draw()
{
	mDrawCalls.ForEach([&](const DrawCall& pDrawCall)
	{
		mOpenGLAPI->draw(pDrawCall);
	});

	if (mLightManager.mRenderLightPositions)
	{
		mLightManager.getPointLights().ForEach([&](const PointLight &pPointLight)
		{
			lightPosition.mPosition = pPointLight.mPosition;
			lightPosition.mColour	= pPointLight.mColour;
			mOpenGLAPI->draw(lightPosition);
		});
	}

	drawCount++;
}

void Renderer::postDraw()
{
	mOpenGLAPI->postDraw();
}