#include "Stadium3D.h"

#include <algorithm>
#include <array>
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

// Dirt strip of half-width hw along segment a→b (y = y0).
void addDirtPath(
    Mesh3D& m,
    const Vector3& a,
    const Vector3& b,
    float hw,
    float y0,
    sf::Color col
) {
    Vector3 d = b - a;
    d.y = 0.0f;
    float len = d.magnitude();
    if (len < 1e-4f) {
        return;
    }
    d = d * (1.0f / len);
    Vector3 side(-d.z * hw, 0.0f, d.x * hw);
    Vector3 a0 = a + Vector3(0, y0, 0);
    Vector3 b0 = b + Vector3(0, y0, 0);
    addQuad(m, a0 + side, a0 - side, b0 - side, b0 + side, col);
}

// Filled quad (diamond face) at constant y.
void addFlatQuad(
    Mesh3D& m,
    const Vector3& a,
    const Vector3& b,
    const Vector3& c,
    const Vector3& d,
    float y,
    sf::Color col
) {
    addQuad(
        m,
        Vector3(a.x, y, a.z),
        Vector3(b.x, y, b.z),
        Vector3(c.x, y, c.z),
        Vector3(d.x, y, d.z),
        col
    );
}

void addDisk(Mesh3D& m, const Vector3& center, float radius, float y, int n, sf::Color col) {
    Vector3 c(center.x, y, center.z);
    for (int i = 0; i < n; i++) {
        float a0 = (static_cast<float>(i) / n) * pi * 2.0f;
        float a1 = (static_cast<float>(i + 1) / n) * pi * 2.0f;
        Vector3 p0 = c + Vector3(std::cos(a0) * radius, 0.0f, std::sin(a0) * radius);
        Vector3 p1 = c + Vector3(std::cos(a1) * radius, 0.0f, std::sin(a1) * radius);
        addTri(m, c, p0, p1, col);
    }
}

