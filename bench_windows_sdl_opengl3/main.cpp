// dear imgui: standalone example application for SDL2 + OpenGL
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)
// (GL3W is a helper library to access OpenGL functions since there is no standard header to access modern OpenGL functions easily. Alternatives are GLEW, Glad, etc.)

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "model.h"
#include "geometry.h"
#include <stdio.h>
#include <algorithm>
#include <SDL.h>
#include <vector>
#include <iostream>
#include <cmath>
#include <limits>

#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 400
#define DEPTH 255
#define PI 3.141592653
// About Desktop OpenGL function loaders:
//  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
//  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
//  You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h> // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h> // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h> // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING)
#include <glbinding/glbinding.h> // Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

Model *model = NULL;
Vec3f light_dir = Vec3f(-1, 1, -1).normalize();
Vec3f eye(1, 0, 3);
Vec3f center(0, 0, 0);

int mainbitmap_width = SCREEN_WIDTH;
int mainbitmap_height = SCREEN_HEIGHT;
GLuint mainbitmap_texture = 0;
GLuint zbufferbitmap_texture = 0;
unsigned char *image_data;
unsigned char render_data[SCREEN_HEIGHT][SCREEN_WIDTH][4];
unsigned char zbimage[SCREEN_HEIGHT][SCREEN_WIDTH][4];
int zbuffer[SCREEN_HEIGHT][SCREEN_WIDTH];

