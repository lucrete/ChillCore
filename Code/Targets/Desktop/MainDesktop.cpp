#include <stdlib.h>
#include <stdio.h>
#include "CoreMain.h"
#include "AppMain.h"

#ifdef CC_ENABLE_XR
#include "XrManager.h"
#endif

int main(void)
{
    CC::CoreMain* coreMain = new CC::CoreMain();

    coreMain->Init();

    // Before AppMain, so a state's Init can see whether XR exists at all.
    // Init only creates the instance; no session runs until one is started.
#ifdef CC_ENABLE_XR
    CC::XrManager* xrManager = new CC::XrManager();
    bool isXrAvailable = xrManager->Init();
#endif

    AppMain* appMain = new AppMain();
    appMain->Init();

    // With a headset present the application boots into it. The loop runs
    // whether or not a session is live, so it drives the frame either way and
    // XR can be entered later even when none was found at launch.
#ifdef CC_ENABLE_XR
    if (isXrAvailable)
    {
        xrManager->StartSession();
    }
    xrManager->RunFrameLoop(coreMain, appMain);
    delete(xrManager);
#else
    coreMain->Run(appMain);
#endif

    appMain->Shutdown();
    coreMain->Shutdown();

    delete(appMain);
    delete(coreMain);

    exit(EXIT_SUCCESS);
}
