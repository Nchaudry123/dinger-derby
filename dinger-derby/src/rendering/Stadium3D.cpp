#include "Stadium3D.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// Open-air minor-league ballpark (Fredericksburg-class reference).
// Horseshoe blue bowl + red club seats, blue OF wall, dirt diamond,
// striped grass, light towers. No closed dome.

namespace Stadium3D {
namespace {

constexpr float pi = 3.1415926535f;

float hash01(int n) {
    unsigned x = static_cast<unsigned>(n) * 747796405u + 2891336453u;
    x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    x = (x >> 22u) ^ x;
    return static_cast<float>(x) / 4294967295.0f;
}

sf::Color shadeColor(sf::Color c, float mul) {
    return sf::Color(
        static_cast<std::uint8_t>(std::clamp(c.r * mul, 0.0f, 255.0f)),
        static_cast<std::uint8_t>(std::clamp(c.g * mul, 0.0f, 255.0f)),
        static_cast<std::uint8_t>(std::clamp(c.b * mul, 0.0f, 255.0f)),
        c.a
    );
}

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
        center + Vector3(-hw, -hh, -hd), center + Vector3(hw, -hh, -hd),
        center + Vector3(hw, -hh, hd),  center + Vector3(-hw, -hh, hd),
        center + Vector3(-hw, hh, -hd),  center + Vector3(hw, hh, -hd),
        center + Vector3(hw, hh, hd),   center + Vector3(-hw, hh, hd),
    };
    addQuad(m, p[0], p[1], p[2], p[3], col);
    addQuad(m, p[4], p[7], p[6], p[5], col);
    addQuad(m, p[0], p[4], p[5], p[1], col);
    addQuad(m, p[1], p[5], p[6], p[2], col);
    addQuad(m, p[2], p[6], p[7], p[3], col);
    addQuad(m, p[3], p[7], p[4], p[0], col);
}

void addDisk(Mesh3D& m, const Vector3& c, float r, float y, int segs, sf::Color col) {
    for (int i = 0; i < segs; i++) {
        float a0 = (static_cast<float>(i) / segs) * 2.0f * pi;
        float a1 = (static_cast<float>(i + 1) / segs) * 2.0f * pi;
        addTri(
            m,
            Vector3(c.x, y, c.z),
            Vector3(c.x + std::cos(a0) * r, y, c.z + std::sin(a0) * r),
            Vector3(c.x + std::cos(a1) * r, y, c.z + std::sin(a1) * r),
            col
        );
    }
}

void addDirtPath(
    Mesh3D& m,
    const Vector3& a,
    const Vector3& b,
    float halfW,
    float y,
    sf::Color col
) {
    Vector3 d = b - a;
    float len = std::sqrt(d.x * d.x + d.z * d.z);
    if (len < 1e-4f) {
        return;
    }
    Vector3 n(-d.z / len * halfW, 0.0f, d.x / len * halfW);
    Vector3 a0 = a + n;
    Vector3 a1 = a - n;
    Vector3 b0 = b + n;
    Vector3 b1 = b - n;
    a0.y = a1.y = b0.y = b1.y = y;
    addQuad(m, a0, b0, b1, a1, col);
}

// ── Field ─────────────────────────────────────────────────────────────

