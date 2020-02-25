#include <cmath>
#include <limits>
#include <cstdlib>
#include "glcore.h"
Matrix Mvp = Matrix::identity(), Mper = Matrix::identity(), Mcam = Matrix::identity();
Matrix ModelView;
Matrix Viewport;
Matrix Projection;
Vec3f light_dir(-1, 1, -1);
Vec3f eye(0, 0, -1);
Vec3f direction(0, 0, -1);
Vec3f center = eye + direction;
Vec3f up(0, 1, 0.0f);
long long facecount=0;
float fov = 90 * (PI / 180.0f);
float near = 1.0f / std::tan(fov / 2), far = 100.f;
unsigned char render_data[SCREEN_HEIGHT][SCREEN_WIDTH][4];
unsigned char zbimage[SCREEN_HEIGHT][SCREEN_WIDTH][4];
float zbuffer[SCREEN_HEIGHT][SCREEN_WIDTH];

void updatematrix()
{
    Mvp[0][0] = (float)SCREEN_WIDTH / 2;
    Mvp[0][3] = ((float)SCREEN_WIDTH - 1) / 2;
    Mvp[1][1] = (float)SCREEN_HEIGHT / 2;
    Mvp[1][3] = ((float)SCREEN_HEIGHT - 1) / 2;

    near = 1.0f / std::tan(fov / 2);
    Mper[0][0] = near;
    Mper[1][1] = near * ASPECT;
    Mper[2][2] = (far + near) / (near - far);
    Mper[2][3] = 2 * far * near / (far - near);
    Mper[3][2] = 1.0f;
    Mper[3][3] = 0.0f;

    Vec3f w = direction * -1.0f;
    Vec3f u = cross(up, w).normalize();
    Vec3f v = cross(w, u);
    Mcam[0][0] = u.x;
    Mcam[0][1] = u.y;
    Mcam[0][2] = u.z;
    Mcam[1][0] = v.x;
    Mcam[1][1] = v.y;
    Mcam[1][2] = v.z;
    Mcam[2][0] = w.x;
    Mcam[2][1] = w.y;
    Mcam[2][2] = w.z;
    Mcam[0][3] = u * eye * -1;
    Mcam[1][3] = v * eye * -1;
    Mcam[2][3] = w * eye * -1;
}

Vec3f barycentric(Vec2f A, Vec2f B, Vec2f C, Vec2f P)
{
    Vec3f s[2];
    for (int i = 2; i--;)
    {
        s[i][0] = C[i] - A[i];
        s[i][1] = B[i] - A[i];
        s[i][2] = A[i] - P[i];
    }
    Vec3f u = cross(s[0], s[1]);
    if (std::abs(u[2]) > 1e-2) // dont forget that u[2] is integer. If it is zero then triangle ABC is degenerate
        return Vec3f(1.f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z);
    return Vec3f(-1, 1, 1); // in this case generate negative coordinates, it will be thrown away by the rasterizator
}

void triangle(mat<3, 4, float> &pts, float intensity, bool force)
{
    //mat<3, 4, float> pts = (clipc).transpose(); // transposed to ease access to each of the points
    if (!force && (pts[0][3] > far || pts[1][3] > far || pts[2][3] > far || pts[0][3] < 0.f || pts[1][3] < 0.f || pts[2][3] < 0.f))
        return;
    facecount++;
    Vec2f bboxmin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vec2f bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    Vec2f clamp(SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
    for (int i = 0; i < 3; i++)
    {
        pts[i] = pts[i] / pts[i][3];
        for (int j = 0; j < 2; j++)
        {
            bboxmin[j] = std::max(0.f, std::min(bboxmin[j], pts[i][j]));
            bboxmax[j] = std::min(clamp[j], std::max(bboxmax[j], pts[i][j]));
        }
    }

    Vec2i P;
    Vec3f bc_screen;
    Vec3f s[2];
    Vec3f u;
    for (int i = 2; i--;)
    {
        s[i][0] = pts[2][i] - pts[0][i];
        s[i][1] = pts[1][i] - pts[0][i];
        //s[i][2] = A[i] - P[i];
    }
    for (P.x = bboxmin.x; P.x <= bboxmax.x; P.x++)
    {
        for (P.y = bboxmin.y; P.y <= bboxmax.y; P.y++)
        {
            //bc_screen = barycentric(pts2[0], pts2[1], pts2[2], P);
            for (int i = 2; i--;)
            {
                //s[i][0] = C[i] - A[i];
                //s[i][1] = B[i] - A[i];
                s[i][2] = pts[0][i] - P[i];
            }
            u = cross(s[0], s[1]);
            if (std::abs(u[2]) > 1e-2) // dont forget that u[2] is integer. If it is zero then triangle ABC is degenerate
                bc_screen = Vec3f(1.f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z);
            else
            {
                continue;
                bc_screen = Vec3f(-1, 1, 1); // in this case generate negative coordinates, it will be thrown away by the rasterizator
            }
            if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0)
                continue;
            float Pz = pts[0][2] * bc_screen.x + pts[1][2] * bc_screen.y + pts[2][2] * bc_screen.z;
            if (zbuffer[P.y][P.x] > Pz)
                continue;
            if (1)
            {
                zbuffer[P.y][P.x] = Pz;
                render_data[SCREEN_HEIGHT - 1 - P.y][P.x][0] = 255 * intensity;
                render_data[SCREEN_HEIGHT - 1 - P.y][P.x][1] = 255 * intensity;
                render_data[SCREEN_HEIGHT - 1 - P.y][P.x][2] = 255 * intensity;
                render_data[SCREEN_HEIGHT - 1 - P.y][P.x][3] = 255;
            }
        }
    }
}
void wireframe(mat<3, 4, float> &pts, float intensity, bool force)
{

    float x0 = std::max(0.f, std::min(pts[0][0], (float)SCREEN_WIDTH - 1));
    float x1 = std::max(0.f, std::min(pts[1][0], (float)SCREEN_WIDTH - 1));
    float y0 = std::max(0.f, std::min(pts[0][1], (float)SCREEN_HEIGHT - 1));
    float y1 = std::max(0.f, std::min(pts[1][1], (float)SCREEN_HEIGHT - 1));
    bool steep = false;
    if (std::fabs(x0 - x1) < std::fabs(y0 - y1))
    {
        std::swap(x0, y0);
        std::swap(x1, y1);
        steep = true;
    }
    if (x0 > x1)
    {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    for (int x = x0; x <= x1; x++)
    {
        float t = (x - x0) / (float)(x1 - x0);
        int y = y0 * (1. - t) + y1 * t;
        if (steep)
        {
            render_data[SCREEN_HEIGHT - 1 - x][y][0] = 255 * intensity;
            render_data[SCREEN_HEIGHT - 1 - x][y][1] = 255 * intensity;
            render_data[SCREEN_HEIGHT - 1 - x][y][2] = 255 * intensity;
            render_data[SCREEN_HEIGHT - 1 - x][y][3] = 255;
        }
        else
        {
            render_data[SCREEN_HEIGHT - 1 - y][x][0] = 255 * intensity;
            render_data[SCREEN_HEIGHT - 1 - y][x][1] = 255 * intensity;
            render_data[SCREEN_HEIGHT - 1 - y][x][2] = 255 * intensity;
            render_data[SCREEN_HEIGHT - 1 - y][x][3] = 255;
        }
    }
}