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

        // Creates the instance and finds the system. False where no runtime
        // is installed or no headset is present, which is ordinary and not an
        // error. No session is started; the instance outlives every session.
        bool Init();
        void Shutdown();

        // A session owns the swapchains, eye targets, eye cameras and action
        // set, so each one builds and tears down all of them. Both are safe
        // to call when already in that state.
        // Whether a runtime and headset were found. False until Init has
        // succeeded, which StartSession retries so a headset connected after
        // launch can still be entered.
        bool IsAvailable() const;

        bool StartSession();
        void EndSession();
        bool IsSessionActive() const;

        // Requests taken up between frames. Starting or ending a session
        // mid-frame would pull targets out from under an open render pass.
        void RequestStartSession();
        void RequestEndSession();

        // Drives frames until the runtime asks the session to end or the
        // application requests a quit. Replaces CoreMain::Run.
        void RunFrameLoop(CoreMain* coreMain, IAppMain* appMain);

        int GetViewCount() const;
        const XrEyeView& GetEyeView(int index) const;

        bool IsSessionRunning() const;

        // Running is not the same as focused. Only a focused session delivers
        // button and axis input; poses locate either way.
        bool IsSessionFocused() const;

        // ========================
        // Tracked controllers
        // ========================
        //
        // Poses and axes are read here rather than through the action map:
        // there is no sense in which a hand position is remappable, and the
        // action map carries no value type for one. Buttons do go through the
        // three layers and are read as ordinary actions.

        const TrackedPose& GetHandPose(XrHand hand, XrPoseKind kind) const;
        void  GetThumbstick(XrHand hand, float& outX, float& outY) const;
        float GetTriggerValue(XrHand hand) const;
        float GetSqueezeValue(XrHand hand) const;
        void  TriggerHaptic(XrHand hand, float amplitude, float durationSeconds);

        static const int MAX_EYE_VIEWS = 2;

    private:
        static XrManager* instance;

        struct XrState;
        XrState* state;

        XrEyeView    eyeViews[MAX_EYE_VIEWS];
        CameraXrEye* eyeCameras[MAX_EYE_VIEWS];
        int          viewCount;

        bool isSessionActive;
        bool isSessionRunning;
        bool isExitRequested;
        bool isStartRequested;
        bool isEndRequested;

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
        void ApplyPendingSessionRequests();
        bool CreateSession();
        bool CreateReferenceSpace();
        bool CreateSwapchains();
        void DestroySwapchains();
        bool CreateInput();
    };
}

#endif // XRMANAGER_H
