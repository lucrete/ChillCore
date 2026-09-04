#include "XrManager.h"

#include <string.h>
#include <vector>

// openxr_platform.h declares the OpenGL structs against types the GL and
// Windows headers define, so both must come first.
#include <windows.h>
#include <glad/gl.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "CCAssert.h"
#include "XrInput.h"
#include "CameraXrEye.h"
#include "CoreMain.h"
#include "AppMainInterface.h"
#include "GfxRenderApi.h"
#include "PlatformWindow.h"
#include "PrintManager.h"
#include "RenderManager.h"
#include "RenderView.h"

namespace CC
{
    XrManager* XrManager::instance = nullptr;

    // ========================
    // Opaque state
    // ========================
    //
    // Every OpenXR type the subsystem holds. Kept out of the header so
    // nothing above this file sees one.

    struct XrManager::XrState
    {
        XrInstance instance = XR_NULL_HANDLE;
        XrSystemId systemId = XR_NULL_SYSTEM_ID;
        XrSession  session  = XR_NULL_HANDLE;
        XrSpace    space    = XR_NULL_HANDLE;

        XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;

        XrTime predictedDisplayTime = 0;
        bool   shouldRenderFrame    = false;

        // xrEndFrame must pair with an xrBeginFrame that succeeded, and every
        // image acquired must be released even on a frame that is abandoned
        // partway through.
        bool   isFrameBegun      = false;
        int    acquiredEyeCount  = 0;

        XrView                     views[MAX_EYE_VIEWS] = {};
        XrCompositionLayerProjectionView projectionViews[MAX_EYE_VIEWS] = {};

        // One swapchain per eye. Two chains rather than one array chain: the
        // engine renders the eyes one after the other into separate targets,
        // so there is nothing an array layer would buy.
        XrSwapchain swapchain[MAX_EYE_VIEWS] = { XR_NULL_HANDLE, XR_NULL_HANDLE };

        // A render target per swapchain image, built once. The runtime hands
        // back an image index each frame, which selects one.
        std::vector<Gfx::TextureHandle>      swapchainTexture[MAX_EYE_VIEWS];
        std::vector<Gfx::RenderTargetHandle> swapchainTarget[MAX_EYE_VIEWS];
        uint32_t                             acquiredImageIndex[MAX_EYE_VIEWS] = { 0, 0 };

        // Engine-owned, multisampled, what the scene actually renders into.
        Gfx::TextureHandle      sceneColorTexture[MAX_EYE_VIEWS];
        Gfx::TextureHandle      sceneDepthTexture[MAX_EYE_VIEWS];
        Gfx::RenderTargetHandle sceneTarget[MAX_EYE_VIEWS];

        XrInput input;
    };

    namespace
    {
        // The runtime reports failure through a result code on every call.
        // Logging the call that failed is the difference between a diagnosable
        // problem and a silent black screen.
        bool XrSucceeded(XrResult result, const char* what)
        {
            bool isSuccess = XR_SUCCEEDED(result);
            if (!isSuccess)
            {
                CCPrint(PrintManager::CHANNEL_RENDER, "OpenXR: %s failed (%d)", what, (int)result);
            }
            return isSuccess;
        }

        Vector3 ToVector3(const XrVector3f& value)
        {
            return Vector3(value.x, value.y, value.z);
        }

        Quaternion ToQuaternion(const XrQuaternionf& value)
        {
            return Quaternion(value.x, value.y, value.z, value.w);
        }
    }

    // ========================
    // Lifetime
    // ========================

    XrManager::XrManager()
        : state(nullptr)
        , viewCount(0)
        , isSessionRunning(false)
        , isExitRequested(false)
    {
        CC_ASSERT(instance == nullptr, "XrManager already created");
        instance = this;

        for (int i = 0; i < MAX_EYE_VIEWS; i++)
        {
            eyeCameras[i] = nullptr;
        }
    }

    XrManager::~XrManager()
    {
        Shutdown();
        instance = nullptr;
    }

