#ifndef MAINMENUCONTROLLER_H
#define MAINMENUCONTROLLER_H

#include "UiScreenController.h"
#include <functional>

class MainMenuController : public CC::UiScreenController
{
public:
    MainMenuController();
    MainMenuController(std::function<void()> onEnterShowcase);

    void Init() override;

private:
    std::function<void()> onEnterShowcaseCallback;
};

#endif // MAINMENUCONTROLLER_H