void InitialBitmap()
{
    // Create a OpenGL texture identifier
    glGenTextures(1, &mainbitmap_texture);
    glBindTexture(GL_TEXTURE_2D, mainbitmap_texture);
    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Upload pixels into texture
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

Matrix viewport(int x, int y, int w, int h)
{
    Matrix m = Matrix::identity(4);
    m[0][3] = x + w / 2.f;
    m[1][3] = y + h / 2.f;
    m[2][3] = DEPTH / 2.f;

    m[0][0] = w / 2.f;
    m[1][1] = h / 2.f;
    m[2][2] = DEPTH / 2.f;
    return m;
}

Matrix lookat(Vec3f eye, Vec3f center, Vec3f up)
{
    Vec3f z = (eye - center).normalize();
    Vec3f x = (up ^ z).normalize();
    Vec3f y = (z ^ x).normalize();
    Matrix res = Matrix::identity(4);
    for (int i = 0; i < 3; i++)
    {
        res[0][i] = x[i];
        res[1][i] = y[i];
        res[2][i] = z[i];
        res[i][3] = -center[i];
    }
    return res;
}

void triangle(Vec3i t0, Vec3i t1, Vec3i t2, float intensity)
{
    if (t0.y == t1.y && t0.y == t2.y)
        return; // i dont care about degenerate triangles
    if (t0.y > t1.y)
    {
        std::swap(t0, t1);
    }
    if (t0.y > t2.y)
    {
        std::swap(t0, t2);
    }
    if (t1.y > t2.y)
    {
        std::swap(t1, t2);
    }

    int total_height = t2.y - t0.y;
    for (int i = 0; i < total_height; i++)
    {
        bool second_half = i > t1.y - t0.y || t1.y == t0.y;
        int segment_height = second_half ? t2.y - t1.y : t1.y - t0.y;
        float alpha = (float)i / total_height;
        float beta = (float)(i - (second_half ? t1.y - t0.y : 0)) / segment_height; // be careful: with above conditions no division by zero here
        Vec3i A = t0 + Vec3f(t2 - t0) * alpha;
        Vec3i B = second_half ? t1 + Vec3f(t2 - t1) * beta : t0 + Vec3f(t1 - t0) * beta;
        if (A.x > B.x)
        {
            std::swap(A, B);
        }
        for (int j = A.x; j <= B.x; j++)
        {
            float phi = B.x == A.x ? 1. : (float)(j - A.x) / (float)(B.x - A.x);
            Vec3i P = Vec3f(A) + Vec3f(B - A) * phi;
            if (P.x>=SCREEN_WIDTH||P.y>=SCREEN_HEIGHT||P.x<0||P.y<0) continue;
            if (zbuffer[P.y][P.x] < P.z)
            {
                zbuffer[P.y][P.x] = P.z;
                render_data[SCREEN_HEIGHT-1-P.y][P.x][0]= 255*intensity;
                render_data[SCREEN_HEIGHT-1-P.y][P.x][1]= 255*intensity;
                render_data[SCREEN_HEIGHT-1-P.y][P.x][2]= 255*intensity;
                render_data[SCREEN_HEIGHT-1-P.y][P.x][3]= 255;
            }
        }
    }
}

// Main code
int main(int argc, char **argv)
{

    if (2 == argc)
    {
        model = new Model(argv[1]);
    }
    else
    {
        model = new Model("obj/LowPoly.obj");
    }

    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 Core + GLSL 150
    const char *glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow(u8"一个用于嵌入式设备的3D光栅器引擎 by XYSheldon", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING)
    bool err = false;
    glbinding::initialize([](const char *name) { return (glbinding::ProcAddress)SDL_GL_GetProcAddress(name); });
#else
    bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    /* Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    */
    ImFontConfig fontconfig;
    fontconfig.FontNo = 0;
    ImFont *font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\Dengb.ttf", 18.0f, &fontconfig, io.Fonts->GetGlyphRangesChineseFull());
    IM_ASSERT(font != NULL);

    // Our state
    bool show_demo_window = false;
    bool show_another_window = true;
    bool show_bitmap_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    InitialBitmap();

    for (int i = 0; i < SCREEN_HEIGHT; i++)
        for (int j = 0; j < SCREEN_WIDTH; j++)
        {
            zbuffer[i][j] = std::numeric_limits<int>::min();
        }

    for (int i = 0; i < SCREEN_HEIGHT; i++)
        for (int j = i / 2; j < i; j++)
        {
            render_data[i][j][0] = 128;
            render_data[i][j][1] = 128;
            render_data[i][j][2] = 128;
            render_data[i][j][3] = 255;
        }

    ImVec2 pos;
    int scopex, scopey;
    float ang = 0;
    Matrix ViewPort = viewport(SCREEN_WIDTH / 8, SCREEN_HEIGHT / 8, SCREEN_WIDTH * 3 / 4, SCREEN_HEIGHT * 3 / 4);
    // Main loop
    bool done = false;
    while (!done)
    {
        ang += 0.1;
        if (ang > PI * 2)
            ang = 0;
        eye[0] = 2*cos(ang);
        eye[2] = 2*sin(ang);
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        {
            for (int i = 0; i < SCREEN_HEIGHT; i++)
                for (int j = 0; j < SCREEN_WIDTH; j++)
                {
                    zbuffer[i][j] = std::numeric_limits<int>::min();
                }
            memset(render_data, 0, sizeof(render_data));
            // draw the model
            Matrix ModelView = lookat(eye, center, Vec3f(0, 1, 0));
            Matrix Projection = Matrix::identity(4);
            Projection[3][2] = -1.f / (eye - center).norm();

            //std::cerr << ModelView << std::endl;
            //std::cerr << Projection << std::endl;
            //std::cerr << ViewPort << std::endl;
            Matrix z = (ViewPort * Projection * ModelView);
            //std::cerr << z << std::endl;
            Vec3i screen_coords[3];
            Vec3f world_coords[3];
            std::vector<int> face;
            Vec3f v,n;
            for (int i = 0; i < model->nfaces(); i++)
            {
                face = model->face(i);

                for (int j = 0; j < 3; j++)
                {
                    v = model->vert(face[j]);
                    screen_coords[j] = Vec3f(z * Matrix(v));
                    world_coords[j] = v;
                }
                n = (world_coords[2] - world_coords[0]) ^ (world_coords[1] - world_coords[0]);
                n.normalize();
                float intensity = n * light_dir;
                if (intensity > 0)
                {
                    triangle(screen_coords[0], screen_coords[1], screen_coords[2], intensity);
                }else
                {
                    triangle(screen_coords[0], screen_coords[1], screen_coords[2], 0);
                }
            }
        }
        { /* dump z-buffer (debugging purposes only)
            for (int i = 0; i < SCREEN_WIDTH; i++)
            {
                for (int j = 0; j < SCREEN_HEIGHT; j++)
                {
                    zbimage[j][i][0] = zbuffer[j][i];
                    zbimage[j][i][1] = zbuffer[j][i];
                    zbimage[j][i][2] = zbuffer[j][i];
                    zbimage[j][i][3] = 255;
                }
            }*/
        }

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin(u8"窗口管理器"); // Create a window called "Hello, world!" and append into it.

            ImGui::Text(u8"这是一个用于管理工作区窗口的工具");       // Display some text (you can use a format strings too)
            ImGui::Checkbox(u8"imgui的官方Demo", &show_demo_window); // Edit bools storing our window open/close state
            ImGui::Checkbox(u8"XYSheldon", &show_another_window);
            ImGui::Checkbox(u8"Bitmap(LCD NOKIA 5110)屏幕输出模拟", &show_bitmap_window);
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);             // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float *)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("XYSheldon", &show_another_window); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        if (show_bitmap_window)
        {
            image_data = &render_data[0][0][0];
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mainbitmap_width, mainbitmap_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

            scopex = std::min(std::max((int)(io.MousePos.x - pos.x), 0), SCREEN_WIDTH - 1);
            scopey = std::min(std::max((int)(io.MousePos.y - pos.y), 0), SCREEN_HEIGHT - 1);
            char buf[128];
            //sprintf(buf, "Animated title %c %d###AnimatedTitle", "|/-\\"[(int)(ImGui::GetTime() / 0.25f) & 3], ImGui::GetFrameCount());
            sprintf(buf, u8"位图模拟(LCD NOKIA 5110) 无缩放 1:1 Size: (%d x %d)", mainbitmap_width, mainbitmap_height);
            ImGui::Begin(buf);
            //ImGui::Text("pointer = %p", mainbitmap_texture);
            if (ImGui::IsMousePosValid())
            {
                ImGui::Text("Mouse pos: (%d, %d) |", scopex, scopey);
                ImGui::SameLine();
                ImGui::Text("Pixel Color = R:%d G:%d B:%d A:%d", render_data[scopey][scopex][0], render_data[scopey][scopex][1], render_data[scopey][scopex][2], render_data[scopey][scopex][3]);
            }
            else
                ImGui::Text("Mouse pos: <INVALID>");
            ImGui::Text(u8"Average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            pos = ImGui::GetCursorScreenPos();
            ImGui::Image((void *)(intptr_t)mainbitmap_texture, ImVec2(mainbitmap_width, mainbitmap_height));
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    delete model;
    return 0;
}
