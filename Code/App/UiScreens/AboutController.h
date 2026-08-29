#ifndef ABOUTCONTROLLER_H
#define ABOUTCONTROLLER_H

#include "UiScreenController.h"

class AboutController : public CC::UiScreenController
{
public:
    void Init() override;

private:
    void SetElementText(const char* elementId, const char* text);
};

#endif // ABOUTCONTROLLER_H
