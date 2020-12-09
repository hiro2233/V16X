#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <pthread.h>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <SDL2/SDL_ttf.h>
#include <GL/gl.h>
#include <GL/glut.h>

#include "external.h"
#include "events.h"

SDL_Surface* CreateRGBASurface(int width, int height);


int default_port = 8080;
int connfd = 0;
int retconn = 0;
typedef struct sockaddr SA;

extern "C" {

int EXPORTFUNC connect_to_deepservice(char ip[], int len, int port)
{
    struct sockaddr_in serveraddr;
    char *iptmp = new char[len + 1];

    memset(iptmp, 0, len + 1);
    memset(&serveraddr, 0, sizeof(serveraddr));
    memcpy(iptmp, ip, len);

    if ((connfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("connect failed 1\n");
    }
    fcntl(connfd, F_SETFL, O_NONBLOCK);

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    //inet_aton("127.0.0.1", &serveraddr.sin_addr.s_addr);
    serveraddr.sin_addr.s_addr = inet_addr(iptmp);
    //serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);

    if (connect(connfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0)
    {
        printf("connect failed 2 fd: %d\n", connfd);
    }

    printf("Connected\n");
    retconn = 1;
    return retconn;
}

EM_JS(void, on_send_sds_failed, (), {
    globals.on_send_sds_failed();
});

void EXPORTFUNC send_deepservice_ping()
{
    static int cnt_msg = 0;
    const char message[] = "V16X qstr=1&ping=1";
    char server_reply[1024] = {0};

    if (read(connfd, server_reply, 1024) < 0)
    {
        printf("SDS recv failed\n");
    }

    if (strlen(server_reply) > 0) {
        printf("SDS reply %d: %s\n", cnt_msg++, server_reply);
    }

    if (write(connfd, message, strlen(message)) < 0)
    {
        printf("SDS send failed\n");
        close(connfd);
        on_send_sds_failed();
    }
}

}

bool sw_internal = false;

extern "C" {
int Sum(int a, int b)
{
    int sumab = a + b;
    return sumab;
}

int Sum_ccall(int a, int b)
{
    int sumab = a + b;
    return sumab;
}

void EXPORTFUNC console_printf(char dat[])
{
    printf("%s\n", dat);
}
}

EM_JS(void, tim2, (), {
    if (globals.initialized) {
        var s1 = 'Sum internal: ' + Module._Sum(8, 20);
        console.log(s1);
        console_printf(s1);
    } else {
        console_printf('Tim2 nel');
    }
    console_printf('Tim2 global: ' + testglobal);
    console.log("Tim2 global:", testglobal);
});

EM_JS(void, take_args, (int x, float y), {
    setTimeout(tim2, 1000);
    console_printf('Take args received: [' + x + ', ' + y + ']');
    console_printf('Call Sum from Take args: ' + Module._Sum(1, 4));
    //var data = "test";
    //var stream = FS.open('data.txt', 'w+');
    //FS.write(stream, data, 0, data.length, 0);
    //FS.close(stream);
});

EM_JS(void, set_element_fps_angle, (float fps, float angle), {
    var element_fps = document.getElementById('fps');
    var element_angle = document.getElementById('angle');
    var result_fps = Math.round((fps * 100) / 100);

    element_fps.innerHTML = result_fps;
    element_angle.innerHTML = Math.round(angle);
});

EM_JS(void, init_globals, (), {
    console.log("init globals internal");
    var imported = document.createElement('script');
    var xmlhttp_system = new XMLHttpRequest();

    function xmlhttp_respone() {

        if (this.readyState == 4 && this.status == 200) {
            var obj_response = this.responseText;
            imported.type = "application/javascript";
            imported.text = obj_response;
            document.getElementsByTagName('body')[0].appendChild(imported);
            window.testglobal = "XHR ready state1";
        }
    };

    xmlhttp_system.onreadystatechange = xmlhttp_respone;

    xmlhttp_system.overrideMimeType("application/javascript");
    xmlhttp_system.open("GET", "globals.js", true);
    xmlhttp_system.send();
});

int init_functions()
{
    if (sw_internal) {
        take_args(0, 0);
    }

    EM_ASM(
        console.log("EM_ASM: init functions: write data1.txt");
/*
		FS.mkdir('/home/web_user/test');
        FS.mkdir('/home/web_user/test/data');
		FS.mount(IDBFS, {}, '/home/web_user/test/data');
		FS.syncfs(true, function(err) {
			if (err) {
				console.log(err);
			} else {
				console.log('Init functions: File system synced.');
			}
		});
*/
        FS.writeFile("data1.txt", "a=1\nb=2\n");
        FS.writeFile("data1.txt", new Uint8Array([99, 61, 51]) /* c=3 */, { flags: "a" });
    );

    printf("Native: init functions: write data1.txt\n");
    std::ifstream file("data1.txt");

    while(!file.eof() && !file.fail()) {
        std::string line;
        getline(file, line);
        std::string name;

        std::cout << "read " << line << std::endl;

        size_t equalsPos = 1;

        size_t notSpace = line.find_first_not_of(" \t", equalsPos);

        if(notSpace != std::string::npos && notSpace != equalsPos)
        {
            line.erase(std::remove_if(line.begin(), line.begin() + notSpace, isspace), line.end());

            equalsPos = line.find('=');
        }

        if(equalsPos == std::string::npos)
            continue;

        name = line.substr(0, equalsPos);
    }

    return 0;
}

/*
int main_sdl(int argc, char** argv) {
    printf("sdl init\n");

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface *screen = SDL_SetVideoMode(640, 480, 8, SDL_HWSURFACE);
    //SDL_Surface *screen = SDL_SetVideoMode(800, 600, 32, SDL_OPENGL);
    //SDL_Surface *screen = CreateRGBASurface(640, 480);

#ifdef TEST_SDL_LOCK_OPTS
    EM_ASM("SDL.defaults.copyOnLock = false; SDL.defaults.discardOnLock = true; SDL.defaults.opaqueFrontBuffer = false;");
#endif

    if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
    for (int i = 0; i < 512; i++) {
        for (int j = 0; j < 512; j++) {
#ifdef TEST_SDL_LOCK_OPTS
        // Alpha behaves like in the browser, so write proper opaque pixels.
        int alpha = 256;
#else
        // To emulate native behavior with blitting to screen, alpha component is ignored. Test that it is so by outputting
        // data (and testing that it does get discarded)
        int alpha = (i+j) % 512;
#endif
        *((Uint32*)screen->pixels + i * 512 + j) = SDL_MapRGBA(screen->format, i, j, 511-i, alpha);
        }
    }
    if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
    SDL_Flip(screen); 
    SDL_Quit();

    return 0;
}

void drawCube(float size)
{
glBegin(GL_QUADS);
    // front face
    glColor3f(1.0,0.0,0.0);
    glVertex3f(size/2,size/2,size/2);
    glVertex3f(-size/2,size/2,size/2);
    glVertex3f(-size/2,-size/2,size/2);
    glVertex3f(size/2,-size/2,size/2);
    // left face
    glColor3f(0.0,1.0,0.0);
    glVertex3f(-size/2,size/2,size/2);
    glVertex3f(-size/2,-size/2,size/2);
    glVertex3f(-size/2,-size/2,-size/2);
    glVertex3f(-size/2,size/2,-size/2);
    // back face
    glColor3f(0.0,0.0,1.0);
    glVertex3f(size/2,size/2,-size/2);
    glVertex3f(-size/2,size/2,-size/2);
    glVertex3f(-size/2,-size/2,-size/2);
    glVertex3f(size/2,-size/2,-size/2);
    // right face
    glColor3f(1.0,1.0,0.0);
    glVertex3f(size/2,size/2,size/2);
    glVertex3f(size/2,-size/2,size/2);
    glVertex3f(size/2,-size/2,-size/2);
    glVertex3f(size/2,size/2,-size/2);
    // top face
    glColor3f(1.0,0.0,1.0);
    glVertex3f(size/2,size/2,size/2);
    glVertex3f(-size/2,size/2,size/2);
    glVertex3f(-size/2,size/2,-size/2);
    glVertex3f(size/2,size/2,-size/2);
    // bottom face
    glColor3f(0.0,1.0,1.0);
    glVertex3f(size/2,-size/2,size/2);
    glVertex3f(-size/2,-size/2,size/2);
    glVertex3f(-size/2,-size/2,-size/2);
    glVertex3f(size/2,-size/2,-size/2);
glEnd();
}

float angle = 0.0;
const int triangle = 1;
 
void init()
{
    glClearColor(0.0,0.0,0.0,1.0);  //background color and alpha
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45,640.0/480.0,1.0,500.0);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0,0.0,-5.0);
    glRotatef(angle,1.0,1.0,1.0);   // angle, x-axis, y-axis, z-axis
    drawCube(1.0);
}

int draw_cube_main1(int argc, char** argv)
{
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Surface *screen = SDL_SetVideoMode(640, 480, 8, SDL_OPENGL | SDL_DOUBLEBUF | SDL_HWACCEL);
    //screen = SDL_SetVideoMode(640, 480, 8, SDL_SWSURFACE);
    //SDL_Surface *screen = CreateRGBASurface(640, 480);
    //screen = SDL_SetVideoMode(640, 480, 8, SDL_SWSURFACE | SDL_OPENGL);
    //screen = SDL_SetVideoMode(640, 480, 8, SDL_SWSURFACE|SDL_FULLSCREEN);
    bool running = true;
    const int FPS = 30;
    Uint32 start;
    SDL_Event event;
    init();
    while(running) {
        start = SDL_GetTicks();
        while(SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
            }
        }
 
        display();
        SDL_GL_SwapBuffers();
        angle += 0.5;
        if(angle > 360)
            angle -= 360;
        if(1000/FPS > SDL_GetTicks()-start)
            SDL_Delay(1000/FPS-(SDL_GetTicks()-start));
    }
    SDL_Quit();
    return 0;
}
*/

/// draw cube 2

static SDL_GLContext ctx;
static SDL_Window* window;
float angle = 0.0;
static bool drawing = false;
static ImVec4 clear_color = ImVec4(0.0f, 0.0f, 1.0f, 1.00f);
static char websock_response[128] = {0};
static char stream_response[128] = {0};

void display();

extern "C" {
bool get_drawing();

void EXPORTFUNC set_websock_response(char resp[], int len)
{
    if (len > 128) {
        len = 128;
    }
    memset(websock_response, 0, 128);
    if (len > 0) {
        memcpy(websock_response, resp, len);
        return;
    }
    sprintf(websock_response, "%s", resp);
}

void EXPORTFUNC set_stream_response(char resp[], int len)
{
    if (len > 128) {
        len = 128;
    }
    memset(stream_response, 0, 128);
    if (len > 0) {
        memcpy(stream_response, resp, len);
        return;
    }
    sprintf(stream_response, "%s", resp);
}

void set_cube_angle(float cube_angle)
{
    angle = cube_angle;

    if(!drawing && window) {
        if (drawing) {
            return;
        }
    }
}

void quit(int rc)
{
/*
    if (ctx) {
        SDL_GL_DeleteContext(ctx);
    }

    SDL_DestroyWindow( window );
*/
    SDL_Quit();
}

void stop_drawing()
{
    if (drawing) {
        drawing = false;
        printf("stop autorotate cube\n");
        EM_ASM(
            var drawing = Module._get_drawing();
            console.log("stop drawing cube:", drawing);
        );
    }
}

bool get_drawing() {
    return drawing;
}
}

static void Render()
{
    static float color[8][3] = {
        {1.0, 1.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 1.0, 1.0},
        {1.0, 1.0, 1.0},
        {1.0, 0.0, 1.0},
        {0.0, 0.0, 1.0}
    };
    static float cube[8][3] = {
        {0.5, 0.5, -0.5},
        {0.5, -0.5, -0.5},
        {-0.5, -0.5, -0.5},
        {-0.5, 0.5, -0.5},
        {-0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5},
        {0.5, -0.5, 0.5},
        {-0.5, -0.5, 0.5}
    };

    glBegin(GL_QUADS);

    glColor3fv(color[0]);
    glVertex3fv(cube[0]);
    glColor3fv(color[1]);
    glVertex3fv(cube[1]);
    glColor3fv(color[2]);
    glVertex3fv(cube[2]);
    glColor3fv(color[3]);
    glVertex3fv(cube[3]);

    glColor3fv(color[3]);
    glVertex3fv(cube[3]);
    glColor3fv(color[4]);
    glVertex3fv(cube[4]);
    glColor3fv(color[7]);
    glVertex3fv(cube[7]);
    glColor3fv(color[2]);
    glVertex3fv(cube[2]);

    glColor3fv(color[0]);
    glVertex3fv(cube[0]);
    glColor3fv(color[5]);
    glVertex3fv(cube[5]);
    glColor3fv(color[6]);
    glVertex3fv(cube[6]);
    glColor3fv(color[1]);
    glVertex3fv(cube[1]);

    glColor3fv(color[5]);
    glVertex3fv(cube[5]);
    glColor3fv(color[4]);
    glVertex3fv(cube[4]);
    glColor3fv(color[7]);
    glVertex3fv(cube[7]);
    glColor3fv(color[6]);
    glVertex3fv(cube[6]);

    glColor3fv(color[5]);
    glVertex3fv(cube[5]);
    glColor3fv(color[0]);
    glVertex3fv(cube[0]);
    glColor3fv(color[3]);
    glVertex3fv(cube[3]);
    glColor3fv(color[4]);
    glVertex3fv(cube[4]);

    glColor3fv(color[6]);
    glVertex3fv(cube[6]);
    glColor3fv(color[1]);
    glVertex3fv(cube[1]);
    glColor3fv(color[2]);
    glVertex3fv(cube[2]);
    glColor3fv(color[7]);
    glVertex3fv(cube[7]);

    glEnd();
}

void display()
{
    static ImVec2 *vec2main;
    static bool show_another_window = false;
    static ImVec2 window_size;
    static int cnt_click = 0;

    ImGuiIO& io = ImGui::GetIO();

    int w, h;
    SDL_GL_MakeCurrent(window, ctx);
    SDL_GetWindowSize(window, &w, &h);
    glViewport(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0,0.0,-5.0);
    glMatrixMode(GL_MODELVIEW);
    glRotatef(angle,-1.0,-1.0,0.0); // angle, x-axis, y-axis, z-axis

    Render();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();

    float xpos = (io.DisplaySize.x / 2) - (window_size.x / 2);
    float ypos = (io.DisplaySize.y / 2) - (window_size.y / 2);
    if (!vec2main && window_size.x > 100) {
        vec2main = new ImVec2(260, 200);
        printf("Screen size x: %d y: %d\n", (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        printf("Window size x: %d y: %d\n", (int)window_size.x, (int)window_size.y);
        ImGui::SetNextWindowPos(ImVec2(xpos, ypos));
    }

    // Create a window called "Hello, world!" and append into it.
    ImGui::Begin("Hello World!");

    // Display some text (you can use a format strings too)
    ImGui::Text("V16X server test.\n\nClick on the button to show another\ndialog.\n\n");
    //ImGui::Text("Screen size imgui x: %.0f y: %.0f\n", io.DisplaySize.x, io.DisplaySize.y);
    //ImGui::Text("Screen size sdl x: %d y: %d\n", w, h);
    if (strlen(websock_response) <= 0) {
        sprintf(websock_response, "%s", "None");
    }
    if (strlen(stream_response) <= 0) {
        sprintf(stream_response, "%s", "None");
    }
    ImGui::Text("Web Socket Event response:\n%s\n\n", websock_response);
    ImGui::Text("Stream Event response:\n%s\n\n\n", stream_response);
    if (ImGui::Button("Show")) {
        show_another_window = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Print Console")) {
        printf("Click! %d\n", cnt_click++);
    }

    if (show_another_window) {
        // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)

#ifdef BUILD_EXTRA_DEMO
        ImGui::ShowDemoWindow(&show_another_window);
#endif
        ImGui::Begin("Another Window", &show_another_window);
        ImGui::Text("Hello from another window!.\n");
        ImGui::Text("Try resizing and moving this window!\n\n");
        if (ImGui::Button("Close"))
            show_another_window = false;
        ImGui::End();
    }

    window_size = ImGui::GetWindowSize();

    ImGui::End();
    ImGui::EndFrame();

    ImGui::Render();

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);

}

void draw_cube_main2(void* args)
{
    int done;
    uint64_t then, now, frames;
    const char* glsl_version = "#version 100";

    //Initialize SDL
    if (!window) {
        if (SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) < 0) {
            printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
            return;
        }
        //Create window
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_DisplayMode current;
        SDL_GetCurrentDisplayMode(0, &current);
        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        window = SDL_CreateWindow("Hello World!", 0, 0, 640, 480, window_flags);
    }

    if (window == NULL) {
        printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    //creating new context
    if (!ctx) {
        ctx = SDL_GL_CreateContext(window);
        SDL_GL_SetSwapInterval(1);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsClassic();

        // Setup Platform/Renderer backends
        ImGui_ImplSDL2_InitForOpenGL(window, ctx);
        ImGui_ImplOpenGL3_Init(glsl_version);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        //glOrtho(-2.0, 2.0, -2.0, 2.0, -20.0, 20.0);
        gluPerspective(45,640.0/250.0,1.0, 500.0);
        glMatrixMode(GL_MODELVIEW);
        //glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glShadeModel(GL_SMOOTH);
        glEnable(GL_LINE_SMOOTH);
        glEnable(GL_BLEND);
        glEnable(GL_POLYGON_SMOOTH );
    }

    frames = 0;
    then = (uint64_t)SDL_GetTicks();
    done = 0;
    const uint64_t FPS = 30;
    uint64_t start = 0;
    now = 0;

    while (!sw_internal) {
        frames++;
        SDL_Event ev;
        start = (uint64_t)SDL_GetTicks();

        while(SDL_PollEvent(&ev))
        {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if((SDL_QUIT == ev.type) ||
               (SDL_KEYDOWN == ev.type && SDLK_ESCAPE == ev.key.keysym.sym))
            {
                break;
            }
        }

        display();

        if (drawing) {
            angle += 0.5;
            if(angle > 360)
                angle -= 360;
        }

        if(1000 / FPS > (uint64_t)SDL_GetTicks() - start) {
            SDL_Delay(1000 / FPS - (uint64_t)(SDL_GetTicks() - start));
            now = (uint64_t)SDL_GetTicks();
            if (now > then) {
                float fps_data = ((float)frames * 1000) / (now - then);
                set_element_fps_angle(fps_data, angle);
            }
        }
    }

    printf("draw cube exit\n");
    drawing = false;
    emscripten_cancel_main_loop();
}

extern "C" {
void draw_cube()
{
    EM_ASM(
        var drawing = Module._get_drawing();
        console.log("drawing cube:", drawing);
    );

    if (drawing) {
        return;
    }

    printf("start autorotate cube\n");
/*
    if (drawing) {
        drawing = false;
        while (!drawing)
        {
            EM_ASM(
                var drawing = Module._get_drawing();
                console.log("Main: draw cube exit:", drawing);
            );
            drawing = false;
        }
    }
*/
    //emscripten_cancel_main_loop();
    drawing = true;

#ifdef __EMS__
    //int fps = 0; // Use browser's requestAnimationFrame
    //emscripten_set_main_loop_arg(draw_cube_main2, NULL, fps, false);
#else
    draw_cube_main2(NULL);
#endif
}
}

/// draw triangle

#if BUILD_TRIANGLE_EXAMPLE == 1
// Vertex shader
GLint shaderPan, shaderZoom, shaderAspect;
const GLchar* vertexSource =
    "uniform vec2 pan;                             \n"
    "uniform float zoom;                           \n"
    "uniform float aspect;                         \n"
    "attribute vec4 position;                      \n"
    "varying vec3 color;                           \n"
    "void main()                                   \n"
    "{                                             \n"
    "    gl_Position = vec4(position.xyz, 1.0);    \n"
    "    gl_Position.xy += pan;                    \n"
    "    gl_Position.xy *= zoom;                   \n"
    "    gl_Position.y *= aspect;                  \n"
    "    color = gl_Position.xyz + vec3(0.5);      \n"
    "}                                             \n";

// Fragment/pixel shader
const GLchar* fragmentSource =
    "precision mediump float;                     \n"
    "varying vec3 color;                          \n"
    "void main()                                  \n"
    "{                                            \n"
    "    gl_FragColor = vec4 ( color, 1.0 );      \n"
    "}                                            \n";

void updateShader(EventHandler& eventHandler)
{
    Camera& camera = eventHandler.camera();

    glUniform2fv(shaderPan, 1, camera.pan());
    glUniform1f(shaderZoom, camera.zoom()); 
    glUniform1f(shaderAspect, camera.aspect());
}

GLuint initShader(EventHandler& eventHandler)
{
    // Create and compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);

    // Create and compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);

    // Link vertex and fragment shader into shader program and use it
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);

    // Get shader variables and initialize them
    shaderPan = glGetUniformLocation(shaderProgram, "pan");
    shaderZoom = glGetUniformLocation(shaderProgram, "zoom");    
    shaderAspect = glGetUniformLocation(shaderProgram, "aspect");
    updateShader(eventHandler);

    return shaderProgram;
}

void initGeometry(GLuint shaderProgram)
{
    // Create vertex buffer object and copy vertex data into it
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLfloat vertices[] = 
    {
        0.0f, 0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Specify the layout of the shader vertex data (positions only, 3 floats)
    GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
}

void redraw(EventHandler& eventHandler)
{
    // Clear screen
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw the vertex buffer
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Swap front/back framebuffers
    eventHandler.swapWindow();
}

void mainLoop(void* mainLoopArg) 
{
    EventHandler& eventHandler = *((EventHandler*)mainLoopArg);
    eventHandler.processEvents();

    // Update shader if camera changed
    if (eventHandler.camera().updated())
        updateShader(eventHandler);

    redraw(eventHandler);
}

int draw_triangle_main(int argc, char** argv)
{
    EventHandler eventHandler("Hello Triangle");

    // Initialize shader and geometry
    GLuint shaderProgram = initShader(eventHandler);
    initGeometry(shaderProgram);

    // Start the main loop
    void* mainLoopArg = &eventHandler;

#ifdef __EMS__
    int fps = 0; // Use browser's requestAnimationFrame
    emscripten_set_main_loop_arg(mainLoop, mainLoopArg, fps, true);
#else
    while(true)
        mainLoop(mainLoopArg);
#endif

    return 0;
}
#endif

int main(int argc, char ** argv)
{
    init_globals();
    printf("Native: hello world main\n");

    init_functions();

    printf("Native: open and read preloaded data.txt chars with fgetc in while\n");
    FILE *file = fopen("data.txt", "r");
    while (!feof(file)) {
        char c = fgetc(file);
        if (c != EOF) {
        putchar(c);
        }
    }

    printf("Native: write data.txt\n");
	FILE *handle = fopen("data.txt", "wb");
	fputs("hello from file!", handle);
	fclose(handle);

    printf("Native: open and read data.txt chars with fgets\n");
	handle = fopen("data.txt", "r");
	char str[256] = {};
	fgets(str, 255, handle);
	printf("%s\n", str);
#ifdef REPORT_RESULT
	REPORT_RESULT(strcmp(str, "hello from file!"));
#endif
    fclose(handle);

    printf("Native: open and read data.txt chars with fgetc in while\n");
    file = fopen("data.txt", "r");
    while (!feof(file)) {
        char c = fgetc(file);
        if (c != EOF) {
        putchar(c);
        }
    }

    fclose (file);
    printf("\n");

    //main_sdl(argc, argv);
    //draw_cube_main1(argc, argv);

    drawing = true;
#ifdef __EMS__
    int fps = 0; // Use browser's requestAnimationFrame
    emscripten_set_main_loop_arg(draw_cube_main2, NULL, fps, false);
#else
    draw_cube_main2(NULL);
#endif
    //draw_triangle_main(argc, argv);

    return 0;
}
