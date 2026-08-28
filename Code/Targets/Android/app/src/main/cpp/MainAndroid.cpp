// ============================================================================
// MainAndroid.cpp — Android entry point for ChillCore
// ============================================================================
//
// Owns android_main and the ALooper-driven outer loop. Routes Android app
// lifecycle commands to the engine, polls input events between frames,
// and drives CoreMain::TickFrame.
//
// The engine is constructed on the first APP_CMD_INIT_WINDOW and lives
// until APP_CMD_DESTROY. Subsequent TERM_WINDOW/INIT_WINDOW pairs (lock
// screen, task switch) route to OnSurfaceLost / OnSurfaceRestored on
// PlatformWindow and Gfx::RenderApi. The EGL context is preserved via
// a 1×1 pbuffer while no on-screen surface is bound, so GL handles
// survive the cycle. While the surface is gone, TickFrame is skipped
// and ALooper_pollOnce blocks until the next event so the process is
// fully idle.

#include <jni.h>
#include <android/log.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <game-activity/GameActivity.h>

#include "AndroidAppContext.h"
#include "CoreMain.h"
#include "AppMain.h"
#include "PlatformInputAndroid.h"
#include "PlatformWindow.h"

#define LOG_TAG "ChillCore"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace
{
    CC::CoreMain* coreMain          = nullptr;
    AppMain*      appMain           = nullptr;
    bool          isEngineAlive     = false;
    bool          hasSurface        = false;
    bool          isFinishRequested = false;

    void BringUpEngine()
    {
        if (isEngineAlive)
        {
            return;
        }

        coreMain = new CC::CoreMain();
        coreMain->Init();

        appMain = new AppMain();
        appMain->Init();

        isEngineAlive = true;
        LOGI("Engine alive");
    }

    void TearDownEngine()
    {
        if (!isEngineAlive)
        {
            return;
        }

        if (appMain != nullptr)
        {
            appMain->Shutdown();
            delete appMain;
            appMain = nullptr;
        }
        if (coreMain != nullptr)
        {
            coreMain->Shutdown();
            delete coreMain;
            coreMain = nullptr;
        }

        isEngineAlive = false;
        LOGI("Engine torn down");
    }
}

