#include "RenderAPI.h"
#include "PlatformBase.h"

#if SUPPORT_OPENGL_UNIFIED

#include <assert.h>
#include <string>
#include <vector>

#include "gl3w/gl3w.h"
#include "GL/glext.h"

class RenderAPI_OpenGLCoreES : public RenderAPI {

public:
	RenderAPI_OpenGLCoreES(UnityGfxRenderer apiType);
	virtual ~RenderAPI_OpenGLCoreES() { }

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);
	virtual void InitializeSSBOs(
		PointXYZW* pPositionVertices,
		PointXYZW* pColorVertices,
		int pVertexCount
	);
	virtual void DrawMeshShader(
		const float pProjectionMatrix[16],
		const float pViewMatrix[16],
		const float pModelMatrix[16],
		float pPointSize,
		int pAmountOfWorkgroups
	);

private:
	GLuint createShader(GLuint type, const char* sourceText);
	void createResources();

private:
	UnityGfxRenderer _apiType;

	GLuint _vertexShaderId;
	GLuint _fragmentShaderId;
	GLuint _programId;
	GLuint _ssboPosition;
	GLuint _ssboColor;

	PointXYZW* _positions;
	PointXYZW* _colors;
	int _vertexCount;

	int _projMatrixUniformLoc;
	int _viewMatrixUniformLoc;
	int _modelMatrixUniformLoc;

	PFNGLDRAWMESHTASKSNVPROC _glDrawMeshTasksNV;
};

const GLchar* _msContent = R"(
	#version 450
	#extension GL_NV_mesh_shader : require

	layout(local_size_x = 32) in;
	layout(points, max_vertices = 256, max_primitives = 256) out;

	layout (std430, binding = 1) buffer _vertices {
		vec4 vertices[];
	} vbPosition;

	layout (std430, binding = 2) buffer _colors {
		vec4 vertices[];
	} vbColor;

	out PerVertexData {
	  vec4 color;
	} outColor[];

	uniform	mat4 	projectionMatrix;
	uniform	mat4 	viewMatrix;
	uniform	mat4 	modelMatrix;

	void main() {
		uint local_thread_id = gl_LocalInvocationID.x;
		uint workgroup_id = gl_WorkGroupID.x;
	
		uint primAmount = 256; 
		uint groupSize = gl_WorkGroupSize.x; 
		uint primLoops = max(1, primAmount / groupSize); 
	
		for (int i = 0; i < primLoops; i++) {
			uint localVertexIndex = local_thread_id + i * groupSize;
			uint globalVertexIndex = localVertexIndex + (workgroup_id * (primAmount + 1));
		
			gl_MeshVerticesNV[localVertexIndex].gl_Position = projectionMatrix * viewMatrix * modelMatrix * vbPosition.vertices[globalVertexIndex];

			outColor[localVertexIndex].color = vbColor.vertices[globalVertexIndex];

			gl_PrimitiveIndicesNV[localVertexIndex] = localVertexIndex;
		}
		gl_PrimitiveCountNV = primAmount; 
	}
)";

const GLchar* _fsContent = R"(
	#version 450
	#extension GL_NV_fragment_shader_barycentric : enable

	in PerVertexData {
	  vec4 color;
	} inColor;  

	layout(location = 0) out vec4 outColor;

	void main() {
		outColor = inColor.color;
	}
)";


RenderAPI* CreateRenderAPI_OpenGLCoreES(UnityGfxRenderer apiType) {
	return new RenderAPI_OpenGLCoreES(apiType);
}

GLuint RenderAPI_OpenGLCoreES::createShader(GLuint type, const char* sourceText) {
	GLuint shaderId = glCreateShader(type);
	glShaderSource(shaderId, 1, &sourceText, NULL);
	glCompileShader(shaderId);
	return shaderId;
}

void RenderAPI_OpenGLCoreES::createResources() {
	// Initialize OpenGL functions
	gl3wInit();

	// Create shader program
	_programId = glCreateProgram();

	// Create and attach shaders
	_vertexShaderId = createShader(GL_MESH_SHADER_NV, _msContent);
	_fragmentShaderId = createShader(GL_FRAGMENT_SHADER, _fsContent);

	glAttachShader(_programId, _vertexShaderId);
	glAttachShader(_programId, _fragmentShaderId);

	// Link shaders into a program
	glLinkProgram(_programId);

	GLint status = 0;
	glGetProgramiv(_programId, GL_LINK_STATUS, &status);
	assert(status == GL_TRUE);

	// find uniform locations
	_projMatrixUniformLoc = glGetUniformLocation(_programId, "projectionMatrix");
	_viewMatrixUniformLoc = glGetUniformLocation(_programId, "viewMatrix");
	_modelMatrixUniformLoc = glGetUniformLocation(_programId, "modelMatrix");

	assert(glGetError() == GL_NO_ERROR);
}


RenderAPI_OpenGLCoreES::RenderAPI_OpenGLCoreES(UnityGfxRenderer apiType) : _apiType(apiType) {
	// Initialize mesh shader function(s)
	_glDrawMeshTasksNV = (PFNGLDRAWMESHTASKSNVPROC) wglGetProcAddress("glDrawMeshTasksNV");
}


void RenderAPI_OpenGLCoreES::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) {
	if (type == kUnityGfxDeviceEventInitialize) {
		createResources();
	} else if (type == kUnityGfxDeviceEventShutdown) {
		//@TODO: release resources
	}
}

void RenderAPI_OpenGLCoreES::InitializeSSBOs(
	PointXYZW* pPositionVertices,
	PointXYZW* pColorVertices,
	int pVertexCount
) {
	_positions = pPositionVertices;
	_colors = pColorVertices;
	_vertexCount = pVertexCount;

	glGenBuffers(1, &_ssboPosition);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboPosition);
	glBufferData(GL_SHADER_STORAGE_BUFFER, pVertexCount * (sizeof(PointXYZW)), NULL, GL_STATIC_DRAW);

	glGenBuffers(1, &_ssboColor);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboColor);
	glBufferData(GL_SHADER_STORAGE_BUFFER, pVertexCount * (sizeof(PointXYZW)), NULL, GL_STATIC_DRAW);
}

void RenderAPI_OpenGLCoreES::DrawMeshShader(
	const float pProjectionMatrix[16],
	const float pViewMatrix[16],
	const float pModelMatrix[16],
	float pPointSize,
	int pAmountOfWorkgroups
) {
	// Set-up basic render state
	glEnable(GL_DEPTH_TEST);
	glFrontFace(GL_CCW);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_PROGRAM_POINT_SIZE);
	glPointSize(pPointSize);

	glUseProgram(_programId);

	glUniformMatrix4fv(_projMatrixUniformLoc, 1, GL_FALSE, pProjectionMatrix);
	glUniformMatrix4fv(_viewMatrixUniformLoc, 1, GL_FALSE, pViewMatrix);
	glUniformMatrix4fv(_modelMatrixUniformLoc, 1, GL_FALSE, pModelMatrix);

	// Position Storage Buffer
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboPosition);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _vertexCount * (sizeof(PointXYZW)), _positions, GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _ssboPosition);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Color Storage Buffer
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboColor);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _vertexCount * (sizeof(PointXYZW)), _colors, GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboColor);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	
	_glDrawMeshTasksNV(0, pAmountOfWorkgroups);
}

#endif // #if SUPPORT_OPENGL_UNIFIED
