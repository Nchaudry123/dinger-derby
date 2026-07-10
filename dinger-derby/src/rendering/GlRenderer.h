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
    void updatePositionsNormals(const Mesh3D& mesh); // same topology, new verts/normals
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

// Minimal OpenGL 3.3-core style renderer (works as 2.1-compat GLSL 120 fallback).
// Draws lit meshes; SFML still owns the window and 2D UI overlay.
class GlRenderer {
public:
    GlRenderer() = default;
    ~GlRenderer();

    bool initialize(sf::RenderWindow& window);
    void shutdown();
    bool ready() const { return ready_; }

    // Call once per frame before 3D draws (clears color+depth, sets viewport).
    void beginFrame(sf::RenderWindow& window, const Camera3D& camera, sf::Color clearColor);
    void drawMesh(const GlMesh& mesh, const Matrix4& model, float alpha = 1.0f);
    // Simple ground strip under the field for depth context.
    void drawGround(float halfWidth, float zNear, float zFar, sf::Color color);
    // Restore state so SFML 2D drawing works afterward.
    void endFrame(sf::RenderWindow& window);

    static Matrix4 perspectiveFromCamera(const Camera3D& camera, float width, float height);
    static Matrix4 viewFromCamera(const Camera3D& camera);

private:
    bool ready_ = false;
    bool useCore_ = false;
    unsigned int program_ = 0;
    unsigned int groundVao_ = 0;
    unsigned int groundVbo_ = 0;
    int locMvp_ = -1;
    int locModel_ = -1;
    int locNormalMat_ = -1;
    int locLightDir_ = -1;
    int locViewPos_ = -1;
    int locAlpha_ = -1;

    Matrix4 view_{};
    Matrix4 proj_{};
    Vector3 viewPos_{};

    bool compileShaders();
    static unsigned int compileShader(unsigned int type, const char* source, std::string& log);
    static void setUniformMat4(int loc, const Matrix4& m);
};
