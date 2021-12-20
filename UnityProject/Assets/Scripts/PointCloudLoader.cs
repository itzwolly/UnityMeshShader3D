using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading.Tasks;
using UnityEngine;

public class PointCloudLoader {
    public virtual async Task<PointCloud> LoadFile(string pFilePath) {
        return new PointCloud(new List<PointXYZW>(), new List<PointXYZW>());
    }
}