Mesh3D buildField(const Layout& L) {
    Mesh3D m;
    const float aL = -L.foulAngleRad();
    const float aR = L.foulAngleRad();
    const float bp = L.basePath();
    const Vector3 home = L.home();
    const Vector3 b1 = L.firstBase();
    const Vector3 b2 = L.secondBase();
    const Vector3 b3 = L.thirdBase();
    const sf::Color grass = grassColor();
    const sf::Color grassDk = grassDarkColor();
    const sf::Color dirt = dirtColor();
    const sf::Color dirtDk(150, 100, 55);
    const sf::Color track = warningTrackColor();
    const float trackW = 3.2f;

    // Fair grass pie (home → warning track).
    const int fairSegs = 72;
    for (int i = 0; i < fairSegs; i++) {
        float t0 = static_cast<float>(i) / fairSegs;
        float t1 = static_cast<float>(i + 1) / fairSegs;
        float ang0 = aL + (aR - aL) * t0;
        float ang1 = aL + (aR - aL) * t1;
        float r0 = L.wallRAtAngle(ang0) - trackW;
        float r1 = L.wallRAtAngle(ang1) - trackW;
        addQuad(
            m,
            L.fromHome(0.3f, ang0, 0.0f),
            L.fromHome(0.3f, ang1, 0.0f),
            L.fromHome(r1, ang1, 0.0f),
            L.fromHome(r0, ang0, 0.0f),
            grass
        );
        // Mowing stripes
        for (int s = 0; s < 8; s++) {
            float u0 = 0.28f + s * 0.08f;
            float u1 = u0 + 0.038f;
            float ri0 = std::max(bp * 1.2f, r0 * u0);
            float ri1 = std::max(bp * 1.2f, r1 * u0);
            float ro0 = std::min(r0 - 0.4f, r0 * u1);
            float ro1 = std::min(r1 - 0.4f, r1 * u1);
            if (ro0 <= ri0 + 0.3f) {
                continue;
            }
            sf::Color sc = (s % 2 == 0) ? grassDk : shadeColor(grass, 0.96f);
            addQuad(
                m,
                L.fromHome(ri0, ang0, 0.01f),
                L.fromHome(ri1, ang1, 0.01f),
                L.fromHome(ro1, ang1, 0.01f),
                L.fromHome(ro0, ang0, 0.01f),
                sc
            );
        }
        // Warning track
        float w0 = L.wallRAtAngle(ang0);
        float w1 = L.wallRAtAngle(ang1);
        addQuad(
            m,
            L.fromHome(r0, ang0, 0.015f),
            L.fromHome(r1, ang1, 0.015f),
            L.fromHome(w1 - 0.15f, ang1, 0.015f),
            L.fromHome(w0 - 0.15f, ang0, 0.015f),
            track
        );
    }

    // Thin foul grass ribbons just outside foul lines (not a big lobe).
    for (int side = 0; side < 2; side++) {
        float foulAng = (side == 0) ? aL : aR;
        float sign = (side == 0) ? -1.0f : 1.0f;
        for (int i = 0; i < 40; i++) {
            float t0 = static_cast<float>(i) / 40.0f;
            float t1 = static_cast<float>(i + 1) / 40.0f;
            float s0 = 4.0f + t0 * (L.wallRAtAngle(foulAng) - 6.0f);
            float s1 = 4.0f + t1 * (L.wallRAtAngle(foulAng) - 6.0f);
            Vector3 along0 = L.fromHome(s0, foulAng, 0.0f);
            Vector3 along1 = L.fromHome(s1, foulAng, 0.0f);
            Vector3 perp(std::cos(foulAng) * sign * 4.5f, 0, std::sin(foulAng) * sign * 4.5f);
            // fromHome dir = (sin a, 0, -cos a); perp into foul ≈ (cos a, 0, sin a) * sign
            Vector3 p0(
                std::cos(foulAng) * sign,
                0.0f,
                std::sin(foulAng) * sign
            );
            addQuad(
                m,
                along0,
                along1,
                along1 + p0 * 5.5f,
                along0 + p0 * 5.5f,
                shadeColor(grass, 0.92f)
            );
            (void)perp;
        }
    }

    // Dirt diamond lip + base paths
    {
        Vector3 h = home;
        Vector3 pts[4] = {h, b1, b2, b3};
        for (int i = 0; i < 4; i++) {
            Vector3 a = pts[i];
            Vector3 b = pts[(i + 1) % 4];
            addDirtPath(m, a, b, 1.55f, 0.02f, dirt);
            addDirtPath(m, a, b, 0.35f, 0.022f, dirtDk);
        }
    }
    // Infield dirt skin (diamond fill)
    {
        Vector3 c = (home + b1 + b2 + b3) * 0.25f;
        c.y = 0.012f;
        addQuad(m, home + Vector3(0, 0.012f, 0), b1 + Vector3(0, 0.012f, 0),
                b2 + Vector3(0, 0.012f, 0), b3 + Vector3(0, 0.012f, 0), shadeColor(dirt, 0.97f));
        // Grass infield diamond inside dirt (classic cutout look)
        Vector3 ih = home + (b2 - home) * 0.18f;
        Vector3 i1 = home + (b1 - home) * 0.72f + (b2 - b1) * 0.12f;
        Vector3 i2 = b2 + (home - b2) * 0.12f;
        Vector3 i3 = home + (b3 - home) * 0.72f + (b2 - b3) * 0.12f;
        ih.y = i1.y = i2.y = i3.y = 0.016f;
        addQuad(m, ih, i1, i2, i3, grass);
    }

    // Mound / home / base cutouts
    addDisk(m, L.mound(), 4.2f, 0.025f, 28, dirt);
    addDisk(m, L.mound(), 2.4f, 0.028f, 20, dirtDk);
    addBox(m, L.mound() + Vector3(0, 0.12f, 0), 1.5f, 0.22f, 2.2f, shadeColor(dirt, 1.05f));
    addDisk(m, home + Vector3(0, 0, -0.3f), 5.5f, 0.024f, 28, dirt);
    addDisk(m, b1, 2.3f, 0.026f, 18, dirt);
    addDisk(m, b2, 2.3f, 0.026f, 18, dirt);
    addDisk(m, b3, 2.3f, 0.026f, 18, dirt);
    // Bases
    auto bag = [&](const Vector3& p) {
        addBox(m, p + Vector3(0, 0.06f, 0), 0.85f, 0.08f, 0.85f, sf::Color(245, 245, 240));
    };
    bag(b1);
    bag(b2);
    bag(b3);
    // Home plate
    {
        float pz = L.plateZ();
        Vector3 tip(0, 0.04f, pz + 0.55f);
        Vector3 bl(-0.55f, 0.04f, pz - 0.35f);
        Vector3 br(0.55f, 0.04f, pz - 0.35f);
        Vector3 fl(-0.55f, 0.04f, pz + 0.15f);
        Vector3 fr(0.55f, 0.04f, pz + 0.15f);
        addTri(m, tip, fl, fr, sf::Color(250, 250, 248));
        addQuad(m, fl, fr, br, bl, sf::Color(250, 250, 248));
    }

    m.rebuildNormals();
    return m;
}

// ── OF wall + foul poles ──────────────────────────────────────────────

