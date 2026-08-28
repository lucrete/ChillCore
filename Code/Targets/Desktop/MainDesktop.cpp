#include <stdlib.h>
#include <stdio.h>
#include "CoreMain.h"
#include "AppMain.h"

int main(void)
{
    CC::CoreMain* coreMain = new CC::CoreMain();

    coreMain->Init();
    AppMain* appMain = new AppMain();
    appMain->Init();
    coreMain->Run(appMain);
    appMain->Shutdown();
    coreMain->Shutdown();

    delete(appMain);
    delete(coreMain);

    exit(EXIT_SUCCESS);
}
