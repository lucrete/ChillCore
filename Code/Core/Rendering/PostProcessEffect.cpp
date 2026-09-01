#include "PostProcessEffect.h"

#include <cstring>

#include "CCAssert.h"

namespace CC
{
    PostProcessEffect::PostProcessEffect()
        : name("")
        , isEnabled(false)
        , isEnabledByDefault(false)
        , paramCount(0)
    {
    }

    PostProcessEffect::~PostProcessEffect()
    {
    }

    void PostProcessEffect::Configure(const char* effectName, bool _isEnabledByDefault)
    {
        name = effectName;
        isEnabled = _isEnabledByDefault;
        isEnabledByDefault = _isEnabledByDefault;
        paramCount = 0;
    }

    void PostProcessEffect::ResetEnabledToDefault()
    {
        isEnabled = isEnabledByDefault;
    }

    void PostProcessEffect::AddParam(const char* paramName, float defaultValue, float minValue, float maxValue)
    {
        AddColorParam(paramName, Vector3(defaultValue, defaultValue, defaultValue), minValue, maxValue);
        params[paramCount - 1].type = PostProcessParamType::Scalar;
    }

    void PostProcessEffect::AddColorParam(const char* paramName, const Vector3& defaultValue, float minValue, float maxValue)
    {
        CC_ASSERT(paramCount < MAX_POST_PROCESS_PARAMS, "Too many post-process parameters for one effect");

        PostProcessParam& param = params[paramCount];
        param.name         = paramName;
        param.type         = PostProcessParamType::Color;
        param.value        = defaultValue;
        param.defaultValue = defaultValue;
        param.minValue     = minValue;
        param.maxValue     = maxValue;
        paramCount++;
    }

    const char* PostProcessEffect::GetName() const
    {
        return name;
    }

    void PostProcessEffect::SetEnabled(bool _isEnabled)
    {
        isEnabled = _isEnabled;
    }

    bool PostProcessEffect::IsEnabled() const
    {
        return isEnabled;
    }

    bool PostProcessEffect::SetParamValue(const char* paramName, float value)
    {
        return SetParamColor(paramName, Vector3(value, value, value));
    }

    float PostProcessEffect::GetParamValue(const char* paramName) const
    {
        return GetParamColor(paramName).x;
    }

    bool PostProcessEffect::SetParamColor(const char* paramName, const Vector3& value)
    {
        bool result = false;
        int index = FindParamIndex(paramName);

        if (index >= 0)
        {
            params[index].value = value;
            result = true;
        }

        return result;
    }

    Vector3 PostProcessEffect::GetParamColor(const char* paramName) const
    {
        Vector3 result(0.0f, 0.0f, 0.0f);
        int index = FindParamIndex(paramName);

        if (index >= 0)
        {
            result = params[index].value;
        }

        return result;
    }

    int PostProcessEffect::GetParamCount() const
    {
        return paramCount;
    }

    PostProcessParam& PostProcessEffect::GetParam(int index)
    {
        CC_ASSERT(index >= 0 && index < paramCount, "Post-process parameter index out of range");
        return params[index];
    }

    const PostProcessParam& PostProcessEffect::GetParam(int index) const
    {
        CC_ASSERT(index >= 0 && index < paramCount, "Post-process parameter index out of range");
        return params[index];
    }

    void PostProcessEffect::ResetToDefaults()
    {
        for (int i = 0; i < paramCount; i++)
        {
            params[i].value = params[i].defaultValue;
        }
    }

    // ========================
    // Private
    // ========================

    int PostProcessEffect::FindParamIndex(const char* paramName) const
    {
        int result = -1;

        for (int i = 0; i < paramCount && result < 0; i++)
        {
            if (strcmp(params[i].name, paramName) == 0)
            {
                result = i;
            }
        }

        return result;
    }
}