    XrManager* XrManager::Get()
    {
        return instance;
    }

    bool XrManager::Init()
    {
        CC_ASSERT(state == nullptr, "XrManager::Init called twice");
        state = new XrState();

        bool isReady = CreateInstanceAndSystem()
                    && CreateSession()
                    && CreateReferenceSpace()
                    && CreateSwapchains()
                    && CreateInput();

        if (!isReady)
        {
            Shutdown();
        }

        return isReady;
    }

    void XrManager::Shutdown()
    {
        if (state != nullptr)
        {
            state->input.Shutdown();
            DestroySwapchains();

            if (state->space != XR_NULL_HANDLE)   { xrDestroySpace(state->space); }
            if (state->session != XR_NULL_HANDLE) { xrDestroySession(state->session); }
            if (state->instance != XR_NULL_HANDLE) { xrDestroyInstance(state->instance); }

            delete state;
            state = nullptr;
        }

        for (int i = 0; i < MAX_EYE_VIEWS; i++)
        {
            delete eyeCameras[i];
            eyeCameras[i] = nullptr;
        }

        isSessionRunning = false;
        viewCount = 0;
    }

    bool XrManager::IsSessionRunning() const
    {
        return isSessionRunning;
    }

    int XrManager::GetViewCount() const
    {
        return viewCount;
    }

    const XrEyeView& XrManager::GetEyeView(int index) const
    {
        CC_ASSERT(index >= 0 && index < viewCount, "GetEyeView index out of range");
        return eyeViews[index];
    }

    // ========================
    // Tracked controllers
    // ========================

    const TrackedPose& XrManager::GetHandPose(XrHand hand, XrPoseKind kind) const
    {
        return state->input.GetHandPose(hand, kind);
    }

    void XrManager::GetThumbstick(XrHand hand, float& outX, float& outY) const
    {
        state->input.GetThumbstick(hand, outX, outY);
    }

    float XrManager::GetTriggerValue(XrHand hand) const
    {
        return state->input.GetTriggerValue(hand);
    }

    float XrManager::GetSqueezeValue(XrHand hand) const
    {
        return state->input.GetSqueezeValue(hand);
    }

    void XrManager::TriggerHaptic(XrHand hand, float amplitude, float durationSeconds)
    {
        state->input.TriggerHaptic(hand, amplitude, durationSeconds);
    }

    // ========================
    // Setup
    // ========================

    bool XrManager::CreateInstanceAndSystem()
    {
        const char* const extensions[] = { XR_KHR_OPENGL_ENABLE_EXTENSION_NAME };

        XrInstanceCreateInfo instanceInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
        instanceInfo.enabledExtensionCount = 1;
        instanceInfo.enabledExtensionNames = extensions;
        strcpy_s(instanceInfo.applicationInfo.applicationName, "ChillCore");
        instanceInfo.applicationInfo.applicationVersion = 1;
        strcpy_s(instanceInfo.applicationInfo.engineName, "ChillCore");
        instanceInfo.applicationInfo.engineVersion = 1;
        instanceInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;

        bool isReady = XrSucceeded(xrCreateInstance(&instanceInfo, &state->instance), "xrCreateInstance");

        if (isReady)
        {
            XrInstanceProperties instanceProperties = { XR_TYPE_INSTANCE_PROPERTIES };
            if (XR_SUCCEEDED(xrGetInstanceProperties(state->instance, &instanceProperties)))
            {
                CCPrint(PrintManager::CHANNEL_RENDER, "OpenXR runtime: %s", instanceProperties.runtimeName);
            }

            XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
            systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
            isReady = XrSucceeded(xrGetSystem(state->instance, &systemInfo, &state->systemId), "xrGetSystem");
        }

        return isReady;
    }

