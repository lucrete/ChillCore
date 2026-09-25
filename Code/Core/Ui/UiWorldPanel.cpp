#include "UiWorldPanel.h"

#include <stdlib.h>

#include "CCAssert.h"
#include "CCMat4x4.h"
#include "ComponentFactory.h"
#include "Material.h"
#include "MaterialManager.h"
#include "PrintManager.h"
#include "RenderableQuad.h"
#include "SceneObject.h"
#include "UiManager.h"
#include "UiSurface.h"

namespace CC
{
    UiWorldPanel::UiWorldPanel(const std::string& _surfaceName, int _textureWidth, int _textureHeight)
        : surfaceName(_surfaceName)
        , textureWidth(_textureWidth)
        , textureHeight(_textureHeight)
    {
    }

    UiWorldPanel::~UiWorldPanel()
    {
    }

    void UiWorldPanel::SetEnabled(bool _isEnabled)
    {
        Component::SetEnabled(_isEnabled);

        if (surface != nullptr)
        {
            surface->SetEnabled(_isEnabled);
        }
    }

    void UiWorldPanel::Init()
    {
        CC_ASSERT(owner != nullptr, "UiWorldPanel needs an owner");

        surface = UiManager::Get()->CreateSurface(surfaceName, textureWidth, textureHeight);

        // One material per panel: the texture override is what makes it show
        // this panel rather than another.
        std::string materialName = "UiWorldPanel_" + surfaceName;
        if (!MaterialManager::Get()->HasMaterial(materialName))
        {
            MaterialManager::Get()->CreateMaterial(materialName, "TextureShader");
        }
        material = MaterialManager::Get()->GetMaterial(materialName);

        // The surface clears to transparent, so whatever the screen does not
        // cover has to blend away rather than become a slab.
        material->SetAlphaMode(AlphaBlendMode::Blend);
        material->SetTextureHandleOverride(surface->GetOutputTexture());

        owner->AddComponent(new RenderableQuad(material));

        surface->SetEnabled(isEnabled);

        RebuildPlane();
        UiManager::Get()->GetPointerRouter().RegisterTarget(this);
    }

    void UiWorldPanel::Update()
    {
        RebuildPlane();
    }

    void UiWorldPanel::Shutdown()
    {
        UiManager::Get()->GetPointerRouter().UnregisterTarget(this);

        if (surface != nullptr)
        {
            UiManager::Get()->DestroySurface(surface);
            surface = nullptr;
        }

        // The material outlives the panel — MaterialManager owns it — so the
        // override has to go, or it points at a destroyed texture.
        if (material != nullptr)
        {
            material->SetTextureHandleOverride(Gfx::TextureHandle());
            material = nullptr;
        }
    }

    // ========================
    // Plane
    // ========================
    void UiWorldPanel::RebuildPlane()
    {
        Mat4x4 worldMatrix;
        owner->GetWorldMatrix(worldMatrix);

        // Column-major: the first three columns are the basis vectors with
        // scale still in them, the fourth is the translation.
        Vector3 scaledRight (worldMatrix.m[0][0], worldMatrix.m[0][1], worldMatrix.m[0][2]);
        Vector3 scaledUp    (worldMatrix.m[1][0], worldMatrix.m[1][1], worldMatrix.m[1][2]);
        Vector3 scaledNormal(worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2]);

        // The quad spans -0.5 to 0.5 in its own X and Y, so the basis length
        // is the panel's full width and height in metres.
        halfWidthMetres  = scaledRight.Magnitude() * 0.5f;
        halfHeightMetres = scaledUp.Magnitude() * 0.5f;

        scaledRight.Normalize();
        scaledUp.Normalize();
        scaledNormal.Normalize();