Mesh3D buildField(const Layout& L) {
    Mesh3D m;
    const float aL = -L.foulAngleRad();
    const float aR = L.foulAngleRad();
    const float plateZ = L.plateZ();
    const int segs = 56;
    const float bp = L.basePath();

    // True diamond corners (square rotated 45° in spray space).
    const Vector3 home = L.home();
    const Vector3 b1 = L.firstBase();
    const Vector3 b2 = L.secondBase();
    const Vector3 b3 = L.thirdBase();

    sf::Color grass(34, 110, 48);
    sf::Color grassDark(28, 92, 40);
    sf::Color grassInfield(38, 118, 52);
    sf::Color dirt(168, 120, 70);
    sf::Color dirtDark(140, 98, 55);
    sf::Color dirtPath(175, 128, 78);
    sf::Color track(150, 120, 70);

    // ── Fair-territory grass out to the fence (beyond the diamond) ─────
    // Skip the inner diamond radius so dirt/grass cutouts own the infield.
    const float infieldSkip = bp * 1.55f; // past 2B + margin
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = aL + (aR - aL) * t0;
        float ang1 = aL + (aR - aL) * t1;
        float rFence0 = L.wallRAtAngle(ang0) - 0.4f;
        float rFence1 = L.wallRAtAngle(ang1) - 0.4f;
        float r0 = std::min(infieldSkip, rFence0 - 1.0f);
        float r1 = std::min(infieldSkip, rFence1 - 1.0f);
        addQuad(
            m,
            L.fromHome(r0, ang0, 0.0f),
            L.fromHome(r1, ang1, 0.0f),
            L.fromHome(rFence1, ang1, 0.0f),
            L.fromHome(rFence0, ang0, 0.0f),
            grass
        );
        // Darker mid-OF band
        float mid0 = rFence0 * 0.55f;
        float mid1 = rFence1 * 0.55f;
        float mid0b = rFence0 * 0.68f;
        float mid1b = rFence1 * 0.68f;
        if (mid0b > infieldSkip) {
            addQuad(
                m,
                L.fromHome(std::max(mid0, infieldSkip), ang0, 0.01f),
                L.fromHome(std::max(mid1, infieldSkip), ang1, 0.01f),
                L.fromHome(mid1b, ang1, 0.01f),
                L.fromHome(mid0b, ang0, 0.01f),
                grassDark
            );
        }
        // Warning track
        addQuad(
            m,
            L.fromHome(rFence0 - 3.5f, ang0, 0.02f),
            L.fromHome(rFence1 - 3.5f, ang1, 0.02f),
            L.fromHome(rFence1, ang1, 0.02f),
            L.fromHome(rFence0, ang0, 0.02f),
            track
        );
    }

    // ── Foul-territory grass: home → stands along arcs outside foul lines ─
    // Uses the same bowl inner radius helper so grass meets the crowd.
    auto foulGrassR = [&](float ang) {
        // Mirror of bowlInnerRadius without including Stadium internals twice:
        // keep a modest apron so dirt diamond + dugouts sit on grass.
        float fa = L.foulAngleRad();
        float a = ang;
        while (a > pi) {
            a -= 2.0f * pi;
        }
        while (a < -pi) {
            a += 2.0f * pi;
        }
        if (std::abs(a) <= fa + 0.02f) {
            return L.wallRAtAngle(a) - 0.5f;
        }
        float foulPoleR = L.wallRAtAngle(a >= 0.0f ? fa : -fa) + 1.0f;
        float backR = 16.0f;
        float fromFoul = std::abs(a) - fa;
        float span = pi - fa;
        float u = std::clamp(fromFoul / std::max(span, 0.01f), 0.0f, 1.0f);
        // Hold wide along baselines, then pull in behind home.
        float hold = 0.28f;
        float u2 = u < hold ? 0.0f : (u - hold) / (1.0f - hold);
        u2 = u2 * u2 * (3.0f - 2.0f * u2);
        float dugoutClear = bp * 0.9f + 20.0f;
        float r = foulPoleR + (backR - foulPoleR) * u2;
        float minClear = backR + (dugoutClear - backR) * (1.0f - u2);
        return std::max(r, minClear);
    };

    const int foulSegs = 40;
    for (int side = 0; side < 2; side++) {
        // side 0: RF foul (+X, +ang → +pi); side 1: LF foul (−ang → −pi)
        for (int i = 0; i < foulSegs; i++) {
            float t0 = static_cast<float>(i) / foulSegs;
            float t1 = static_cast<float>(i + 1) / foulSegs;
            float ang0, ang1;
            if (side == 0) {
                ang0 = aR + (pi - aR) * t0;
                ang1 = aR + (pi - aR) * t1;
            } else {
                ang0 = -aR - (pi - aR) * t0;
                ang1 = -aR - (pi - aR) * t1;
            }
            float r0 = foulGrassR(ang0);
            float r1 = foulGrassR(ang1);
            // Inner edge follows diamond footprint (scaled diamond ring).
            float inner0 = bp * 0.35f;
            float inner1 = bp * 0.35f;
            addQuad(
                m,
                L.fromHome(inner0, ang0, 0.0f),
                L.fromHome(inner1, ang1, 0.0f),
                L.fromHome(r1, ang1, 0.0f),
                L.fromHome(r0, ang0, 0.0f),
                grass
            );
        }
    }

    // ── Skinned dirt diamond (square home–1B–2B–3B, slightly oversized) ─
    // Expand a bit past the base bags so dirt frames the bags.
    auto expandDiamond = [&](float scale) {
        Vector3 c = (home + b1 + b2 + b3) * 0.25f;
        auto push = [&](const Vector3& p) {
            Vector3 d = p - c;
            d.y = 0.0f;
            return c + d * scale + Vector3(0, 0, 0);
        };
        return std::array<Vector3, 4>{push(home), push(b1), push(b2), push(b3)};
    };
    // Full dirt pad under the diamond
    {
        auto d = expandDiamond(1.12f);
        addFlatQuad(m, d[0], d[1], d[2], d[3], 0.025f, dirt);
    }
    // Infield grass "skin" — classic diamond cut (smaller square inside dirt)
    {
        auto g = expandDiamond(0.72f);
        addFlatQuad(m, g[0], g[1], g[2], g[3], 0.035f, grassInfield);
    }

    // Base paths (dirt strips on top of grass skin)
    const float pathHw = 2.0f; // ~4 ft wide paths in world units (1u≈2ft → ~8ft visual)
    addDirtPath(m, home, b1, pathHw, 0.04f, dirtPath);
    addDirtPath(m, b1, b2, pathHw, 0.04f, dirtPath);
    addDirtPath(m, b2, b3, pathHw, 0.04f, dirtPath);
    addDirtPath(m, b3, home, pathHw, 0.04f, dirtPath);
    // Coach's box / cutoff path toward 2B from mound area
    addDirtPath(m, L.mound(), b2, pathHw * 0.65f, 0.04f, dirtPath);

    // Pitcher's dirt circle + home plate circle (classic look)
    addDisk(m, L.mound(), 6.5f, 0.045f, 28, dirt);
    addDisk(m, home, 8.0f, 0.045f, 28, dirt);
    // Slight dirt cutouts around each bag
    addDisk(m, b1, 3.2f, 0.05f, 16, dirt);
    addDisk(m, b2, 3.2f, 0.05f, 16, dirt);
    addDisk(m, b3, 3.2f, 0.05f, 16, dirt);

    // Mound mound (raised)
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

    // Home plate — tip toward catcher (+Z)
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

    // Bases at true diamond corners
    auto placeBase = [&](const Vector3& p) {
        addBox(
            m,
            Vector3(p.x, 0.08f, p.z),
            1.15f,
            0.08f,
            1.15f,
            sf::Color(245, 240, 225)
        );
    };
    placeBase(b1);
    placeBase(b2);
    placeBase(b3);

    // Batter's boxes
    addBox(m, Vector3(-1.5f, 0.04f, plateZ - 0.2f), 1.4f, 0.04f, 2.2f, dirtDark);
    addBox(m, Vector3(1.5f, 0.04f, plateZ - 0.2f), 1.4f, 0.04f, 2.2f, dirtDark);

    // Dugouts outside 1B/3B lines (foul side of diamond)
    {
        Vector3 mid1 = (home + b1) * 0.5f;
        Vector3 mid3 = (home + b3) * 0.5f;
        auto foulNormal = [](const Vector3& along, float preferXSign) {
            Vector3 n(-along.z, 0.0f, along.x);
            float nm = n.magnitude();
            if (nm > 1e-4f) {
                n = n * (1.0f / nm);
            }
            if (n.x * preferXSign < 0.0f) {
                n = n * -1.0f;
            }
            return n;
        };
        Vector3 n1 = foulNormal(b1 - home, +1.0f);
        Vector3 n3 = foulNormal(b3 - home, -1.0f);
        addBox(
            m,
            Vector3(mid1.x, 1.0f, mid1.z) + n1 * 10.0f,
            14.0f,
            2.0f,
            4.0f,
            sf::Color(50, 60, 70)
        );
        addBox(
            m,
            Vector3(mid3.x, 1.0f, mid3.z) + n3 * 10.0f,
            14.0f,
            2.0f,
            4.0f,
            sf::Color(50, 60, 70)
        );
    }

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

