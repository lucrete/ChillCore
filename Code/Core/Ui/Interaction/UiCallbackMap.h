#ifndef UICALLBACKMAP_H
#define UICALLBACKMAP_H

#include <string>
#include <unordered_map>
#include <functional>

namespace CC
{
    class UiCallbackMap
    {
    public:
        void RegisterButton(const std::string& key, std::function<void()> callback);
        void RegisterSlider(const std::string& key, std::function<void(float)> callback);
        void RegisterToggle(const std::string& key, std::function<void(bool)> callback);
        void RegisterDropdown(const std::string& key, std::function<void(int)> callback);
        void RegisterJoystick(const std::string& key, std::function<void(float, float)> callback);

        bool InvokeButton(const std::string& key);
        bool InvokeSlider(const std::string& key, float value);
        bool InvokeToggle(const std::string& key, bool value);
        bool InvokeDropdown(const std::string& key, int index);
        bool InvokeJoystick(const std::string& key, float x, float y);

    private:
        std::unordered_map<std::string, std::function<void()>> buttonCallbacks;
        std::unordered_map<std::string, std::function<void(float)>> sliderCallbacks;
        std::unordered_map<std::string, std::function<void(bool)>> toggleCallbacks;
        std::unordered_map<std::string, std::function<void(int)>> dropdownCallbacks;
        std::unordered_map<std::string, std::function<void(float, float)>> joystickCallbacks;
    };
}

#endif // UICALLBACKMAP_H