Mesh3D buildWalls(const Layout& L) {
    Mesh3D m;
    const float aL = -L.foulAngleRad();
    const float aR = L.foulAngleRad();
    const int segs = 56;
    sf::Color face = ofWallColor();
    sf::Color faceAlt = shadeColor(face, 1.08f);
    sf::Color top = ofWallTopColor();

    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = aL + (aR - aL) * t0;
        float ang1 = aL + (aR - aL) * t1;
        float r0 = L.wallRAtAngle(ang0);
        float r1 = L.wallRAtAngle(ang1);
        float h0 = L.wallHeightAtAngle(ang0);
        float h1 = L.wallHeightAtAngle(ang1);
        sf::Color fc = (i % 3 == 0) ? faceAlt : face;
        // Front face
        addQuad(
            m,
            L.fromHome(r0, ang0, 0.0f),
            L.fromHome(r1, ang1, 0.0f),
            L.fromHome(r1, ang1, h1),
            L.fromHome(r0, ang0, h0),
            fc
        );
        // Top cap
        addQuad(
            m,
            L.fromHome(r0, ang0, h0),
            L.fromHome(r1, ang1, h1),
            L.fromHome(r1 + 0.9f, ang1, h1),
            L.fromHome(r0 + 0.9f, ang0, h0),
            top
        );
        // Panel seams (slight inset vertical strip every few)
        if (i % 4 == 0) {
            addBox(
                m,
                L.fromHome(r0 + 0.05f, ang0, h0 * 0.5f),
                0.12f,
                h0 * 0.95f,
                0.12f,
                shadeColor(face, 0.85f)
            );
        }
    }

    // Foul poles
    auto pole = [&](float ang) {
        float r = L.wallRAtAngle(ang);
        float h = L.wallHeightAtAngle(ang) * 3.2f;
        Vector3 base = L.fromHome(r, ang, 0.0f);
        addBox(m, base + Vector3(0, h * 0.5f, 0), 0.45f, h, 0.45f, sf::Color(230, 60, 55));
        addBox(m, base + Vector3(0, h + 0.4f, 0), 1.2f, 0.5f, 0.15f, sf::Color(240, 240, 245));
    };
    pole(aL);
    pole(aR);

    // Dugout roofs (1B / 3B)
    {
        float z = L.plateZ() - 8.0f;
        addBox(m, Vector3(14.0f, 1.6f, z), 10.0f, 2.2f, 4.0f, ofWallColor());
        addBox(m, Vector3(-14.0f, 1.6f, z), 10.0f, 2.2f, 4.0f, ofWallColor());
        addBox(m, Vector3(14.0f, 2.9f, z), 10.5f, 0.35f, 4.4f, facadeGrayColor());
        addBox(m, Vector3(-14.0f, 2.9f, z), 10.5f, 0.35f, 4.4f, facadeGrayColor());
    }

    m.rebuildNormals();
    return m;
}

// ── Stands (horseshoe) ────────────────────────────────────────────────

Mesh3D buildStands(const Layout& L) {
    Mesh3D m;
    const int angSegs = 120;
    const int rowsLower = 14;
    const float dRow = 1.45f;
    const float rise = 0.95f;
    sf::Color blueA = seatBlueColor();
    sf::Color blueB = seatBlueAltColor();
    sf::Color redA = seatRedColor();
    sf::Color redB = seatRedAltColor();
    sf::Color riser(35, 55, 95);
    sf::Color conc = concourseColor();

    for (int i = 0; i < angSegs; i++) {
        float t0 = static_cast<float>(i) / angSegs;
        float t1 = static_cast<float>(i + 1) / angSegs;
        // Full circle sample; skip open OF.
        float ang0 = -pi + t0 * 2.0f * pi;
        float ang1 = -pi + t1 * 2.0f * pi;
        float angM = 0.5f * (ang0 + ang1);
        if (!L.isSeatingArc(angM)) {
            continue;
        }

        float rIn = L.bowlInnerRadius(angM);
        float y0 = L.bowlBaseHeight(angM);
        bool cornerRed = false;
        {
            float fa = L.foulAngleRad();
            float absA = std::abs(angM);
            // Red pockets near foul poles / corners
            if (absA > fa + 0.05f && absA < fa + 0.55f) {
                cornerRed = true;
            }
        }

        float r = rIn;
        float y = y0;
        for (int row = 0; row < rowsLower; row++) {
            float r1 = r + dRow * 0.92f;
            float y1 = y + rise * 0.85f;
            bool useRed = cornerRed && row < 8;
            sf::Color sc = useRed ? ((row + i) % 2 ? redA : redB)
                                  : ((row + i) % 2 ? blueA : blueB);
            sc = shadeColor(sc, 0.94f + 0.06f * (hash01(i * 3 + row)));
            addQuad(
                m,
                L.fromHome(r, ang0, y1),
                L.fromHome(r, ang1, y1),
                L.fromHome(r1, ang1, y1),
                L.fromHome(r1, ang0, y1),
                sc
            );
            addQuad(
                m,
                L.fromHome(r, ang0, y),
                L.fromHome(r, ang1, y),
                L.fromHome(r, ang1, y1),
                L.fromHome(r, ang0, y1),
                riser
            );
            r = r1 + dRow * 0.06f;
            y += rise;
        }

        // Concourse walk behind lower bowl
        float rC0 = r + 0.4f;
        float rC1 = r + 3.5f;
        float yC = y + 0.2f;
        addQuad(
            m,
            L.fromHome(rC0, ang0, yC),
            L.fromHome(rC0, ang1, yC),
            L.fromHome(rC1, ang1, yC),
            L.fromHome(rC1, ang0, yC),
            conc
        );

        // Upper / club deck only behind home (and partial 1B/3B)
        float absA = std::abs(angM);
        if (absA > 1.55f) { // near π
            float rU = rC1 + 0.5f;
            float yU = yC + 3.5f;
            int rowsUp = 6;
            for (int row = 0; row < rowsUp; row++) {
                float r1 = rU + dRow * 0.9f;
                float y1 = yU + rise * 0.8f;
                sf::Color sc = (row + i) % 2 ? redA : redB;
                addQuad(
                    m,
                    L.fromHome(rU, ang0, y1),
                    L.fromHome(rU, ang1, y1),
                    L.fromHome(r1, ang1, y1),
                    L.fromHome(r1, ang0, y1),
                    sc
                );
                addQuad(
                    m,
                    L.fromHome(rU, ang0, yU),
                    L.fromHome(rU, ang1, yU),
                    L.fromHome(rU, ang1, y1),
                    L.fromHome(rU, ang0, y1),
                    sf::Color(50, 70, 110)
                );
                rU = r1 + 0.08f;
                yU += rise;
            }
            // Upper concourse roof slab
            addQuad(
                m,
                L.fromHome(rC1, ang0, yC + 3.2f),
                L.fromHome(rC1, ang1, yC + 3.2f),
                L.fromHome(rU + 1.5f, ang1, yC + 3.2f),
                L.fromHome(rU + 1.5f, ang0, yC + 3.2f),
                facadeGrayColor()
            );
        }
    }

    // Backstop netting / low wall behind plate
    {
        float backR = 14.5f;
        for (int i = 0; i < 28; i++) {
            float t0 = static_cast<float>(i) / 28.0f;
            float t1 = static_cast<float>(i + 1) / 28.0f;
            float ang0 = pi - 1.0f + t0 * 2.0f;
            float ang1 = pi - 1.0f + t1 * 2.0f;
            addQuad(
                m,
                L.fromHome(backR, ang0, 0.0f),
                L.fromHome(backR, ang1, 0.0f),
                L.fromHome(backR, ang1, 4.5f),
                L.fromHome(backR, ang0, 4.5f),
                sf::Color(70, 85, 100)
            );
        }
    }

    m.rebuildNormals();
    return m;
}

