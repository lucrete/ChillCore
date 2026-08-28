#ifndef PROCEDURALARTCONTROLLER_H
#define PROCEDURALARTCONTROLLER_H

#include "RenderableFullscreenQuad.h"
#include "InputManager.h"
#include "Material.h"
#include <vector>
#include <string>

class ProceduralArtController
{
public:
    ProceduralArtController();
    ~ProceduralArtController();

    void Init();
    void Shutdown();
    void Update();
    void OnRender(CC::Material& material);

private:
    enum Fractal2DActions
    {
        MoveLeft = CC::InputAction::GameActionStart,
        MoveRight,
        MoveUp,
        MoveDown,
        ZoomIn,
        ZoomOut,
        NextEffect,
        PrevEffect,
        Fractal2DActionMax
    };

    struct EffectDefinition
    {
        CC::Material* material;
        std::string name;
        bool useCustomControls;

        EffectDefinition(CC::Material* mat, const std::string& effectName, bool customControls)
            : material(mat), name(effectName), useCustomControls(customControls) {
        }
    };

    void InitEffects();
    void CycleEffect(int direction);

    CC::RenderableFullscreenQuad* renderableFullscreenQuad;
    std::vector<EffectDefinition> effects;
    int currentEffectIndex;

    CC::Vector2 center;
    float scale;
    float velocityMove;
    float velocityScale;

};

#endif // PROCEDURALARTCONTROLLER_H