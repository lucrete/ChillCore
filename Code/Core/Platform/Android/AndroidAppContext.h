#ifndef ANDROIDAPPCONTEXT_H
#define ANDROIDAPPCONTEXT_H

struct android_app;

namespace CC
{
    // ========================
    // AndroidAppContext
    // ========================
    //
    // Holds a pointer to the GameActivity native_app_glue android_app
    // structure, registered once by MainAndroid.cpp at startup. Platform
    // backends (PlatformWindowAndroid, PlatformFileSystemAndroid,
    // PlatformInputAndroid) read it to obtain ANativeWindow*,
    // AAssetManager*, internalDataPath, and the input event buffers.
    //
    // Single-threaded access — set once before any backend Init(),
    // queried from the main thread thereafter. No synchronisation.

    void          SetAndroidApp(android_app* pApp);
    android_app*  GetAndroidApp();
}

#endif // ANDROIDAPPCONTEXT_H
