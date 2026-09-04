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
    AppMain* appMain = new AppMain();
    appMain->Init();

    // An XR runtime takes over frame pacing, so it drives TickFrame itself
    // rather than going through Run. Absent one — no headset, no runtime
    // installed — Init fails and the flat desktop loop runs unchanged.
#ifdef CC_ENABLE_XR
    CC::XrManager* xrManager = new CC::XrManager();
    if (xrManager->Init())
    {
        xrManager->RunFrameLoop(coreMain, appMain);
    }
    else
    {
        coreMain->Run(appMain);
    }
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
