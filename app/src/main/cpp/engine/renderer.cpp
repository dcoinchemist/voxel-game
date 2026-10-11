#include "engine/renderer.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <vector>

#include "engine/math.h"
#include "engine/mesher.h"
#include "engine/player.h"

#define LOG_TAG "VoxelGame"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr int MAX_QUADS_PER_CHUNK = 32768;

const char* kVertexSrc = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMvp;
out vec3 vColor;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

const char* kFragmentSrc = R"(#version 300 es
precision mediump float;
in vec3 vColor;
uniform float uWire;
out vec4 outColor;
void main() {
    outColor = vec4(mix(vColor, vec3(1.0), uWire), 1.0);
}
)";

struct ChunkGpu {
    GLuint vao = 0;
    GLuint vbo = 0;
    int quads = 0;
};

World gWorld;
std::vector<ChunkMesh> gMeshes;
std::vector<ChunkGpu> gGpu;
bool gWorldBuilt = false;
double gMeshMs = 0.0;
int gTotalQuads = 0;
long long gNaiveFaces = 0;

Player gPlayer;
bool gPlayerSpawned = false;

GLuint gProgram = 0;
GLuint gTriEbo = 0;
GLuint gLineEbo = 0;
GLint gMvpLoc = -1;
GLint gWireLoc = -1;
int gWidth = 1;
int gHeight = 1;
std::atomic<bool> gWireframe{false};
std::chrono::steady_clock::time_point gLastFrame;

// Touch input arrives on the UI thread and is consumed on the GL thread.
std::mutex gInputMutex;
float gMoveX = 0.0f;
float gMoveY = 0.0f;
bool gJump = false;
float gLookYaw = 0.0f;
float gLookPitch = 0.0f;

