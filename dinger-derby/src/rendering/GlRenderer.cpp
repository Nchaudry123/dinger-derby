#include "GlRenderer.h"

#define GL_SILENCE_DEPRECATION
#if defined(__APPLE__)
// Compatibility / 2.1 headers — SFML graphics on macOS requires a legacy
// context, so the GPU path must not depend on Core-only VAOs.
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#include <SFML/OpenGL.hpp>
#endif

#include <cmath>
#include <iostream>
#include <utility>

namespace {

// GLSL 1.20 works on macOS OpenGL 2.1 (SFML-compatible legacy context) and
// on compatibility profiles elsewhere.
const char* kVertSrc = R"GLSL(
#version 120
attribute vec3 aPos;
attribute vec3 aNormal;
attribute vec3 aColor;
varying vec3 vWorldPos;
varying vec3 vNormal;
varying vec3 vColor;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMat;
void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = normalize(uNormalMat * aNormal);
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

const char* kFragSrc = R"GLSL(
#version 120
varying vec3 vWorldPos;
varying vec3 vNormal;
varying vec3 vColor;
uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform float uAlpha;
void main() {
    vec3 n = normalize(vNormal);
    float ndotl = abs(dot(n, normalize(uLightDir)));
    float ambient = 0.42;
    float diffuse = 0.58 * ndotl;
    vec3 viewDir = normalize(uViewPos - vWorldPos);
    vec3 halfV = normalize(normalize(uLightDir) + viewDir);
    float spec = pow(max(dot(n, halfV), 0.0), 28.0) * 0.15 * ndotl;
    vec3 lit = vColor * (ambient + diffuse) + vec3(spec);
    gl_FragColor = vec4(clamp(lit, 0.0, 1.0), uAlpha);
}
)GLSL";

void uploadMat4(int loc, const Matrix4& m) {
    if (loc < 0) {
        return;
    }
    float t[16];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            t[c * 4 + r] = m.values[r * 4 + c];
        }
    }
    glUniformMatrix4fv(loc, 1, GL_FALSE, t);
}

void uploadMat3(int loc, const float* colMajor9) {
    if (loc < 0) {
        return;
    }
    glUniformMatrix3fv(loc, 1, GL_FALSE, colMajor9);
}

void normalMatrixFromModel(const Matrix4& model, float outColMajor[9]) {
    float r0[3] = {model.values[0], model.values[1], model.values[2]};
    float r1[3] = {model.values[4], model.values[5], model.values[6]};
    float r2[3] = {model.values[8], model.values[9], model.values[10]};
    outColMajor[0] = r0[0]; outColMajor[1] = r1[0]; outColMajor[2] = r2[0];
    outColMajor[3] = r0[1]; outColMajor[4] = r1[1]; outColMajor[5] = r2[1];
    outColMajor[6] = r0[2]; outColMajor[7] = r1[2]; outColMajor[8] = r2[2];
}

void bindMeshAttribs(unsigned int vbo, unsigned int ebo) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    const GLsizei stride = 9 * static_cast<GLsizei>(sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
}