// ── Suite facade (behind home) ────────────────────────────────────────

Mesh3D buildHotel(const Layout& L) {
    Mesh3D m;
    // Multi-level suite box wrapping behind home (blue glass + tan structure).
    float z = L.plateZ() + 28.0f;
    addBox(m, Vector3(0, 10.0f, z), 42.0f, 8.0f, 12.0f, facadeGrayColor());
    addBox(m, Vector3(0, 14.5f, z - 1.0f), 40.0f, 3.5f, 0.4f, sf::Color(40, 100, 170));
    addBox(m, Vector3(0, 11.0f, z - 1.1f), 38.0f, 2.8f, 0.25f, sf::Color(80, 160, 210, 180));
    // Side wings
    addBox(m, Vector3(28.0f, 8.0f, z - 4.0f), 14.0f, 10.0f, 16.0f, facadeTanColor());
    addBox(m, Vector3(-28.0f, 8.0f, z - 4.0f), 14.0f, 10.0f, 16.0f, facadeTanColor());
    addBox(m, Vector3(22.0f, 12.0f, z - 6.0f), 10.0f, 4.0f, 8.0f, facadeGrayColor());
    addBox(m, Vector3(-22.0f, 12.0f, z - 6.0f), 10.0f, 4.0f, 8.0f, facadeGrayColor());
    m.rebuildNormals();
    return m;
}

// ── Scoreboard / OF boards ────────────────────────────────────────────

Mesh3D buildScoreboardScreen(const Layout& L) {
    Mesh3D m;
    // CF wall advertising boards (blue panels) — no brand text.
    const int n = 10;
    for (int i = 0; i < n; i++) {
        float t = (static_cast<float>(i) + 0.5f) / n;
        float ang = -L.foulAngleRad() * 0.85f + t * L.foulAngleRad() * 1.7f;
        float r = L.wallRAtAngle(ang) + 0.6f;
        float h = L.wallHeightAtAngle(ang) * 0.55f;
        Vector3 c = L.fromHome(r, ang, h);
        addBox(m, c, 6.5f, L.wallH() * 0.75f, 0.35f, ofWallColor());
    }
    // Main CF scoreboard chassis (white face, abstract)
    {
        float r = L.wallRAtAngle(0.0f) + 4.0f;
        Vector3 c = L.fromHome(r, 0.0f, 9.0f);
        addBox(m, c, 18.0f, 8.0f, 1.2f, boardChassisColor());
        addBox(m, c + Vector3(0, 0, -0.7f), 16.0f, 6.5f, 0.3f, sf::Color(25, 55, 100));
        // Small roof over board
        addBox(m, c + Vector3(0, 4.5f, 0.2f), 19.0f, 0.4f, 2.0f, facadeGrayColor());
    }
    // RF corner sign plate (blank white — no logos)
    {
        float ang = L.foulAngleRad() * 0.72f;
        Vector3 c = L.fromHome(L.wallRAtAngle(ang) + 3.0f, ang, 5.5f);
        addBox(m, c, 10.0f, 4.5f, 0.5f, sf::Color(245, 245, 248));
        addBox(m, c + Vector3(0, 0, -0.3f), 8.5f, 2.2f, 0.15f, sf::Color(180, 40, 45));
    }
    m.rebuildNormals();
    return m;
}

// ── Light towers + rails ──────────────────────────────────────────────

Mesh3D buildStructure(const Layout& L) {
    Mesh3D m;
    sf::Color pole(230, 230, 225);
    sf::Color lamp(255, 250, 230);

    auto lightTower = [&](float ang, float r) {
        Vector3 base = L.fromHome(r, ang, 0.0f);
        float h = 28.0f + hash01(static_cast<int>(ang * 100)) * 6.0f;
        addBox(m, base + Vector3(0, h * 0.5f, 0), 0.45f, h, 0.45f, pole);
        // Cross arm
        addBox(m, base + Vector3(0, h, 0), 5.5f, 0.35f, 0.35f, pole);
        addBox(m, base + Vector3(0, h - 1.2f, 0), 4.5f, 0.3f, 0.3f, pole);
        for (int k = -2; k <= 2; k++) {
            addBox(
                m,
                base + Vector3(static_cast<float>(k) * 1.1f, h + 0.5f, 0),
                0.7f,
                0.55f,
                0.5f,
                lamp
            );
        }
    };

    // Towers around seating + OF corners
    const float towers[][2] = {
        {2.4f, 42.0f},  {-2.4f, 42.0f}, {1.9f, 55.0f}, {-1.9f, 55.0f},
        {1.2f, 90.0f},  {-1.2f, 90.0f}, {0.55f, 155.0f}, {-0.55f, 155.0f},
        {0.0f, 175.0f}, {2.9f, 35.0f},  {-2.9f, 35.0f},
    };
    for (const auto& t : towers) {
        lightTower(t[0], t[1]);
    }

    // CF bullpen shed
    {
        Vector3 c = L.fromHome(L.wallRAtAngle(0.15f) - 8.0f, 0.18f, 1.2f);
        addBox(m, c, 6.0f, 2.4f, 4.0f, facadeGrayColor());
    }

    // Railings along OF wall top (segments)
    for (int i = 0; i < 20; i++) {
        float t = (static_cast<float>(i) + 0.5f) / 20.0f;
        float ang = -L.foulAngleRad() + t * 2.0f * L.foulAngleRad();
        float r = L.wallRAtAngle(ang) + 0.5f;
        float h = L.wallHeightAtAngle(ang) + 0.8f;
        addBox(m, L.fromHome(r, ang, h), 0.12f, 1.4f, 0.12f, railColor());
    }

    m.rebuildNormals();
    return m;
}

