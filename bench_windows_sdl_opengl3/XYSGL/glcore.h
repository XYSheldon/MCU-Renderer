#ifndef __GLCORE_H__
#define __GLCORE_H__
#include "geometry.h"

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 360;
const float ASPECT = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;
const float PI = 3.141592653;
extern Matrix Mvp,Mper,Mcam;
extern Matrix Viewport;
extern Matrix ModelView;
extern Matrix Projection;
extern unsigned char render_data[SCREEN_HEIGHT][SCREEN_WIDTH][4];
extern unsigned char zbimage[SCREEN_HEIGHT][SCREEN_WIDTH][4];
extern float zbuffer[SCREEN_HEIGHT][SCREEN_WIDTH];
extern Vec3f light_dir,eye,direction,center,up;
extern float fov,far,near;
extern long long facecount;
void updatematrix();
//void triangle(Vec4f *pts, IShader &shader, TGAImage &image, float *zbuffer);
void triangle(mat<3, 4, float> &pts, float intensity, bool force);
void wireframe(mat<3, 4, float> &pts, float intensity, bool force);
#endif //__GLCORE_H__
