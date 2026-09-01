#ifndef POSTPROCESSEFFECT_H
#define POSTPROCESSEFFECT_H

#include "CCVector3.h"

namespace CC
{
    // ========================
    // PostProcessEffect
    // ========================
    //
    // One tunable effect in the post-process stack: an enable flag plus its
    // parameters. Parameters carry their own name, range, and default so the
    // developer panel builds its sliders and the YAML dump writes its keys
    // without either of them knowing which effects exist.
    //
    // An effect describes what to compute, not how. Every effect but bloom is
    // a uniform-gated block inside the single uber pass, so adding one here
    // costs a shader block rather than another full-screen read and write.

    static const int MAX_POST_PROCESS_PARAMS = 12;

    // A colour counts as one parameter rather than three, which is what keeps
    // a per-channel grade inside the parameter budget, and what lets the
    // developer panel offer a colour picker instead of three sliders.
    enum class PostProcessParamType
    {
        Scalar,
        Color
    };

    struct PostProcessParam
    {
        const char*          name         = nullptr;
        PostProcessParamType type         = PostProcessParamType::Scalar;
        // A scalar uses x alone. Range applies per channel for a colour.
        Vector3              value        = Vector3(0.0f, 0.0f, 0.0f);
        Vector3              defaultValue = Vector3(0.0f, 0.0f, 0.0f);
        float                minValue     = 0.0f;
        float                maxValue     = 1.0f;
    };

    class PostProcessEffect
    {
    public:
        PostProcessEffect();
        ~PostProcessEffect();

        void Configure(const char* effectName, bool isEnabledByDefault);
        void AddParam(const char* paramName, float defaultValue, float minValue, float maxValue);
        void AddColorParam(const char* paramName, const Vector3& defaultValue, float minValue, float maxValue);

        const char* GetName() const;

        void SetEnabled(bool isEnabled);
        bool IsEnabled() const;

        // Named access is what the scene loader and the developer panel use.
        // An unknown name is reported rather than silently ignored, because a
        // typo in a scene file would otherwise look like an effect that does
        // nothing.
        bool SetParamValue(const char* paramName, float value);
        float GetParamValue(const char* paramName) const;

        // A colour read as a scalar returns its red channel, and a scalar
        // written as a colour takes the red channel, so a caller that does not
        // care about the distinction still gets something sensible.
        bool SetParamColor(const char* paramName, const Vector3& value);
        Vector3 GetParamColor(const char* paramName) const;

        int GetParamCount() const;
        PostProcessParam& GetParam(int index);
        const PostProcessParam& GetParam(int index) const;

        // Parameters only. The developer panel's per-effect Reset uses this,
        // where also flipping the enable flag would collapse the very effect
        // the developer is tuning.
        void ResetToDefaults();

        // The enable flag on its own, for a caller restoring a whole stack to
        // the state it configures itself with.
        void ResetEnabledToDefault();

    private:
        int FindParamIndex(const char* paramName) const;

        const char*      name;
        bool             isEnabled;
        bool             isEnabledByDefault;
        PostProcessParam params[MAX_POST_PROCESS_PARAMS];
        int              paramCount;
    };
}

#endif // POSTPROCESSEFFECT_H