// ── Exterior berm / apron ─────────────────────────────────────────────

Mesh3D buildCity(const Layout& L) {
    Mesh3D m;
    // Irregular outer apron past seating / OF wall (tan concrete + grass patches).
    const int segs = 48;
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = -pi + t0 * 2.0f * pi;
        float ang1 = -pi + t1 * 2.0f * pi;
        float angM = 0.5f * (ang0 + ang1);
        float rIn = L.isSeatingArc(angM) ? L.bowlInnerRadius(angM) + 28.0f
                                         : L.wallRAtAngle(angM) + 6.0f;
        if (!L.isSeatingArc(angM) && std::abs(angM) > L.foulAngleRad()) {
            rIn = L.bowlInnerRadius(angM) + 8.0f;
        }
        float rOut = rIn + 18.0f + 6.0f * hash01(i * 7);
        sf::Color apron = facadeTanColor();
        addQuad(
            m,
            L.fromHome(rIn, ang0, -0.4f),
            L.fromHome(rIn, ang1, -0.4f),
            L.fromHome(rOut, ang1, -1.2f),
            L.fromHome(rOut, ang0, -1.2f),
            apron
        );
        // Outer grass lip
        addQuad(
            m,
            L.fromHome(rOut, ang0, -1.15f),
            L.fromHome(rOut, ang1, -1.15f),
            L.fromHome(rOut + 10.0f, ang1, -1.5f),
            L.fromHome(rOut + 10.0f, ang0, -1.5f),
            shadeColor(grassColor(), 0.85f)
        );
    }
    // Concourse buildings 1B/3B exterior
    addBox(m, Vector3(48.0f, 5.0f, L.plateZ() + 8.0f), 22.0f, 10.0f, 28.0f, facadeGrayColor());
    addBox(m, Vector3(-48.0f, 5.0f, L.plateZ() + 8.0f), 22.0f, 10.0f, 28.0f, facadeGrayColor());
    addBox(m, Vector3(55.0f, 3.5f, L.plateZ() - 20.0f), 14.0f, 7.0f, 18.0f, facadeTanColor());
    addBox(m, Vector3(-55.0f, 3.5f, L.plateZ() - 20.0f), 14.0f, 7.0f, 18.0f, facadeTanColor());

    m.rebuildNormals();
    return m;
}

// ── Foul lines + chalk ────────────────────────────────────────────────

Mesh3D buildLines(const Layout& L) {
    Mesh3D m;
    sf::Color chalk(245, 245, 240);
    auto thickLine = [&](const Vector3& a, const Vector3& b, float hw, float y) {
        addDirtPath(m, a, b, hw, y, chalk);
    };
    float aL = -L.foulAngleRad();
    float aR = L.foulAngleRad();
    thickLine(L.home(), L.fromHome(L.wallRAtAngle(aL), aL, 0), 0.12f, 0.04f);
    thickLine(L.home(), L.fromHome(L.wallRAtAngle(aR), aR, 0), 0.12f, 0.04f);
    thickLine(L.home(), L.firstBase(), 0.08f, 0.045f);
    thickLine(L.firstBase(), L.secondBase(), 0.08f, 0.045f);
    thickLine(L.secondBase(), L.thirdBase(), 0.08f, 0.045f);
    thickLine(L.thirdBase(), L.home(), 0.08f, 0.045f);
    m.rebuildNormals();
    return m;
}

// ── Sparse crowd ──────────────────────────────────────────────────────

void addLowPolyFan(Mesh3D& m, const Vector3& feet, float scale, sf::Color shirt, sf::Color skin) {
    addBox(m, feet + Vector3(0, 0.55f * scale, 0), 0.35f * scale, 1.1f * scale, 0.28f * scale, shirt);
    addBox(m, feet + Vector3(0, 1.25f * scale, 0), 0.28f * scale, 0.28f * scale, 0.28f * scale, skin);
}

sf::Color fanShirt(int id) {
    sf::Color opts[] = {
        seatBlueColor(), seatRedColor(), sf::Color(240, 240, 245), sf::Color(40, 40, 50),
        sf::Color(30, 120, 70), sf::Color(200, 160, 40)};
    return opts[static_cast<unsigned>(id) % 6];
}

std::vector<Mesh3D> buildFanSectors(const Layout& L) {
    std::vector<Mesh3D> sectors(kFanSectorCount);
    const int angSamples = 140;
    const int rows = 12;
    const float dRow = 1.45f;
    const float rise = 0.95f;
    int fanId = 0;

    for (int i = 0; i < angSamples; i++) {
        float t = (static_cast<float>(i) + 0.5f) / angSamples;
        float ang = -pi + t * 2.0f * pi;
        if (!L.isSeatingArc(ang)) {
            continue;
        }
        int sector = static_cast<int>(t * kFanSectorCount) % kFanSectorCount;
        float r = L.bowlInnerRadius(ang) + 0.6f;
        float y = L.bowlBaseHeight(ang) + 0.7f;
        for (int row = 0; row < rows; row++) {
            if (hash01(fanId * 11 + row) > 0.72f) {
                fanId++;
                r += dRow;
                y += rise;
                continue;
            }
            Vector3 seat = L.fromHome(r, ang, y);
            seat.x += (hash01(fanId) - 0.5f) * 0.35f;
            seat.z += (hash01(fanId + 2) - 0.5f) * 0.35f;
            float sc = 0.85f + 0.2f * hash01(fanId + 5);
            addLowPolyFan(
                sectors[sector],
                seat,
                sc,
                fanShirt(fanId),
                sf::Color(210, 170, 140)
            );
            fanId++;
            r += dRow;
            y += rise;
        }
    }
    for (auto& s : sectors) {
        s.rebuildNormals();
    }
    return sectors;
}

