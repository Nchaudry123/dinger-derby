#include "Stadium3D.h"

#include <algorithm>
#include <cmath>

namespace Stadium3D {
namespace {

constexpr float pi = 3.1415926535f;

void addTri(Mesh3D& m, const Vector3& a, const Vector3& b, const Vector3& c, sf::Color col) {
    int i = static_cast<int>(m.vertices.size());
    m.vertices.push_back(a);
    m.vertices.push_back(b);
    m.vertices.push_back(c);
    m.triangles.push_back({i, i + 1, i + 2});
    m.triangleColors.push_back(col);
}

void addQuad(
    Mesh3D& m,
    const Vector3& a,
    const Vector3& b,
    const Vector3& c,
    const Vector3& d,
    sf::Color col
) {
    addTri(m, a, b, c, col);
    addTri(m, a, c, d, col);
}

void addBox(Mesh3D& m, const Vector3& center, float w, float h, float d, sf::Color col) {
    float hw = w * 0.5f;
    float hh = h * 0.5f;
    float hd = d * 0.5f;
    Vector3 p[8] = {
        center + Vector3(-hw, -hh, -hd),
        center + Vector3(hw, -hh, -hd),
        center + Vector3(hw, -hh, hd),
        center + Vector3(-hw, -hh, hd),
        center + Vector3(-hw, hh, -hd),
        center + Vector3(hw, hh, -hd),
        center + Vector3(hw, hh, hd),
        center + Vector3(-hw, hh, hd),
    };
    addQuad(m, p[0], p[1], p[2], p[3], col);
    addQuad(m, p[4], p[7], p[6], p[5], col);
    addQuad(m, p[0], p[4], p[5], p[1], col);
    addQuad(m, p[1], p[5], p[6], p[2], col);
    addQuad(m, p[2], p[6], p[7], p[3], col);
    addQuad(m, p[3], p[7], p[4], p[0], col);
}

void addAnnulusSector(
    Mesh3D& m,
    const Layout& L,
    float r0,
    float r1,
    float a0,
    float a1,
    float y,
    int segs,
    sf::Color col
) {
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = a0 + (a1 - a0) * t0;
        float ang1 = a0 + (a1 - a0) * t1;
        Vector3 a = L.fromHome(r0, ang0, y);
        Vector3 b = L.fromHome(r0, ang1, y);
        Vector3 c = L.fromHome(r1, ang1, y);
        Vector3 d = L.fromHome(r1, ang0, y);
        addQuad(m, a, b, c, d, col);
    }
}

void addArcWall(
    Mesh3D& m,
    const Layout& L,
    float r,
    float a0,
    float a1,
    float y0,
    float y1,
    int segs,
    sf::Color face,
    sf::Color top
) {
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = a0 + (a1 - a0) * t0;
        float ang1 = a0 + (a1 - a0) * t1;
        Vector3 b0 = L.fromHome(r, ang0, y0);
        Vector3 b1 = L.fromHome(r, ang1, y0);
        Vector3 tA = L.fromHome(r, ang0, y1);
        Vector3 tB = L.fromHome(r, ang1, y1);
        addQuad(m, b0, b1, tB, tA, face);
        addQuad(m, b1, b0, tA, tB, face);
        Vector3 outer0 = L.fromHome(r + 1.8f, ang0, y1);
        Vector3 outer1 = L.fromHome(r + 1.8f, ang1, y1);
        addQuad(m, tA, tB, outer1, outer0, top);
        addQuad(m, tB, tA, outer0, outer1, top);
    }
}

