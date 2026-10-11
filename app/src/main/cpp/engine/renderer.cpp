#include "engine/renderer.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "engine/math.h"
#include "engine/mesher.h"

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

GLuint gProgram = 0;
GLuint gTriEbo = 0;
GLuint gLineEbo = 0;
GLint gMvpLoc = -1;
GLint gWireLoc = -1;
int gWidth = 1;
int gHeight = 1;
std::atomic<bool> gWireframe{false};
std::chrono::steady_clock::time_point gStart;

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
    gStart = std::chrono::steady_clock::now();
}

void rendererResize(int width, int height) {
    gWidth = width > 0 ? width : 1;
    gHeight = height > 0 ? height : 1;
    glViewport(0, 0, gWidth, gHeight);
}

void rendererFrame() {
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

    float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - gStart).count();
    float angle = t * 0.2f;
    Vec3 center = {WORLD_SIZE * 0.5f, 8.0f, WORLD_SIZE * 0.5f};
    Vec3 eye = {center.x + std::cos(angle) * 55.0f, center.y + 28.0f,
                center.z + std::sin(angle) * 55.0f};
    float aspect = (float)gWidth / (float)gHeight;
    Mat4 proj = perspective(60.0f * 3.14159265f / 180.0f, aspect, 0.1f, 300.0f);
    Mat4 view = lookAt(eye, center, {0.0f, 1.0f, 0.0f});
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

std::string rendererStats() {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "Chunks: %d  Quads: %d (naive %lld)\nTris: %d  Mesh build: %.1f ms\nTap screen: %s",
             (int)gMeshes.size(), gTotalQuads, gNaiveFaces, gTotalQuads * 2, gMeshMs,
             gWireframe.load() ? "shaded view" : "wireframe");
    return std::string(buf);
}
