#pragma once

#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "rendering/Camera3D.h"
#include "rendering/FrameBuffer.h"
#include "rendering/Mesh3D.h"
#include "rendering/Rasterizer3D.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

// renderScale: 1.0 = native window resolution. Prefer >= 1.0 when AA is on;
// values below 1.0 upscale and reintroduce stair-stepping that undoes coverage AA.
inline sf::Vector2u rasterSizeForWindow(sf::Vector2u windowSize, float renderScale = 1.0f) {
    renderScale = std::max(renderScale, 0.25f);

    return sf::Vector2u(
        std::max(1u, static_cast<unsigned int>(windowSize.x * renderScale + 0.5f)),
        std::max(1u, static_cast<unsigned int>(windowSize.y * renderScale + 0.5f))
    );
}

// Main framebuffer stays native. Full quality supersamples the ball pass only
// (see rasterizeMeshTrianglesSupersampled), not the whole window.
inline float rasterScaleForQuality(bool fullQuality, bool antiAliasingEnabled = false) {
    (void)fullQuality;
    (void)antiAliasingEnabled;
    return 1.0f;
}

inline float rasterScaleForAntiAliasing(bool antiAliasingEnabled) {
    (void)antiAliasingEnabled;
    return 1.0f;
}

inline float ballSuperSampleForQuality(bool fullQuality) {
    return fullQuality ? 2.0f : 1.0f;
}

// Directional light used for Gouraud shading when meshes have vertexNormals.
inline Vector3 defaultLightDirection() {
    return Vector3(-0.35f, 0.75f, -0.55f).normalized();
}

inline sf::Color shadeAlbedo(sf::Color albedo, const Vector3& worldNormal, const Vector3& lightDir) {
    float ndotl = std::max(0.0f, worldNormal.normalized().dot(lightDir));
    float shade = 0.52f + 0.48f * ndotl;

    return sf::Color(
        static_cast<std::uint8_t>(std::clamp(albedo.r * shade, 0.0f, 255.0f)),
        static_cast<std::uint8_t>(std::clamp(albedo.g * shade, 0.0f, 255.0f)),
        static_cast<std::uint8_t>(std::clamp(albedo.b * shade, 0.0f, 255.0f))
    );
}

struct RasterMeshRenderCache {
    std::vector<Vector3> worldVertices;
    std::vector<Vector3> cameraVertices;

    void reserveFor(const Mesh3D& mesh) {
        worldVertices.clear();
        cameraVertices.clear();

        worldVertices.reserve(mesh.vertices.size());
        cameraVertices.reserve(mesh.vertices.size());
    }
};

struct RasterRenderStats {
    int objectsSubmitted = 0;
    int objectsVisible = 0;
    int objectsCulled = 0;
    int trianglesSubmitted = 0;
    int trianglesDrawn = 0;
    int trianglesSkipped = 0;

    void reset() {
        objectsSubmitted = 0;
        objectsVisible = 0;
        objectsCulled = 0;
        trianglesSubmitted = 0;
        trianglesDrawn = 0;
        trianglesSkipped = 0;
    }
};

inline Vector3 projectCameraPointToScreen(
    const Vector3& cameraPoint,
    const Camera3D& camera,
    const FrameBuffer& frameBuffer
) {
    float scale = camera.fieldOfView / cameraPoint.z;

    return Vector3(
        frameBuffer.getWidth() * 0.5f + cameraPoint.x * scale,
        frameBuffer.getHeight() * 0.5f - cameraPoint.y * scale,
        cameraPoint.z
    );
}

inline Vector3 intersectNearPlane(
    const Vector3& a,
    const Vector3& b,
    float nearPlane
) {
    float t = (nearPlane - a.z) / (b.z - a.z);
    return a + (b - a) * t;
}

inline int clipTriangleAgainstNearPlane(
    const Vector3& a,
    const Vector3& b,
    const Vector3& c,
    float nearPlane,
    std::array<Vector3, 4>& clipped
) {
    std::array<Vector3, 3> input = {a, b, c};
    int outputCount = 0;

    for (int i = 0; i < 3; i++) {
        const Vector3& current = input[i];
        const Vector3& previous = input[(i + 2) % 3];
        bool currentInside = current.z >= nearPlane;
        bool previousInside = previous.z >= nearPlane;

        if (currentInside != previousInside) {
            clipped[outputCount] = intersectNearPlane(previous, current, nearPlane);
            outputCount++;
        }

        if (currentInside) {
            clipped[outputCount] = current;
            outputCount++;
        }
    }

    return outputCount;
}

inline Vector3 intersectScreenX(const Vector3& a, const Vector3& b, float x) {
    float t = (x - a.x) / (b.x - a.x);
    return a + (b - a) * t;
}

