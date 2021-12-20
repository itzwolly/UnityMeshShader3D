using UnityEngine;
using System;
using System.Collections;
using System.Runtime.InteropServices;
using UnityEditor;
using System.Threading.Tasks;

public class UseRenderingPlugin : MonoBehaviour {

	[SerializeField] private UnityEngine.Object _pointCloud;
	[SerializeField] private float _pointSize = 1;
	[SerializeField] private int _amountOfWorkgroups = 1;

	private enum PluginEvent {
		None = 0,
		Initialize = 1,
		Render = 2
	}

	private delegate void DebugCallback(string message);

	[DllImport("RenderingPlugin")]
	private static extern void RegisterDebugCallback(DebugCallback callback);

	[DllImport("RenderingPlugin")]
	private extern unsafe static IntPtr SetShaderPointData(
		int pVertexCount,
		[MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 0)]
		PointXYZW* pPositionsData,
		[MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 0)]
		PointXYZW* pColorsData
	);

	[DllImport("RenderingPlugin")]
	private extern static IntPtr SetShaderDrawData(
		float pPointSize,
		int pAmountOfWorkgroups
	);

	[DllImport("RenderingPlugin")]
	private static extern IntPtr SetGraphicsMatrices(
		float[] pProjectionMatrix,
		float[] pViewMatrix,
		float[] pModelMatrix
	);

	[DllImport("RenderingPlugin")]
	private static extern IntPtr GetRenderEventFunc();

    private void Awake() {
		// Register Debug Callback for native plugin
		RegisterDebugCallback(new DebugCallback(DebugMethod));
	}

    private void Start() {
		loadPointCloud();
	}

	private async void loadPointCloud() {
		// Load point cloud data asynchronously
		PLYLoader plyLoader = new PLYLoader();
		Task<PointCloud> pointCloudLoadTask;
		await (pointCloudLoadTask = plyLoader.LoadFile(AssetDatabase.GetAssetPath(_pointCloud)));

		// Send point cloud position and color data to the native plugin to be used by the mesh shader
		PointCloud output = pointCloudLoadTask.Result;
		setShaderPointDataInternal(output.GetVertexPositions().ToArray(), output.GetVertexColors().ToArray(), output.GetVertexCount());

		// Alert the plugin that it can start initializing the relevant data
		GL.IssuePluginEvent(GetRenderEventFunc(), (int) PluginEvent.Initialize);

		// Start calling the render method of the plugin
		StartCoroutine(callPluginAtEndOfFrame());
	}

    private IEnumerator callPluginAtEndOfFrame() {
		while (true) {
			// Wait until all frame rendering is done
			yield return new WaitForEndOfFrame();

			// Update graphics matrices
			updateGraphics();

			// Issue a plugin event with rendering id
			GL.IssuePluginEvent(GetRenderEventFunc(), (int) PluginEvent.Render);
		}
	}

    private void OnValidate() {
		// Update point size and workgroups automatically on validate
		if (_pointSize > 0 && _amountOfWorkgroups > 0) {
			setShaderDrawDataInternal(_pointSize, _amountOfWorkgroups);
		}
    }

    private static void setShaderDrawDataInternal(float pPointSize, int pAmountOfWorkgroups) {
		SetShaderDrawData(pPointSize, pAmountOfWorkgroups);
    }

	/// <summary>
	/// Sends a pointer to the position array and color array of the current pointcloud, so that the vertices
	/// can be used in the mesh shader, by calling the SetShaderPointData method of the plugin.
	/// </summary>
	/// <param name="pPositions">An array of position vertices</param>
	/// <param name="pColors">An array of color vertices</param>
	/// <param name="pVertexCount">Amount of vertices</param>
	private static unsafe void setShaderPointDataInternal(
		PointXYZW[] pPositions,
		PointXYZW[] pColors,
		int pVertexCount
	) {
        fixed (PointXYZW* positionPtr = &pPositions[0], colorPtr = &pColors[0]) {
            SetShaderPointData(pVertexCount, positionPtr, colorPtr);
        }
    }

	/// <summary>
	/// Sends the relevant matrices (projection, view and model) to the plugin by calling the SetGraphicsMatrices method of the plugin.
	/// </summary>
    private void updateGraphics() {
		Matrix4x4 projectionMatrix = GL.GetGPUProjectionMatrix(Camera.main.projectionMatrix, false);
		Matrix4x4 viewMatrix = Camera.main.worldToCameraMatrix;
		Matrix4x4 modelMatrix = transform.localToWorldMatrix;

		SetGraphicsMatrices(
			matrixToArray(projectionMatrix),
			matrixToArray(viewMatrix),
			matrixToArray(modelMatrix)
		);
	}

	/// <summary>
	/// Converts a Matrix4x4 to a 1D float array.
	/// </summary>
	/// <param name="pMatrix">A 4x4 Matrix</param>
	/// <returns>1D float array consisting of matrix data</returns>
	private float[] matrixToArray(Matrix4x4 pMatrix) {
		float[] result = new float[16];
		for (int row = 0; row < 4; row++) {
			for (int col = 0; col < 4; col++) {
				result[row + col * 4] = pMatrix[row, col];
			}
		}
		return result;
	}

	private static void DebugMethod(string message) {
		Debug.Log("[RenderingPlugin] Debug: " + message);
	}
}
