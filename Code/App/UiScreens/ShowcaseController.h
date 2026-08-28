#ifndef SHOWCASECONTROLLER_H
#define SHOWCASECONTROLLER_H

#include "UiScreenController.h"

class ShowcaseController : public CC::UiScreenController
{
public:
    void Init() override;
    void OnEnter() override;
};

#endif // SHOWCASECONTROLLER_H