void unbindMeshAttribs() {
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace

// ── GlMesh ──────────────────────────────────────────────────────────────

GlMesh::GlMesh(GlMesh&& other) noexcept {
    *this = std::move(other);
}

GlMesh& GlMesh::operator=(GlMesh&& other) noexcept {
    if (this != &other) {
        destroy();
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        ebo_ = other.ebo_;
        indexCount_ = other.indexCount_;
        vertexCount_ = other.vertexCount_;
        other.vao_ = other.vbo_ = other.ebo_ = 0;
        other.indexCount_ = other.vertexCount_ = 0;
    }
    return *this;
}

GlMesh::~GlMesh() {
    destroy();
}

void GlMesh::destroy() {
    if (ebo_) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    // vao unused on the compatibility path (kept in the header for ABI stability).
    vao_ = 0;
    indexCount_ = 0;
    vertexCount_ = 0;
}

void GlMesh::clear() {
    destroy();
}

void GlMesh::packMesh(
    const Mesh3D& mesh,
    std::vector<float>& interleaved,
    std::vector<unsigned int>& indices
) {
    interleaved.clear();
    indices.clear();
    interleaved.reserve(mesh.triangles.size() * 3 * 9);
    indices.reserve(mesh.triangles.size() * 3);

    auto pushVert = [&](int vi, sf::Color color, const Vector3& n) {
        const Vector3& p = mesh.vertices[static_cast<size_t>(vi)];
        interleaved.push_back(p.x);
        interleaved.push_back(p.y);
        interleaved.push_back(p.z);
        interleaved.push_back(n.x);
        interleaved.push_back(n.y);
        interleaved.push_back(n.z);
        interleaved.push_back(color.r / 255.0f);
        interleaved.push_back(color.g / 255.0f);
        interleaved.push_back(color.b / 255.0f);
    };

    for (int t = 0; t < static_cast<int>(mesh.triangles.size()); t++) {
        const Triangle3D& tri = mesh.triangles[static_cast<size_t>(t)];
        sf::Color color = (t < static_cast<int>(mesh.triangleColors.size()))
            ? mesh.triangleColors[static_cast<size_t>(t)]
            : sf::Color(200, 200, 200);

        Vector3 nFlat(0.0f, 1.0f, 0.0f);
        if (t < static_cast<int>(mesh.triangleNormals.size())) {
            nFlat = mesh.triangleNormals[static_cast<size_t>(t)];
        } else if (
            tri.a >= 0 && tri.b >= 0 && tri.c >= 0 &&
            tri.a < static_cast<int>(mesh.vertices.size()) &&
            tri.b < static_cast<int>(mesh.vertices.size()) &&
            tri.c < static_cast<int>(mesh.vertices.size())
        ) {
            Vector3 e1 = mesh.vertices[static_cast<size_t>(tri.b)] - mesh.vertices[static_cast<size_t>(tri.a)];
            Vector3 e2 = mesh.vertices[static_cast<size_t>(tri.c)] - mesh.vertices[static_cast<size_t>(tri.a)];
            nFlat = e1.cross(e2);
            float nm = nFlat.magnitude();
            if (nm > 1e-8f) {
                nFlat = nFlat * (1.0f / nm);
            }
        }

        auto normalFor = [&](int vi) {
            if (vi >= 0 && vi < static_cast<int>(mesh.vertexNormals.size())) {
                Vector3 n = mesh.vertexNormals[static_cast<size_t>(vi)];
                float nm = n.magnitude();
                return nm > 1e-8f ? n * (1.0f / nm) : nFlat;
            }
            return nFlat;
        };

        unsigned int base = static_cast<unsigned int>(interleaved.size() / 9);
        pushVert(tri.a, color, normalFor(tri.a));
        pushVert(tri.b, color, normalFor(tri.b));
        pushVert(tri.c, color, normalFor(tri.c));
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
    }
}

void GlMesh::upload(const Mesh3D& mesh) {
    destroy();
    std::vector<float> data;
    std::vector<unsigned int> indices;
    packMesh(mesh, data, indices);
    if (indices.empty()) {
        return;
    }

    vertexCount_ = static_cast<int>(data.size() / 9);
    indexCount_ = static_cast<int>(indices.size());

    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    // Mark valid without a VAO (valid() checks vao_ || we change valid()).
    vao_ = 1; // non-zero sentinel: buffers exist

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(data.size() * sizeof(float)),
        data.data(),
        GL_DYNAMIC_DRAW
    );
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
        indices.data(),
        GL_STATIC_DRAW
    );
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GlMesh::updatePositionsNormals(const Mesh3D& mesh) {
    if (!valid()) {
        upload(mesh);
        return;
    }
    std::vector<float> data;
    std::vector<unsigned int> indices;
    packMesh(mesh, data, indices);
    if (static_cast<int>(indices.size()) != indexCount_) {
        upload(mesh);
        return;
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        static_cast<GLsizeiptr>(data.size() * sizeof(float)),
        data.data()
    );
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GlMesh::draw() const {
    if (!valid()) {
        return;
    }
    bindMeshAttribs(vbo_, ebo_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    unbindMeshAttribs();
}

// ── GlRenderer ──────────────────────────────────────────────────────────

GlRenderer::~GlRenderer() {
    shutdown();
}

unsigned int GlRenderer::compileShader(unsigned int type, const char* source, std::string& log) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, buf);
        log = buf;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool GlRenderer::compileShaders() {
    std::string log;
    unsigned int vs = compileShader(GL_VERTEX_SHADER, kVertSrc, log);
    if (!vs) {
        std::cerr << "GL vertex shader: " << log << std::endl;
        return false;
    }
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, kFragSrc, log);
    if (!fs) {
        std::cerr << "GL fragment shader: " << log << std::endl;
        glDeleteShader(vs);
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glBindAttribLocation(program_, 0, "aPos");
    glBindAttribLocation(program_, 1, "aNormal");
    glBindAttribLocation(program_, 2, "aColor");
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    int ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetProgramInfoLog(program_, 1024, nullptr, buf);
        std::cerr << "GL link: " << buf << std::endl;
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }

    locMvp_ = glGetUniformLocation(program_, "uMVP");
    locModel_ = glGetUniformLocation(program_, "uModel");
    locNormalMat_ = glGetUniformLocation(program_, "uNormalMat");
    locLightDir_ = glGetUniformLocation(program_, "uLightDir");
    locViewPos_ = glGetUniformLocation(program_, "uViewPos");
    locAlpha_ = glGetUniformLocation(program_, "uAlpha");
    return true;
}

