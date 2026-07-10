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
    const float wallR = L.wallR();
    const float plateZ = L.plateZ();
    const int segs = 48;

    sf::Color grass(34, 110, 48);
    sf::Color grassDark(28, 92, 40);
    addAnnulusSector(m, L, 0.5f, wallR - 4.0f, aL, aR, 0.0f, segs, grass);

    for (int band = 0; band < 6; band++) {
        float r0 = 20.0f + band * 28.0f;
        float r1 = r0 + 10.0f;
        if (r1 > wallR - 4.0f) {
            break;
        }
        addAnnulusSector(m, L, r0, r1, aL, aR, 0.01f, segs, grassDark);
    }

    sf::Color track(150, 120, 70);
    addAnnulusSector(m, L, wallR - 4.0f, wallR - 0.3f, aL, aR, 0.02f, segs, track);

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
    const float wallR = L.wallR();
    const float wallH = L.wallH();
    const int segs = 56;

    sf::Color wall(55, 70, 95);
    sf::Color wallTop(70, 88, 115);
    sf::Color pad(40, 55, 75);
    addArcWall(m, L, wallR, aL, aR, 0.0f, wallH, segs, wall, wallTop);
    addArcWall(m, L, wallR - 0.35f, aL, aR, 0.0f, wallH * 0.55f, segs, pad, pad);

    sf::Color pole(240, 230, 80);
    Vector3 left = L.fromHome(wallR, aL, 0.0f);
    Vector3 right = L.fromHome(wallR, aR, 0.0f);
    addBox(m, left + Vector3(0, wallH * 1.6f, 0), 0.55f, wallH * 3.2f, 0.55f, pole);
    addBox(m, right + Vector3(0, wallH * 1.6f, 0), 0.55f, wallH * 3.2f, 0.55f, pole);
    addBox(
        m,
        left + Vector3(0, wallH * 2.8f, 0),
        0.12f,
        wallH * 2.0f,
        2.5f,
        sf::Color(200, 200, 190, 180)
    );
    addBox(
        m,
        right + Vector3(0, wallH * 2.8f, 0),
        0.12f,
        wallH * 2.0f,
        2.5f,
        sf::Color(200, 200, 190, 180)
    );

    // CF scoreboard beyond wall
    Vector3 cf = L.fromHome(wallR + 6.0f, 0.0f, wallH + 8.0f);
    addBox(m, cf, 28.0f, 14.0f, 2.5f, sf::Color(30, 35, 45));
    // Screen faces home (+Z from CF board)
    addBox(m, cf + Vector3(0, 0, 1.4f), 24.0f, 10.0f, 0.4f, sf::Color(20, 90, 50));

    m.rebuildNormals();
    return m;
}

Mesh3D buildStands(const Layout& L) {
    Mesh3D m;
    const float aL = -L.foulAngleRad() - 0.12f;
    const float aR = L.foulAngleRad() + 0.12f;
    const float wallR = L.wallR();
    const float wallH = L.wallH();
    sf::Color seat(70, 95, 140);
    sf::Color riser(50, 68, 100);
    sf::Color upper(90, 70, 75);

    addStands(m, L, wallR + 1.5f, aL, aR, wallH, 12, 40, 3.2f, 1.15f, seat, riser);
    addStands(
        m,
        L,
        wallR + 1.5f + 12 * 3.2f,
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

    sf::Color steel(180, 185, 190);
    auto tower = [&](float ang) {
        Vector3 base = L.fromHome(wallR + 18.0f, ang, 0.0f);
        addBox(m, base + Vector3(0, 22.0f, 0), 1.2f, 44.0f, 1.2f, steel);
        addBox(m, base + Vector3(0, 44.0f, 0), 8.0f, 2.0f, 3.0f, sf::Color(240, 240, 220));
    };
    tower(-0.55f);
    tower(0.0f);
    tower(0.55f);

    m.rebuildNormals();
    return m;
}

Mesh3D buildLines(const Layout& L) {
    Mesh3D m;
    sf::Color chalk(245, 245, 235);
    float len = L.wallR() + 2.0f;
    auto line = [&](float ang) {
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

Vector3 Layout::fromHome(float radius, float angleRad, float y) const {
    // angle 0 = CF = −Z from home; +angle toward +X (1B).
    return Vector3(
        std::sin(angleRad) * radius,
        y,
        plateZ() - std::cos(angleRad) * radius
    );
}

Vector3 Layout::parkCenter() const {
    return Vector3(0.0f, 4.0f, plateZ() - wallR() * 0.42f);
}

Layout defaultPlayLayout() {
    return Layout{};
}

Meshes build(const Layout& layout) {
    Meshes out;
    out.field = buildField(layout);
    out.walls = buildWalls(layout);
    out.stands = buildStands(layout);
    out.lines = buildLines(layout);
    return out;
}

float recommendedFarPlane(const Layout& layout) {
    // Wall ~190, stands ~250, orbit/camera room.
    return std::max(2500.0f, layout.wallR() * 8.0f + 400.0f);
}

} // namespace Stadium3D