    bool XrManager::CreateSession()
    {
        // The runtime states the GL version it needs before a session can be
        // created, and refuses one against a context older than that.
        PFN_xrGetOpenGLGraphicsRequirementsKHR getRequirements = nullptr;
        xrGetInstanceProcAddr(state->instance, "xrGetOpenGLGraphicsRequirementsKHR",
                              (PFN_xrVoidFunction*)&getRequirements);

        bool isReady = getRequirements != nullptr;
        if (!isReady)
        {
            CCPrint(PrintManager::CHANNEL_RENDER, "OpenXR: the runtime does not expose the OpenGL extension");
        }

        if (isReady)
        {
            XrGraphicsRequirementsOpenGLKHR requirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR };
            isReady = XrSucceeded(getRequirements(state->instance, state->systemId, &requirements),
                                  "xrGetOpenGLGraphicsRequirementsKHR");
        }

        PlatformWindow::NativeGraphicsBinding nativeBinding;
        if (isReady)
        {
            isReady = PlatformWindow::Get()->GetNativeGraphicsBinding(nativeBinding);
            if (!isReady)
            {
                CCPrint(PrintManager::CHANNEL_RENDER, "OpenXR: the platform did not supply a graphics binding");
            }
        }

        if (isReady)
        {
            XrGraphicsBindingOpenGLWin32KHR graphicsBinding = { XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
            graphicsBinding.hDC   = (HDC)nativeBinding.displayOrDeviceContext;
            graphicsBinding.hGLRC = (HGLRC)nativeBinding.renderContext;

            XrSessionCreateInfo sessionInfo = { XR_TYPE_SESSION_CREATE_INFO };
            sessionInfo.next     = &graphicsBinding;
            sessionInfo.systemId = state->systemId;

            isReady = XrSucceeded(xrCreateSession(state->instance, &sessionInfo, &state->session),
                                  "xrCreateSession");
        }

        return isReady;
    }

    bool XrManager::CreateReferenceSpace()
    {
        // Stage is the room: its origin sits on the floor at the centre of
        // the play area, which is what a room-scale scene wants. Not every
        // runtime has one configured, and local — origin at the headset's
        // start pose — is the fallback every runtime supports.
        XrReferenceSpaceCreateInfo spaceInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;

        XrResult result = xrCreateReferenceSpace(state->session, &spaceInfo, &state->space);

        if (XR_FAILED(result))
        {
            CCPrint(PrintManager::CHANNEL_RENDER, "OpenXR: no stage space, falling back to local");
            spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            result = xrCreateReferenceSpace(state->session, &spaceInfo, &state->space);
        }

        return XrSucceeded(result, "xrCreateReferenceSpace");
    }

    bool XrManager::CreateSwapchains()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

        uint32_t configuredViewCount = 0;
        bool isReady = XrSucceeded(xrEnumerateViewConfigurationViews(
                                       state->instance, state->systemId,
                                       XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                       0, &configuredViewCount, nullptr),
                                   "xrEnumerateViewConfigurationViews");

        std::vector<XrViewConfigurationView> configuredViews(configuredViewCount,
                                                            { XR_TYPE_VIEW_CONFIGURATION_VIEW });
        if (isReady)
        {
            isReady = XrSucceeded(xrEnumerateViewConfigurationViews(
                                      state->instance, state->systemId,
                                      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                      configuredViewCount, &configuredViewCount, configuredViews.data()),
                                  "xrEnumerateViewConfigurationViews");
        }

        if (isReady && configuredViewCount != MAX_EYE_VIEWS)
        {
            CCPrint(PrintManager::CHANNEL_RENDER,
                    "OpenXR: expected %d views, the runtime configured %u", MAX_EYE_VIEWS, configuredViewCount);
            isReady = false;
        }