void GlRenderer::queryDrawableSize(sf::RenderWindow& window, int& outW, int& outH) {
    sf::Vector2u size = window.getSize();
    outW = static_cast<int>(size.x);
    outH = static_cast<int>(size.y);
    if (outW < 1) {
        outW = 1;
    }
    if (outH < 1) {
        outH = 1;
    }
}

bool GlRenderer::initialize(sf::RenderWindow& window) {
    if (ready_) {
        return true;
    }
    if (!window.setActive(true)) {
        std::cerr << "GL: failed to activate window context" << std::endl;
        return false;
    }

    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::cerr << "OpenGL " << (version ? version : "?")
              << " | " << (renderer ? renderer : "?")
              << " | " << (vendor ? vendor : "?") << std::endl;

    if (!compileShaders()) {
        return false;
    }

    float ground[] = {
        -6.0f, 0.0f, -4.0f,  0,1,0,  0.07f,0.09f,0.12f,
         6.0f, 0.0f, -4.0f,  0,1,0,  0.07f,0.09f,0.12f,
         6.0f, 0.0f, 40.0f,  0,1,0,  0.09f,0.13f,0.09f,
        -6.0f, 0.0f, -4.0f,  0,1,0,  0.07f,0.09f,0.12f,
         6.0f, 0.0f, 40.0f,  0,1,0,  0.09f,0.13f,0.09f,
        -6.0f, 0.0f, 40.0f,  0,1,0,  0.09f,0.13f,0.09f,
    };
    glGenBuffers(1, &groundVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, groundVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ground), ground, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    groundVao_ = 1; // sentinel

    queryDrawableSize(window, fbWidth_, fbHeight_);
    ready_ = true;
    std::cerr << "GL: compatibility present path ready (" << fbWidth_ << "x" << fbHeight_ << ")"
              << std::endl;
    return true;
}

void GlRenderer::shutdown() {
    if (groundVbo_) {
        glDeleteBuffers(1, &groundVbo_);
        groundVbo_ = 0;
    }
    groundVao_ = 0;
    if (program_) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    ready_ = false;
}

Matrix4 GlRenderer::viewFromCamera(const Camera3D& camera) {
    // Software camera looks down +Z. OpenGL eye looks down -Z → flip Z.
    Matrix4 toCamera =
        Matrix4::rotationZ(-camera.rotation.z) *
        Matrix4::rotationX(-camera.rotation.x) *
        Matrix4::rotationY(-camera.rotation.y) *
        Matrix4::translation(Vector3(-camera.position.x, -camera.position.y, -camera.position.z));
    return Matrix4::scale(Vector3(1.0f, 1.0f, -1.0f)) * toCamera;
}

