// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"
#include "PointXYZW.h"

#include <assert.h>
#include <math.h>
#include <string>
#include <vector>

#include "GL/glew.h"
#include "GL/glext.h"


// --------------------------------------------------------------------------
// RegisterDebugCallback

void debugInUnity(std::string message);
typedef void(__stdcall* DebugCallback) (const char* str);
DebugCallback gDebugCallback;

extern "C" void UNITY_INTERFACE_EXPORT RegisterDebugCallback(DebugCallback callback) {
	if (callback) {
		gDebugCallback = callback;
	}
}

// --------------------------------------------------------------------------
// SetShaderPointData

static int _vertexCount;
static PointXYZW* _positionArray;
static PointXYZW* _colorArray;

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetShaderPointData(
	int pVertexCount,
	PointXYZW* pPositionData,
	PointXYZW* pColorData
) {
	_vertexCount = pVertexCount;
	_positionArray = pPositionData;
	_colorArray = pColorData;
}

// --------------------------------------------------------------------------
// SetShaderDrawData

static float _pointSize;
static int _amountOfWorkgroups;

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetShaderDrawData(
	float pPointSize,
	int pAmountOfWorkgroups
) {
	_pointSize = pPointSize;
	_amountOfWorkgroups = pAmountOfWorkgroups;
}

// --------------------------------------------------------------------------
// SetGraphicsMatrices

static float _projectionMatrix[16], _viewMatrix[16], _modelMatrix[16];

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetGraphicsMatrices(
	float pProjectMatrix[16],
	float pViewMatrix[16],
	float pModelMatrix[16]
) {
	std::copy(pProjectMatrix, pProjectMatrix + 16, _projectionMatrix);
	std::copy(pViewMatrix, pViewMatrix + 16, _viewMatrix);
	std::copy(pModelMatrix, pModelMatrix + 16, _modelMatrix);
}

// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces * unityInterfaces) {
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload() {
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

#if UNITY_WEBGL
typedef void	(UNITY_INTERFACE_API* PluginLoadFunc)(IUnityInterfaces* unityInterfaces);
typedef void	(UNITY_INTERFACE_API* PluginUnloadFunc)();

extern "C" void	UnityRegisterRenderingPlugin(PluginLoadFunc loadPlugin, PluginUnloadFunc unloadPlugin);

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RegisterPlugin() {
	UnityRegisterRenderingPlugin(UnityPluginLoad, UnityPluginUnload);
}
#endif

// --------------------------------------------------------------------------
// GraphicsDeviceEvent


static RenderAPI* s_CurrentAPI = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;


static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
	// Create graphics API implementation upon initialization
	if (eventType == kUnityGfxDeviceEventInitialize) {
		assert(s_CurrentAPI == NULL);
		s_DeviceType = s_Graphics->GetRenderer();
		s_CurrentAPI = CreateRenderAPI(s_DeviceType);
	}

	// Let the implementation process the device related events
	if (s_CurrentAPI) {
		s_CurrentAPI->ProcessDeviceEvent(eventType, s_UnityInterfaces);
	}

	// Cleanup graphics API implementation upon shutdown
	if (eventType == kUnityGfxDeviceEventShutdown) {
		delete s_CurrentAPI;
		s_CurrentAPI = NULL;
		s_DeviceType = kUnityGfxRendererNull;
	}
}

static void InitializeSSBOs() {
	s_CurrentAPI->InitializeSSBOs(_positionArray, _colorArray, _vertexCount);
}

static void DrawMeshShader() {
	s_CurrentAPI->DrawMeshShader(
		_projectionMatrix,
		_viewMatrix,
		_modelMatrix,
		_pointSize,
		_amountOfWorkgroups
	);
}

static void UNITY_INTERFACE_API OnRenderEvent(int eventID) {
	// Unknown / unsupported graphics device type? Do nothing
	if (s_CurrentAPI == NULL) {
		return;
	}

	if (s_DeviceType == kUnityGfxRendererOpenGLCore) {
		switch (eventID) {
			case RenderAPI::PluginEvent::Initialize:
				InitializeSSBOs();
				break;
			case RenderAPI::PluginEvent::Render:
				DrawMeshShader();
				break;
			case RenderAPI::PluginEvent::None:
			default:
				// Empty
				break;
		}
	}

	debugInUnity("GL_ERROR: " + std::to_string(glGetError()));
}


// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() {
	return OnRenderEvent;
}

void debugInUnity(std::string message) {
	if (gDebugCallback) {
		gDebugCallback(message.c_str());
	}
}