#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <string>
#include <vector>

#include "../math/Matrix4.h"
#include "../math/Vector3.h"
#include "Camera3D.h"
#include "Mesh3D.h"

// GPU mesh built from Mesh3D (expanded per-triangle vertex colors).
class GlMesh {
public:
    GlMesh() = default;
    ~GlMesh();

    GlMesh(const GlMesh&) = delete;
    GlMesh& operator=(const GlMesh&) = delete;
    GlMesh(GlMesh&& other) noexcept;
    GlMesh& operator=(GlMesh&& other) noexcept;

    void upload(const Mesh3D& mesh);
    void updatePositionsNormals(const Mesh3D& mesh);
    void clear();

    void draw() const;
    bool valid() const { return vao_ != 0 && indexCount_ > 0; }
    int indexCount() const { return indexCount_; }

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    int indexCount_ = 0;
    int vertexCount_ = 0;

    void destroy();
    static void packMesh(
        const Mesh3D& mesh,
        std::vector<float>& interleaved,
        std::vector<unsigned int>& indices
    );
};

// OpenGL lit mesh renderer.
//
// Draws into the window's default framebuffer, then restores SFML state so
// 2D UI can be drawn on top without clearing the 3D color buffer.
// Do NOT call window.clear() between endFrame() and the SFML overlay draws.
class GlRenderer {
public:
    GlRenderer() = default;
    ~GlRenderer();

    bool initialize(sf::RenderWindow& window);
    void shutdown();
    bool ready() const { return ready_; }

    // Activate context, clear color+depth, set camera uniforms.
    void beginFrame(sf::RenderWindow& window, const Camera3D& camera, sf::Color clearColor);
    void drawMesh(const GlMesh& mesh, const Matrix4& model, float alpha = 1.0f);
    void drawGround(float halfWidth, float zNear, float zFar, sf::Color color);
    // Soft elliptical contact shadow on the ground (y≈0). radius in world units.
    void drawGroundShadow(const Vector3& worldPos, float radius, float alpha = 0.35f);

    // Leave depth off and restore SFML GL states for 2D overlay drawing.
    // Preserves the color buffer (does not clear).
    void endFrame(sf::RenderWindow& window);

    static Matrix4 perspectiveFromCamera(const Camera3D& camera, float width, float height);
    static Matrix4 viewFromCamera(const Camera3D& camera);

private:
    bool ready_ = false;
    unsigned int program_ = 0;
    unsigned int groundVao_ = 0;
    unsigned int groundVbo_ = 0;
    unsigned int shadowVbo_ = 0;

    int locMvp_ = -1;
    int locModel_ = -1;
    int locNormalMat_ = -1;
    int locLightDir_ = -1;
    int locViewPos_ = -1;
    int locAlpha_ = -1;

    Matrix4 view_{};
    Matrix4 proj_{};
    Vector3 viewPos_{};
    int fbWidth_ = 0;
    int fbHeight_ = 0;

    bool compileShaders();
    static unsigned int compileShader(unsigned int type, const char* source, std::string& log);
    static void queryDrawableSize(sf::RenderWindow& window, int& outW, int& outH);
};