Matrix4 GlRenderer::perspectiveFromCamera(const Camera3D& camera, float width, float height) {
    float fovY = 2.0f * std::atan((height * 0.5f) / std::max(camera.fieldOfView, 1.0f));
    float aspect = width / std::max(height, 1.0f);
    float nearZ = std::max(camera.nearPlane, 0.05f);
    float farZ = 250.0f;
    float f = 1.0f / std::tan(fovY * 0.5f);

    Matrix4 p;
    for (int i = 0; i < 16; i++) {
        p.values[i] = 0.0f;
    }
    p.values[0] = f / aspect;
    p.values[5] = f;
    p.values[10] = (farZ + nearZ) / (nearZ - farZ);
    p.values[11] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    p.values[14] = -1.0f;
    p.values[15] = 0.0f;
    return p;
}

void GlRenderer::beginFrame(sf::RenderWindow& window, const Camera3D& camera, sf::Color clearColor) {
    if (!ready_) {
        return;
    }
    if (!window.setActive(true)) {
        return;
    }

    queryDrawableSize(window, fbWidth_, fbHeight_);
    glViewport(0, 0, fbWidth_, fbHeight_);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_ALPHA_TEST);

    glClearColor(clearColor.r / 255.0f, clearColor.g / 255.0f, clearColor.b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    sf::Vector2u logical = window.getSize();
    float projW = static_cast<float>(logical.x > 0 ? logical.x : fbWidth_);
    float projH = static_cast<float>(logical.y > 0 ? logical.y : fbHeight_);

    view_ = viewFromCamera(camera);
    proj_ = perspectiveFromCamera(camera, projW, projH);
    viewPos_ = camera.position;

    glUseProgram(program_);
    if (locLightDir_ >= 0) {
        Vector3 light = Vector3(-0.35f, 0.85f, -0.40f).normalized();
        glUniform3f(locLightDir_, light.x, light.y, light.z);
    }
    if (locViewPos_ >= 0) {
        glUniform3f(locViewPos_, viewPos_.x, viewPos_.y, viewPos_.z);
    }
}

void GlRenderer::drawMesh(const GlMesh& mesh, const Matrix4& model, float alpha) {
    if (!ready_ || !mesh.valid()) {
        return;
    }
    Matrix4 mvp = proj_ * view_ * model;
    uploadMat4(locMvp_, mvp);
    uploadMat4(locModel_, model);
    float nrm[9];
    normalMatrixFromModel(model, nrm);
    uploadMat3(locNormalMat_, nrm);
    if (locAlpha_ >= 0) {
        glUniform1f(locAlpha_, alpha);
    }
    mesh.draw();
}

void GlRenderer::drawGround(float halfWidth, float zNear, float zFar, sf::Color color) {
    if (!ready_ || !groundVbo_) {
        return;
    }
    (void)halfWidth;
    (void)zNear;
    (void)zFar;
    (void)color;
    Matrix4 model = Matrix4::identity();
    Matrix4 mvp = proj_ * view_ * model;
    uploadMat4(locMvp_, mvp);
    uploadMat4(locModel_, model);
    float nrm[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    uploadMat3(locNormalMat_, nrm);
    if (locAlpha_ >= 0) {
        glUniform1f(locAlpha_, 1.0f);
    }

    glBindBuffer(GL_ARRAY_BUFFER, groundVbo_);
    const GLsizei stride = 9 * static_cast<GLsizei>(sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    unbindMeshAttribs();
}

void GlRenderer::endFrame(sf::RenderWindow& window) {
    if (!ready_) {
        return;
    }
    glUseProgram(0);
    unbindMeshAttribs();

    // Preserve color (3D scene); clear depth so SFML 2D always passes.
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    (void)window.setActive(true);
    window.resetGLStates();
    window.setView(window.getDefaultView());
}