// Fence with distance + height varying by spray angle (asymmetric park).
void addAsymmetricOutfieldWall(
    Mesh3D& m,
    const Layout& L,
    float a0,
    float a1,
    int segs,
    sf::Color face,
    sf::Color top
) {
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = a0 + (a1 - a0) * t0;
        float ang1 = a0 + (a1 - a0) * t1;
        float r0 = L.wallRAtAngle(ang0);
        float r1 = L.wallRAtAngle(ang1);
        float h0 = L.wallHeightAtAngle(ang0);
        float h1 = L.wallHeightAtAngle(ang1);
        Vector3 b0 = L.fromHome(r0, ang0, 0.0f);
        Vector3 b1 = L.fromHome(r1, ang1, 0.0f);
        Vector3 tA = L.fromHome(r0, ang0, h0);
        Vector3 tB = L.fromHome(r1, ang1, h1);
        addQuad(m, b0, b1, tB, tA, face);
        addQuad(m, b1, b0, tA, tB, face);
        Vector3 outer0 = L.fromHome(r0 + 1.8f, ang0, h0);
        Vector3 outer1 = L.fromHome(r1 + 1.8f, ang1, h1);
        addQuad(m, tA, tB, outer1, outer0, top);
        addQuad(m, tB, tA, outer0, outer1, top);
    }
}

void addStands(
    Mesh3D& m,
    const Layout& L,
    float rInner,
    float a0,
    float a1,
    float wallH,
    int rows,
    int segs,
    float rowDepth,
    float rowRise,
    sf::Color seat,
    sf::Color riser
) {
    for (int row = 0; row < rows; row++) {
        float r0 = rInner + row * rowDepth;
        float r1 = r0 + rowDepth;
        float y0 = wallH + 0.4f + row * rowRise;
        float y1 = y0 + rowRise;
        for (int i = 0; i < segs; i++) {
            float t0 = static_cast<float>(i) / segs;
            float t1 = static_cast<float>(i + 1) / segs;
            float ang0 = a0 + (a1 - a0) * t0;
            float ang1 = a0 + (a1 - a0) * t1;
            addQuad(
                m,
                L.fromHome(r0, ang0, y1),
                L.fromHome(r0, ang1, y1),
                L.fromHome(r1, ang1, y1),
                L.fromHome(r1, ang0, y1),
                seat
            );
            addQuad(
                m,
                L.fromHome(r0, ang0, y0),
                L.fromHome(r0, ang1, y0),
                L.fromHome(r0, ang1, y1),
                L.fromHome(r0, ang0, y1),
                riser
            );
        }
    }
}

