// DevUiStub.cpp
//
// No-op replacement for DevUi.cpp on builds where the developer overlay
// is disabled (CC_DISABLE_DEVUI). The real DevUi pulls in ImGui + the
// GLFW backend, neither of which exists on Android. The Android
// CMakeLists compiles this file *instead of* DevUi.cpp / DevUiView*.cpp
// / CommandConsole.cpp; the rest of the engine sees the same DevUi.h
// surface and continues to call DevUi::Get()->Update() etc., all of
// which are silently no-ops here.

#include "DevUi.h"
#include "CCAssert.h"

namespace CC
{
    DevUi* DevUi::instance = nullptr;

    DevUi::DevUi()
    {
        CC_ASSERT(instance == nullptr, "DevUi already created");
        instance = this;
    }

    DevUi::~DevUi()
    {
        instance = nullptr;
    }

    DevUi* DevUi::Get()
    {
        CC_ASSERT(instance != nullptr, "DevUi not created yet");
        return instance;
    }

    void DevUi::Init()       {}
    void DevUi::Shutdown()   {}
    void DevUi::Update()     {}
    void DevUi::StartFrame() {}
    void DevUi::EndFrame()   {}

    bool DevUi::IsHovered()
    {
        return false;
    }

    bool DevUi::IsCapturingKeyboard()
    {
        return false;
    }

    void DevUi::SetContextText(const std::string& text)
    {
        contextText = text;
    }
}
