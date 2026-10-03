#include "Viva/Scene.h"

#include "Viva/Assert.h"
#include "Viva/Camera.h"
#include "Viva/MeshRenderer.h"
#include "Viva/Renderer.h"

#include <utility>

namespace Viva {

GameObject& Scene::CreateGameObject(std::string name, GameObject* parent)
{
    // The scene owns every object through a unique_ptr, so the object itself never moves, even
    // when the vector grows: references to it stay valid.
    m_GameObjects.push_back(std::make_unique<GameObject>(*this, std::move(name)));
    GameObject& gameObject = *m_GameObjects.back();
    if (parent)
        gameObject.GetTransform().SetParent(&parent->GetTransform());
    return gameObject;
}

void Scene::Destroy(GameObject& gameObject)
{
    VIVA_ASSERT(&gameObject.GetScene() == this, "Destroy: {} belongs to another scene", gameObject.GetName());
    // Only marked for now: the frame may still be running code that uses it. Scene::Render
    // removes it before drawing.
    gameObject.MarkDestroyed();
}

Camera* Scene::GetMainCamera() const
{
    for (const std::unique_ptr<GameObject>& gameObject : m_GameObjects) {
        if (gameObject->CanUpdate()) {
            if (Camera* camera = gameObject->GetComponent<Camera>())
                return camera;
        }
    }
    return nullptr;
}

// Like Unity, every component's OnStart comes before its first update, fixed or not. The loops
// count with an index because the code they call may create GameObjects, which can move the
// vector's unique_ptrs (see GameObject.cpp). Objects created during this pass are visited too.

void Scene::StartNewComponents()
{
    for (size_t i = 0; i < m_GameObjects.size(); ++i)
        m_GameObjects[i]->StartComponents();
}

void Scene::FixedUpdate(float fixedDt)
{
    StartNewComponents();
    for (size_t i = 0; i < m_GameObjects.size(); ++i)
        m_GameObjects[i]->FixedUpdateComponents(fixedDt);
}

void Scene::Update(float dt)
{
    StartNewComponents();
    for (size_t i = 0; i < m_GameObjects.size(); ++i)
        m_GameObjects[i]->UpdateComponents(dt);
}

void Scene::Render(Renderer& renderer)
{
    RemoveDestroyed();

    const Camera* camera = GetMainCamera();
    if (!camera)
        return;
    renderer.SetCamera(camera->ViewMatrix(), camera->ProjectionMatrix(renderer.GetAspectRatio()));

    for (const std::unique_ptr<GameObject>& gameObject : m_GameObjects) {
        if (!gameObject->CanUpdate())
            continue;
        const MeshRenderer* meshRenderer = gameObject->GetComponent<MeshRenderer>();
        if (meshRenderer && meshRenderer->Mesh && meshRenderer->Material)
            renderer.Submit(meshRenderer->Mesh, meshRenderer->Material, gameObject->GetTransform().WorldMatrix());
    }
}

void Scene::RemoveDestroyed()
{
    // Marked again, because objects may have been put below a destroyed one since it was marked:
    // as in Unity, they go with it.
    for (const std::unique_ptr<GameObject>& gameObject : m_GameObjects) {
        if (gameObject->m_Destroyed)
            gameObject->MarkDestroyed();
    }

    // The destroyed objects move out of the list first, into a local one: their components'
    // destructors may create or destroy other objects, which mustn't change the scene's list while
    // it's being edited. std::erase (C++20, like C#'s List.RemoveAll) then drops the empty
    // unique_ptrs the moves left behind.
    std::vector<std::unique_ptr<GameObject>> destroyed;
    for (std::unique_ptr<GameObject>& gameObject : m_GameObjects) {
        if (gameObject->m_Destroyed)
            destroyed.push_back(std::move(gameObject));
    }
    std::erase(m_GameObjects, nullptr);

    // Leaving this function destroys `destroyed` and the GameObjects in it: their components (a
    // MeshRenderer lets go of its mesh and material, which the renderer frees once the GPU is done
    // with them), then their Transforms, which leave the hierarchy (see ~Transform).
}

} // namespace Viva