Mesh3D buildField(const Layout& L) {
    Mesh3D m;
    const float aL = -L.foulAngleRad();
    const float aR = L.foulAngleRad();
    const float wallR = L.maxWallR();
    const float plateZ = L.plateZ();
    const int segs = 56;

    sf::Color grass(34, 110, 48);
    sf::Color grassDark(28, 92, 40);
    // Grass filled to local fence radius (asymmetric OF).
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = aL + (aR - aL) * t0;
        float ang1 = aL + (aR - aL) * t1;
        float rFence0 = L.wallRAtAngle(ang0) - 0.4f;
        float rFence1 = L.wallRAtAngle(ang1) - 0.4f;
        Vector3 o = L.home() + Vector3(0, 0.0f, 0);
        Vector3 a = L.fromHome(0.5f, ang0, 0.0f);
        Vector3 b = L.fromHome(0.5f, ang1, 0.0f);
        Vector3 c = L.fromHome(rFence1, ang1, 0.0f);
        Vector3 d = L.fromHome(rFence0, ang0, 0.0f);
        (void)o;
        addQuad(m, a, b, c, d, grass);
        // Darker band mid-OF
        float mid0 = rFence0 * 0.55f;
        float mid1 = rFence1 * 0.55f;
        float mid0b = rFence0 * 0.68f;
        float mid1b = rFence1 * 0.68f;
        addQuad(
            m,
            L.fromHome(mid0, ang0, 0.01f),
            L.fromHome(mid1, ang1, 0.01f),
            L.fromHome(mid1b, ang1, 0.01f),
            L.fromHome(mid0b, ang0, 0.01f),
            grassDark
        );
        // Warning track
        sf::Color track(150, 120, 70);
        addQuad(
            m,
            L.fromHome(rFence0 - 3.5f, ang0, 0.02f),
            L.fromHome(rFence1 - 3.5f, ang1, 0.02f),
            L.fromHome(rFence1, ang1, 0.02f),
            L.fromHome(rFence0, ang0, 0.02f),
            track
        );
    }
    (void)wallR;

    sf::Color dirt(168, 120, 70);
    sf::Color dirtDark(140, 98, 55);
    addAnnulusSector(m, L, 0.0f, L.infieldR(), aL * 1.15f, aR * 1.15f, 0.03f, 36, dirt);

    // Mound at origin
    {
        Vector3 c(0.0f, 0.12f, L.moundZ());
        const int n = 20;
        float mr = 4.2f;
        for (int i = 0; i < n; i++) {
            float a0 = (static_cast<float>(i) / n) * pi * 2.0f;
            float a1 = (static_cast<float>(i + 1) / n) * pi * 2.0f;
            Vector3 p0 = c + Vector3(std::cos(a0) * mr, -0.08f, std::sin(a0) * mr * 0.55f);
            Vector3 p1 = c + Vector3(std::cos(a1) * mr, -0.08f, std::sin(a1) * mr * 0.55f);
            addTri(m, c, p0, p1, dirtDark);
        }
        addBox(m, Vector3(0.0f, 0.18f, L.moundZ()), 1.0f, 0.06f, 0.3f, sf::Color(230, 225, 210));
    }

    // Home plate at plateZ — tip toward catcher (+Z)
    {
        float z = plateZ;
        Vector3 tip(0.0f, 0.06f, z + 0.55f);
        Vector3 bl(-0.55f, 0.06f, z - 0.15f);
        Vector3 br(0.55f, 0.06f, z - 0.15f);
        Vector3 fl(-0.35f, 0.06f, z - 0.55f);
        Vector3 fr(0.35f, 0.06f, z - 0.55f);
        sf::Color plate(240, 235, 220);
        addTri(m, tip, br, bl, plate);
        addQuad(m, bl, br, fr, fl, plate);
    }

    // Bases (approx diamond)
    float bp = L.basePath();
    auto base = [&](float x, float z) {
        addBox(m, Vector3(x, 0.08f, z), 1.1f, 0.08f, 1.1f, sf::Color(245, 240, 225));
    };
    // 1B +X, 3B -X slightly toward mound; 2B toward CF
    base(bp * 0.85f, plateZ - bp * 0.35f);
    base(0.0f, plateZ - bp * 0.95f);
    base(-bp * 0.85f, plateZ - bp * 0.35f);

    // Batter's boxes
    addBox(m, Vector3(-1.5f, 0.04f, plateZ - 0.2f), 1.4f, 0.04f, 2.2f, dirtDark);
    addBox(m, Vector3(1.5f, 0.04f, plateZ - 0.2f), 1.4f, 0.04f, 2.2f, dirtDark);

    // Dugouts along baselines near home
    addBox(m, Vector3(-18.0f, 1.0f, plateZ - 12.0f), 14.0f, 2.0f, 4.0f, sf::Color(50, 60, 70));
    addBox(m, Vector3(18.0f, 1.0f, plateZ - 12.0f), 14.0f, 2.0f, 4.0f, sf::Color(50, 60, 70));

    m.rebuildNormals();
    return m;
}

