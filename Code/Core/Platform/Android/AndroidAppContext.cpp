#include "AndroidAppContext.h"

namespace CC
{
    static android_app* gAndroidApp = nullptr;

    void SetAndroidApp(android_app* pApp)
    {
        gAndroidApp = pApp;
    }

    android_app* GetAndroidApp()
    {
        return gAndroidApp;
    }
}
