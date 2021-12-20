#pragma once

#include "Unity/IUnityGraphics.h"
#include "PointXYZW.h"

#include <stddef.h>
#include <vector>

struct IUnityInterfaces;


// Super-simple "graphics abstraction". This is nothing like how a proper platform abstraction layer would look like;
// all this does is a base interface for whatever our plugin sample needs. Which is only "draw some triangles"
// and "modify a texture" at this point.
//
// There are implementations of this base class for D3D9, D3D11, OpenGL etc.; see individual RenderAPI_* files.
class RenderAPI
{
public:
	virtual ~RenderAPI() { }

	// Process general event like initialization, shutdown, device loss/reset etc.
	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) = 0;
	virtual void InitializeSSBOs(
		PointXYZW* pPositionVertices,
		PointXYZW* pColorVertices,
		int pVertexCount
	) = 0;
	virtual void DrawMeshShader(
		const float pProjectionMatrix[16],
		const float pViewMatrix[16],
		const float pModelMatrix[16],
		float pPointSize,
		int pAmountOfWorkgroups
	) = 0;

	enum PluginEvent {
		None = 0,
		Initialize = 1,
		Render = 2
	};
};


// Create a graphics API implementation instance for the given API type.
RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType);

