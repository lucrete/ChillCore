#ifndef POSTPROCESSEFFECT_H
#define POSTPROCESSEFFECT_H

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

    struct PostProcessParam
    {
        const char* name         = nullptr;
        float       value        = 0.0f;
        float       defaultValue = 0.0f;
        float       minValue     = 0.0f;
        float       maxValue     = 1.0f;
    };

    class PostProcessEffect
    {
    public:
        PostProcessEffect();
        ~PostProcessEffect();

        void Configure(const char* effectName, bool isEnabledByDefault);
        void AddParam(const char* paramName, float defaultValue, float minValue, float maxValue);

        const char* GetName() const;

        void SetEnabled(bool isEnabled);
        bool IsEnabled() const;

        // Named access is what the scene loader and the developer panel use.
        // An unknown name is reported rather than silently ignored, because a
        // typo in a scene file would otherwise look like an effect that does
        // nothing.
        bool SetParamValue(const char* paramName, float value);
        float GetParamValue(const char* paramName) const;

        int GetParamCount() const;
        PostProcessParam& GetParam(int index);
        const PostProcessParam& GetParam(int index) const;

        void ResetToDefaults();

    private:
        int FindParamIndex(const char* paramName) const;

        const char*      name;
        bool             isEnabled;
        PostProcessParam params[MAX_POST_PROCESS_PARAMS];
        int              paramCount;
    };
}

#endif // POSTPROCESSEFFECT_H
