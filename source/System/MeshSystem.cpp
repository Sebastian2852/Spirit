#include "MeshSystem.hpp"

// System
#include "TextureSystem.hpp"

// Utility
#include "File.hpp"
#include "Logger.hpp"

namespace System
{
    MeshSystem::MeshSystem(TextureSystem& pTextureSystem) noexcept
        : mTextureSystem{pTextureSystem}
        , mAvailableModels{}
        , mModelManager{}
        , mConePrimitive{mModelManager.create(Utility::File::modelDirectory / "cone" / "cone_32.obj", mTextureSystem.mTextureManager)}
        , mCubePrimitive{mModelManager.create(Utility::File::modelDirectory / "cube" / "cube.obj", mTextureSystem.mTextureManager)}
        , mCylinderPrimitive{mModelManager.create(Utility::File::modelDirectory / "cylinder" / "cylinder_32.obj", mTextureSystem.mTextureManager)}
        , mPlanePrimitive{mModelManager.create(Utility::File::modelDirectory / "plane" / "plane.obj", mTextureSystem.mTextureManager)}
        , mSpherePrimitive{mModelManager.create(Utility::File::modelDirectory / "Sphere" / "Icosphere_2.obj", mTextureSystem.mTextureManager)}
    {
        Utility::File::forEachFileRecursive(Utility::File::modelDirectory,
            [&](const std::filesystem::directory_entry& entry)
            {
                if (entry.is_regular_file() && entry.path().has_extension() && entry.path().extension() == ".obj")
                    mAvailableModels.push_back(entry.path());
            });
    }

    ModelRef MeshSystem::getModel(const std::filesystem::path& pFilePath)
    {
        return mModelManager.getOrCreate([&pFilePath](const Data::Model& pModel)
        {
            return pModel.mFilePath == pFilePath;
        }, pFilePath, mTextureSystem.mTextureManager);
    }
} // namespace System