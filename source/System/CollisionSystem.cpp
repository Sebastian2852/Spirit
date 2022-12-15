#include "CollisionSystem.hpp"

// ECS
#include "Storage.hpp"

// System
#include "MeshSystem.hpp"
#include "SceneSystem.hpp"

// Component
#include "Transform.hpp"
#include "Mesh.hpp"
#include "Collider.hpp"

// Geometry
#include "Intersect.hpp"
#include "Ray.hpp"

namespace System
{
    CollisionSystem::CollisionSystem(SceneSystem& pSceneSystem, const MeshSystem& pMeshSystem)
        : mSceneSystem{pSceneSystem}
        , mMeshSystem{pMeshSystem}
    {}

    void CollisionSystem::checkCollisions()
    {
        auto& scene = mSceneSystem.getCurrentScene();
        scene.foreach([&](Component::Collider& pColliderA, Component::Transform& pTransformA, Component::MeshDraw& pMeshA)
        {
            scene.foreach([&](Component::Collider& pColliderB, Component::Transform& pTransformB, Component::MeshDraw& pMeshB)
            {
                if (&pColliderA != &pColliderB)
                {
                    auto& AABB_A = mMeshSystem.getMesh(pMeshA.mID).mAABB;
                    const auto rotateScale_A = glm::scale(glm::mat4_cast(pTransformA.mOrientation), pTransformA.mScale);
                    const auto worldSpaceAABB_A = Geometry::AABB::transform(AABB_A, pTransformA.mPosition, rotateScale_A);

                    auto& AABB_B = mMeshSystem.getMesh(pMeshB.mID).mAABB;
                    const auto rotateScale_B = glm::scale(glm::mat4_cast(pTransformB.mOrientation), pTransformB.mScale);
                    const auto worldSpaceAABB_B = Geometry::AABB::transform(AABB_B, pTransformB.mPosition, rotateScale_B);

                    if (Geometry::intersect(worldSpaceAABB_A, worldSpaceAABB_B))
                    {
                        pColliderA.mCollided = true;
                        pColliderB.mCollided = true;
                    }
                    else
                    {
                        pColliderA.mCollided = false;
                        pColliderB.mCollided = false;
                    }
                }
            });

        });
    }

    bool CollisionSystem::castRay(const Geometry::Ray& pRay, glm::vec3& outFirstIntersection) const
    {
        std::optional<float> nearestIntersectionAlongRay;

        mSceneSystem.getCurrentScene().foreach([&](Component::Collider& pCollider, Component::Transform& pTransform, Component::MeshDraw& pMesh)
        {
            auto& AABB = mMeshSystem.getMesh(pMesh.mID).mAABB;
            const auto rotateScale = glm::scale(glm::mat4_cast(pTransform.mOrientation), pTransform.mScale);
            const auto worldSpaceAABB = Geometry::AABB::transform(AABB, pTransform.mPosition, rotateScale);

            glm::vec3 collisionPoint;
            float lengthAlongRay;
            if (Geometry::intersect(worldSpaceAABB, pRay, &collisionPoint, &lengthAlongRay))
            {
                pCollider.mCollided = true;

                if (!nearestIntersectionAlongRay.has_value() || lengthAlongRay < nearestIntersectionAlongRay)
                    outFirstIntersection = collisionPoint;
            }
        });

        return nearestIntersectionAlongRay.has_value();
    }

    std::vector<std::pair<ECS::EntityID, float>> CollisionSystem::getEntitiesAlongRay(const Geometry::Ray& pRay) const
    {
        std::vector<std::pair<ECS::EntityID, float>> entitiesAndDistance;

        mSceneSystem.getCurrentScene().foreach([&](ECS::EntityID& pEntity, Component::Collider& pCollider, Component::Transform& pTransform, Component::MeshDraw& pMesh)
        {
            auto& AABB = mMeshSystem.getMesh(pMesh.mID).mAABB;
            const auto rotateScale = glm::scale(glm::mat4_cast(pTransform.mOrientation), pTransform.mScale);
            const auto worldSpaceAABB = Geometry::AABB::transform(AABB, pTransform.mPosition, rotateScale);

            glm::vec3 collisionPoint;
            float lengthAlongRay;
            if (Geometry::intersect(worldSpaceAABB, pRay, &collisionPoint, &lengthAlongRay))
                entitiesAndDistance.push_back({pEntity, lengthAlongRay});
        });

        return entitiesAndDistance;
    }
} // namespace System