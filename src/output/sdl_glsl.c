#define GL_GLEXT_PROTOTYPES 0
#ifdef _MSC_VER
#include <GL/glew.h>
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#endif
#include "cava/output/sdl_glsl.h"

#include <stdbool.h>
#include <stdlib.h>

#include "cava/util.h"

SDL_Window *glWindow = NULL;
GLuint shading_program;
GLint uniform_bars;
GLint uniform_bars_count;

SDL_GLContext *glContext = NULL;

struct colors {
    uint16_t R;
    uint16_t G;
    uint16_t B;
};

static void parse_color(char *color_string, struct colors *color) {
    if (color_string[0] == '#') {
        sscanf(++color_string, "%02hx%02hx%02hx", &color->R, &color->G, &color->B);
    }
}

GLuint get_shader(GLenum, const char *);

GLuint custom_shaders(const char *, const char *);

const char *read_file(const char *);

GLuint compile_shader(GLenum type, const char **);
GLuint program_check(GLuint);

void init_sdl_glsl_window(int width, int height, int x, int y, int full_screen,
                          char *const vertex_shader, char *const fragmnet_shader) {
    if (x == -1)
        x = SDL_WINDOWPOS_UNDEFINED;

    if (y == -1)
        y = SDL_WINDOWPOS_UNDEFINED;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }

#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

    Uint32 sdl_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

    if (full_screen == 1)
        sdl_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    glWindow = SDL_CreateWindow("cava", x, y, width, height, sdl_flags);
    if (glWindow == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_GLContext glContext = SDL_GL_CreateContext(glWindow);
    if (glContext == NULL) {
        fprintf(stderr, "GLContext could not be created! SDL Error: %s\n", SDL_GetError());
        exit(1);
    }

#ifdef _MSC_VER
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        printf(stderr, "Error initializing GLEW! %s\n", glewGetErrorString(glewError));
        exit(1);
    }
#endif

    // Use Vsync
    if (SDL_GL_SetSwapInterval(1) < 0) {
        printf("Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError());
    }

    shading_program = custom_shaders(vertex_shader, fragmnet_shader);
    glReleaseShaderCompiler();
    if (shading_program == 0) {
        fprintf(stderr, "could not compile shaders: %s\n", SDL_GetError());
        exit(1);
    }

    glUseProgram(shading_program);

    GLint gVertexPos2DLocation = -1;

    gVertexPos2DLocation = glGetAttribLocation(shading_program, "vertexPosition_modelspace");
    if (gVertexPos2DLocation == -1) {
        fprintf(stderr, "could not find vertex position shader variable!\n");
        exit(1);
    }

    glClearColor(0.f, 0.f, 0.f, 1.f);

    GLfloat vertexData[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

    GLuint indexData[] = {0, 1, 2, 3};

    GLuint gVBO = 0;
    glGenBuffers(1, &gVBO);
    glBindBuffer(GL_ARRAY_BUFFER, gVBO);
    glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(GLfloat), vertexData, GL_STATIC_DRAW);

    GLuint gIBO = 0;
    glGenBuffers(1, &gIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4 * sizeof(GLuint), indexData, GL_STATIC_DRAW);

    GLuint gVAO = 0;
    glGenVertexArrays(1, &gVAO);
    glBindVertexArray(gVAO);
    glEnableVertexAttribArray(gVertexPos2DLocation);

    glBindBuffer(GL_ARRAY_BUFFER, gVBO);
    glVertexAttribPointer(gVertexPos2DLocation, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIBO);

    uniform_bars = glGetUniformLocation(shading_program, "bars");
    uniform_bars_count = glGetUniformLocation(shading_program, "bars_count");

    int error = glGetError();
    if (error != 0) {
        fprintf(stderr, "glError on init: %d\n", error);
        exit(1);
    }
}

