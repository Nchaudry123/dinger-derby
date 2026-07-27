// Screenshot Capture — headless promo-shot renderer.
//
// Renders the Crown Jewel ballpark (and the procedural characters) through the
// engine's own software rasterizer into a FrameBuffer, then saves PNGs — no
// window or GPU needed, so it runs in CI / build agents:
//
//   docs/screenshots/stadium_overview.png   full-park aerial (bowl + canopy)
//   docs/screenshots/stadium_broadcast.png  high-home broadcast view to CF
//   docs/screenshots/plate_duel.png         pitcher windup vs batter stance
//   docs/screenshots/homer_celebration.png  batter celebrating a no-doubter
//
// Run from the project root:  ./build/screenshot_capture

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "RasterDemo3D.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "rendering/BaseballAnims.h"
#include "rendering/Camera3D.h"
#include "rendering/CharacterModel3D.h"
#include "rendering/FrameBuffer.h"
#include "rendering/Mesh3D.h"
#include "rendering/SkeletonAnimator.h"
#include "rendering/SkinnedModel3D.h"
#include "rendering/Stadium3D.h"

namespace {

constexpr float kPi = 3.14159265f;
constexpr int kWidth = 1600;
constexpr int kHeight = 900;

void lookAt(Camera3D& cam, const Vector3& pos, const Vector3& target) {
    cam.position = pos;
    Vector3 to = target - pos;
    float h = std::sqrt(to.x * to.x + to.z * to.z);
    cam.rotation.y = std::atan2(to.x, to.z);
    cam.rotation.x = -std::atan2(to.y, h);
    cam.rotation.z = 0.0f;
}

void configureCamera(Camera3D& cam, float fov) {
    cam.nearPlane = 0.5f;
    cam.farPlane = Stadium3D::recommendedFarPlane();
    cam.fieldOfView = fov;
}

void drawMesh(
    FrameBuffer& fb,
    const Camera3D& cam,
    const Mesh3D& mesh,
    const Matrix4& xform,
    sf::Color fallback,
    RasterMeshRenderCache& cache,
    bool cullBackFaces = true
) {
    rasterizeMeshTriangles(fb, cam, mesh, xform, fallback, cache, cullBackFaces);
}

// Whole park: sky shell, field, bowl, city, fans + flags mid-cheer.
void renderStadium(
    FrameBuffer& fb,
    const Camera3D& cam,
    const Stadium3D::Meshes& meshes,
    float cheerTime,
    float cheerBoost = 1.15f
) {
    RasterMeshRenderCache cache;
    fb.clear(Stadium3D::skyColor());
    fb.clearDepth(std::numeric_limits<float>::infinity());

    // Sky shell is viewed from inside — disable backface culling.
    drawMesh(fb, cam, meshes.skyDome, Matrix4::identity(), Stadium3D::skyColor(), cache, false);
    drawMesh(fb, cam, meshes.city, Matrix4::identity(), sf::Color(80, 110, 80), cache);
    drawMesh(fb, cam, meshes.field, Matrix4::identity(), Stadium3D::grassColor(), cache);
    drawMesh(fb, cam, meshes.walls, Matrix4::identity(), Stadium3D::ofWallColor(), cache);
    drawMesh(fb, cam, meshes.stands, Matrix4::identity(), Stadium3D::seatBlueColor(), cache);
    drawMesh(fb, cam, meshes.hotel, Matrix4::identity(), Stadium3D::facadeTanColor(), cache);
    drawMesh(fb, cam, meshes.structure, Matrix4::identity(), sf::Color(120, 125, 130), cache);
    drawMesh(fb, cam, meshes.lines, Matrix4::identity(), sf::Color(245, 245, 240), cache);
    drawMesh(
        fb, cam, meshes.scoreboardScreen, Matrix4::identity(),
        sf::Color(20, 30, 60), cache, false
    );

    for (int i = 0; i < static_cast<int>(meshes.fanSectors.size()); i++) {
        float bob = Stadium3D::fanCheerOffsetY(i, cheerTime, cheerBoost);
        float sway = Stadium3D::fanCheerOffsetX(i, cheerTime, cheerBoost);
        drawMesh(
            fb, cam, meshes.fanSectors[i],
            Matrix4::translation(Vector3(sway, bob, 0.0f)),
            sf::Color(150, 60, 60), cache
        );
    }
    for (int i = 0; i < static_cast<int>(meshes.flagMeshes.size()); i++) {
        if (i >= static_cast<int>(meshes.flagBases.size())) {
            break;
        }
        float yaw = Stadium3D::flagSwayYaw(i, cheerTime);
        drawMesh(
            fb, cam, meshes.flagMeshes[i],
            Matrix4::translation(meshes.flagBases[i]) * Matrix4::rotationY(yaw),
            Stadium3D::foulPoleColor(), cache, false
        );
    }
}

bool saveShot(const FrameBuffer& fb, const std::string& path) {
    const auto& pixels = fb.getPixels();
    sf::Image image(
        sf::Vector2u(
            static_cast<unsigned>(fb.getWidth()),
            static_cast<unsigned>(fb.getHeight())
        ),
        reinterpret_cast<const std::uint8_t*>(pixels.data())
    );
    if (!image.saveToFile(path)) {
        std::cerr << "FAILED to save " << path << std::endl;
        return false;
    }
    std::cout << "Saved " << path << std::endl;
    return true;
}

// Simple tapered wooden bat along +Y (knob at origin), for the stance shot.
Mesh3D makeBatMesh(float length = 1.35f) {
    Mesh3D mesh;
    const int rings = 8;
    const int segs = 10;
    auto radiusAt = [&](float t) {
        if (t < 0.04f) return 0.055f;          // knob
        if (t < 0.45f) return 0.032f;          // handle
        float u = (t - 0.45f) / 0.55f;
        return 0.032f + u * 0.05f;             // barrel taper
    };
    for (int r = 0; r <= rings; r++) {
        float t = static_cast<float>(r) / rings;
        float y = t * length;
        float rad = radiusAt(t);
        for (int s = 0; s < segs; s++) {
            float a = 2.0f * kPi * static_cast<float>(s) / segs;
            mesh.vertices.emplace_back(rad * std::cos(a), y, rad * std::sin(a));
        }
    }
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segs; s++) {
            int a = r * segs + s;
            int b = r * segs + (s + 1) % segs;
            int c = (r + 1) * segs + s;
            int d = (r + 1) * segs + (s + 1) % segs;
            mesh.triangles.push_back({a, b, c});
            mesh.triangles.push_back({b, d, c});
            sf::Color col = (static_cast<float>(r) / rings > 0.4f)
                ? sf::Color(210, 150, 70)
                : sf::Color(175, 120, 60);
            mesh.triangleColors.push_back(col);
            mesh.triangleColors.push_back(col);
        }
    }
    mesh.rebuildNormals();
    return mesh;
}

