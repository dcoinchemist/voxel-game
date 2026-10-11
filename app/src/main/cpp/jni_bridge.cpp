#include <jni.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <chrono>
#include <cmath>

#define LOG_TAG "VoxelGame"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static const char* kVertexSrc = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aColor;
uniform float uAngle;
uniform float uAspect;
out vec3 vColor;
void main() {
    float c = cos(uAngle);
    float s = sin(uAngle);
    vec2 p = vec2(aPos.x * c - aPos.y * s, aPos.x * s + aPos.y * c);
    p.x /= uAspect;
    gl_Position = vec4(p, 0.0, 1.0);
    vColor = aColor;
}
)";

static const char* kFragmentSrc = R"(#version 300 es
precision mediump float;
in vec3 vColor;
out vec4 outColor;
void main() {
    outColor = vec4(vColor, 1.0);
}
)";

static GLuint gProgram = 0;
static GLuint gVao = 0;
static GLuint gVbo = 0;
static GLint gAngleLoc = -1;
static GLint gAspectLoc = -1;
static int gWidth = 1;
static int gHeight = 1;
static std::chrono::steady_clock::time_point gStart;

static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(shader, sizeof(buf), nullptr, buf);
        LOGE("Shader compile error: %s", buf);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint buildProgram() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentSrc);
    if (!vs || !fs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
        LOGE("Program link error: %s", buf);
        return 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxelgame_NativeBridge_nativeInit(JNIEnv*, jobject) {
    LOGI("nativeInit");
    gProgram = buildProgram();
    if (gProgram) {
        gAngleLoc = glGetUniformLocation(gProgram, "uAngle");
        gAspectLoc = glGetUniformLocation(gProgram, "uAspect");
    }

    // x, y, r, g, b
    const GLfloat verts[] = {
         0.0f,  0.6f, 1.0f, 0.2f, 0.2f,
        -0.5f, -0.4f, 0.2f, 1.0f, 0.2f,
         0.5f, -0.4f, 0.2f, 0.4f, 1.0f,
    };

    glGenVertexArrays(1, &gVao);
    glBindVertexArray(gVao);
    glGenBuffers(1, &gVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                          (void*)(2 * sizeof(GLfloat)));
    glBindVertexArray(0);

    gStart = std::chrono::steady_clock::now();
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxelgame_NativeBridge_nativeResize(JNIEnv*, jobject, jint width, jint height) {
    gWidth = width > 0 ? width : 1;
    gHeight = height > 0 ? height : 1;
    glViewport(0, 0, gWidth, gHeight);
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxelgame_NativeBridge_nativeFrame(JNIEnv*, jobject) {
    glClearColor(0.08f, 0.10f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!gProgram) return;

    float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - gStart).count();
    glUseProgram(gProgram);
    glUniform1f(gAngleLoc, t);
    glUniform1f(gAspectLoc, (float)gWidth / (float)gHeight);
    glBindVertexArray(gVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}