// Deterministic pseudo-random in [0,1) from integer seed.
float hash01(int n) {
    unsigned x = static_cast<unsigned>(n) * 1664525u + 1013904223u;
    return static_cast<float>(x & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

// Inner radius of lower bowl seating for a given angle (full 360° stadium).
// Follows the OF fence in fair territory; along foul territory holds wide
// past the diamond / dugouts, then pulls in tightly behind the backstop so
// the crowd wraps a true diamond rather than a circle centered on home.
float bowlInnerRadius(const Layout& L, float ang) {
    while (ang > pi) {
        ang -= 2.0f * pi;
    }
    while (ang < -pi) {
        ang += 2.0f * pi;
    }
    float fa = L.foulAngleRad();
    if (std::abs(ang) <= fa + 0.05f) {
        // Behind OF fence
        return L.wallRAtAngle(ang) + 2.2f;
    }

    float foulPoleR = L.wallRAtAngle(ang >= 0.0f ? fa : -fa) + 2.2f;
    float backR = 14.0f; // just behind catcher / backstop
    float bp = L.basePath();
    // Keep stands outside diamond + dugout apron along the baselines.
    float dugoutClear = bp * 0.95f + 22.0f;

    float fromFoul = std::abs(ang) - fa;
    float span = pi - fa;
    float u = std::clamp(fromFoul / std::max(span, 0.01f), 0.0f, 1.0f);
    // Hold near foul-pole radius along the 1B/3B sides so the bowl
    // traces the diamond sides, then ease into the backstop.
    const float hold = 0.30f;
    float u2 = u < hold ? 0.0f : (u - hold) / (1.0f - hold);
    u2 = u2 * u2 * (3.0f - 2.0f * u2);

    float r = foulPoleR + (backR - foulPoleR) * u2;
    float minClear = backR + (dugoutClear - backR) * (1.0f - u2);
    return std::max(r, minClear);
}

float bowlBaseHeight(const Layout& L, float ang) {
    float fa = L.foulAngleRad();
    if (std::abs(ang) <= fa + 0.05f) {
        return L.wallHeightAtAngle(ang) + 0.5f;
    }
    // Lower near dugouts / baselines; ramp up behind home for backstop seating.
    float u = std::clamp((std::abs(ang) - fa) / (pi - fa), 0.0f, 1.0f);
    const float hold = 0.30f;
    float u2 = u < hold ? 0.0f : (u - hold) / (1.0f - hold);
    u2 = u2 * u2 * (3.0f - 2.0f * u2);
    return 2.2f + u2 * 5.5f;
}

void addLowPolyFan(Mesh3D& m, const Vector3& seatTop, float scale, sf::Color shirt, sf::Color skin) {
    float s = scale;
    // Torso
    addBox(m, seatTop + Vector3(0, 0.42f * s, 0), 0.38f * s, 0.55f * s, 0.28f * s, shirt);
    // Head
    addBox(m, seatTop + Vector3(0, 0.85f * s, 0), 0.26f * s, 0.26f * s, 0.26f * s, skin);
    // Arms (slightly raised — "cheering" pose)
    addBox(m, seatTop + Vector3(-0.28f * s, 0.55f * s, 0), 0.12f * s, 0.45f * s, 0.12f * s, skin);
    addBox(m, seatTop + Vector3(0.28f * s, 0.55f * s, 0), 0.12f * s, 0.45f * s, 0.12f * s, skin);
}

sf::Color fanShirtColor(int seed) {
    static const sf::Color shirts[] = {
        sf::Color(200, 40, 50),   // red
        sf::Color(30, 60, 160),   // blue
        sf::Color(240, 240, 245), // white
        sf::Color(20, 20, 25),    // black
        sf::Color(230, 160, 40),  // gold
        sf::Color(40, 120, 70),   // green
        sf::Color(90, 50, 140),   // purple
        sf::Color(50, 140, 180),  // teal
    };
    return shirts[static_cast<unsigned>(seed) % 8];
}

sf::Color fanSkinColor(int seed) {
    static const sf::Color skins[] = {
        sf::Color(240, 200, 170),
        sf::Color(210, 160, 120),
        sf::Color(160, 110, 80),
        sf::Color(120, 80, 55),
        sf::Color(250, 220, 190),
    };
    return skins[static_cast<unsigned>(seed) % 5];
}

Mesh3D buildStands(const Layout& L) {
    Mesh3D m;
    const float plateZ = L.plateZ();
    sf::Color seat(55, 100, 165);      // stadium blue seats
    sf::Color seatAlt(45, 85, 145);
    sf::Color riser(40, 55, 80);
    sf::Color aisle(90, 95, 100);
    sf::Color concourse(70, 75, 85);
    sf::Color upper(100, 75, 80);

    // Full 360° lower bowl + upper deck
    const int angSegs = 72;
    const int lowerRows = 14;
    const int upperRows = 8;
    const float rowDepth = 2.6f;
    const float rowRise = 1.05f;
    const float upperGap = 3.5f;

    for (int i = 0; i < angSegs; i++) {
        float t0 = static_cast<float>(i) / angSegs;
        float t1 = static_cast<float>(i + 1) / angSegs;
        float ang0 = -pi + t0 * 2.0f * pi;
        float ang1 = -pi + t1 * 2.0f * pi;
        float angM = 0.5f * (ang0 + ang1);
        float rIn = bowlInnerRadius(L, angM);
        float yBase = bowlBaseHeight(L, angM);

        // Aisle every 6 sectors
        bool isAisle = (i % 6) == 0;
        sf::Color rowSeat = isAisle ? aisle : ((i % 2) ? seat : seatAlt);

        for (int row = 0; row < lowerRows; row++) {
            float r0 = rIn + row * rowDepth;
            float r1 = r0 + rowDepth * 0.92f;
            float y0 = yBase + row * rowRise;
            float y1 = y0 + rowRise * 0.85f;
            // Seat tread
            addQuad(
                m,
                L.fromHome(r0, ang0, y1),
                L.fromHome(r0, ang1, y1),
                L.fromHome(r1, ang1, y1),
                L.fromHome(r1, ang0, y1),
                rowSeat
            );
            // Riser
            addQuad(
                m,
                L.fromHome(r0, ang0, y0),
                L.fromHome(r0, ang1, y0),
                L.fromHome(r0, ang1, y1),
                L.fromHome(r0, ang0, y1),
                riser
            );
        }

        // Concourse shelf
        float rLowTop = rIn + lowerRows * rowDepth;
        float yConc = yBase + lowerRows * rowRise + 0.3f;
        addQuad(
            m,
            L.fromHome(rLowTop, ang0, yConc),
            L.fromHome(rLowTop, ang1, yConc),
            L.fromHome(rLowTop + upperGap, ang1, yConc),
            L.fromHome(rLowTop + upperGap, ang0, yConc),
            concourse
        );

        // Upper deck
        float rUp0 = rLowTop + upperGap;
        float yUp0 = yConc + 1.2f;
        for (int row = 0; row < upperRows; row++) {
            float r0 = rUp0 + row * (rowDepth * 1.15f);
            float r1 = r0 + rowDepth * 1.05f;
            float y0 = yUp0 + row * (rowRise * 1.15f);
            float y1 = y0 + rowRise;
            addQuad(
                m,
                L.fromHome(r0, ang0, y1),
                L.fromHome(r0, ang1, y1),
                L.fromHome(r1, ang1, y1),
                L.fromHome(r1, ang0, y1),
                upper
            );
            addQuad(
                m,
                L.fromHome(r0, ang0, y0),
                L.fromHome(r0, ang1, y0),
                L.fromHome(r0, ang1, y1),
                L.fromHome(r0, ang0, y1),
                sf::Color(55, 45, 48)
            );
        }
    }

    // Backstop / netting behind home
    const float backL = pi - 0.95f;
    const float backR = pi + 0.95f;
    float backInner = 12.5f;
    addArcWall(
        m, L, backInner, backL, backR, 0.0f, 9.0f, 32,
        sf::Color(90, 100, 110), sf::Color(110, 120, 130)
    );
    addArcWall(
        m, L, backInner + 0.35f, backL + 0.04f, backR - 0.04f, 5.0f, 18.0f, 28,
        sf::Color(190, 200, 210, 85), sf::Color(170, 180, 190, 90)
    );

    // Press level behind home
    addBox(m, Vector3(0.0f, 24.0f, plateZ + 32.0f), 28.0f, 7.0f, 10.0f, sf::Color(55, 65, 80));
    addBox(m, Vector3(0.0f, 29.0f, plateZ + 32.0f), 22.0f, 2.8f, 7.0f, sf::Color(40, 50, 60));
    // Facade glass
    addBox(m, Vector3(0.0f, 24.0f, plateZ + 27.2f), 24.0f, 5.0f, 0.4f, sf::Color(100, 150, 190, 160));

    // Dugout roofs already in field; add suite boxes on 1B/3B lines
    addBox(m, Vector3(22.0f, 8.0f, plateZ - 8.0f), 10.0f, 4.0f, 6.0f, sf::Color(60, 70, 90));
    addBox(m, Vector3(-22.0f, 8.0f, plateZ - 8.0f), 10.0f, 4.0f, 6.0f, sf::Color(60, 70, 90));

    sf::Color steel(180, 185, 190);
    auto tower = [&](float ang) {
        float r = bowlInnerRadius(L, ang) + 28.0f;
        Vector3 base = L.fromHome(r, ang, 0.0f);
        addBox(m, base + Vector3(0, 24.0f, 0), 1.4f, 48.0f, 1.4f, steel);
        addBox(m, base + Vector3(0, 48.0f, 0), 9.0f, 2.2f, 3.5f, sf::Color(245, 245, 230));
    };
    tower(-0.55f);
    tower(0.0f);
    tower(0.55f);
    tower(pi - 0.4f);
    tower(pi + 0.4f);
    tower(pi * 0.5f);
    tower(-pi * 0.5f);

    m.rebuildNormals();
    return m;
}

// Build low-poly fans in kFanSectors angular wedges for cheer animation.
std::vector<Mesh3D> buildFanSectors(const Layout& L) {
    std::vector<Mesh3D> sectors(kFanSectorCount);
    const int angSamples = 96;
    const int lowerRows = 14;
    const int upperRows = 8;
    const float rowDepth = 2.6f;
    const float rowRise = 1.05f;
    const float upperGap = 3.5f;

    int fanId = 0;
    for (int i = 0; i < angSamples; i++) {
        float t = (static_cast<float>(i) + 0.5f) / angSamples;
        float ang = -pi + t * 2.0f * pi;
        int sector = static_cast<int>((t * kFanSectorCount)) % kFanSectorCount;
        float rIn = bowlInnerRadius(L, ang);
        float yBase = bowlBaseHeight(L, ang);

        // Lower bowl fans — skip aisles, thin density for performance
        for (int row = 1; row < lowerRows; row += 1) {
            if ((i + row) % 2 != 0) {
                continue; // checkerboard occupancy ~50%
            }
            if (i % 6 == 0) {
                continue; // aisle
            }
            float r = rIn + (row + 0.45f) * rowDepth;
            float y = yBase + (row + 1.0f) * rowRise;
            Vector3 seat = L.fromHome(r, ang, y);
            addLowPolyFan(
                sectors[sector],
                seat,
                0.95f + 0.15f * hash01(fanId),
                fanShirtColor(fanId),
                fanSkinColor(fanId + 3)
            );
            fanId++;
        }

        // Upper deck fans (sparser)
        float rLowTop = rIn + lowerRows * rowDepth;
        float yConc = yBase + lowerRows * rowRise + 0.3f;
        float rUp0 = rLowTop + upperGap;
        float yUp0 = yConc + 1.2f;
        for (int row = 0; row < upperRows; row += 1) {
            if ((i + row * 3) % 3 != 0) {
                continue;
            }
            float r = rUp0 + (row + 0.4f) * (rowDepth * 1.15f);
            float y = yUp0 + (row + 1.0f) * (rowRise * 1.15f);
            Vector3 seat = L.fromHome(r, ang, y);
            addLowPolyFan(
                sectors[sector],
                seat,
                0.9f,
                fanShirtColor(fanId + 11),
                fanSkinColor(fanId + 7)
            );
            fanId++;
        }
    }

    for (auto& sec : sectors) {
        sec.rebuildNormals();
    }
    return sectors;
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
    auto thickLine = [&](const Vector3& aIn, const Vector3& bIn, float halfW, float y) {
        Vector3 a = aIn + Vector3(0, y, 0);
        Vector3 b = bIn + Vector3(0, y, 0);
        Vector3 dir = b - a;
        dir.y = 0.0f;
        float dlen = dir.magnitude();
        if (dlen < 1e-4f) {
            return;
        }
        dir = dir * (1.0f / dlen);
        Vector3 side(-dir.z * halfW, 0.0f, dir.x * halfW);
        addQuad(m, a + side, a - side, b - side, b + side, chalk);
    };
    // Foul lines home → poles
    auto foulLine = [&](float ang) {
        float len = L.wallRAtAngle(ang) + 1.0f;
        thickLine(L.home(), L.fromHome(len, ang, 0.0f), 0.18f, 0.07f);
    };
    foulLine(-L.foulAngleRad());
    foulLine(L.foulAngleRad());
    // Diamond chalk: base paths
    thickLine(L.home(), L.firstBase(), 0.12f, 0.075f);
    thickLine(L.firstBase(), L.secondBase(), 0.12f, 0.075f);
    thickLine(L.secondBase(), L.thirdBase(), 0.12f, 0.075f);
    thickLine(L.thirdBase(), L.home(), 0.12f, 0.075f);
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

void Layout::polarFromHome(const Vector3& worldPos, float& radiusOut, float& angleRadOut) const {
    float dx = worldPos.x;
    float dz = worldPos.z - plateZ();
    radiusOut = std::sqrt(dx * dx + dz * dz);
    angleRadOut = std::atan2(dx, -dz); // 0 = CF (−Z)
}

float Layout::radiusFromHome(const Vector3& worldPos) const {
    float r = 0.0f;
    float a = 0.0f;
    polarFromHome(worldPos, r, a);
    (void)a;
    return r;
}

WallClearResult evaluateWallClear(
    const Layout& layout,
    Vector3 position,
    Vector3 velocity,
    float gravityY,
    float dragK
) {
    WallClearResult out;
    float r0 = 0.0f;
    float ang0 = 0.0f;
    layout.polarFromHome(position, r0, ang0);
    out.sprayDeg = ang0 * (180.0f / pi);
    out.fair = std::abs(out.sprayDeg) <= layout.foulAngleDegrees + 0.5f;
    out.fenceFeet = layout.wallFeetAtAngle(ang0);
    out.wallTopY = layout.wallHeightAtAngle(ang0);

    if (!out.fair) {
        // Still project landing distance for readout.
        out.landFeet = r0 * layout.feetPerUnit;
    }

    const float dt = 1.0f / 180.0f;
    const int maxSteps = 4000;
    const float groundY = 0.05f;
    float prevR = r0;
    Vector3 pos = position;
    Vector3 vel = velocity;

    for (int i = 0; i < maxSteps; i++) {
        Vector3 prevPos = pos;

        // Quadratic drag (direction opposite velocity) + gravity.
        float spd = vel.magnitude();
        Vector3 acc(0.0f, gravityY, 0.0f);
        if (spd > 1e-4f && dragK > 0.0f) {
            acc = acc - vel * (dragK * spd);
        }
        vel = vel + acc * dt;
        pos = pos + vel * dt;

        float r = 0.0f;
        float ang = 0.0f;
        layout.polarFromHome(pos, r, ang);
        float wallR = layout.wallRAtAngle(ang);
        float wallH = layout.wallHeightAtAngle(ang);

        // Crossed fence radius this step (outward).
        if (prevR < wallR && r >= wallR) {
            float u = (wallR - prevR) / std::max(r - prevR, 1e-5f);
            u = std::clamp(u, 0.0f, 1.0f);
            float yFence = prevPos.y + (pos.y - prevPos.y) * u;

            out.sprayDeg = ang * (180.0f / pi);
            out.fair = std::abs(out.sprayDeg) <= layout.foulAngleDegrees + 0.5f;
            out.fenceFeet = layout.wallFeetAtAngle(ang);
            out.wallTopY = wallH;
            out.heightAtFence = yFence;
            out.marginFeet = (yFence - wallH) * layout.feetPerUnit;

            if (out.fair && yFence >= wallH - 0.05f) {
                out.clearsWall = true;
                // Keep integrating for landing distance past the wall.
            } else if (out.fair) {
                out.hitsWallFace = true;
                out.landFeet = out.fenceFeet;
                return out;
            } else {
                // Foul past the foul line — not an HR clear.
                out.landFeet = r * layout.feetPerUnit;
                return out;
            }
        }

        if (pos.y <= groundY && vel.y <= 0.0f) {
            pos.y = groundY;
            float landR = 0.0f;
            float landA = 0.0f;
            layout.polarFromHome(pos, landR, landA);
            out.landFeet = landR * layout.feetPerUnit;
            out.sprayDeg = landA * (180.0f / pi);
            out.fair = std::abs(out.sprayDeg) <= layout.foulAngleDegrees + 0.5f;
            return out;
        }

        // Escaped far past park without landing.
        if (r > layout.maxWallR() + 120.0f) {
            out.landFeet = r * layout.feetPerUnit;
            if (out.clearsWall) {
                return out;
            }
            break;
        }

        prevR = r;
    }

    float endR = 0.0f;
    float endA = 0.0f;
    layout.polarFromHome(pos, endR, endA);
    if (out.landFeet <= 0.0f) {
        out.landFeet = endR * layout.feetPerUnit;
    }
    return out;
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
    out.fanSectors = buildFanSectors(layout);
    return out;
}

float recommendedFarPlane(const Layout& layout) {
    return std::max(3500.0f, layout.maxWallR() * 12.0f + 800.0f);
}

float fanCheerOffsetY(int sectorIndex, float timeSec) {
    // Traveling wave of fans standing / bouncing.
    float phase = static_cast<float>(sectorIndex) * 0.55f;
    float wave = std::sin(timeSec * 5.5f + phase);
    float wave2 = std::sin(timeSec * 3.2f + phase * 1.7f);
    return 0.12f + 0.22f * std::max(0.0f, wave) + 0.06f * wave2;
}

} // namespace Stadium3D