inline Vector3 intersectScreenY(const Vector3& a, const Vector3& b, float y) {
    float t = (y - a.y) / (b.y - a.y);
    return a + (b - a) * t;
}

template <typename IsInside, typename Intersect>
inline int clipScreenPolygonEdge(
    const std::array<Vector3, 12>& input,
    int inputCount,
    std::array<Vector3, 12>& output,
    IsInside isInside,
    Intersect intersect
) {
    if (inputCount == 0) {
        return 0;
    }

    int outputCount = 0;

    for (int i = 0; i < inputCount; i++) {
        const Vector3& current = input[i];
        const Vector3& previous = input[(i + inputCount - 1) % inputCount];
        bool currentInside = isInside(current);
        bool previousInside = isInside(previous);

        if (currentInside != previousInside) {
            output[outputCount] = intersect(previous, current);
            outputCount++;
        }

        if (currentInside) {
            output[outputCount] = current;
            outputCount++;
        }
    }

    return outputCount;
}

inline int clipScreenPolygonToFrameBuffer(
    const std::array<Vector3, 12>& input,
    int inputCount,
    const FrameBuffer& frameBuffer,
    std::array<Vector3, 12>& clipped
) {
    std::array<Vector3, 12> scratchA = input;
    std::array<Vector3, 12> scratchB;
    int count = inputCount;
    float maxX = static_cast<float>(frameBuffer.getWidth() - 1);
    float maxY = static_cast<float>(frameBuffer.getHeight() - 1);

    count = clipScreenPolygonEdge(
        scratchA,
        count,
        scratchB,
        [](const Vector3& point) { return point.x >= 0.0f; },
        [](const Vector3& a, const Vector3& b) { return intersectScreenX(a, b, 0.0f); }
    );

    count = clipScreenPolygonEdge(
        scratchB,
        count,
        scratchA,
        [maxX](const Vector3& point) { return point.x <= maxX; },
        [maxX](const Vector3& a, const Vector3& b) { return intersectScreenX(a, b, maxX); }
    );

    count = clipScreenPolygonEdge(
        scratchA,
        count,
        scratchB,
        [](const Vector3& point) { return point.y >= 0.0f; },
        [](const Vector3& a, const Vector3& b) { return intersectScreenY(a, b, 0.0f); }
    );

    count = clipScreenPolygonEdge(
        scratchB,
        count,
        clipped,
        [maxY](const Vector3& point) { return point.y <= maxY; },
        [maxY](const Vector3& a, const Vector3& b) { return intersectScreenY(a, b, maxY); }
    );

    return count;
}