        planeRight  = scaledRight;
        planeUp     = scaledUp;
        planeNormal = scaledNormal;
        planeOrigin = Vector3(worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2]);
    }

    // ========================
    // UiPointerTarget
    // ========================
    bool UiWorldPanel::IntersectPointerRay(const Vector3& origin, const Vector3& direction,
                                           float maxDistance, float& outDistance) const
    {
        bool isHit = false;

        if (isEnabled && owner != nullptr && owner->IsEnabled())
        {
            // Vector3's arithmetic is not const-qualified, so the plane and
            // the ray are copied before use.
            Vector3 rayOrigin = origin;
            Vector3 rayDirection = direction;
            Vector3 normal = planeNormal;
            Vector3 panelOrigin = planeOrigin;

            float denominator = rayDirection.Dot(normal);

            // Near zero means the ray runs along the panel rather than into
            // it, and the intersection is either nowhere or everywhere.
            if (denominator < -0.0001f || denominator > 0.0001f)
            {
                Vector3 toPanel = panelOrigin - rayOrigin;
                float distance = toPanel.Dot(normal) / denominator;

                if (distance > 0.0f && distance <= maxDistance)
                {
                    Vector3 hit = rayOrigin + rayDirection * distance;

                    float pixelX = 0.0f;
                    float pixelY = 0.0f;
                    if (HitToPixels(hit, pixelX, pixelY))
                    {
                        outDistance = distance;
                        isHit = true;
                    }
                }
            }
        }

        return isHit;
    }

    bool UiWorldPanel::HitToPixels(const Vector3& hit, float& outPixelX, float& outPixelY) const
    {
        bool isInside = false;

        Vector3 hitPoint = hit;
        Vector3 panelOrigin = planeOrigin;
        Vector3 right = planeRight;
        Vector3 up = planeUp;

        Vector3 local = hitPoint - panelOrigin;
        float localX = local.Dot(right);
        float localY = local.Dot(up);

        if (localX >= -halfWidthMetres && localX <= halfWidthMetres
         && localY >= -halfHeightMetres && localY <= halfHeightMetres)
        {
            // Panel space is centred and Y-up; the UI is corner-origin and
            // Y-down.
            outPixelX = ((localX + halfWidthMetres) / (halfWidthMetres * 2.0f)) * (float)textureWidth;
            outPixelY = (1.0f - ((localY + halfHeightMetres) / (halfHeightMetres * 2.0f))) * (float)textureHeight;
            isInside = true;
        }

        return isInside;
    }

    void UiWorldPanel::OnPointerHit(int pointerId, const Vector3& origin, const Vector3& direction,
                                    float distance, bool isPressed)
    {
        Vector3 rayOrigin = origin;
        Vector3 rayDirection = direction;
        Vector3 hit = rayOrigin + rayDirection * distance;

        UiPointerState pointer;
        if (HitToPixels(hit, pointer.x, pointer.y))
        {
            pointer.isDown = isPressed;
            pointer.isActive = true;
        }

        surface->GetInputHandler().SetPointer(pointerId, pointer);
        isPointerOn[pointerId] = pointer.isActive;
    }

    void UiWorldPanel::OnPointerLeft(int pointerId)
    {
        surface->GetInputHandler().ClearPointer(pointerId);
        isPointerOn[pointerId] = false;
    }

    bool UiWorldPanel::IsPointerOn(int pointerId) const
    {
        bool result = false;

        if (pointerId >= 0 && pointerId < UiPointerRouter::MAX_POINTERS)
        {
            result = isPointerOn[pointerId];
        }

        return result;
    }
}

// ========================
// Scene file registration
// ========================
static CC::Component* CreateUiWorldPanel(ryml::ConstNodeRef componentData)
{
    std::string surfaceName = "UiWorldPanel";
    int textureWidth  = 1024;
    int textureHeight = 768;

    if (componentData.has_child("surface"))
    {
        surfaceName = NodeToString(componentData["surface"]);
    }
    if (componentData.has_child("textureWidth"))
    {
        textureWidth = atoi(NodeToString(componentData["textureWidth"]).c_str());
    }
    if (componentData.has_child("textureHeight"))
    {
        textureHeight = atoi(NodeToString(componentData["textureHeight"]).c_str());
    }

    return new CC::UiWorldPanel(surfaceName, textureWidth, textureHeight);
}

static CC::ComponentRegistrar uiWorldPanelRegistrar("UiWorldPanel", CreateUiWorldPanel);