// Local +Y → axis basis, translated so the knob sits in the hands.
Matrix4 batModelMatrix(const Vector3& hands, const Vector3& axis) {
    Vector3 y = axis.normalized();
    Vector3 x = Vector3(0.0f, 1.0f, 0.0f).cross(y);
    if (x.magnitude() < 0.2f) {
        x = Vector3(1.0f, 0.0f, 0.0f).cross(y);
    }
    x = x.normalized();
    Vector3 z = x.cross(y).normalized();
    x = y.cross(z).normalized();

    Matrix4 r = Matrix4::identity();
    r.values[0] = x.x; r.values[4] = x.y; r.values[8]  = x.z;
    r.values[1] = y.x; r.values[5] = y.y; r.values[9]  = y.z;
    r.values[2] = z.x; r.values[6] = z.y; r.values[10] = z.z;
    return Matrix4::translation(hands) * r;
}

bool shotOverview(const Stadium3D::Layout& L, const Stadium3D::Meshes& meshes) {
    FrameBuffer fb(kWidth, kHeight);
    Camera3D cam;
    configureCamera(cam, 640.0f);
    lookAt(cam, Vector3(0.0f, 112.0f, L.plateZ() + 78.0f), L.parkCenter());
    renderStadium(fb, cam, meshes, 1.3f);
    return saveShot(fb, "docs/screenshots/stadium_overview.png");
}

bool shotBroadcast(const Stadium3D::Layout& L, const Stadium3D::Meshes& meshes) {
    FrameBuffer fb(kWidth, kHeight);
    Camera3D cam;
    configureCamera(cam, 760.0f);
    lookAt(
        cam,
        Vector3(0.0f, 8.5f, L.plateZ() + 14.0f),
        Vector3(0.0f, 1.5f, L.plateZ() - L.maxWallR() * 0.42f)
    );
    renderStadium(fb, cam, meshes, 2.1f);
    return saveShot(fb, "docs/screenshots/stadium_broadcast.png");
}