GLuint compileShader(GLenum type, const char* src) {
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

GLuint buildProgram() {
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

void buildWorldOnce() {
    if (gWorldBuilt) return;
    auto t0 = std::chrono::steady_clock::now();

    gWorld.generate();
    gMeshes.clear();
    gTotalQuads = 0;
    gNaiveFaces = 0;
    for (int cz = 0; cz < WORLD_CHUNKS; ++cz) {
        for (int cx = 0; cx < WORLD_CHUNKS; ++cx) {
            ChunkMesh mesh = buildChunkMesh(gWorld, cx, cz);
            if (mesh.quadCount > MAX_QUADS_PER_CHUNK) {
                LOGE("Chunk %d,%d has too many quads (%d), truncating", cx, cz, mesh.quadCount);
                mesh.quadCount = MAX_QUADS_PER_CHUNK;
                mesh.vertices.resize((size_t)MAX_QUADS_PER_CHUNK * 4 * 6);
            }
            gTotalQuads += mesh.quadCount;
            gNaiveFaces += (long long)mesh.solidBlocks * 6;
            gMeshes.push_back(std::move(mesh));
        }
    }

    gMeshMs = std::chrono::duration<double, std::milli>(
                  std::chrono::steady_clock::now() - t0).count();
    gWorldBuilt = true;
    LOGI("World built: %d quads (naive %lld) in %.1f ms", gTotalQuads, gNaiveFaces, gMeshMs);
}

// One index buffer shared by every chunk: triangles for normal drawing,
// line segments for the wireframe view.
void createIndexBuffers() {
    std::vector<uint32_t> tri;
    std::vector<uint32_t> lines;
    tri.reserve((size_t)MAX_QUADS_PER_CHUNK * 6);
    lines.reserve((size_t)MAX_QUADS_PER_CHUNK * 8);
    for (int q = 0; q < MAX_QUADS_PER_CHUNK; ++q) {
        uint32_t b = (uint32_t)q * 4;
        tri.push_back(b);     tri.push_back(b + 1); tri.push_back(b + 2);
        tri.push_back(b);     tri.push_back(b + 2); tri.push_back(b + 3);
        lines.push_back(b);     lines.push_back(b + 1);
        lines.push_back(b + 1); lines.push_back(b + 2);
        lines.push_back(b + 2); lines.push_back(b + 3);
        lines.push_back(b + 3); lines.push_back(b);
    }
    glGenBuffers(1, &gTriEbo);
    glBindBuffer(GL_COPY_WRITE_BUFFER, gTriEbo);
    glBufferData(GL_COPY_WRITE_BUFFER, tri.size() * sizeof(uint32_t), tri.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &gLineEbo);
    glBindBuffer(GL_COPY_WRITE_BUFFER, gLineEbo);
    glBufferData(GL_COPY_WRITE_BUFFER, lines.size() * sizeof(uint32_t), lines.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

}  // namespace

void rendererInit() {
    LOGI("rendererInit");
    buildWorldOnce();

    if (!gPlayerSpawned) {
        gPlayer.spawn(gWorld, 32.5f, 20.5f);
        gPlayerSpawned = true;
    }

    // Objects from a previous GL context are already gone; just forget them.
    gGpu.clear();
    gProgram = 0;
    gTriEbo = 0;
    gLineEbo = 0;

    gProgram = buildProgram();
    if (!gProgram) return;
    gMvpLoc = glGetUniformLocation(gProgram, "uMvp");
    gWireLoc = glGetUniformLocation(gProgram, "uWire");

    createIndexBuffers();

    const GLsizei stride = 6 * sizeof(GLfloat);
    for (const ChunkMesh& m : gMeshes) {
        ChunkGpu g;
        g.quads = m.quadCount;
        if (m.quadCount > 0) {
            glGenVertexArrays(1, &g.vao);
            glBindVertexArray(g.vao);
            glGenBuffers(1, &g.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
            glBufferData(GL_ARRAY_BUFFER, m.vertices.size() * sizeof(float),
                         m.vertices.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                                  (void*)(3 * sizeof(GLfloat)));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gTriEbo);
            glBindVertexArray(0);
        }
        gGpu.push_back(g);
    }
    gLastFrame = std::chrono::steady_clock::now();
}

void rendererResize(int width, int height) {
    gWidth = width > 0 ? width : 1;
    gHeight = height > 0 ? height : 1;
    glViewport(0, 0, gWidth, gHeight);
}

void rendererFrame() {
    // Time since the previous frame.
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - gLastFrame).count();
    gLastFrame = now;

    // Take this frame's input.
    PlayerInput input;
    float lookYaw = 0.0f;
    float lookPitch = 0.0f;
    {
        std::lock_guard<std::mutex> lock(gInputMutex);
        input.moveX = gMoveX;
        input.moveY = gMoveY;
        input.jump = gJump;
        lookYaw = gLookYaw;
        lookPitch = gLookPitch;
        gLookYaw = 0.0f;
        gLookPitch = 0.0f;
    }
    gPlayer.look(lookYaw, lookPitch);
    gPlayer.update(gWorld, dt, input);

    bool wire = gWireframe.load();
    if (wire) {
        glClearColor(0.04f, 0.05f, 0.08f, 1.0f);
    } else {
        glClearColor(0.53f, 0.72f, 0.95f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!gProgram) return;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    Vec3 eye = gPlayer.eyePosition();
    Vec3 dir = gPlayer.forwardDir();
    float aspect = (float)gWidth / (float)gHeight;
    Mat4 proj = perspective(70.0f * 3.14159265f / 180.0f, aspect, 0.05f, 300.0f);
    Mat4 view = lookAt(eye, eye + dir, {0.0f, 1.0f, 0.0f});
    Mat4 mvp = mul(proj, view);

    glUseProgram(gProgram);
    glUniformMatrix4fv(gMvpLoc, 1, GL_FALSE, mvp.m);
    glUniform1f(gWireLoc, wire ? 1.0f : 0.0f);

    for (const ChunkGpu& g : gGpu) {
        if (g.quads == 0) continue;
        glBindVertexArray(g.vao);
        if (wire) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gLineEbo);
            glDrawElements(GL_LINES, g.quads * 8, GL_UNSIGNED_INT, (void*)0);
        } else {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gTriEbo);
            glDrawElements(GL_TRIANGLES, g.quads * 6, GL_UNSIGNED_INT, (void*)0);
        }
    }
    glBindVertexArray(0);
}

void rendererToggleWireframe() {
    gWireframe.store(!gWireframe.load());
}

void rendererSetMove(float x, float y) {
    std::lock_guard<std::mutex> lock(gInputMutex);
    gMoveX = x;
    gMoveY = y;
}

void rendererLook(float dYawDeg, float dPitchDeg) {
    std::lock_guard<std::mutex> lock(gInputMutex);
    gLookYaw += dYawDeg;
    gLookPitch += dPitchDeg;
}

void rendererSetJump(bool pressed) {
    std::lock_guard<std::mutex> lock(gInputMutex);
    gJump = pressed;
}

std::string rendererStats() {
    char buf[320];
    snprintf(buf, sizeof(buf),
             "Chunks: %d  Quads: %d (naive %lld)\n"
             "Tris: %d  Mesh build: %.1f ms\n"
             "Pos: %.1f %.1f %.1f  Ground: %s\n"
             "Wireframe: %s",
             (int)gMeshes.size(), gTotalQuads, gNaiveFaces, gTotalQuads * 2, gMeshMs,
             gPlayer.pos.x, gPlayer.pos.y, gPlayer.pos.z, gPlayer.onGround ? "yes" : "no",
             gWireframe.load() ? "on" : "off");
    return std::string(buf);
}