void buildFlags(const Layout& L, std::vector<Mesh3D>& flags, std::vector<Vector3>& bases) {
    flags.clear();
    bases.clear();
    // A few foul-pole-area flags
    for (int i = 0; i < kFlagCount; i++) {
        float side = (i % 2 == 0) ? 1.0f : -1.0f;
        float ang = side * (L.foulAngleRad() + 0.15f + 0.08f * (i / 2));
        float r = L.wallRAtAngle(side * L.foulAngleRad()) + 4.0f + (i / 2) * 3.0f;
        Vector3 base = L.fromHome(r, ang, 0.0f);
        Mesh3D m;
        addBox(m, Vector3(0, 4.0f, 0), 0.12f, 8.0f, 0.12f, sf::Color(200, 200, 205));
        addQuad(
            m,
            Vector3(0.1f, 7.5f, 0),
            Vector3(2.8f, 7.3f, 0.1f),
            Vector3(2.6f, 5.2f, 0.1f),
            Vector3(0.1f, 5.5f, 0),
            (i % 3 == 0) ? seatRedColor() : ofWallColor()
        );
        m.rebuildNormals();
        flags.push_back(std::move(m));
        bases.push_back(base);
    }
}

} // namespace

// ── Layout methods ────────────────────────────────────────────────────

float Layout::foulAngleRad() const {
    return foulAngleDegrees * (pi / 180.0f);
}

float Layout::wallFeetAtAngle(float angleRad) const {
    float aDeg = angleRad * (180.0f / pi);
    aDeg = std::clamp(aDeg, -foulAngleDegrees, foulAngleDegrees);
    // Symmetric-ish open park: LF/RF ~330, alleys ~365, CF ~400
    static const float samples[][2] = {
        {-45.0f, 330.0f},
        {-28.0f, 355.0f},
        {-15.0f, 375.0f},
        {0.0f, 400.0f},
        {15.0f, 375.0f},
        {28.0f, 355.0f},
        {45.0f, 330.0f},
    };
    constexpr int n = 7;
    if (aDeg <= samples[0][0]) {
        return samples[0][1];
    }
    if (aDeg >= samples[n - 1][0]) {
        return samples[n - 1][1];
    }
    for (int i = 0; i < n - 1; i++) {
        if (aDeg >= samples[i][0] && aDeg <= samples[i + 1][0]) {
            float u = (aDeg - samples[i][0]) / (samples[i + 1][0] - samples[i][0]);
            u = u * u * (3.0f - 2.0f * u);
            return samples[i][1] + (samples[i + 1][1] - samples[i][1]) * u;
        }
    }
    return wallDistanceFeet;
}

float Layout::wallHeightAtAngle(float /*angleRad*/) const {
    return wallHeightFeet / feetPerUnit;
}

Vector3 Layout::domeCenter() const {
    return Vector3(0.0f, 0.0f, plateZ() - domeCenterOffsetFeet / feetPerUnit);
}

float Layout::domeRoofYAtWorld(float /*x*/, float /*z*/) const {
    return 200.0f; // open air — unused
}

float Layout::domeRoofYAtRadius(float /*r*/) const {
    return 200.0f;
}

float Layout::maxRadiusFromHome(float angleRad) const {
    return wallRAtAngle(std::clamp(angleRad, -foulAngleRad(), foulAngleRad())) + 60.0f;
}

float Layout::clampRadiusInDome(float /*ang*/, float radius, float /*m*/) const {
    return std::max(0.0f, radius);
}

bool Layout::isCfScoreboardZone(float angleRad) const {
    while (angleRad > pi) {
        angleRad -= 2.0f * pi;
    }
    while (angleRad < -pi) {
        angleRad += 2.0f * pi;
    }
    return std::abs(angleRad) < 0.12f;
}

float Layout::seatDeckYAtRadius(float radiusFromHome, float angleRad) const {
    float r0 = bowlInnerRadius(angleRad);
    float past = radiusFromHome - r0;
    if (past < 0.0f || !isSeatingArc(angleRad)) {
        return 0.0f;
    }
    return bowlBaseHeight(angleRad) + past * 0.55f;
}

bool Layout::containInsideDome(Vector3& /*p*/, Vector3& /*v*/, float /*r*/) const {
    return false;
}

Vector3 Layout::fromHome(float radius, float angleRad, float y) const {
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
    return Vector3(0.0f, 8.0f, plateZ() - maxWallR() * 0.38f);
}

Vector3 Layout::scoreboardCenter() const {
    return fromHome(wallRAtAngle(0.0f) + 4.0f, 0.0f, 9.0f);
}

void Layout::polarFromHome(const Vector3& worldPos, float& radiusOut, float& angleRadOut) const {
    float dx = worldPos.x;
    float dz = worldPos.z - plateZ();
    radiusOut = std::sqrt(dx * dx + dz * dz);
    angleRadOut = std::atan2(dx, -dz);
}

float Layout::radiusFromHome(const Vector3& worldPos) const {
    float r = 0.0f;
    float a = 0.0f;
    polarFromHome(worldPos, r, a);
    (void)a;
    return r;
}

bool Layout::isSeatingArc(float ang) const {
    while (ang > pi) {
        ang -= 2.0f * pi;
    }
    while (ang < -pi) {
        ang += 2.0f * pi;
    }
    // Open OF: no seats in fair outfield arc. Horseshoe = everything else.
    float fa = foulAngleRad();
    return std::abs(ang) > fa + 0.04f;
}

