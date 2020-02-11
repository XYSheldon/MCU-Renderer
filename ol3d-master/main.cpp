//#include <SPI.h>
#include "ol3d_core.h"
#include "Wolf.h" // .obj data
typedef unsigned char uint8_t;
//void OLED_init();
//void OLED_fill(unsigned char *buf);

// Declare display buffer, see ol3d_render.h
uint8_t buffer[128*128*3] = {};

float roll, yaw;

void setup () {
    //SPI.begin();
    //SPI.setClockDivider(SPI_CLOCK_DIV2);
    //OLED_init(); // initialize your screen
    yaw = 0;
}

int main () {

// MVP matrix
    ol3d_matrix_t p;
    ol3d_matrix_setPerspective(p, 60, 1, 1, 1000);
    ol3d_matrix_t v;
    ol3d_matrix_setTranslate(v, 0, 0, -700);
    ol3d_matrix_t vp;
    ol3d_matrix_multiply(v, p, vp);
    ol3d_matrix_t mvp;

// Render process
    for(;;) {
        // Model matrix of current frame
        ol3d_matrix_t m = MATRIX_UNIT;
        ol3d_matrix_translate(m, 0, -80, 0);    // Translate to center
        ol3d_matrix_rotate(m, (float)yaw, M_AXIS_Y);    // Rotate
        yaw += 8;

        ol3d_matrix_multiply(m, vp, mvp);

        ol3d_clean_buffer(buffer);
        ol3d_draw_Element(buffer, mesh_f, mesh_v, mesh_v, 100, mvp);
        //OLED_fill(buffer);
    }
    return 0;
}