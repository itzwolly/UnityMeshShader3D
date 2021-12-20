using System.Collections.Generic;
using UnityEngine;

public class PointCloud {
    int _vertexCount;
    List<PointXYZW> _positions;
    List<PointXYZW> _colors;

    public PointCloud(List<PointXYZW> pPositions, List<PointXYZW> pColors) {
        _positions = pPositions;
        _colors = pColors;
        _vertexCount = _positions.Count;
    }

    public int GetVertexCount() {
        return _vertexCount;
    }

    public List<PointXYZW> GetVertexPositions() {
        return _positions;
    }

    public List<PointXYZW> GetVertexColors() {
        return _colors;
    }
}
