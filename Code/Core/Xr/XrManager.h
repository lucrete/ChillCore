#ifndef XRMANAGER_H
#define XRMANAGER_H

#include "XrTypes.h"

namespace CC
{
    class CoreMain;
    class IAppMain;
    class CameraXrEye;

    // ========================
    // XrManager
    // ========================
    //
    // Owns the OpenXR instance, system, session and swapchains, and drives
    // the frame loop that replaces CoreMain::Run when a runtime is present.
    //
    // The OpenXR types stay behind an opaque state pointer so nothing above
    // this header sees them, and so a target built without the loader does
    // not need its headers on the include path.

    class XrManager
    {
    public:
        XrManager();
        ~XrManager();

        static XrManager* Get();

        // Creates the instance, system and session. False where no runtime is
        // installed or the system is unavailable, which is the ordinary case
        // on a machine with no headset and is not an error: the caller falls
        // back to the flat desktop path.
        bool Init();
        void Shutdown();

        // Drives frames until the runtime asks the session to end or the
        // application requests a quit. Replaces CoreMain::Run.
        void RunFrameLoop(CoreMain* coreMain, IAppMain* appMain);

        int GetViewCount() const;
        const XrEyeView& GetEyeView(int index) const;

        bool IsSessionRunning() const;

        static const int MAX_EYE_VIEWS = 2;

    private:
        static XrManager* instance;

        struct XrState;
        XrState* state;

        XrEyeView    eyeViews[MAX_EYE_VIEWS];
        CameraXrEye* eyeCameras[MAX_EYE_VIEWS];
        int          viewCount;

        bool isSessionRunning;
        bool isExitRequested;

        // ========================
        // Frame
        // ========================

        void PollEvents();

        // Waits on the runtime's frame pacing and locates the views against
        // the predicted display time. False where the runtime says this frame
        // should not be rendered, in which case the frame is still submitted
        // empty so pacing is preserved.
        bool BeginXrFrame();
        void EndXrFrame();

        void PublishRenderViews();

        // ========================
        // Setup
        // ========================

        bool CreateInstanceAndSystem();
        bool CreateSession();
        bool CreateReferenceSpace();
        bool CreateSwapchains();
        void DestroySwapchains();
    };
}

#endif // XRMANAGER_H
