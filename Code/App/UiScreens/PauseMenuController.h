#ifndef PAUSEMENUCONTROLLER_H
#define PAUSEMENUCONTROLLER_H

#include "UiScreenController.h"
#include <functional>

class PauseMenuController : public CC::UiScreenController
{
public:
    PauseMenuController(std::function<void()> onResume, std::function<void()> onBackToMenu,
                        std::function<void()> onQuit);

    void Init() override;

private:
    std::function<void()> onResumeCallback;
    std::function<void()> onBackToMenuCallback;
    std::function<void()> onQuitCallback;
};

#endif // PAUSEMENUCONTROLLER_H