Mesh3D buildWalls(const Layout& L) {
    Mesh3D m;
    const float aL = -L.foulAngleRad();
    const float aR = L.foulAngleRad();
    const int segs = 64;

    // Main fence — Citizens Bank–style distances (329 LF … 404 deep … 330 RF).
    sf::Color wall(55, 70, 95);
    sf::Color wallTop(70, 88, 115);
    sf::Color pad(40, 55, 75);
    addAsymmetricOutfieldWall(m, L, aL, aR, segs, wall, wallTop);
    // Inner pad follows same shape, slightly inboard.
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = aL + (aR - aL) * t0;
        float ang1 = aL + (aR - aL) * t1;
        float r0 = L.wallRAtAngle(ang0) - 0.4f;
        float r1 = L.wallRAtAngle(ang1) - 0.4f;
        float h0 = L.wallHeightAtAngle(ang0) * 0.55f;
        float h1 = L.wallHeightAtAngle(ang1) * 0.55f;
        Vector3 b0 = L.fromHome(r0, ang0, 0.0f);
        Vector3 b1 = L.fromHome(r1, ang1, 0.0f);
        Vector3 tA = L.fromHome(r0, ang0, h0);
        Vector3 tB = L.fromHome(r1, ang1, h1);
        addQuad(m, b0, b1, tB, tA, pad);
        addQuad(m, b1, b0, tA, tB, pad);
    }

    sf::Color pole(240, 230, 80);
    float hL = L.wallHeightAtAngle(aL);
    float hR = L.wallHeightAtAngle(aR);
    Vector3 left = L.wallPoint(aL, 0.0f);
    Vector3 right = L.wallPoint(aR, 0.0f);
    addBox(m, left + Vector3(0, hL * 1.8f, 0), 0.55f, hL * 3.4f, 0.55f, pole);
    addBox(m, right + Vector3(0, hR * 1.8f, 0), 0.55f, hR * 3.4f, 0.55f, pole);
    addBox(
        m,
        left + Vector3(0, hL * 3.0f, 0),
        0.12f,
        hL * 2.2f,
        2.5f,
        sf::Color(200, 200, 190, 180)
    );
    addBox(
        m,
        right + Vector3(0, hR * 3.0f, 0),
        0.12f,
        hR * 2.2f,
        2.5f,
        sf::Color(200, 200, 190, 180)
    );

    // Distance markers on wall (readable from field)
    auto mark = [&](float angDeg, const char* /*label*/) {
        float ang = angDeg * pi / 180.0f;
        float r = L.wallRAtAngle(ang);
        float h = L.wallHeightAtAngle(ang);
        Vector3 c = L.fromHome(r - 0.6f, ang, h * 0.55f);
        addBox(m, c, 2.4f, 1.6f, 0.35f, sf::Color(250, 250, 245));
    };
    mark(-45.0f, "329");
    mark(0.0f, "401");
    mark(45.0f, "330");

    // CF scoreboard beyond deepest CF
    float cfR = L.wallRAtAngle(0.0f);
    float cfH = L.wallHeightAtAngle(0.0f);
    Vector3 cf = L.fromHome(cfR + 8.0f, 0.0f, cfH + 10.0f);
    addBox(m, cf, 32.0f, 16.0f, 3.0f, sf::Color(30, 35, 45));
    addBox(m, cf + Vector3(0, 0, 1.6f), 28.0f, 12.0f, 0.4f, sf::Color(20, 90, 50));

    m.rebuildNormals();
    return m;
}