extern "C" {

static void HandleAppCmd(android_app* pApp, int32_t cmd)
{
    switch (cmd)
    {
        case APP_CMD_INIT_WINDOW:
            LOGI("APP_CMD_INIT_WINDOW");
            // First INIT_WINDOW builds the engine against the live
            // ANativeWindow. Subsequent INIT_WINDOWs only rebind the
            // EGL context to the new on-screen surface.
            if (!isEngineAlive)
            {
                BringUpEngine();
            }
            else
            {
                coreMain->OnSurfaceRestored();
            }
            hasSurface = true;
            break;
        case APP_CMD_TERM_WINDOW:
            LOGI("APP_CMD_TERM_WINDOW");
            // Engine and EGL context stay alive across surface loss;
            // only the on-screen EGLSurface is destroyed. Context is
            // parked on a 1×1 pbuffer so GL handles remain valid.
            hasSurface = false;
            if (isEngineAlive)
            {
                coreMain->OnSurfaceLost();
            }
            break;
        case APP_CMD_WINDOW_RESIZED:
            LOGI("APP_CMD_WINDOW_RESIZED");
            break;
        case APP_CMD_GAINED_FOCUS:
            LOGI("APP_CMD_GAINED_FOCUS");
            break;
        case APP_CMD_LOST_FOCUS:
            LOGI("APP_CMD_LOST_FOCUS");
            break;
        case APP_CMD_PAUSE:
            LOGI("APP_CMD_PAUSE");
            break;
        case APP_CMD_RESUME:
            LOGI("APP_CMD_RESUME");
            break;
        case APP_CMD_STOP:
            LOGI("APP_CMD_STOP");
            break;
        case APP_CMD_DESTROY:
            LOGI("APP_CMD_DESTROY");
            break;
        default:
            break;
    }
}

static bool MotionEventFilter(const GameActivityMotionEvent* motionEvent)
{
    auto sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
    return (sourceClass == AINPUT_SOURCE_CLASS_POINTER ||
            sourceClass == AINPUT_SOURCE_CLASS_JOYSTICK);
}

// ============================================================================
// PumpMotionEvents
// ============================================================================
//
// Drain GameActivity's motion-event queue and forward single-finger touch
// state into PlatformInputAndroid. The engine's UI layer then sees this as
// a mouse pointer with the left button held while a finger is down,
// making menu hit-testing work without UI-side changes.

static void PumpMotionEvents(android_app* pApp)
{
    if (!isEngineAlive)
    {
        return;
    }

    CC::PlatformInputAndroid* input = CC::PlatformInputAndroid::Get();
    if (input == nullptr)
    {
        return;
    }

    android_input_buffer* inputBuffer = android_app_swap_input_buffers(pApp);
    if (inputBuffer == nullptr)
    {
        return;
    }

    for (uint64_t i = 0; i < inputBuffer->motionEventsCount; i++)
    {
        GameActivityMotionEvent* event = &inputBuffer->motionEvents[i];
        const int32_t action = event->action & AMOTION_EVENT_ACTION_MASK;
        const int32_t actionIndex =
            (event->action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
            >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        if (event->pointerCount == 0)
        {
            continue;
        }

        switch (action)
        {
            case AMOTION_EVENT_ACTION_DOWN:
            {
                // First finger of a new gesture.
                const int id = event->pointers[0].id;
                const float x = GameActivityPointerAxes_getX(&event->pointers[0]);
                const float y = GameActivityPointerAxes_getY(&event->pointers[0]);
                input->OnPointerDown(id, static_cast<int>(x), static_cast<int>(y));
                break;
            }
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
            {
                // Additional finger; actionIndex says which pointer changed.
                if (actionIndex < event->pointerCount)
                {
                    const int id = event->pointers[actionIndex].id;
                    const float x = GameActivityPointerAxes_getX(&event->pointers[actionIndex]);
                    const float y = GameActivityPointerAxes_getY(&event->pointers[actionIndex]);
                    input->OnPointerDown(id, static_cast<int>(x), static_cast<int>(y));
                }
                break;
            }
            case AMOTION_EVENT_ACTION_MOVE:
            {
                // All active pointers may have moved; update each.
                for (uint32_t p = 0; p < event->pointerCount; p++)
                {
                    const int id = event->pointers[p].id;
                    const float x = GameActivityPointerAxes_getX(&event->pointers[p]);
                    const float y = GameActivityPointerAxes_getY(&event->pointers[p]);
                    input->OnPointerMove(id, static_cast<int>(x), static_cast<int>(y));
                }
                break;
            }
            case AMOTION_EVENT_ACTION_UP:
            {
                // Last finger lifted.
                const int id = event->pointers[0].id;
                input->OnPointerUp(id);
                break;
            }
            case AMOTION_EVENT_ACTION_POINTER_UP:
            {
                if (actionIndex < event->pointerCount)
                {
                    const int id = event->pointers[actionIndex].id;
                    input->OnPointerUp(id);
                }
                break;
            }
            case AMOTION_EVENT_ACTION_CANCEL:
                input->OnPointerCancelAll();
                break;
            default:
                break;
        }
    }
    android_app_clear_motion_events(inputBuffer);

    // Key events are drained too so the buffer doesn't grow unbounded.
    // Routing them to the engine is v2 work; for v1 we simply clear.
    android_app_clear_key_events(inputBuffer);
}

void android_main(struct android_app* pApp)
{
    LOGI("ChillCore android_main entry");

    CC::SetAndroidApp(pApp);

    pApp->onAppCmd = HandleAppCmd;
    android_app_set_motion_event_filter(pApp, MotionEventFilter);

    do
    {
        bool eventsDrained = false;
        while (eventsDrained == false && pApp->destroyRequested == 0)
        {
            // Poll non-blocking while a surface is bound; block
            // indefinitely while backgrounded so the process consumes
            // no CPU or battery until the next Android event wakes
            // the looper.
            const int pollTimeoutMs = hasSurface ? 0 : -1;

            int events = 0;
            android_poll_source* pSource = nullptr;
            int result = ALooper_pollOnce(pollTimeoutMs, nullptr, &events,
                                          reinterpret_cast<void**>(&pSource));
            switch (result)
            {
                case ALOOPER_POLL_TIMEOUT:
                case ALOOPER_POLL_WAKE:
                    eventsDrained = true;
                    break;
                case ALOOPER_EVENT_ERROR:
                    LOGE("ALooper_pollOnce returned an error");
                    break;
                case ALOOPER_POLL_CALLBACK:
                    break;
                default:
                    if (pSource != nullptr)
                    {
                        pSource->process(pApp, pSource);
                    }
                    break;
            }
        }

        // Engine ticks only when alive and an on-screen surface is
        // bound. The engine survives backgrounding; TickFrame pauses
        // (no update, no render) while the surface is gone.
        if (isEngineAlive && hasSurface && coreMain != nullptr && appMain != nullptr)
        {
            PumpMotionEvents(pApp);

            if (!coreMain->IsQuitRequested())
            {
                coreMain->TickFrame(appMain);
            }

            // Detected by PlatformWindow when eglSwapBuffers returns
            // EGL_CONTEXT_LOST. Policy is to drop the dead context and
            // ask Android to finish the activity; the framework's
            // normal destroy lifecycle then exits the loop and the
            // user (or framework) relaunches cold. Any state worth
            // preserving belongs in the persistence layer.
            if (CC::PlatformWindow::Get()->IsContextLost() && !isFinishRequested)
            {
                LOGI("EGL context lost — finishing activity for clean restart");
                coreMain->OnContextLost();
                GameActivity_finish(pApp->activity);
                isFinishRequested = true;
            }

            // App-driven quit (e.g. main-menu Quit button) sets
            // CoreMain's flag. Translate it into the Android equivalent:
            // ask the framework to finish the activity, which drives the
            // destroy lifecycle and exits the outer loop. The guard
            // prevents repeated GameActivity_finish calls while the
            // lifecycle plays out.
            if (coreMain->IsQuitRequested() && !isFinishRequested)
            {
                LOGI("Quit requested — finishing activity");
                GameActivity_finish(pApp->activity);
                isFinishRequested = true;
            }
        }

    } while (pApp->destroyRequested == 0);

    TearDownEngine();
    CC::SetAndroidApp(nullptr);

    LOGI("ChillCore android_main exit");
}

} // extern "C"