        if (isReady)
        {
            viewCount = (int)configuredViewCount;
            for (int eye = 0; eye < viewCount && isReady; eye++)
            {
                const int width  = (int)configuredViews[eye].recommendedImageRectWidth;
                const int height = (int)configuredViews[eye].recommendedImageRectHeight;

                XrSwapchainCreateInfo swapchainInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
                swapchainInfo.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
                                          | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
                swapchainInfo.format      = GL_SRGB8_ALPHA8;
                swapchainInfo.width       = width;
                swapchainInfo.height      = height;
                swapchainInfo.sampleCount = 1;
                swapchainInfo.faceCount   = 1;
                swapchainInfo.arraySize   = 1;
                swapchainInfo.mipCount    = 1;

                isReady = XrSucceeded(xrCreateSwapchain(state->session, &swapchainInfo, &state->swapchain[eye]),
                                      "xrCreateSwapchain");

                uint32_t imageCount = 0;
                if (isReady)
                {
                    isReady = XrSucceeded(xrEnumerateSwapchainImages(state->swapchain[eye], 0, &imageCount, nullptr),
                                          "xrEnumerateSwapchainImages");
                }

                std::vector<XrSwapchainImageOpenGLKHR> images(
                    imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
                if (isReady)
                {
                    isReady = XrSucceeded(xrEnumerateSwapchainImages(
                                              state->swapchain[eye], imageCount, &imageCount,
                                              (XrSwapchainImageBaseHeader*)images.data()),
                                          "xrEnumerateSwapchainImages");
                }

                if (isReady)
                {
                    // Registered once. The runtime cycles the images; nothing
                    // is created or destroyed per frame.
                    for (uint32_t i = 0; i < imageCount; i++)
                    {
                        Gfx::ExternalTextureDescription externalDesc;
                        externalDesc.nativeHandle = images[i].image;
                        externalDesc.format       = Gfx::TextureFormat::Rgba8Srgb;
                        externalDesc.width        = width;
                        externalDesc.height       = height;
                        externalDesc.debugName    = "XrSwapchainImage";
                        Gfx::TextureHandle texture = gfxApi->RegisterExternalTexture(externalDesc);

                        Gfx::RenderTargetDescription targetDesc;
                        targetDesc.width                          = width;
                        targetDesc.height                         = height;
                        targetDesc.colorAttachmentCount           = 1;
                        targetDesc.colorAttachments[0].texture    = texture;
                        targetDesc.colorAttachments[0].loadOp     = Gfx::LoadOp::Clear;
                        targetDesc.colorAttachments[0].storeOp    = Gfx::StoreOp::Store;
                        targetDesc.hasDepthStencil                = false;
                        targetDesc.sampleCount                    = 1;
                        targetDesc.debugName                      = "XrSwapchainTarget";

                        state->swapchainTexture[eye].push_back(texture);
                        state->swapchainTarget[eye].push_back(gfxApi->CreateRenderTarget(targetDesc));
                    }

                    // The scene target is the engine's own: multisampled, and
                    // half-float so the post-process stack has range above
                    // white to tone map. It resolves into its colour texture,
                    // which the post pass then encodes into the swapchain
                    // image.
                    const Gfx::GfxCapabilities& caps = gfxApi->GetCapabilities();

                    Gfx::TextureDescription colorDesc;
                    colorDesc.width          = width;
                    colorDesc.height         = height;
                    colorDesc.format         = caps.supportsHalfFloatRenderTargets
                                             ? Gfx::TextureFormat::Rgba16Float
                                             : Gfx::TextureFormat::Rgba8Unorm;
                    colorDesc.isRenderTarget = true;
                    colorDesc.debugName      = "XrSceneColor";
                    state->sceneColorTexture[eye] = gfxApi->CreateTexture(colorDesc);

                    Gfx::TextureDescription depthDesc;
                    depthDesc.width          = width;
                    depthDesc.height         = height;
                    depthDesc.format         = Gfx::TextureFormat::Depth24Stencil8;
                    depthDesc.isRenderTarget = true;
                    depthDesc.debugName      = "XrSceneDepth";
                    state->sceneDepthTexture[eye] = gfxApi->CreateTexture(depthDesc);

                    Gfx::RenderTargetDescription sceneDesc;
                    sceneDesc.width                       = width;
                    sceneDesc.height                      = height;
                    sceneDesc.colorAttachmentCount        = 1;
                    sceneDesc.colorAttachments[0].texture = state->sceneColorTexture[eye];
                    sceneDesc.colorAttachments[0].loadOp  = Gfx::LoadOp::Clear;
                    sceneDesc.colorAttachments[0].storeOp = Gfx::StoreOp::Store;
                    sceneDesc.hasDepthStencil                = true;
                    sceneDesc.depthStencilAttachment.texture = state->sceneDepthTexture[eye];
                    sceneDesc.depthStencilAttachment.loadOp  = Gfx::LoadOp::Clear;
                    sceneDesc.depthStencilAttachment.storeOp = Gfx::StoreOp::DontCare;
                    sceneDesc.sampleCount                    = RenderManager::Get()->GetMsaaSamples();
                    sceneDesc.debugName                      = "XrSceneTarget";
                    state->sceneTarget[eye] = gfxApi->CreateRenderTarget(sceneDesc);

                    eyeCameras[eye] = new CameraXrEye();
                    eyeCameras[eye]->SetViewportSize(width, height);

                    eyeViews[eye].width             = width;
                    eyeViews[eye].height            = height;
                    eyeViews[eye].sceneTarget       = state->sceneTarget[eye];
                    eyeViews[eye].sceneColorTexture = state->sceneColorTexture[eye];

                    state->views[eye] = { XR_TYPE_VIEW };
                    state->projectionViews[eye] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };

                    CCPrint(PrintManager::CHANNEL_RENDER,
                            "OpenXR: eye %d swapchain %dx%d, %u images", eye, width, height, imageCount);
                }
            }
        }

