#ifndef __MODEL_H__
#define __MODEL_H__
#include <vector>
#include <string>
#include "geometry.h"

class Model {
private:
    std::vector<Vec3f> verts_;
    std::vector<std::vector<Vec3i> > faces_; // attention, this Vec3i means vertex/uv/normal
    std::vector<Vec3f> faces_n;
public:
    Model(const char *filename);
    ~Model();
    int nverts();                       //Number of Vertexes 
    int nfaces();                       //Number of Faces
    Vec3f vert(int i);
    Vec3f vert(int iface, int nthvert);
    Vec3f faces_normal(int iface);
    std::vector<int> face(int idx);
};
#endif //__MODEL_H__