float Layout::bowlInnerRadius(float ang) const {
    while (ang > pi) {
        ang -= 2.0f * pi;
    }
    while (ang < -pi) {
        ang += 2.0f * pi;
    }
    float fa = foulAngleRad();
    float absA = std::abs(ang);
    if (absA <= fa + 0.04f) {
        // Not used for seats (open OF) — return past wall for safety.
        return wallRAtAngle(ang) + 2.0f;
    }
    // Parallel to foul line, then backstop behind home.
    const float clearance = 5.5f;
    const float backMin = 16.0f;
    float delta = std::max(absA - fa, 1e-3f);
    float foulPoleR = wallRAtAngle(ang >= 0.0f ? fa : -fa) + 1.2f;
    float rParallel = clearance / std::sin(std::min(delta, 1.4f));
    float r = std::min(rParallel, foulPoleR);
    return std::clamp(r, backMin, foulPoleR);
}

float Layout::bowlBaseHeight(float ang) const {
    while (ang > pi) {
        ang -= 2.0f * pi;
    }
    while (ang < -pi) {
        ang += 2.0f * pi;
    }
    float fa = foulAngleRad();
    if (std::abs(ang) <= fa + 0.05f) {
        return wallHeightAtAngle(ang) + 0.2f;
    }
    float u = std::clamp((std::abs(ang) - fa) / (pi - fa), 0.0f, 1.0f);
    float u2 = u * u * (3.0f - 2.0f * u);
    return 0.4f + u2 * 1.8f; // low near poles, slightly raised behind home
}

// ── Collision ─────────────────────────────────────────────────────────

namespace {

void killOutward(Vector3& vel, const Vector3& n) {
    float vn = vel.x * n.x + vel.y * n.y + vel.z * n.z;
    if (vn > 0.0f) {
        vel = vel - n * vn;
    }
}

} // namespace

BallCollisionHit collideBall(
    const Layout& layout,
    Vector3& position,
    Vector3& velocity,
    float radius,
    bool stickOnContact
) {
    BallCollisionHit hit;
    const float groundY = radius + 0.01f;
    const float fa = layout.foulAngleRad();

    float r = 0.0f;
    float ang = 0.0f;
    layout.polarFromHome(position, r, ang);
    hit.sprayDeg = ang * (180.0f / pi);
    bool fair = std::abs(ang) <= fa + 0.02f;
    hit.fenceFeet = layout.wallFeetAtAngle(ang);
    hit.wallTopY = layout.wallHeightAtAngle(ang);

    bool onGround = false;
    if (position.y < groundY) {
        position.y = groundY;
        onGround = true;
        hit.surface = HitSurface::Ground;
        hit.impactY = groundY;
        if (velocity.y < 0.0f) {
            velocity.y = -velocity.y * 0.38f;
            velocity.x *= 0.86f;
            velocity.z *= 0.86f;
        }
        if ((stickOnContact && velocity.magnitude() < 3.8f) || velocity.magnitude() < 2.6f) {
            velocity = Vector3();
            hit.stuck = true;
        }
    } else if (position.y < groundY + 0.12f && velocity.y <= 0.15f) {
        if (velocity.magnitude() < 3.2f || (stickOnContact && velocity.magnitude() < 4.5f)) {
            position.y = groundY;
            velocity = Vector3();
            hit.surface = HitSurface::Ground;
            hit.impactY = groundY;
            hit.stuck = true;
            onGround = true;
        }
    }

    // OF fence
    if (fair && r > 1.0f) {
        float wallR = layout.wallRAtAngle(ang);
        float wallH = layout.wallHeightAtAngle(ang);
        if (r + radius > wallR) {
            float clearY = wallH + std::max(radius, 0.3f);
            float past = r - wallR;
            Vector3 nOut(std::sin(ang), 0.0f, -std::cos(ang));
            if (onGround || hit.stuck) {
                // keep ground
            } else if (position.y > clearY || past > 0.7f) {
                hit.surface = HitSurface::FenceTopClear;
                hit.impactY = position.y;
            } else {
                Vector3 onWall = layout.fromHome(wallR - radius - 0.05f, ang, position.y);
                onWall.y = std::clamp(position.y, groundY, wallH - 0.02f);
                position = position * 0.45f + onWall * 0.55f;
                float vn = velocity.x * nOut.x + velocity.z * nOut.z;
                if (vn > 0.0f) {
                    velocity.x -= nOut.x * vn * 1.6f;
                    velocity.z -= nOut.z * vn * 1.6f;
                }
                velocity = velocity * 0.62f;
                velocity.y = std::min(velocity.y * 0.85f + 1.1f, 4.2f);
                hit.surface = HitSurface::Fence;
                hit.impactY = position.y;
            }
        }
    }

    // Backstop
    {
        float backR = 14.5f;
        if (std::abs(ang) > fa + 0.25f && r + radius > backR && position.y < 5.0f) {
            float behind = -std::cos(ang);
            if (behind > 0.35f) {
                Vector3 n(std::sin(ang), 0.0f, -std::cos(ang));
                Vector3 target = layout.fromHome(backR - radius - 0.05f, ang, position.y);
                target.y = std::max(target.y, groundY);
                position = position * 0.25f + target * 0.75f;
                killOutward(velocity, n);
                velocity = velocity * 0.5f;
                hit.surface = HitSurface::Backstop;
                hit.impactY = position.y;
            }
        }
    }

    // Stands face
    if (layout.isSeatingArc(ang)) {
        float rBowl = layout.bowlInnerRadius(ang) - 0.3f;
        float yTop = layout.bowlBaseHeight(ang) + 12.0f;
        if (r + radius > rBowl && position.y < yTop && position.y > -0.1f) {
            bool pastFence = !fair || r > layout.wallRAtAngle(ang) - 0.5f;
            if (pastFence || !fair) {
                Vector3 n(std::sin(ang), 0.0f, -std::cos(ang));
                Vector3 target = layout.fromHome(rBowl - radius - 0.05f, ang, position.y);
                target.y = std::max(target.y, groundY);
                position = position * 0.4f + target * 0.6f;
                float vn = velocity.x * n.x + velocity.z * n.z;
                if (vn > 0.0f) {
                    velocity.x -= n.x * vn * 1.35f;
                    velocity.z -= n.z * vn * 1.35f;
                }
                velocity = velocity * 0.55f;
                if (hit.surface == HitSurface::None || hit.surface == HitSurface::FenceTopClear) {
                    hit.surface = HitSurface::Stands;
                }
                hit.impactY = position.y;
            }
        }
    }

    return hit;
}