        return isReady;
    }

    bool XrManager::CreateInput()
    {
        // Attaching the action set is irreversible for the session, so it is
        // the last thing set up: nothing after it may add an action.
        return state->input.Init(state->instance, state->session);
    }

    void XrManager::DestroySwapchains()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

        for (int eye = 0; eye < MAX_EYE_VIEWS; eye++)
        {
            for (size_t i = 0; i < state->swapchainTarget[eye].size(); i++)
            {
                gfxApi->DestroyRenderTarget(state->swapchainTarget[eye][i]);
                gfxApi->DestroyTexture(state->swapchainTexture[eye][i]);
            }
            state->swapchainTarget[eye].clear();
            state->swapchainTexture[eye].clear();

            if (state->sceneTarget[eye].IsValid())       { gfxApi->DestroyRenderTarget(state->sceneTarget[eye]); }
            if (state->sceneColorTexture[eye].IsValid()) { gfxApi->DestroyTexture(state->sceneColorTexture[eye]); }
            if (state->sceneDepthTexture[eye].IsValid()) { gfxApi->DestroyTexture(state->sceneDepthTexture[eye]); }

            state->sceneTarget[eye]       = Gfx::RenderTargetHandle();
            state->sceneColorTexture[eye] = Gfx::TextureHandle();
            state->sceneDepthTexture[eye] = Gfx::TextureHandle();

            if (state->swapchain[eye] != XR_NULL_HANDLE)
            {
                xrDestroySwapchain(state->swapchain[eye]);
                state->swapchain[eye] = XR_NULL_HANDLE;
            }
        }
    }

    // ========================
    // Frame
    // ========================

    void XrManager::RunFrameLoop(CoreMain* coreMain, IAppMain* appMain)
    {
        CC_ASSERT(coreMain != nullptr && appMain != nullptr, "RunFrameLoop needs CoreMain and AppMain");

        while (!isExitRequested && !coreMain->IsQuitRequested())
        {
            PollEvents();

            // Between session start and stop the runtime owns the pacing. The
            // window still needs its events pumped either way, or the OS
            // marks the application unresponsive.
            if (isSessionRunning)
            {
                if (BeginXrFrame())
                {
                    PublishRenderViews();
                    coreMain->TickFrame(appMain);
                }
                EndXrFrame();
            }
            else
            {
                PlatformWindow::Get()->PollEvents();
            }
        }
    }

    void XrManager::PollEvents()
    {
        XrEventDataBuffer event = { XR_TYPE_EVENT_DATA_BUFFER };

        while (xrPollEvent(state->instance, &event) == XR_SUCCESS)
        {
            if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
            {
                const XrEventDataSessionStateChanged& changed =
                    *(const XrEventDataSessionStateChanged*)&event;
                state->sessionState = changed.state;

                if (changed.state == XR_SESSION_STATE_READY)
                {
                    XrSessionBeginInfo beginInfo = { XR_TYPE_SESSION_BEGIN_INFO };
                    beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if (XrSucceeded(xrBeginSession(state->session, &beginInfo), "xrBeginSession"))
                    {
                        isSessionRunning = true;
                    }
                }
                else if (changed.state == XR_SESSION_STATE_STOPPING)
                {
                    isSessionRunning = false;
                    xrEndSession(state->session);
                }
                else if (changed.state == XR_SESSION_STATE_EXITING
                      || changed.state == XR_SESSION_STATE_LOSS_PENDING)
                {
                    isSessionRunning = false;
                    isExitRequested  = true;
                }
            }
            else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
            {
                isExitRequested = true;
            }

            event = { XR_TYPE_EVENT_DATA_BUFFER };
        }
    }

    bool XrManager::BeginXrFrame()
    {
        XrFrameWaitInfo waitInfo = { XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState    frameState = { XR_TYPE_FRAME_STATE };

        state->isFrameBegun     = false;
        state->acquiredEyeCount = 0;

        bool isReady = XrSucceeded(xrWaitFrame(state->session, &waitInfo, &frameState), "xrWaitFrame");

        if (isReady)
        {
            state->predictedDisplayTime = frameState.predictedDisplayTime;
            state->shouldRenderFrame    = frameState.shouldRender == XR_TRUE;

            XrFrameBeginInfo beginInfo = { XR_TYPE_FRAME_BEGIN_INFO };
            isReady = XrSucceeded(xrBeginFrame(state->session, &beginInfo), "xrBeginFrame");
            state->isFrameBegun = isReady;
        }

        if (isReady)
        {
            isReady = state->shouldRenderFrame;
        }

        if (isReady)
        {
            XrViewLocateInfo locateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
            locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            locateInfo.displayTime           = state->predictedDisplayTime;
            locateInfo.space                 = state->space;

            XrViewState viewState = { XR_TYPE_VIEW_STATE };
            uint32_t    locatedCount = 0;
            isReady = XrSucceeded(xrLocateViews(state->session, &locateInfo, &viewState,
                                                (uint32_t)viewCount, &locatedCount, state->views),
                                  "xrLocateViews");

            const XrViewStateFlags requiredFlags = XR_VIEW_STATE_POSITION_VALID_BIT
                                                 | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
            if (isReady)
            {
                isReady = (viewState.viewStateFlags & requiredFlags) == requiredFlags;
            }
        }

        // Against the same predicted display time the views were located
        // against, so the hands sit where the head expects them.
        if (isReady)
        {
            state->input.SyncActions(state->space, state->predictedDisplayTime);
        }

        // Acquiring is what hands the frame its target, so it comes last: a
        // frame that will not be rendered must not hold an image.
        if (isReady)
        {
            for (int eye = 0; eye < viewCount && isReady; eye++)
            {
                XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
                isReady = XrSucceeded(xrAcquireSwapchainImage(state->swapchain[eye], &acquireInfo,
                                                              &state->acquiredImageIndex[eye]),
                                      "xrAcquireSwapchainImage");

                if (isReady)
                {
                    state->acquiredEyeCount = eye + 1;

                    XrSwapchainImageWaitInfo waitImageInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
                    waitImageInfo.timeout = XR_INFINITE_DURATION;
                    isReady = XrSucceeded(xrWaitSwapchainImage(state->swapchain[eye], &waitImageInfo),
                                          "xrWaitSwapchainImage");
                }
            }
        }

        return isReady;
    }

    void XrManager::PublishRenderViews()
    {
        RenderView renderViews[MAX_EYE_VIEWS];

        for (int eye = 0; eye < viewCount; eye++)
        {
            const uint32_t imageIndex = state->acquiredImageIndex[eye];

            eyeViews[eye].pose.position    = ToVector3(state->views[eye].pose.position);
            eyeViews[eye].pose.orientation = ToQuaternion(state->views[eye].pose.orientation);
            eyeViews[eye].pose.isTracked   = true;
            eyeViews[eye].angleLeft        = state->views[eye].fov.angleLeft;
            eyeViews[eye].angleRight       = state->views[eye].fov.angleRight;
            eyeViews[eye].angleUp          = state->views[eye].fov.angleUp;
            eyeViews[eye].angleDown        = state->views[eye].fov.angleDown;
            eyeViews[eye].swapchainTarget  = state->swapchainTarget[eye][imageIndex];
            eyeViews[eye].swapchainTexture = state->swapchainTexture[eye][imageIndex];

            eyeCameras[eye]->SetPose(eyeViews[eye].pose.position, eyeViews[eye].pose.orientation);
            eyeCameras[eye]->SetFieldOfViewAngles(eyeViews[eye].angleLeft, eyeViews[eye].angleRight,
                                                  eyeViews[eye].angleUp, eyeViews[eye].angleDown);

            renderViews[eye].camera             = eyeCameras[eye];
            renderViews[eye].sceneTarget        = eyeViews[eye].sceneTarget;
            renderViews[eye].sceneColorTexture  = eyeViews[eye].sceneColorTexture;
            renderViews[eye].outputTarget       = eyeViews[eye].swapchainTarget;
            renderViews[eye].outputColorTexture = eyeViews[eye].swapchainTexture;
            renderViews[eye].width              = eyeViews[eye].width;
            renderViews[eye].height             = eyeViews[eye].height;

            // What the compositor reads back, filled from the same pose the
            // scene was rendered against. A mismatch here shows as the world
            // lagging or swimming against head motion.
            state->projectionViews[eye].pose = state->views[eye].pose;
            state->projectionViews[eye].fov  = state->views[eye].fov;
            state->projectionViews[eye].subImage.swapchain              = state->swapchain[eye];
            state->projectionViews[eye].subImage.imageArrayIndex        = 0;
            state->projectionViews[eye].subImage.imageRect.offset       = { 0, 0 };
            state->projectionViews[eye].subImage.imageRect.extent.width  = eyeViews[eye].width;
            state->projectionViews[eye].subImage.imageRect.extent.height = eyeViews[eye].height;
        }

        RenderManager::Get()->SetRenderViews(renderViews, viewCount);
    }

    void XrManager::EndXrFrame()
    {
        XrCompositionLayerProjection layer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
        const XrCompositionLayerBaseHeader* layers[1] = { (const XrCompositionLayerBaseHeader*)&layer };

        XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO };
        endInfo.displayTime          = state->predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount           = 0;
        endInfo.layers               = nullptr;

        // Release whatever was acquired, which on an abandoned frame may be
        // fewer than every eye. Holding one starves the runtime's rotation.
        for (int eye = 0; eye < state->acquiredEyeCount; eye++)
        {
            XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
            xrReleaseSwapchainImage(state->swapchain[eye], &releaseInfo);
        }

        // A layer is only honest where every eye was drawn.
        if (state->shouldRenderFrame && state->acquiredEyeCount == viewCount)
        {
            layer.space     = state->space;
            layer.viewCount = (uint32_t)viewCount;
            layer.views     = state->projectionViews;

            endInfo.layerCount = 1;
            endInfo.layers     = layers;
        }

        if (state->isFrameBegun)
        {
            xrEndFrame(state->session, &endInfo);
        }

        state->isFrameBegun     = false;
        state->acquiredEyeCount = 0;
    }
}