inline void rasterizeMeshTriangles(
    FrameBuffer& frameBuffer,
    const Camera3D& camera,
    const Mesh3D& mesh,
    const Matrix4& transform,
    sf::Color fallbackColor,
    RasterMeshRenderCache& cache,
    bool cullBackFaces = true,
    bool cullObjects = true,
    RasterRenderStats* stats = nullptr
) {
    if (stats) {
        stats->objectsSubmitted++;
        stats->trianglesSubmitted += static_cast<int>(mesh.triangles.size());
    }

    BoundingSphere3D localSphere = mesh.localBoundingSphere();
    Vector3 worldCenter = transform.transformPoint(localSphere.center);
    float worldRadius = std::max({
        transform.transformDirection(Vector3(localSphere.radius, 0.0f, 0.0f)).magnitude(),
        transform.transformDirection(Vector3(0.0f, localSphere.radius, 0.0f)).magnitude(),
        transform.transformDirection(Vector3(0.0f, 0.0f, localSphere.radius)).magnitude()
    });

    if (
        cullObjects &&
        !camera.canSeeSphere(
            worldCenter,
            worldRadius,
            frameBuffer.getWidth(),
            frameBuffer.getHeight()
        )
    ) {
        if (stats) {
            stats->objectsCulled++;
            stats->trianglesSkipped += static_cast<int>(mesh.triangles.size());
        }
        return;
    }

    if (stats) {
        stats->objectsVisible++;
    }

    cache.reserveFor(mesh);

    for (const Vector3& vertex : mesh.vertices) {
        Vector3 world = transform.transformPoint(vertex);
        Vector3 cameraSpace = camera.worldToCameraPoint(world);

        cache.worldVertices.push_back(world);
        cache.cameraVertices.push_back(cameraSpace);
    }

    for (int i = 0; i < mesh.triangles.size(); i++) {
        const Triangle3D& triangle = mesh.triangles[i];

        Vector3 worldA = cache.worldVertices[triangle.a];

        if (cullBackFaces) {
            Vector3 normal;

            if (i < mesh.triangleNormals.size()) {
                normal = transform.transformDirection(mesh.triangleNormals[i]);
            } else {
                Vector3 worldB = cache.worldVertices[triangle.b];
                Vector3 worldC = cache.worldVertices[triangle.c];
                normal = (worldB - worldA).cross(worldC - worldA);
            }

            Vector3 cameraToTriangle = worldA - camera.position;

            if (normal.dot(cameraToTriangle) >= 0.0f) {
                if (stats) {
                    stats->trianglesSkipped++;
                }
                continue;
            }
        }

        std::array<Vector3, 4> clippedCameraPoints;
        int clippedCount = clipTriangleAgainstNearPlane(
            cache.cameraVertices[triangle.a],
            cache.cameraVertices[triangle.b],
            cache.cameraVertices[triangle.c],
            camera.nearPlane,
            clippedCameraPoints
        );

        if (clippedCount < 3) {
            if (stats) {
                stats->trianglesSkipped++;
            }
            continue;
        }

        sf::Color albedo = fallbackColor;
        if (i < mesh.triangleColors.size()) {
            albedo = mesh.triangleColors[i];
        }

        const bool gouraud =
            !mesh.vertexNormals.empty() &&
            mesh.vertexNormals.size() == mesh.vertices.size();

        sf::Color colorA = albedo;
        sf::Color colorB = albedo;
        sf::Color colorC = albedo;

        if (gouraud) {
            Vector3 light = defaultLightDirection();
            Vector3 normalA = transform.transformDirection(mesh.vertexNormals[triangle.a]).normalized();
            Vector3 normalB = transform.transformDirection(mesh.vertexNormals[triangle.b]).normalized();
            Vector3 normalC = transform.transformDirection(mesh.vertexNormals[triangle.c]).normalized();
            colorA = shadeAlbedo(albedo, normalA, light);
            colorB = shadeAlbedo(albedo, normalB, light);
            colorC = shadeAlbedo(albedo, normalC, light);
        }

        std::array<Vector3, 12> screenPolygon;
        for (int point = 0; point < clippedCount; point++) {
            screenPolygon[point] = projectCameraPointToScreen(
                clippedCameraPoints[point],
                camera,
                frameBuffer
            );
        }

        std::array<Vector3, 12> clippedScreenPolygon;
        int screenPointCount = clipScreenPolygonToFrameBuffer(
            screenPolygon,
            clippedCount,
            frameBuffer,
            clippedScreenPolygon
        );

        if (screenPointCount < 3) {
            if (stats) {
                stats->trianglesSkipped++;
            }
            continue;
        }

        for (int point = 1; point < screenPointCount - 1; point++) {
            // Fan triangles share vertex 0 color; good enough for dense spheres.
            Rasterizer3D::drawTriangle(
                frameBuffer,
                clippedScreenPolygon[0],
                clippedScreenPolygon[point],
                clippedScreenPolygon[point + 1],
                colorA,
                gouraud ? colorB : colorA,
                gouraud ? colorC : colorA
            );

            if (stats) {
                stats->trianglesDrawn++;
            }
        }
    }
}

