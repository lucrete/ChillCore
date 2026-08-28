#ifndef OPTIONSCONTROLLER_H
#define OPTIONSCONTROLLER_H

#include "UiScreenController.h"

class OptionsController : public CC::UiScreenController
{
public:
    void Init() override;
    void OnEnter() override;
};

#endif // OPTIONSCONTROLLER_H
