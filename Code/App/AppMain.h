#ifndef APPMAIN_H
#define APPMAIN_H

#include "AppMainInterface.h"
#include "StateMachine.h"

class AppMain : public CC::IAppMain
{
public:
    AppMain();
    virtual ~AppMain();

    virtual void Init();
    virtual void Update();
    virtual void Shutdown();

private:

    void LoadGlobalUiSfx();

    CC::StateMachine* stateMachine;
};

#endif

