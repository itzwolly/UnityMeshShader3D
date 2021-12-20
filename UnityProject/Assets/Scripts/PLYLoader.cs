using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using UnityEngine;

public class PLYLoader : PointCloudLoader {

    public override async Task<PointCloud> LoadFile(string pFilePath) {
        Task<PointCloud> pcTask = Task.Run(() => loadPointCloud(pFilePath));
        await pcTask;
        return pcTask.Result;
    }

    private PointCloud loadPointCloud(string pFilePath) {
        Debug.Log("[PLYLoader] Async start loading point cloud");

        List<PointXYZW> positions = new List<PointXYZW>();
        List<PointXYZW> colors = new List<PointXYZW>();

        if (!pFilePath.EndsWith(".ply")) {
            Debug.LogError("[PlyLoader] Invalid point cloud file type. Expected: .ply");
            return new PointCloud(positions, colors);
        }

        int readCount = 0;
        int vertexCount = 0;
        int faceCount = 0;

        float? offsetX = null;
        float? offsetY = null;
        float? offsetZ = null;

        List<Property> properties = new List<Property>();

        using (StreamReader sr = new StreamReader(pFilePath)) {
            string line = "";
            while ((line = sr.ReadLine()) != null) {
                readCount += line.Length + 1;
                if (line.Length > 0) {
                    if (line == "end_header") {
                        break;
                    } else {
                        string[] col = line.Split();
                        if (col[0] == "element") {
                            if (col[1] == "vertex") {
                                vertexCount = Convert.ToInt32(col[2]);
                            } else if (col[1] == "face") {
                                faceCount = Convert.ToInt32(col[2]);
                            }
                        } else if (col[0] == "property") {
                            properties.Add(new Property(col[1], col[2]));
                        }
                    }
                }
            }

            sr.BaseStream.Position = readCount;
            BinaryReader br = new BinaryReader(sr.BaseStream);

            for (int i = 0; i < vertexCount; i++) {
                float x = 0;
                float y = 0;
                float z = 0;
                float w = 1;

                byte r = 0;
                byte g = 0;
                byte b = 0;
                byte a = 0;

                for (int j = 0; j < properties.Count; j++) {
                    Property prop = properties[j];
                    switch (prop.GetValueType()) {
                        case "float":
                            if (prop.getName() == "x") {
                                x = br.ReadSingle();
                            } else if (prop.getName() == "y") {
                                y = br.ReadSingle();
                            } else if (prop.getName() == "z") {
                                z = br.ReadSingle();
                            } else {
                                br.ReadSingle(); // ignore if its not x, y or z
                            }
                            continue;
                        case "uchar":
                            if (prop.getName() == "red") {
                                r = br.ReadByte();
                            } else if (prop.getName() == "green") {
                                g = br.ReadByte();
                            } else if (prop.getName() == "blue") {
                                b = br.ReadByte();
                            } else if (prop.getName() == "alpha") {
                                a = br.ReadByte();
                            } else {
                                br.ReadByte(); // ignore if its not red, green, blue or alpha
                            }
                            continue;
                        default:
                            // TODO: implement default case to skip amount of bytes if relevant
                            continue;
                    }
                }

                if (offsetX == null && offsetY == null && offsetZ == null) {
                    offsetX = x;
                    offsetY = y;
                    offsetZ = z;
                }

                if (offsetX != null && offsetY != null && offsetZ != null) {
                    x -= offsetX.Value;
                    y -= offsetY.Value;
                    z -= offsetZ.Value;
                }

                float red = (r > 1) ? r / 255f : r;
                float green = (g > 1) ? g / 255f : g;
                float blue = (b > 1) ? b / 255f : b;
                float alpha = (a > 1) ? a / 255f : a;


                PointXYZW position = new PointXYZW {
                    X = x,
                    Y = y,
                    Z = z,
                    W = w
                };

                PointXYZW color = new PointXYZW {
                    X = red,
                    Y = green,
                    Z = blue,
                    W = alpha
                };

                positions.Add(position);
                colors.Add(color);
            }
        }

        Debug.Log("[PLYLoader] Async done loading point cloud");
        return new PointCloud(positions, colors);
    }
}