// Supersample only the mesh's screen bounds into a small buffer, then downsample
// into the main framebuffer. Avoids 2x full-window clears for a single ball.
inline void rasterizeMeshTrianglesSupersampled(
    FrameBuffer& frameBuffer,
    const Camera3D& camera,
    const Mesh3D& mesh,
    const Matrix4& transform,
    sf::Color fallbackColor,
    RasterMeshRenderCache& cache,
    float superSample
) {
    if (superSample <= 1.001f) {
        rasterizeMeshTriangles(
            frameBuffer,
            camera,
            mesh,
            transform,
            fallbackColor,
            cache
        );
        return;
    }

    BoundingSphere3D localSphere = mesh.localBoundingSphere();
    Vector3 worldCenter = transform.transformPoint(localSphere.center);
    float worldRadius = std::max({
        transform.transformDirection(Vector3(localSphere.radius, 0.0f, 0.0f)).magnitude(),
        transform.transformDirection(Vector3(0.0f, localSphere.radius, 0.0f)).magnitude(),
        transform.transformDirection(Vector3(0.0f, 0.0f, localSphere.radius)).magnitude()
    });

    Vector3 cameraCenter = camera.worldToCameraPoint(worldCenter);
    if (cameraCenter.z <= camera.nearPlane) {
        return;
    }

    const float mainW = static_cast<float>(frameBuffer.getWidth());
    const float mainH = static_cast<float>(frameBuffer.getHeight());
    float scale = camera.fieldOfView / cameraCenter.z;
    float centerX = mainW * 0.5f + cameraCenter.x * scale;
    float centerY = mainH * 0.5f - cameraCenter.y * scale;
    float pixelRadius = worldRadius * scale + 6.0f;

    int x0 = std::max(0, static_cast<int>(std::floor(centerX - pixelRadius)));
    int y0 = std::max(0, static_cast<int>(std::floor(centerY - pixelRadius)));
    int x1 = std::min(frameBuffer.getWidth(), static_cast<int>(std::ceil(centerX + pixelRadius)));
    int y1 = std::min(frameBuffer.getHeight(), static_cast<int>(std::ceil(centerY + pixelRadius)));
    int destW = x1 - x0;
    int destH = y1 - y0;

    if (destW <= 0 || destH <= 0) {
        return;
    }

    int ssW = std::max(1, static_cast<int>(destW * superSample + 0.5f));
    int ssH = std::max(1, static_cast<int>(destH * superSample + 0.5f));
    FrameBuffer superBuffer(ssW, ssH);
    superBuffer.clear(sf::Color(0, 0, 0, 0));
    superBuffer.clearDepth(std::numeric_limits<float>::infinity());

    // Project as if drawing to the full main buffer, then shift/scale into the pad.
    Camera3D superCamera = camera;
    // Keep FOV in main-pixel units; map into superBuffer after.
    cache.reserveFor(mesh);

    for (const Vector3& vertex : mesh.vertices) {
        Vector3 world = transform.transformPoint(vertex);
        Vector3 cameraSpace = superCamera.worldToCameraPoint(world);
        cache.worldVertices.push_back(world);
        cache.cameraVertices.push_back(cameraSpace);
    }

    const bool gouraud =
        !mesh.vertexNormals.empty() &&
        mesh.vertexNormals.size() == mesh.vertices.size();
    Vector3 light = defaultLightDirection();

    for (int i = 0; i < mesh.triangles.size(); i++) {
        const Triangle3D& triangle = mesh.triangles[i];
        Vector3 worldA = cache.worldVertices[triangle.a];

        Vector3 normal;
        if (i < mesh.triangleNormals.size()) {
            normal = transform.transformDirection(mesh.triangleNormals[i]);
        } else {
            Vector3 worldB = cache.worldVertices[triangle.b];
            Vector3 worldC = cache.worldVertices[triangle.c];
            normal = (worldB - worldA).cross(worldC - worldA);
        }

        if (normal.dot(worldA - camera.position) >= 0.0f) {
            continue;
        }

        std::array<Vector3, 4> clippedCameraPoints;
        int clippedCount = clipTriangleAgainstNearPlane(
            cache.cameraVertices[triangle.a],
            cache.cameraVertices[triangle.b],
            cache.cameraVertices[triangle.c],
            camera.nearPlane,
            clippedCameraPoints
        );

        if (clippedCount < 3) {
            continue;
        }

        sf::Color albedo = fallbackColor;
        if (i < mesh.triangleColors.size()) {
            albedo = mesh.triangleColors[i];
        }

        sf::Color colorA = albedo;
        sf::Color colorB = albedo;
        sf::Color colorC = albedo;
        if (gouraud) {
            colorA = shadeAlbedo(
                albedo,
                transform.transformDirection(mesh.vertexNormals[triangle.a]),
                light
            );
            colorB = shadeAlbedo(
                albedo,
                transform.transformDirection(mesh.vertexNormals[triangle.b]),
                light
            );
            colorC = shadeAlbedo(
                albedo,
                transform.transformDirection(mesh.vertexNormals[triangle.c]),
                light
            );
        }

        std::array<Vector3, 12> screenPolygon;
        for (int point = 0; point < clippedCount; point++) {
            Vector3 mainScreen = projectCameraPointToScreen(
                clippedCameraPoints[point],
                camera,
                frameBuffer
            );
            screenPolygon[point] = Vector3(
                (mainScreen.x - static_cast<float>(x0)) * superSample,
                (mainScreen.y - static_cast<float>(y0)) * superSample,
                mainScreen.z
            );
        }

        std::array<Vector3, 12> clippedScreenPolygon;
        int screenPointCount = clipScreenPolygonToFrameBuffer(
            screenPolygon,
            clippedCount,
            superBuffer,
            clippedScreenPolygon
        );

        if (screenPointCount < 3) {
            continue;
        }

        for (int point = 1; point < screenPointCount - 1; point++) {
            Rasterizer3D::drawTriangle(
                superBuffer,
                clippedScreenPolygon[0],
                clippedScreenPolygon[point],
                clippedScreenPolygon[point + 1],
                colorA,
                gouraud ? colorB : colorA,
                gouraud ? colorC : colorA
            );
        }
    }

    superBuffer.blitDownsampleTo(frameBuffer, x0, y0, destW, destH);
}

inline void rasterizeMeshTriangles(
    FrameBuffer& frameBuffer,
    const Camera3D& camera,
    const Mesh3D& mesh,
    const Matrix4& transform,
    sf::Color fallbackColor,
    bool cullBackFaces = true
) {
    RasterMeshRenderCache cache;
    rasterizeMeshTriangles(
        frameBuffer,
        camera,
        mesh,
        transform,
        fallbackColor,
        cache,
        cullBackFaces
    );
}