bool shotPlateDuel(const Stadium3D::Layout& L, const Stadium3D::Meshes& meshes) {
    const float plateZ = L.plateZ();

    SkinnedModel3D pitcherModel =
        CharacterModel3D::build(CharacterModel3D::Role::Pitcher, CharacterModel3D::Detail::High);
    SkinnedModel3D batterModel =
        CharacterModel3D::build(CharacterModel3D::Role::Athlete, CharacterModel3D::Detail::High);

    AnimationClip windupClip = BaseballAnims::yamamotoWindup(pitcherModel);
    AnimationClip stanceClip = BaseballAnims::batterStance(batterModel);

    SkeletonAnimator pitcherAnim;
    pitcherAnim.setModel(pitcherModel);
    pitcherAnim.applyClipNormalized(windupClip, 0.42f); // peak leg lift
    Mesh3D pitcherMesh = pitcherModel.skinToMesh(pitcherAnim.skinMatrices());

    SkeletonAnimator batterAnim;
    batterAnim.setModel(batterModel);
    batterAnim.applyClip(stanceClip, 0.4f, true);
    Mesh3D batterMesh = batterModel.skinToMesh(batterAnim.skinMatrices());

    Matrix4 pitcherXform = Matrix4::translation(Vector3(0.0f, 0.0f, 0.0f));
    Matrix4 batterXform =
        Matrix4::translation(Vector3(-1.05f, 0.0f, plateZ - 0.28f)) *
        Matrix4::rotationY(kPi); // face the mound (−Z)

    // Bat in the stance grip: knob at the palms, barrel up over the rear shoulder.
    Mesh3D batMesh = makeBatMesh();
    Vector3 palmL = batterAnim.jointWorldPosition("Palm_L");
    Vector3 palmR = batterAnim.jointWorldPosition("Palm_R");
    Vector3 handsLocal = (palmL + palmR) * 0.5f;
    Vector3 hands = batterXform.transformPoint(handsLocal);
    Vector3 batAxis = Vector3(0.15f, 1.0f, 0.45f).normalized();
    Matrix4 batXform = batModelMatrix(hands, batAxis);

    FrameBuffer fb(kWidth, kHeight);
    Camera3D cam;
    configureCamera(cam, 430.0f);
    // Low 1B-side angle: batter + raised bat hero-framed, pitcher mid-windup
    // on the mound behind him, skyline over the OF wall. Catcher stays out
    // of frame (behind the camera) to keep the duel readable.
    lookAt(
        cam,
        Vector3(1.9f, 1.85f, plateZ + 2.3f),
        Vector3(-0.7f, 1.3f, plateZ - 3.5f)
    );

    renderStadium(fb, cam, meshes, 0.8f);
    RasterMeshRenderCache cache;
    drawMesh(fb, cam, pitcherMesh, pitcherXform, sf::Color(230, 230, 235), cache);
    drawMesh(fb, cam, batterMesh, batterXform, sf::Color(220, 200, 180), cache);
    drawMesh(fb, cam, batMesh, batXform, sf::Color(210, 150, 70), cache);

    return saveShot(fb, "docs/screenshots/plate_duel.png");
}

bool shotCelebration(const Stadium3D::Layout& L, const Stadium3D::Meshes& meshes) {
    const float plateZ = L.plateZ();

    SkinnedModel3D batterModel =
        CharacterModel3D::build(CharacterModel3D::Role::Athlete, CharacterModel3D::Detail::High);
    AnimationClip celebrateClip = BaseballAnims::batterCelebrate(batterModel);

    SkeletonAnimator batterAnim;
    batterAnim.setModel(batterModel);
    batterAnim.applyClipNormalized(celebrateClip, 0.45f); // arms up, mid-roar
    Mesh3D batterMesh = batterModel.skinToMesh(batterAnim.skinMatrices());

    // He just watched it leave: near the plate, turned toward the mound / CF.
    Matrix4 batterXform =
        Matrix4::translation(Vector3(-0.6f, 0.0f, plateZ - 0.9f)) *
        Matrix4::rotationY(kPi);

    // Tossed bat lying on the dirt beside the box.
    Mesh3D batMesh = makeBatMesh();
    Matrix4 batXform = batModelMatrix(
        Vector3(0.7f, 0.08f, plateZ + 0.4f),
        Vector3(0.9f, 0.05f, 0.35f).normalized()
    );

    FrameBuffer fb(kWidth, kHeight);
    Camera3D cam;
    configureCamera(cam, 400.0f);
    // Broadcast "no-doubter" frame: celebrating batter low-center, crown
    // videoboard and CF skyline over his shoulder, crowd at full roar.
    lookAt(
        cam,
        Vector3(2.0f, 1.7f, plateZ + 2.7f),
        Vector3(-0.7f, 1.9f, plateZ - 22.0f)
    );

    renderStadium(fb, cam, meshes, 2.6f, 1.9f); // crowd at max roar
    RasterMeshRenderCache cache;
    drawMesh(fb, cam, batterMesh, batterXform, sf::Color(220, 200, 180), cache);
    drawMesh(fb, cam, batMesh, batXform, sf::Color(210, 150, 70), cache);

    return saveShot(fb, "docs/screenshots/homer_celebration.png");
}

} // namespace

int main() {
    std::filesystem::create_directories("docs/screenshots");

    Stadium3D::Layout layout = Stadium3D::defaultPlayLayout();
    std::cout << "Building Crown Jewel meshes..." << std::endl;
    Stadium3D::Meshes meshes = Stadium3D::build(layout);
    std::cout << "Stadium tris: field=" << meshes.field.triangles.size()
              << " stands=" << meshes.stands.triangles.size()
              << " city=" << meshes.city.triangles.size()
              << " walls=" << meshes.walls.triangles.size() << std::endl;

    bool ok = true;
    ok &= shotOverview(layout, meshes);
    ok &= shotBroadcast(layout, meshes);
    ok &= shotPlateDuel(layout, meshes);
    ok &= shotCelebration(layout, meshes);

    return ok ? 0 : 1;
}