void init_sdl_glsl_surface(int *w, int *h, char *const fg_color_string, char *const bg_color_string,
                           int bar_width, int bar_spacing, int gradient, int gradient_count,
                           char **gradient_color_strings) {
    struct colors color = {0};

    GLint uniform_bg_col;
    uniform_bg_col = glGetUniformLocation(shading_program, "bg_color");
    parse_color(bg_color_string, &color);
    glUniform3f(uniform_bg_col, (float)color.R / 255.0, (float)color.G / 255.0,
                (float)color.B / 255.0);

    GLint uniform_fg_col;
    uniform_fg_col = glGetUniformLocation(shading_program, "fg_color");
    parse_color(fg_color_string, &color);
    glUniform3f(uniform_fg_col, (float)color.R / 255.0, (float)color.G / 255.0,
                (float)color.B / 255.0);

    GLint uniform_res;
    uniform_res = glGetUniformLocation(shading_program, "u_resolution");
    SDL_GetWindowSize(glWindow, w, h);
    glUniform3f(uniform_res, (float)*w, (float)*h, 0.0f);

    GLint uniform_bar_width;
    uniform_bar_width = glGetUniformLocation(shading_program, "bar_width");
    glUniform1i(uniform_bar_width, bar_width);

    GLint uniform_bar_spacing;
    uniform_bar_spacing = glGetUniformLocation(shading_program, "bar_spacing");
    glUniform1i(uniform_bar_spacing, bar_spacing);

    GLint uniform_gradient_count;
    uniform_gradient_count = glGetUniformLocation(shading_program, "gradient_count");
    if (gradient == 0)
        gradient_count = 0;
    glUniform1i(uniform_gradient_count, gradient_count);

    GLint uniform_gradient_colors;
    uniform_gradient_colors = glGetUniformLocation(shading_program, "gradient_colors");
    float gradient_colors[8][3];
    for (int i = 0; i < gradient_count; i++) {
        parse_color(gradient_color_strings[i], &color);
        gradient_colors[i][0] = (float)color.R / 255.0;
        gradient_colors[i][1] = (float)color.G / 255.0;
        gradient_colors[i][2] = (float)color.B / 255.0;
    }
    glUniform3fv(uniform_gradient_colors, 8, (const GLfloat *)gradient_colors);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, NULL);
    SDL_GL_SwapWindow(glWindow);
}

int draw_sdl_glsl(int bars_count, const float bars[], int frame_time, int re_paint,
                  int continuous_rendering) {

    int rc = 0;
    SDL_Event event;

    if (re_paint || continuous_rendering) {
        glUniform1fv(uniform_bars, bars_count, bars);
        glUniform1i(uniform_bars_count, bars_count);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, NULL);
        SDL_GL_SwapWindow(glWindow);
    }
    SDL_Delay(frame_time);

    SDL_PollEvent(&event);
    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        glViewport(0, 0, event.window.data1, event.window.data2);
        rc = -1;
    }
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_q || event.key.keysym.sym == SDLK_ESCAPE)
            rc = -2;
    }
    if (event.type == SDL_QUIT)
        rc = -2;

    return rc;
}

// general: cleanup
void cleanup_sdl_glsl(void) {
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(glWindow);
    SDL_Quit();
}

const char *read_file(const char *filename) {
    long length = 0;
    char *result = NULL;
    FILE *file = fopen(filename, "r");
    if (file) {
        int status = fseek(file, 0, SEEK_END);
        if (status != 0) {
            fclose(file);
            return NULL;
        }
        length = ftell(file);
        status = fseek(file, 0, SEEK_SET);
        if (status != 0) {
            fclose(file);
            return NULL;
        }
        result = malloc((length + 1) * sizeof(char));
        if (result) {
            size_t actual_length = fread(result, sizeof(char), length, file);
            result[actual_length++] = '\0';
        }
        fclose(file);
        return result;
    }
    fprintf(stderr, "Couldn't open shader file %s", filename);
    exit(1);
    return NULL;
}

GLuint custom_shaders(const char *vsPath, const char *fsPath) {
    GLuint vertexShader;
    GLuint fragmentShader;

    vertexShader = get_shader(GL_VERTEX_SHADER, vsPath);
    fragmentShader = get_shader(GL_FRAGMENT_SHADER, fsPath);

    shading_program = glCreateProgram();

    glAttachShader(shading_program, vertexShader);
    glAttachShader(shading_program, fragmentShader);

    glLinkProgram(shading_program);

    // Error Checking
    GLuint status;
    status = program_check(shading_program);
    if (status == GL_FALSE)
        return 0;
    return shading_program;
}

GLuint get_shader(GLenum eShaderType, const char *filename) {

    const char *shaderSource = read_file(filename);
    GLuint shader = compile_shader(eShaderType, &shaderSource);
    free((char *)shaderSource);
    return shader;
}

GLuint compile_shader(GLenum type, const char **sources) {

    GLuint shader;
    GLint success, len;
    GLsizei srclens[1];

    srclens[0] = (GLsizei)strlen(sources[0]);

    shader = glCreateShader(type);
    glShaderSource(shader, 1, sources, srclens);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            char *log;
            log = malloc(len);
            glGetShaderInfoLog(shader, len, NULL, log);
            fprintf(stderr, "%s\n\n", log);
            free(log);
        }
        fprintf(stderr, "Error compiling shader.\n");
        exit(1);
    }
    return shader;
}

GLuint program_check(GLuint program) {
    // Error Checking
    GLint status;
    glValidateProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        GLint len;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            char *log;
            log = malloc(len);
            glGetProgramInfoLog(program, len, &len, log);
            fprintf(stderr, "%s\n\n", log);
            free(log);
        }
        SDL_Log("Error linking shader default program.\n");
        return GL_FALSE;
    }
    return GL_TRUE;
}