BallCollisionHit collideBallSubsteps(
    const Layout& layout,
    Vector3& position,
    Vector3& velocity,
    float radius,
    bool stickOnContact,
    int substeps
) {
    BallCollisionHit last;
    substeps = std::max(1, substeps);
    for (int i = 0; i < substeps; i++) {
        last = collideBall(layout, position, velocity, radius, stickOnContact);
        if (last.stuck) {
            break;
        }
    }
    return last;
}

WallClearResult evaluateWallClear(
    const Layout& layout,
    Vector3 position,
    Vector3 velocity,
    float gravityY,
    float dragK
) {
    WallClearResult out;
    const float dt = 1.0f / 120.0f;
    float prevR = layout.radiusFromHome(position);
    bool crossed = false;

    for (float t = 0.0f; t < 12.0f; t += dt) {
        float sp = velocity.magnitude();
        if (sp > 1e-4f) {
            velocity = velocity + (Vector3(0, gravityY, 0) + velocity * (-dragK * sp)) * dt;
        } else {
            velocity.y += gravityY * dt;
        }
        position = position + velocity * dt;
        float r = 0.0f;
        float ang = 0.0f;
        layout.polarFromHome(position, r, ang);
        bool stepFair = std::abs(ang * (180.0f / pi)) <= layout.foulAngleDegrees + 0.5f;
        float wallR = stepFair ? layout.wallRAtAngle(ang) : layout.maxWallR() * 1.5f;
        if (stepFair && prevR < wallR && r >= wallR) {
            out.sprayDeg = ang * (180.0f / pi);
            out.fenceFeet = layout.wallFeetAtAngle(ang);
            out.wallTopY = layout.wallHeightAtAngle(ang);
            out.heightAtFence = position.y;
            out.marginFeet = (position.y - out.wallTopY) * layout.feetPerUnit;
            out.fair = true;
            out.clearsWall = position.y > out.wallTopY;
            out.hitsWallFace = !out.clearsWall;
            crossed = true;
            break;
        }
        prevR = r;
        if (position.y < 0.05f && velocity.y <= 0.0f) {
            break;
        }
    }
    float endR = 0.0f;
    float endA = 0.0f;
    layout.polarFromHome(position, endR, endA);
    out.landFeet = std::max(0.0f, endR * layout.feetPerUnit);
    if (!crossed) {
        out.sprayDeg = endA * (180.0f / pi);
        out.fair = std::abs(out.sprayDeg) <= layout.foulAngleDegrees + 0.5f;
        out.fenceFeet = layout.wallFeetAtAngle(endA);
        out.wallTopY = layout.wallHeightAtAngle(endA);
    }
    return out;
}

Layout defaultPlayLayout() {
    Layout L;
    L.wallDistanceFeet = 400.0f;
    L.wallHeightFeet = 8.0f;
    L.closedDome = false;
    L.buildingRadiusFeet = 280.0f;
    L.domeCenterOffsetFeet = 140.0f;
    return L;
}

Meshes build(const Layout& layout) {
    Meshes out;
    out.field = buildField(layout);
    out.walls = buildWalls(layout);
    out.stands = buildStands(layout);
    out.lines = buildLines(layout);
    out.city = buildCity(layout);
    out.scoreboardScreen = buildScoreboardScreen(layout);
    out.hotel = buildHotel(layout);
    out.structure = buildStructure(layout);
    out.fanSectors = buildFanSectors(layout);
    buildFlags(layout, out.flagMeshes, out.flagBases);
    // skyDome / clouds empty (open air)
    return out;
}

float recommendedFarPlane(const Layout& layout) {
    return std::max(1400.0f, layout.maxWallR() * 8.0f + 300.0f);
}

float fanCheerOffsetY(int sectorIndex, float timeSec, float boost) {
    float s = static_cast<float>(sectorIndex);
    float seed = hash01(sectorIndex * 47 + 13);
    float b = std::max(0.35f, boost);
    float period = 0.6f + seed * 1.2f;
    float tHop = std::fmod(timeSec + seed * 3.0f + s * 0.3f, period);
    float hop = (tHop < 0.14f) ? std::sin((tHop / 0.14f) * pi) : 0.0f;
    return (0.03f * std::sin(timeSec * 2.5f + s) + hop * 0.15f) * b;
}

float fanCheerOffsetX(int sectorIndex, float timeSec, float boost) {
    float s = static_cast<float>(sectorIndex);
    float seed = hash01(sectorIndex * 53 + 19);
    return 0.04f * std::sin(timeSec * (1.8f + seed * 2.0f) + s * 2.0f) * std::max(0.35f, boost);
}

float flagSwayYaw(int flagIndex, float timeSec) {
    return 0.2f * std::sin(timeSec * 2.3f + flagIndex * 0.9f);
}

float scoreboardPulse(float timeSec, float excitement) {
    float base = 0.55f + 0.2f * std::sin(timeSec * 3.2f);
    float pop = excitement > 0.01f ? 0.3f * std::sin(timeSec * 11.0f) * excitement : 0.0f;
    return std::clamp(base + pop, 0.3f, 1.0f);
}

} // namespace Stadium3D