Mesh3D buildStands(const Layout& L) {
    Mesh3D m;
    const float aL = -L.foulAngleRad() - 0.12f;
    const float aR = L.foulAngleRad() + 0.12f;
    const float wallR = L.maxWallR();
    const float wallH = L.wallH() + 2.0f;
    const float plateZ = L.plateZ();
    sf::Color seat(70, 95, 140);
    sf::Color riser(50, 68, 100);
    sf::Color upper(90, 70, 75);

    // Outfield bowl (uses max fence as inner radius so stands clear the deepest wall)
    addStands(m, L, wallR + 2.5f, aL, aR, wallH, 12, 40, 3.2f, 1.15f, seat, riser);
    addStands(
        m,
        L,
        wallR + 2.5f + 12 * 3.2f,
        aL * 0.85f,
        aR * 0.85f,
        wallH,
        6,
        32,
        3.6f,
        1.35f,
        upper,
        sf::Color(60, 48, 52)
    );

    // Behind home plate: backstop + grandstand (angle π is +Z behind catcher).
    const float backL = pi - 1.05f;
    const float backR = pi + 1.05f;
    float backInner = 14.0f; // just behind catcher
    // Backstop wall (padded fence)
    addArcWall(
        m,
        L,
        backInner,
        backL,
        backR,
        0.0f,
        8.5f,
        28,
        sf::Color(90, 100, 110),
        sf::Color(110, 120, 130)
    );
    // Netting plane (thin higher wall)
    addArcWall(
        m,
        L,
        backInner + 0.4f,
        backL + 0.05f,
        backR - 0.05f,
        4.0f,
        16.0f,
        24,
        sf::Color(180, 190, 200, 90),
        sf::Color(160, 170, 180, 100)
    );
    // Home-plate stands rising behind backstop
    addStands(m, L, backInner + 2.0f, backL, backR, 6.0f, 10, 28, 2.8f, 1.1f, seat, riser);
    addStands(
        m,
        L,
        backInner + 2.0f + 10 * 2.8f,
        backL + 0.08f,
        backR - 0.08f,
        6.0f,
        5,
        22,
        3.2f,
        1.25f,
        upper,
        sf::Color(60, 48, 52)
    );

    // Press boxes / camera decks behind home
    addBox(
        m,
        Vector3(0.0f, 22.0f, plateZ + 28.0f),
        22.0f,
        6.0f,
        8.0f,
        sf::Color(55, 65, 80)
    );
    addBox(
        m,
        Vector3(0.0f, 26.5f, plateZ + 28.0f),
        18.0f,
        2.5f,
        6.0f,
        sf::Color(40, 50, 60)
    );

    sf::Color steel(180, 185, 190);
    auto tower = [&](float ang) {
        Vector3 base = L.fromHome(wallR + 18.0f, ang, 0.0f);
        addBox(m, base + Vector3(0, 22.0f, 0), 1.2f, 44.0f, 1.2f, steel);
        addBox(m, base + Vector3(0, 44.0f, 0), 8.0f, 2.0f, 3.0f, sf::Color(240, 240, 220));
    };
    tower(-0.55f);
    tower(0.0f);
    tower(0.55f);
    // Lights behind home too
    tower(pi - 0.35f);
    tower(pi + 0.35f);

    m.rebuildNormals();
    return m;
}

