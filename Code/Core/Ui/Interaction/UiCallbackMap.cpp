#include "UiCallbackMap.h"

namespace CC
{
    void UiCallbackMap::RegisterButton(const std::string& key, std::function<void()> callback)
    {
        buttonCallbacks[key] = callback;
    }

    void UiCallbackMap::RegisterSlider(const std::string& key, std::function<void(float)> callback)
    {
        sliderCallbacks[key] = callback;
    }

    void UiCallbackMap::RegisterToggle(const std::string& key, std::function<void(bool)> callback)
    {
        toggleCallbacks[key] = callback;
    }

    void UiCallbackMap::RegisterDropdown(const std::string& key, std::function<void(int)> callback)
    {
        dropdownCallbacks[key] = callback;
    }

    void UiCallbackMap::RegisterJoystick(const std::string& key, std::function<void(float, float)> callback)
    {
        joystickCallbacks[key] = callback;
    }

    bool UiCallbackMap::InvokeButton(const std::string& key)
    {
        auto it = buttonCallbacks.find(key);
        if (it != buttonCallbacks.end())
        {
            it->second();
            return true;
        }
        return false;
    }

    bool UiCallbackMap::InvokeSlider(const std::string& key, float value)
    {
        auto it = sliderCallbacks.find(key);
        if (it != sliderCallbacks.end())
        {
            it->second(value);
            return true;
        }
        return false;
    }

    bool UiCallbackMap::InvokeToggle(const std::string& key, bool value)
    {
        auto it = toggleCallbacks.find(key);
        if (it != toggleCallbacks.end())
        {
            it->second(value);
            return true;
        }
        return false;
    }

    bool UiCallbackMap::InvokeDropdown(const std::string& key, int index)
    {
        auto it = dropdownCallbacks.find(key);
        if (it != dropdownCallbacks.end())
        {
            it->second(index);
            return true;
        }
        return false;
    }

    bool UiCallbackMap::InvokeJoystick(const std::string& key, float x, float y)
    {
        bool result = false;
        auto it = joystickCallbacks.find(key);
        if (it != joystickCallbacks.end())
        {
            it->second(x, y);
            result = true;
        }
        return result;
    }

}