// Deterministic pseudo-random in [0,1) from integer seed.
float hash01(int n) {
    unsigned x = static_cast<unsigned>(n) * 1664525u + 1013904223u;
    return static_cast<float>(x & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

Mesh3D buildCity(const Layout& L) {
    Mesh3D m;
    const float plateZ = L.plateZ();
    const float baseR = L.maxWallR();
    // Suburban ground ring around the park
    sf::Color grassFar(42, 95, 48);
    sf::Color road(55, 55, 58);
    sf::Color sidewalk(120, 118, 110);
    sf::Color roof(90, 55, 45);
    sf::Color brick(140, 95, 75);
    sf::Color stucco(190, 175, 150);
    sf::Color glass(90, 130, 160);
    sf::Color tree(30, 90, 40);
    sf::Color trunk(70, 50, 30);

    // Large flat suburb ground (disk via annulus full circle)
    addAnnulusSector(m, L, baseR + 5.0f, baseR + 220.0f, -pi, pi, -0.02f, 64, grassFar);
    // Outer ring road
    addAnnulusSector(
        m, L, baseR + 55.0f, baseR + 62.0f, -pi, pi, 0.01f, 64, road
    );
    addAnnulusSector(
        m, L, baseR + 120.0f, baseR + 128.0f, -pi, pi, 0.01f, 64, road
    );

    // Buildings full 360° so every camera sees a skyline.
    const int buildingCount = 96;
    for (int i = 0; i < buildingCount; i++) {
        float t = static_cast<float>(i) / buildingCount;
        float ang = -pi + t * (2.0f * pi);
        float r = baseR + 70.0f + hash01(i * 3) * 90.0f;
        // Prefer denser / taller behind home (+Z, ang near π) and in OF corners.
        float behindHome = std::max(0.0f, -std::cos(ang)); // 1 at behind home
        float h = 6.0f + hash01(i * 7) * 14.0f + behindHome * 18.0f;
        float w = 4.0f + hash01(i * 11) * 8.0f;
        float d = 4.0f + hash01(i * 13) * 8.0f;
        Vector3 c = L.fromHome(r, ang, h * 0.5f);
        sf::Color body = (hash01(i) > 0.45f) ? brick : stucco;
        if (behindHome > 0.55f && hash01(i + 50) > 0.55f) {
            body = glass; // a few office blocks behind home
            h += 8.0f;
            c.y = h * 0.5f;
        }
        addBox(m, c, w, h, d, body);
        // Roof
        addBox(m, c + Vector3(0, h * 0.5f + 0.35f, 0), w * 1.05f, 0.7f, d * 1.05f, roof);

        // Occasional tree clusters near houses
        if (hash01(i + 99) > 0.55f) {
            Vector3 tbase = L.fromHome(r - 5.0f - hash01(i) * 4.0f, ang + 0.02f, 0.0f);
            addBox(m, tbase + Vector3(0, 1.2f, 0), 0.5f, 2.4f, 0.5f, trunk);
            addBox(m, tbase + Vector3(0, 3.6f, 0), 3.2f, 2.8f, 3.2f, tree);
        }
    }

    // Water tower / water tank landmark behind RF-ish
    {
        Vector3 wt = L.fromHome(baseR + 95.0f, 0.75f, 18.0f);
        addBox(m, wt + Vector3(0, -6.0f, 0), 1.2f, 12.0f, 1.2f, sf::Color(100, 100, 105));
        addBox(m, wt + Vector3(0, 2.0f, 0), 6.0f, 5.0f, 6.0f, sf::Color(130, 140, 150));
    }

    // Parking lots near home-plate gates
    addBox(
        m,
        Vector3(0.0f, 0.05f, plateZ + 55.0f),
        50.0f,
        0.1f,
        30.0f,
        road
    );
    addBox(
        m,
        Vector3(0.0f, 0.08f, plateZ + 55.0f),
        52.0f,
        0.05f,
        2.0f,
        sidewalk
    );

    // Distant hills (large low boxes) for horizon
    for (int i = 0; i < 12; i++) {
        float ang = -pi + (static_cast<float>(i) / 12.0f) * 2.0f * pi;
        float r = baseR + 200.0f;
        float h = 8.0f + hash01(i + 200) * 14.0f;
        Vector3 c = L.fromHome(r, ang, h * 0.45f);
        addBox(m, c, 40.0f, h, 18.0f, sf::Color(50, 85, 55));
    }

    m.rebuildNormals();
    return m;
}

Mesh3D buildLines(const Layout& L) {
    Mesh3D m;
    sf::Color chalk(245, 245, 235);
    auto line = [&](float ang) {
        float len = L.wallRAtAngle(ang) + 1.0f;
        Vector3 a = L.home() + Vector3(0.0f, 0.07f, 0.0f);
        Vector3 b = L.fromHome(len, ang, 0.07f);
        Vector3 dir = b - a;
        float dlen = dir.magnitude();
        if (dlen < 1e-4f) {
            return;
        }
        dir = dir * (1.0f / dlen);
        Vector3 side(-dir.z, 0, dir.x);
        side = side * 0.18f;
        addQuad(m, a + side, a - side, b - side, b + side, chalk);
    };
    line(-L.foulAngleRad());
    line(L.foulAngleRad());
    m.rebuildNormals();
    return m;
}

} // namespace

float Layout::foulAngleRad() const {
    return foulAngleDegrees * (pi / 180.0f);
}

// Citizens Bank Park–inspired OF distances (feet) vs angle from CF (deg).
// Samples from the reference diagram: LF 329 · 374 · 387 · 404 deep · CF ~401 · 398 · 369 · RF 330.
float Layout::wallFeetAtAngle(float angleRad) const {
    // Clamp to foul territory angles.
    float aDeg = angleRad * (180.0f / pi);
    aDeg = std::clamp(aDeg, -foulAngleDegrees, foulAngleDegrees);

    // (angleDeg, feet) — sorted by angle LF → RF
    static const float samples[][2] = {
        {-45.0f, 329.0f},
        {-35.0f, 345.0f},
        {-25.0f, 360.0f},
        {-18.0f, 374.0f},
        {-10.0f, 387.0f},
        {-4.0f, 404.0f}, // Monty's Angle (deepest)
        {0.0f, 401.0f},  // CF
        {6.0f, 398.0f},
        {18.0f, 369.0f},
        {30.0f, 350.0f},
        {45.0f, 330.0f},
    };
    constexpr int n = 11;
    if (aDeg <= samples[0][0]) {
        return samples[0][1];
    }
    if (aDeg >= samples[n - 1][0]) {
        return samples[n - 1][1];
    }
    for (int i = 0; i < n - 1; i++) {
        if (aDeg >= samples[i][0] && aDeg <= samples[i + 1][0]) {
            float u = (aDeg - samples[i][0]) / (samples[i + 1][0] - samples[i][0]);
            return samples[i][1] + (samples[i + 1][1] - samples[i][1]) * u;
        }
    }
    return wallDistanceFeet;
}

float Layout::wallHeightAtAngle(float angleRad) const {
    // LF wall taller (classic short-porch high wall); RF lower.
    float aDeg = angleRad * (180.0f / pi);
    float t = (aDeg + 45.0f) / 90.0f; // 0 at LF, 1 at RF
    t = std::clamp(t, 0.0f, 1.0f);
    float feet = 13.0f + (1.0f - t) * 8.0f; // ~21 ft LF → ~13 ft RF
    return feet / feetPerUnit;
}

Vector3 Layout::fromHome(float radius, float angleRad, float y) const {
    // angle 0 = CF = −Z from home; +angle toward +X (1B / RF).
    return Vector3(
        std::sin(angleRad) * radius,
        y,
        plateZ() - std::cos(angleRad) * radius
    );
}

Vector3 Layout::wallPoint(float angleRad, float yFraction) const {
    float r = wallRAtAngle(angleRad);
    float h = wallHeightAtAngle(angleRad) * std::clamp(yFraction, 0.0f, 1.0f);
    return fromHome(r, angleRad, h);
}

float Layout::maxWallR() const {
    float mx = 0.0f;
    for (int i = 0; i <= 32; i++) {
        float a = -foulAngleRad() + (2.0f * foulAngleRad()) * (static_cast<float>(i) / 32.0f);
        mx = std::max(mx, wallRAtAngle(a));
    }
    return mx;
}

Vector3 Layout::parkCenter() const {
    return Vector3(0.0f, 4.0f, plateZ() - maxWallR() * 0.42f);
}

Layout defaultPlayLayout() {
    Layout L;
    L.wallDistanceFeet = 401.0f;
    return L;
}

Meshes build(const Layout& layout) {
    Meshes out;
    out.field = buildField(layout);
    out.walls = buildWalls(layout);
    out.stands = buildStands(layout);
    out.lines = buildLines(layout);
    out.city = buildCity(layout);
    return out;
}

float recommendedFarPlane(const Layout& layout) {
    return std::max(3500.0f, layout.maxWallR() * 12.0f + 800.0f);
}

} // namespace Stadium3D
