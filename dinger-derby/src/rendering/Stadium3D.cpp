#include "Stadium3D.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Masterpiece open-air minor-league ballpark (Fredericksburg-class).
// One continuous structure: diamond field, blue horseshoe bowl, red club
// deck, OF bleachers + wall, suite facade, light towers, exterior apron.
// Dense crowds in bowl and outfield. No closed dome.

namespace Stadium3D {
namespace {

constexpr float pi = 3.1415926535f;

float hash01(int n) {
    unsigned x = static_cast<unsigned>(n) * 747796405u + 2891336453u;
    x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    x = (x >> 22u) ^ x;
    return static_cast<float>(x) / 4294967295.0f;
}

sf::Color shade(sf::Color c, float mul) {
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
    Mesh3D& m, const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, sf::Color col
) {
    addTri(m, a, b, c, col);
    addTri(m, a, c, d, col);
}

void addBox(Mesh3D& m, const Vector3& center, float w, float h, float d, sf::Color col) {
    float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;
    Vector3 p[8] = {
        center + Vector3(-hw, -hh, -hd), center + Vector3(hw, -hh, -hd),
        center + Vector3(hw, -hh, hd),   center + Vector3(-hw, -hh, hd),
        center + Vector3(-hw, hh, -hd),  center + Vector3(hw, hh, -hd),
        center + Vector3(hw, hh, hd),    center + Vector3(-hw, hh, hd),
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
            m, Vector3(c.x, y, c.z),
            Vector3(c.x + std::cos(a0) * r, y, c.z + std::sin(a0) * r),
            Vector3(c.x + std::cos(a1) * r, y, c.z + std::sin(a1) * r), col
        );
    }
}

void addRing(
    Mesh3D& m, const Vector3& c, float r0, float r1, float y, int segs, sf::Color col
) {
    for (int i = 0; i < segs; i++) {
        float a0 = (static_cast<float>(i) / segs) * 2.0f * pi;
        float a1 = (static_cast<float>(i + 1) / segs) * 2.0f * pi;
        Vector3 p0(c.x + std::cos(a0) * r0, y, c.z + std::sin(a0) * r0);
        Vector3 p1(c.x + std::cos(a1) * r0, y, c.z + std::sin(a1) * r0);
        Vector3 p2(c.x + std::cos(a1) * r1, y, c.z + std::sin(a1) * r1);
        Vector3 p3(c.x + std::cos(a0) * r1, y, c.z + std::sin(a0) * r1);
        addQuad(m, p0, p1, p2, p3, col);
    }
}

void addPath(
    Mesh3D& m, const Vector3& a, const Vector3& b, float halfW, float y, sf::Color col
) {
    Vector3 d = b - a;
    float len = std::sqrt(d.x * d.x + d.z * d.z);
    if (len < 1e-4f) {
        return;
    }
    Vector3 n(-d.z / len * halfW, 0.0f, d.x / len * halfW);
    Vector3 a0 = a + n, a1 = a - n, b0 = b + n, b1 = b - n;
    a0.y = a1.y = b0.y = b1.y = y;
    addQuad(m, a0, b0, b1, a1, col);
}

void wrapAng(float& ang) {
    while (ang > pi) {
        ang -= 2.0f * pi;
    }
    while (ang < -pi) {
        ang += 2.0f * pi;
    }
}

// Smooth seating profile used by stands, fans, exterior, collision.
float seatInnerR(const Layout& L, float ang) {
    wrapAng(ang);
    float fa = L.foulAngleRad();
    float absA = std::abs(ang);
    // Fair OF: seats sit just outside wall (OF bleachers)
    if (absA <= fa + 0.02f) {
        return L.wallRAtAngle(ang) + 2.8f;
    }
    const float clearance = 6.0f;
    const float backMin = 17.0f;
    float delta = std::max(absA - fa, 1e-3f);
    float foulPoleR = L.wallRAtAngle(ang >= 0.0f ? fa : -fa) + 2.8f;
    float rPar = clearance / std::sin(std::min(delta, 1.45f));
    return std::clamp(std::min(rPar, foulPoleR), backMin, foulPoleR);
}

float seatBaseY(const Layout& L, float ang) {
    wrapAng(ang);
    float fa = L.foulAngleRad();
    float absA = std::abs(ang);
    if (absA <= fa + 0.05f) {
        return L.wallHeightAtAngle(ang) + 0.35f; // OF bleacher deck
    }
    float u = std::clamp((absA - fa) / (pi - fa), 0.0f, 1.0f);
    float u2 = u * u * (3.0f - 2.0f * u);
    return 0.35f + u2 * 2.0f;
}

// Full seating arc: horseshoe + OF bleachers (entire 360° except tiny CF board gap)
bool inSeatArc(const Layout& L, float ang) {
    wrapAng(ang);
    // Leave narrow CF board gap for scoreboard
    if (std::abs(ang) < 0.10f) {
        return false;
    }
    return true;
}

bool isOfBleacher(const Layout& L, float ang) {
    wrapAng(ang);
    return std::abs(ang) <= L.foulAngleRad() + 0.02f && std::abs(ang) >= 0.10f;
}

bool isClubZone(float ang) {
    wrapAng(ang);
    return std::abs(ang) > 1.65f; // behind home
}

bool isCornerRed(const Layout& L, float ang) {
    wrapAng(ang);
    float fa = L.foulAngleRad();
    float absA = std::abs(ang);
    return (absA > fa + 0.02f && absA < fa + 0.70f) ||
           (absA > 0.10f && absA < fa * 0.55f); // OF corners
}

// ═══════════════════════════════════════════════════════════════════════
// FIELD
// ═══════════════════════════════════════════════════════════════════════

Mesh3D buildField(const Layout& L) {
    Mesh3D m;
    const float aL = -L.foulAngleRad();
    const float aR = L.foulAngleRad();
    const float bp = L.basePath();
    const Vector3 home = L.home();
    const Vector3 b1 = L.firstBase(), b2 = L.secondBase(), b3 = L.thirdBase();
    const sf::Color grass = grassColor();
    const sf::Color gDk = grassDarkColor();
    const sf::Color dirt = dirtColor();
    const sf::Color dDk(148, 98, 54);
    const sf::Color track = warningTrackColor();
    const float trackW = 3.4f;

    // Fair grass pie + stripes + warning track
    const int fairSegs = 96;
    for (int i = 0; i < fairSegs; i++) {
        float t0 = static_cast<float>(i) / fairSegs;
        float t1 = static_cast<float>(i + 1) / fairSegs;
        float ang0 = aL + (aR - aL) * t0;
        float ang1 = aL + (aR - aL) * t1;
        float rFence0 = L.wallRAtAngle(ang0);
        float rFence1 = L.wallRAtAngle(ang1);
        float rGrass0 = rFence0 - trackW;
        float rGrass1 = rFence1 - trackW;

        addQuad(
            m, L.fromHome(0.25f, ang0, 0.0f), L.fromHome(0.25f, ang1, 0.0f),
            L.fromHome(rGrass1, ang1, 0.0f), L.fromHome(rGrass0, ang0, 0.0f), grass
        );

        // Dense mowing stripes
        for (int s = 0; s < 12; s++) {
            float u0 = 0.22f + s * 0.06f;
            float u1 = u0 + 0.028f;
            float ri0 = std::max(bp * 1.15f, rGrass0 * u0);
            float ri1 = std::max(bp * 1.15f, rGrass1 * u0);
            float ro0 = std::min(rGrass0 - 0.3f, rGrass0 * u1);
            float ro1 = std::min(rGrass1 - 0.3f, rGrass1 * u1);
            if (ro0 <= ri0 + 0.25f) {
                continue;
            }
            sf::Color sc = (s % 2 == 0) ? gDk : shade(grass, 0.97f);
            addQuad(
                m, L.fromHome(ri0, ang0, 0.008f), L.fromHome(ri1, ang1, 0.008f),
                L.fromHome(ro1, ang1, 0.008f), L.fromHome(ro0, ang0, 0.008f), sc
            );
        }

        // Warning track bands
        for (int b = 0; b < 3; b++) {
            float u0 = static_cast<float>(b) / 3.0f;
            float u1 = static_cast<float>(b + 1) / 3.0f;
            addQuad(
                m,
                L.fromHome(rGrass0 + trackW * u0, ang0, 0.014f),
                L.fromHome(rGrass1 + trackW * u0, ang1, 0.014f),
                L.fromHome(rGrass1 + trackW * u1, ang1, 0.014f),
                L.fromHome(rGrass0 + trackW * u0 + trackW * (u1 - u0), ang0, 0.014f),
                shade(track, (b % 2) ? 0.93f : 1.04f)
            );
        }
        // Fix track outer edge
        addQuad(
            m, L.fromHome(rGrass0, ang0, 0.014f), L.fromHome(rGrass1, ang1, 0.014f),
            L.fromHome(rFence1 - 0.12f, ang1, 0.014f), L.fromHome(rFence0 - 0.12f, ang0, 0.014f),
            track
        );
    }

    // Foul-territory grass: continuous fill from foul lines out to first seats
    const int foulSegs = 100;
    for (int i = 0; i < foulSegs; i++) {
        float t0 = static_cast<float>(i) / foulSegs;
        float t1 = static_cast<float>(i + 1) / foulSegs;
        float ang0 = -pi + t0 * 2.0f * pi;
        float ang1 = -pi + t1 * 2.0f * pi;
        float angM = 0.5f * (ang0 + ang1);
        wrapAng(angM);
        if (std::abs(angM) <= L.foulAngleRad() + 0.02f) {
            continue; // fair handled above
        }
        float rSeat = seatInnerR(L, angM) - 0.4f;
        float rIn = 2.0f;
        // Near foul lines start from the line
        float delta = std::abs(angM) - L.foulAngleRad();
        if (delta < 0.5f) {
            rIn = 3.0f;
        }
        addQuad(
            m, L.fromHome(rIn, ang0, 0.002f), L.fromHome(rIn, ang1, 0.002f),
            L.fromHome(rSeat, ang1, 0.002f), L.fromHome(rSeat, ang0, 0.002f),
            shade(grass, 0.90f + 0.04f * hash01(i))
        );
    }

    // Dirt diamond (paths + skin)
    {
        Vector3 pts[4] = {home, b1, b2, b3};
        for (int i = 0; i < 4; i++) {
            addPath(m, pts[i], pts[(i + 1) % 4], 1.6f, 0.02f, dirt);
            addPath(m, pts[i], pts[(i + 1) % 4], 0.32f, 0.022f, dDk);
        }
        // Filled dirt diamond
        addQuad(
            m, home + Vector3(0, 0.011f, 0), b1 + Vector3(0, 0.011f, 0),
            b2 + Vector3(0, 0.011f, 0), b3 + Vector3(0, 0.011f, 0), shade(dirt, 0.98f)
        );
        // Inner grass diamond
        Vector3 ih = home + (b2 - home) * 0.20f;
        Vector3 i1 = b1 + (b2 - b1) * 0.18f + (home - b1) * 0.12f;
        Vector3 i2 = b2 + (home - b2) * 0.14f;
        Vector3 i3 = b3 + (b2 - b3) * 0.18f + (home - b3) * 0.12f;
        ih.y = i1.y = i2.y = i3.y = 0.016f;
        addQuad(m, ih, i1, i2, i3, grass);
        // Arc dirt between 1B-2B-3B (skinned infield curve)
        for (int i = 0; i < 24; i++) {
            float t0 = static_cast<float>(i) / 24.0f;
            float t1 = static_cast<float>(i + 1) / 24.0f;
            float ang0 = aL + (aR - aL) * t0;
            float ang1 = aL + (aR - aL) * t1;
            float rIn = bp * 1.05f;
            float rOut = bp * 1.42f;
            addQuad(
                m, L.fromHome(rIn, ang0, 0.013f), L.fromHome(rIn, ang1, 0.013f),
                L.fromHome(rOut, ang1, 0.013f), L.fromHome(rOut, ang0, 0.013f),
                shade(dirt, 0.96f)
            );
        }
    }

    // Mound, home circle, bases
    addDisk(m, L.mound(), 4.4f, 0.024f, 32, dirt);
    addRing(m, L.mound(), 2.2f, 4.4f, 0.026f, 28, shade(dirt, 0.94f));
    addDisk(m, L.mound(), 2.2f, 0.028f, 24, dDk);
    addBox(m, L.mound() + Vector3(0, 0.14f, 0), 1.6f, 0.26f, 2.4f, shade(dirt, 1.06f));
    addDisk(m, home + Vector3(0, 0, -0.2f), 5.8f, 0.023f, 32, dirt);
    addDisk(m, b1, 2.4f, 0.025f, 20, dirt);
    addDisk(m, b2, 2.4f, 0.025f, 20, dirt);
    addDisk(m, b3, 2.4f, 0.025f, 20, dirt);
    auto bag = [&](Vector3 p) {
        addBox(m, p + Vector3(0, 0.07f, 0), 0.88f, 0.09f, 0.88f, sf::Color(248, 248, 245));
    };
    bag(b1);
    bag(b2);
    bag(b3);
    // Home plate
    {
        float pz = L.plateZ();
        Vector3 tip(0, 0.045f, pz + 0.55f);
        Vector3 bl(-0.55f, 0.045f, pz - 0.35f), br(0.55f, 0.045f, pz - 0.35f);
        Vector3 fl(-0.55f, 0.045f, pz + 0.12f), fr(0.55f, 0.045f, pz + 0.12f);
        addTri(m, tip, fl, fr, sf::Color(252, 252, 250));
        addQuad(m, fl, fr, br, bl, sf::Color(252, 252, 250));
    }
    // Batter boxes
    addBox(m, Vector3(-1.7f, 0.03f, L.plateZ() - 0.2f), 2.1f, 0.02f, 3.1f, shade(dDk, 1.05f));
    addBox(m, Vector3(1.7f, 0.03f, L.plateZ() - 0.2f), 2.1f, 0.02f, 3.1f, shade(dDk, 1.05f));

    m.rebuildNormals();
    return m;
}

// ═══════════════════════════════════════════════════════════════════════
// WALLS + DUGOUTS + FOUL POLES
// ═══════════════════════════════════════════════════════════════════════

Mesh3D buildWalls(const Layout& L) {
    Mesh3D m;
    const float aL = -L.foulAngleRad();
    const float aR = L.foulAngleRad();
    const int segs = 72;
    sf::Color face = ofWallColor();
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
        sf::Color fc = shade(face, 0.92f + 0.12f * ((i % 4) / 3.0f));
        // Field face
        addQuad(
            m, L.fromHome(r0, ang0, 0.0f), L.fromHome(r1, ang1, 0.0f),
            L.fromHome(r1, ang1, h1), L.fromHome(r0, ang0, h0), fc
        );
        // Thickness + outside face (connects to bleachers)
        addQuad(
            m, L.fromHome(r0 + 1.4f, ang0, 0.0f), L.fromHome(r0, ang0, 0.0f),
            L.fromHome(r0, ang0, h0), L.fromHome(r0 + 1.4f, ang0, h0), shade(face, 0.8f)
        );
        addQuad(
            m, L.fromHome(r0, ang0, h0), L.fromHome(r1, ang1, h1),
            L.fromHome(r1 + 1.4f, ang1, h1), L.fromHome(r0 + 1.4f, ang0, h0), top
        );
        // Padding panel dividers
        if (i % 3 == 0) {
            addBox(
                m, L.fromHome(r0 + 0.08f, ang0, h0 * 0.5f), 0.1f, h0 * 0.92f, 0.1f,
                shade(face, 0.75f)
            );
        }
    }

    // Foul poles (tall, with screen wings)
    auto pole = [&](float ang) {
        float r = L.wallRAtAngle(ang);
        float h = L.wallHeightAtAngle(ang) * 4.0f;
        Vector3 base = L.fromHome(r, ang, 0.0f);
        addBox(m, base + Vector3(0, h * 0.5f, 0), 0.5f, h, 0.5f, sf::Color(220, 50, 48));
        addBox(m, base + Vector3(0, h + 0.5f, 0), 1.4f, 0.55f, 0.18f, sf::Color(245, 245, 248));
        // Screen wing into foul
        float foulSign = ang >= 0.0f ? 1.0f : -1.0f;
        for (int k = 1; k <= 6; k++) {
            float a = ang + foulSign * 0.04f * k;
            float rr = r + k * 0.8f;
            addBox(
                m, L.fromHome(rr, a, h * 0.45f), 0.12f, h * 0.75f, 0.12f,
                sf::Color(180, 190, 200, 160)
            );
        }
    };
    pole(aL);
    pole(aR);

    // Connected dugouts (1B / 3B) — sunk boxes under wall line
    auto dugout = [&](float xSign) {
        float z = L.plateZ() - 10.0f;
        float x = xSign * 16.0f;
        addBox(m, Vector3(x, 1.1f, z), 12.0f, 2.4f, 5.0f, ofWallColor());
        addBox(m, Vector3(x, 2.45f, z), 12.6f, 0.3f, 5.4f, facadeGrayColor());
        addBox(m, Vector3(x, 0.4f, z + 1.5f), 11.0f, 0.8f, 1.2f, shade(ofWallColor(), 0.85f));
        // Rail
        addBox(m, Vector3(x, 2.7f, z + 2.4f), 12.0f, 0.15f, 0.15f, railColor());
    };
    dugout(1.0f);
    dugout(-1.0f);

    m.rebuildNormals();
    return m;
}

// ═══════════════════════════════════════════════════════════════════════
// STANDS — continuous bowl + OF bleachers + club deck
// ═══════════════════════════════════════════════════════════════════════

Mesh3D buildStands(const Layout& L) {
    Mesh3D m;
    const int angSegs = 200;
    const float dRow = 1.35f;
    const float rise = 0.88f;
    sf::Color blueA = seatBlueColor(), blueB = seatBlueAltColor();
    sf::Color redA = seatRedColor(), redB = seatRedAltColor();
    sf::Color riserBlue(30, 50, 95);
    sf::Color riserRed(90, 30, 35);
    sf::Color conc = concourseColor();
    sf::Color aisle(70, 78, 92);

    for (int i = 0; i < angSegs; i++) {
        float t0 = static_cast<float>(i) / angSegs;
        float t1 = static_cast<float>(i + 1) / angSegs;
        float ang0 = -pi + t0 * 2.0f * pi;
        float ang1 = -pi + t1 * 2.0f * pi;
        float angM = 0.5f * (ang0 + ang1);
        if (!inSeatArc(L, angM)) {
            continue;
        }

        float rIn = seatInnerR(L, angM);
        float yBase = seatBaseY(L, angM);
        bool ofBleach = isOfBleacher(L, angM);
        bool club = isClubZone(angM);
        bool corner = isCornerRed(L, angM);
        bool isAisle = (i % 10) == 0;

        // OF bleachers: fewer rows, low; horseshoe: deep lower bowl
        int rowsLower = ofBleach ? 8 : (club ? 16 : 15);
        float r = rIn;
        float y = yBase;

        for (int row = 0; row < rowsLower; row++) {
            float r1 = r + dRow * 0.92f;
            float y1 = y + rise * 0.82f;
            bool useRed = corner || (ofBleach && row < 3);
            sf::Color sc;
            if (isAisle) {
                sc = aisle;
            } else if (useRed) {
                sc = (row + i) % 2 ? redA : redB;
            } else {
                sc = (row + i) % 2 ? blueA : blueB;
            }
            sc = shade(sc, 0.93f + 0.07f * hash01(i * 5 + row));
            addQuad(
                m, L.fromHome(r, ang0, y1), L.fromHome(r, ang1, y1),
                L.fromHome(r1, ang1, y1), L.fromHome(r1, ang0, y1), sc
            );
            addQuad(
                m, L.fromHome(r, ang0, y), L.fromHome(r, ang1, y),
                L.fromHome(r, ang1, y1), L.fromHome(r, ang0, y1),
                useRed ? riserRed : riserBlue
            );
            r = r1 + dRow * 0.05f;
            y += rise;
        }

        // Concourse deck (continuous ring)
        float rC0 = r + 0.3f;
        float rC1 = r + (ofBleach ? 2.8f : 4.2f);
        float yC = y + 0.15f;
        addQuad(
            m, L.fromHome(rC0, ang0, yC), L.fromHome(rC0, ang1, yC),
            L.fromHome(rC1, ang1, yC), L.fromHome(rC1, ang0, yC), conc
        );
        // Fascia wall under concourse (connects structure)
        addQuad(
            m, L.fromHome(rC0, ang0, yBase), L.fromHome(rC0, ang1, yBase),
            L.fromHome(rC0, ang1, yC), L.fromHome(rC0, ang0, yC), shade(facadeGrayColor(), 0.9f)
        );

        // Club / suite upper deck behind home
        if (club) {
            float rU = rC1 + 0.6f;
            float yU = yC + 3.2f;
            // Mid facade
            addQuad(
                m, L.fromHome(rC1, ang0, yC), L.fromHome(rC1, ang1, yC),
                L.fromHome(rC1, ang1, yU), L.fromHome(rC1, ang0, yU),
                shade(ofWallColor(), 1.1f)
            );
            for (int row = 0; row < 7; row++) {
                float r1 = rU + dRow * 0.88f;
                float y1 = yU + rise * 0.78f;
                sf::Color sc = (row + i) % 2 ? redA : redB;
                addQuad(
                    m, L.fromHome(rU, ang0, y1), L.fromHome(rU, ang1, y1),
                    L.fromHome(r1, ang1, y1), L.fromHome(r1, ang0, y1), sc
                );
                addQuad(
                    m, L.fromHome(rU, ang0, yU), L.fromHome(rU, ang1, yU),
                    L.fromHome(rU, ang1, y1), L.fromHome(rU, ang0, y1), riserRed
                );
                rU = r1 + 0.06f;
                yU += rise;
            }
            // Upper roof slab
            addQuad(
                m, L.fromHome(rC1, ang0, yC + 3.0f), L.fromHome(rC1, ang1, yC + 3.0f),
                L.fromHome(rU + 2.0f, ang1, yC + 3.0f), L.fromHome(rU + 2.0f, ang0, yC + 3.0f),
                facadeGrayColor()
            );
            // Glass rail
            addQuad(
                m, L.fromHome(rC1 + 0.1f, ang0, yC + 3.5f),
                L.fromHome(rC1 + 0.1f, ang1, yC + 3.5f),
                L.fromHome(rC1 + 0.1f, ang1, yC + 5.2f),
                L.fromHome(rC1 + 0.1f, ang0, yC + 5.2f),
                sf::Color(70, 150, 200, 140)
            );
        }

        // Outer apron under seats (connects to exterior)
        float rOut = rC1 + (ofBleach ? 6.0f : 10.0f);
        addQuad(
            m, L.fromHome(rC1, ang0, 0.0f), L.fromHome(rC1, ang1, 0.0f),
            L.fromHome(rOut, ang1, -0.5f), L.fromHome(rOut, ang0, -0.5f),
            facadeTanColor()
        );
    }

    // Continuous backstop wall + net posts
    {
        float backR = 15.5f;
        for (int i = 0; i < 36; i++) {
            float t0 = static_cast<float>(i) / 36.0f;
            float t1 = static_cast<float>(i + 1) / 36.0f;
            float ang0 = pi - 1.15f + t0 * 2.3f;
            float ang1 = pi - 1.15f + t1 * 2.3f;
            addQuad(
                m, L.fromHome(backR, ang0, 0.0f), L.fromHome(backR, ang1, 0.0f),
                L.fromHome(backR, ang1, 5.0f), L.fromHome(backR, ang0, 5.0f),
                sf::Color(65, 80, 95)
            );
            if (i % 3 == 0) {
                addBox(
                    m, L.fromHome(backR + 0.15f, ang0, 7.0f), 0.15f, 14.0f, 0.15f,
                    sf::Color(170, 175, 180)
                );
            }
        }
        // Net mesh suggestion (thin quads)
        for (int i = 0; i < 20; i++) {
            float t0 = static_cast<float>(i) / 20.0f;
            float t1 = static_cast<float>(i + 1) / 20.0f;
            float ang0 = pi - 1.0f + t0 * 2.0f;
            float ang1 = pi - 1.0f + t1 * 2.0f;
            addQuad(
                m, L.fromHome(backR + 0.2f, ang0, 5.0f), L.fromHome(backR + 0.2f, ang1, 5.0f),
                L.fromHome(backR + 0.2f, ang1, 14.0f), L.fromHome(backR + 0.2f, ang0, 14.0f),
                sf::Color(190, 200, 210, 50)
            );
        }
    }

    m.rebuildNormals();
    return m;
}

// ═══════════════════════════════════════════════════════════════════════
// SUITE / PRESS FACADE
// ═══════════════════════════════════════════════════════════════════════

Mesh3D buildHotel(const Layout& L) {
    Mesh3D m;
    float z = L.plateZ() + 30.0f;
    // Main press / suite block (connects into bowl)
    addBox(m, Vector3(0, 11.0f, z), 48.0f, 10.0f, 14.0f, facadeGrayColor());
    addBox(m, Vector3(0, 16.0f, z - 0.5f), 46.0f, 2.5f, 0.5f, ofWallColor());
    // Glass bands
    for (int row = 0; row < 3; row++) {
        addBox(
            m, Vector3(0, 9.0f + row * 2.8f, z - 6.8f), 44.0f, 1.8f, 0.3f,
            sf::Color(90, 170, 220, 170)
        );
    }
    // Side wings continuous with exterior
    addBox(m, Vector3(32.0f, 8.5f, z - 2.0f), 18.0f, 12.0f, 18.0f, facadeTanColor());
    addBox(m, Vector3(-32.0f, 8.5f, z - 2.0f), 18.0f, 12.0f, 18.0f, facadeTanColor());
    addBox(m, Vector3(38.0f, 6.0f, z - 12.0f), 14.0f, 9.0f, 16.0f, facadeGrayColor());
    addBox(m, Vector3(-38.0f, 6.0f, z - 12.0f), 14.0f, 9.0f, 16.0f, facadeGrayColor());
    // Entry canopies
    addBox(m, Vector3(20.0f, 4.5f, z + 6.0f), 10.0f, 0.4f, 6.0f, facadeGrayColor());
    addBox(m, Vector3(-20.0f, 4.5f, z + 6.0f), 10.0f, 0.4f, 6.0f, facadeGrayColor());

    m.rebuildNormals();
    return m;
}

// ═══════════════════════════════════════════════════════════════════════
// SCOREBOARD + OF BOARDS
// ═══════════════════════════════════════════════════════════════════════

Mesh3D buildScoreboardScreen(const Layout& L) {
    Mesh3D m;
    // Continuous blue OF board ribbon
    const int n = 18;
    for (int i = 0; i < n; i++) {
        float t = (static_cast<float>(i) + 0.5f) / n;
        float ang = -L.foulAngleRad() * 0.92f + t * L.foulAngleRad() * 1.84f;
        if (std::abs(ang) < 0.08f) {
            continue; // CF board gap
        }
        float r = L.wallRAtAngle(ang) + 0.55f;
        float h = L.wallHeightAtAngle(ang) * 0.52f;
        addBox(
            m, L.fromHome(r, ang, h), 5.2f, L.wallH() * 0.78f, 0.4f,
            shade(ofWallColor(), 0.95f + 0.08f * (i % 3) / 2.0f)
        );
    }
    // Main CF scoreboard stack
    {
        float r = L.wallRAtAngle(0.0f) + 5.5f;
        Vector3 c = L.fromHome(r, 0.0f, 10.0f);
        addBox(m, c, 22.0f, 11.0f, 2.0f, facadeGrayColor());
        addBox(m, c + Vector3(0, 0.2f, -1.1f), 19.0f, 9.0f, 0.4f, sf::Color(20, 45, 90));
        // LED strips
        for (int row = -3; row <= 3; row++) {
            addBox(
                m, c + Vector3(0, row * 1.1f, -1.25f), 17.5f, 0.7f, 0.12f,
                sf::Color(30, 80, 160)
            );
        }
        addBox(m, c + Vector3(0, 6.2f, 0.3f), 23.0f, 0.5f, 3.0f, facadeTanColor());
        // Support posts
        addBox(m, c + Vector3(-8, -6, 0.5f), 0.6f, 8.0f, 0.6f, facadeGrayColor());
        addBox(m, c + Vector3(8, -6, 0.5f), 0.6f, 8.0f, 0.6f, facadeGrayColor());
    }
    // Corner sign plates (abstract)
    for (float side : {1.0f, -1.0f}) {
        float ang = side * L.foulAngleRad() * 0.75f;
        Vector3 c = L.fromHome(L.wallRAtAngle(ang) + 3.5f, ang, 5.8f);
        addBox(m, c, 11.0f, 5.0f, 0.55f, sf::Color(248, 248, 250));
        addBox(m, c + Vector3(0, 0, -0.35f), 9.0f, 2.4f, 0.15f, seatRedColor());
    }
    m.rebuildNormals();
    return m;
}

// ═══════════════════════════════════════════════════════════════════════
// LIGHTS + RAILS + BULLPENS
// ═══════════════════════════════════════════════════════════════════════

Mesh3D buildStructure(const Layout& L) {
    Mesh3D m;
    sf::Color pole(235, 235, 230);
    sf::Color lamp(255, 252, 235);

    auto tower = [&](float ang, float r, float h) {
        Vector3 base = L.fromHome(r, ang, 0.0f);
        addBox(m, base + Vector3(0, h * 0.5f, 0), 0.5f, h, 0.5f, pole);
        // Ladder suggestion
        addBox(m, base + Vector3(0.35f, h * 0.5f, 0), 0.12f, h * 0.9f, 0.12f, shade(pole, 0.85f));
        addBox(m, base + Vector3(0, h, 0), 6.5f, 0.4f, 0.4f, pole);
        addBox(m, base + Vector3(0, h - 1.4f, 0), 5.5f, 0.35f, 0.35f, pole);
        for (int k = -2; k <= 2; k++) {
            for (int row = 0; row < 2; row++) {
                addBox(
                    m,
                    base + Vector3(static_cast<float>(k) * 1.2f, h + 0.55f + row * 0.9f, 0),
                    0.85f, 0.55f, 0.55f, lamp
                );
            }
        }
    };

    // Towers around full perimeter (connected park lighting)
    const float tw[][3] = {
        {2.55f, 48.0f, 32.0f},  {-2.55f, 48.0f, 32.0f},
        {2.1f, 70.0f, 34.0f},   {-2.1f, 70.0f, 34.0f},
        {1.55f, 100.0f, 36.0f}, {-1.55f, 100.0f, 36.0f},
        {1.0f, 140.0f, 34.0f},  {-1.0f, 140.0f, 34.0f},
        {0.45f, 175.0f, 30.0f}, {-0.45f, 175.0f, 30.0f},
        {0.25f, 190.0f, 28.0f}, {-0.25f, 190.0f, 28.0f},
        {2.9f, 38.0f, 30.0f},   {-2.9f, 38.0f, 30.0f},
    };
    for (const auto& t : tw) {
        tower(t[0], t[1], t[2]);
    }

    // Bullpen sheds LF/RF
    for (float side : {1.0f, -1.0f}) {
        float ang = side * 0.35f;
        Vector3 c = L.fromHome(L.wallRAtAngle(ang) - 10.0f, ang, 1.3f);
        addBox(m, c, 7.0f, 2.6f, 5.0f, facadeGrayColor());
        addBox(m, c + Vector3(0, 1.5f, 0), 7.4f, 0.3f, 5.4f, facadeTanColor());
    }

    // Railings on wall + concourse
    for (int i = 0; i < 40; i++) {
        float t = (static_cast<float>(i) + 0.5f) / 40.0f;
        float ang = -L.foulAngleRad() + t * 2.0f * L.foulAngleRad();
        float r = L.wallRAtAngle(ang) + 0.55f;
        float h = L.wallHeightAtAngle(ang) + 0.9f;
        addBox(m, L.fromHome(r, ang, h), 0.1f, 1.5f, 0.1f, railColor());
    }

    m.rebuildNormals();
    return m;
}

// ═══════════════════════════════════════════════════════════════════════
// EXTERIOR — seamless ground cover + dense suburb (no bare spots)
// ═══════════════════════════════════════════════════════════════════════

void addTree(Mesh3D& m, const Vector3& base, float scale, int seed) {
    float trunkH = 2.2f * scale;
    float canopyR = 2.4f * scale + hash01(seed) * 1.2f * scale;
    sf::Color trunk(95, 70, 45);
    sf::Color leaf = shade(sf::Color(40, 110, 50), 0.85f + 0.2f * hash01(seed + 3));
    addBox(m, base + Vector3(0, trunkH * 0.5f, 0), 0.45f * scale, trunkH, 0.45f * scale, trunk);
    addBox(
        m, base + Vector3(0, trunkH + canopyR * 0.35f, 0), canopyR * 1.6f, canopyR * 0.9f,
        canopyR * 1.6f, leaf
    );
    addBox(
        m, base + Vector3(0, trunkH + canopyR * 0.95f, 0), canopyR * 1.15f, canopyR * 0.7f,
        canopyR * 1.15f, shade(leaf, 1.08f)
    );
    addBox(
        m, base + Vector3(0, trunkH + canopyR * 1.45f, 0), canopyR * 0.7f, canopyR * 0.5f,
        canopyR * 0.7f, shade(leaf, 0.92f)
    );
}

void addBush(Mesh3D& m, const Vector3& base, float scale, int seed) {
    sf::Color leaf = shade(sf::Color(45, 105, 48), 0.88f + 0.15f * hash01(seed));
    addBox(m, base + Vector3(0, 0.55f * scale, 0), 1.4f * scale, 1.1f * scale, 1.4f * scale, leaf);
    addBox(
        m, base + Vector3(0.3f * scale, 0.75f * scale, 0.2f * scale), 1.0f * scale, 0.8f * scale,
        1.0f * scale, shade(leaf, 1.1f)
    );
}

void addCar(Mesh3D& m, const Vector3& c, float yaw, int seed) {
    sf::Color bodyCols[] = {
        sf::Color(40, 45, 55), sf::Color(180, 50, 45), sf::Color(50, 90, 160),
        sf::Color(220, 220, 225), sf::Color(60, 120, 70), sf::Color(140, 100, 40)};
    sf::Color body = bodyCols[static_cast<unsigned>(seed) % 6];
    // Simple axis-aligned car (yaw ignored for density — fine from overhead)
    (void)yaw;
    addBox(m, c + Vector3(0, 0.55f, 0), 2.2f, 0.7f, 4.4f, body);
    addBox(m, c + Vector3(0, 1.05f, -0.2f), 1.9f, 0.55f, 2.4f, shade(body, 0.85f));
    addBox(m, c + Vector3(0, 1.15f, -0.2f), 1.7f, 0.35f, 2.0f, sf::Color(80, 140, 180, 150));
}

Mesh3D buildCity(const Layout& L) {
    Mesh3D m;
    const float parkR = L.maxWallR() + 28.0f;
    const int segs = 128; // high-res so no pie-slice gaps
    const float yG = -1.85f;
    const float rPark0 = parkR;
    const float rPark1 = parkR + 52.0f;
    const float rSuburb = parkR + 140.0f;
    const float rFar = parkR + 480.0f;
    const float rHorizon = parkR + 620.0f;

    sf::Color asphalt(68, 70, 76);
    sf::Color asphaltLine(195, 195, 185);
    sf::Color grassA = shade(grassColor(), 0.80f);
    sf::Color grassB = shade(sf::Color(48, 95, 45), 0.95f);
    sf::Color grassC = shade(sf::Color(60, 110, 52), 0.88f);
    sf::Color dirtLot(115, 95, 68);

    // ── Continuous ground from seat outer edge → horizon (NO bare gaps) ─
    // Each ring slightly overlaps the next so nothing peeks through.
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = -pi + t0 * 2.0f * pi;
        float ang1 = -pi + t1 * 2.0f * pi;
        float angM = 0.5f * (ang0 + ang1);
        float rSeat = seatInnerR(L, angM);
        float rApron0 = rSeat + (isOfBleacher(L, angM) ? 9.0f : 16.0f);
        float rApron1 = rApron0 + (isOfBleacher(L, angM) ? 12.0f : 18.0f);

        // Tan apron (overlaps parking start)
        addQuad(
            m, L.fromHome(rApron0, ang0, -0.15f), L.fromHome(rApron0, ang1, -0.15f),
            L.fromHome(rApron1 + 2.0f, ang1, yG + 0.05f),
            L.fromHome(rApron1 + 2.0f, ang0, yG + 0.05f),
            shade(facadeTanColor(), 0.94f + 0.06f * hash01(i))
        );
        // Grass strip apron → parking
        addQuad(
            m, L.fromHome(rApron1, ang0, yG + 0.02f), L.fromHome(rApron1, ang1, yG + 0.02f),
            L.fromHome(rPark0 + 1.5f, ang1, yG), L.fromHome(rPark0 + 1.5f, ang0, yG),
            (i % 2 == 0) ? grassA : grassB
        );
        // Parking asphalt (full solid)
        addQuad(
            m, L.fromHome(rPark0, ang0, yG), L.fromHome(rPark0, ang1, yG),
            L.fromHome(rPark1 + 1.0f, ang1, yG), L.fromHome(rPark1 + 1.0f, ang0, yG),
            shade(asphalt, 0.90f + 0.1f * hash01(i + 3))
        );
        // Suburb yards (mowed strips so no flat clear spots)
        for (int band = 0; band < 4; band++) {
            float u0 = static_cast<float>(band) / 4.0f;
            float u1 = static_cast<float>(band + 1) / 4.0f;
            float ri = rPark1 + (rSuburb - rPark1) * u0 - 0.5f;
            float ro = rPark1 + (rSuburb - rPark1) * u1 + 0.5f;
            sf::Color g = ((i + band) % 3 == 0) ? grassA : (((i + band) % 3 == 1) ? grassB : grassC);
            addQuad(
                m, L.fromHome(ri, ang0, yG - 0.02f * band),
                L.fromHome(ri, ang1, yG - 0.02f * band),
                L.fromHome(ro, ang1, yG - 0.05f * band),
                L.fromHome(ro, ang0, yG - 0.05f * band), g
            );
        }
        // Outer fields (dense color variation)
        for (int band = 0; band < 5; band++) {
            float u0 = static_cast<float>(band) / 5.0f;
            float u1 = static_cast<float>(band + 1) / 5.0f;
            float ri = rSuburb + (rFar - rSuburb) * u0 - 1.0f;
            float ro = rSuburb + (rFar - rSuburb) * u1 + 1.0f;
            sf::Color g = shade(
                ((i + band) % 2 == 0) ? grassA : grassB, 0.85f + 0.08f * hash01(i * 5 + band)
            );
            addQuad(
                m, L.fromHome(ri, ang0, yG - 0.15f - band * 0.05f),
                L.fromHome(ri, ang1, yG - 0.15f - band * 0.05f),
                L.fromHome(ro, ang1, yG - 0.2f - band * 0.06f),
                L.fromHome(ro, ang0, yG - 0.2f - band * 0.06f), g
            );
        }
        // Horizon ground pad (extends past hills)
        addQuad(
            m, L.fromHome(rFar - 2.0f, ang0, yG - 0.55f),
            L.fromHome(rFar - 2.0f, ang1, yG - 0.55f),
            L.fromHome(rHorizon, ang1, yG - 1.2f), L.fromHome(rHorizon, ang0, yG - 1.2f),
            shade(grassC, 0.75f + 0.1f * hash01(i + 90))
        );
    }

    // Berm walls (vertical face — solid connection)
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = -pi + t0 * 2.0f * pi;
        float ang1 = -pi + t1 * 2.0f * pi;
        float angM = 0.5f * (ang0 + ang1);
        float rSeat = seatInnerR(L, angM);
        float rOut = rSeat + (isOfBleacher(L, angM) ? 22.0f : 32.0f);
        addQuad(
            m, L.fromHome(rOut, ang0, yG), L.fromHome(rOut, ang1, yG),
            L.fromHome(rOut, ang1, 0.6f), L.fromHome(rOut, ang0, 0.6f),
            shade(facadeTanColor(), 0.88f)
        );
    }

    // Ring road + radial roads (asphalt details over solid parking)
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = -pi + t0 * 2.0f * pi;
        float ang1 = -pi + t1 * 2.0f * pi;
        float r = (rPark0 + rPark1) * 0.5f;
        addQuad(
            m, L.fromHome(r - 5.0f, ang0, yG + 0.03f), L.fromHome(r - 5.0f, ang1, yG + 0.03f),
            L.fromHome(r + 5.0f, ang1, yG + 0.03f), L.fromHome(r + 5.0f, ang0, yG + 0.03f),
            shade(asphalt, 1.1f)
        );
    }
    for (int i = 0; i < 16; i++) {
        float ang = -pi + (static_cast<float>(i) + 0.5f) / 16.0f * 2.0f * pi;
        addPath(
            m, L.fromHome(rPark0 + 1.0f, ang, yG), L.fromHome(rSuburb + 20.0f, ang, yG - 0.1f),
            3.5f, yG + 0.04f, shade(asphalt, 1.06f)
        );
        addPath(
            m, L.fromHome(rPark0 + 1.0f, ang, yG), L.fromHome(rSuburb + 20.0f, ang, yG - 0.1f),
            0.14f, yG + 0.06f, asphaltLine
        );
    }
    // Parking stalls + cars
    for (int i = 0; i < 96; i++) {
        float ang = -pi + (static_cast<float>(i) + 0.5f) / 96.0f * 2.0f * pi;
        float r0 = rPark0 + 6.0f + hash01(i) * 28.0f;
        Vector3 a = L.fromHome(r0, ang, yG + 0.04f);
        Vector3 b = L.fromHome(r0 + 5.0f, ang, yG + 0.04f);
        addPath(m, a, b, 0.1f, yG + 0.05f, asphaltLine);
        if (hash01(i * 7) > 0.35f) {
            Vector3 car = L.fromHome(r0 + 2.5f, ang + 0.012f, yG);
            addCar(m, car, ang, i);
        }
    }

    // ── Dense buildings — extra thick BEHIND home (ang ~ π) ───────────
    const sf::Color houseCols[] = {
        sf::Color(190, 175, 155), sf::Color(160, 150, 145), sf::Color(200, 185, 160),
        sf::Color(145, 155, 165), sf::Color(175, 140, 120), sf::Color(150, 160, 140),
        sf::Color(180, 170, 185), facadeGrayColor(), facadeTanColor(),
        sf::Color(130, 125, 120), sf::Color(165, 155, 140),
    };
    auto placeBuilding = [&](float ang, float r, int seed, bool tallOk) {
        float w = 5.5f + hash01(seed * 7) * 12.0f;
        float d = 5.5f + hash01(seed * 11) * 14.0f;
        float h = 3.5f + hash01(seed * 13) * (tallOk ? 22.0f : 12.0f);
        sf::Color col = houseCols[static_cast<unsigned>(seed) % 11];
        col = shade(col, 0.88f + 0.14f * hash01(seed + 20));
        Vector3 c = L.fromHome(r, ang, h * 0.5f + yG);
        addBox(m, c, w, h, d, col);
        addBox(
            m, c + Vector3(0, h * 0.5f + 0.3f, 0), w + 0.7f, 0.45f, d + 0.7f, shade(col, 0.72f)
        );
        // Yard patch under building (kills clear ground under props)
        addBox(m, L.fromHome(r, ang, yG + 0.08f), w + 2.5f, 0.12f, d + 2.5f, dirtLot);
        if (tallOk && hash01(seed * 17) > 0.7f) {
            float h2 = 16.0f + hash01(seed) * 30.0f;
            Vector3 c2 = L.fromHome(r + 10.0f, ang + 0.03f, h2 * 0.5f + yG);
            addBox(m, c2, 7.0f + hash01(seed + 1) * 8.0f, h2, 7.0f, facadeGrayColor());
            for (int fl = 1; fl < static_cast<int>(h2 / 3.2f); fl++) {
                addBox(
                    m, c2 + Vector3(0, -h2 * 0.5f + fl * 3.2f, 0.1f), 6.5f, 1.1f, 0.15f,
                    sf::Color(95, 155, 200, 150)
                );
            }
        }
    };

    // Full ring suburbs
    for (int i = 0; i < 120; i++) {
        float ang = -pi + (static_cast<float>(i) + 0.2f * hash01(i * 3)) / 120.0f * 2.0f * pi;
        float r = rPark1 + 12.0f + hash01(i * 5) * 70.0f;
        placeBuilding(ang, r, i, true);
    }
    // Extra dense pack BEHIND home plate (camera / chase often looks here)
    for (int i = 0; i < 80; i++) {
        float u = (static_cast<float>(i) + 0.5f) / 80.0f;
        float ang = pi - 1.35f + u * 2.7f; // wrap behind home
        if (ang > pi) {
            ang -= 2.0f * pi;
        }
        float r = rPark1 + 8.0f + hash01(i * 9) * 95.0f + (i % 5) * 6.0f;
        placeBuilding(ang, r, i + 300, true);
    }
    // Extra pack behind CF / OF (HR landings)
    for (int i = 0; i < 50; i++) {
        float ang = -0.95f + (static_cast<float>(i) / 50.0f) * 1.9f;
        float r = L.wallRAtAngle(ang) + 40.0f + hash01(i * 4) * 90.0f;
        placeBuilding(ang, r, i + 500, true);
    }
    // Outer commercial
    for (int i = 0; i < 64; i++) {
        float ang = -pi + (static_cast<float>(i) + 0.5f) / 64.0f * 2.0f * pi;
        float r = rSuburb + 15.0f + hash01(i * 9) * 100.0f;
        float h = 8.0f + hash01(i * 3) * 28.0f;
        float w = 12.0f + hash01(i * 5) * 22.0f;
        Vector3 c = L.fromHome(r, ang, h * 0.5f + yG);
        sf::Color col = shade(facadeGrayColor(), 0.82f + 0.18f * hash01(i));
        addBox(m, c, w, h, w * 0.65f, col);
        addBox(m, c + Vector3(0, h * 0.5f + 0.35f, 0), w + 1.0f, 0.45f, w * 0.65f + 1.0f, shade(col, 0.7f));
        addBox(m, L.fromHome(r, ang, yG + 0.06f), w + 3.0f, 0.1f, w * 0.65f + 3.0f, asphalt);
    }

    // Park service buildings
    float pz = L.plateZ();
    addBox(m, Vector3(52.0f, 5.5f, pz + 6.0f), 24.0f, 11.0f, 32.0f, facadeGrayColor());
    addBox(m, Vector3(-52.0f, 5.5f, pz + 6.0f), 24.0f, 11.0f, 32.0f, facadeGrayColor());
    addBox(m, Vector3(58.0f, 4.0f, pz - 25.0f), 16.0f, 8.0f, 20.0f, facadeTanColor());
    addBox(m, Vector3(-58.0f, 4.0f, pz - 25.0f), 16.0f, 8.0f, 20.0f, facadeTanColor());
    addBox(m, Vector3(0.0f, 6.0f, pz + 48.0f), 30.0f, 12.0f, 18.0f, facadeGrayColor()); // behind home
    addBox(m, Vector3(22.0f, 4.5f, pz + 42.0f), 14.0f, 9.0f, 14.0f, facadeTanColor());
    addBox(m, Vector3(-22.0f, 4.5f, pz + 42.0f), 14.0f, 9.0f, 14.0f, facadeTanColor());

    // ── Dense trees / bushes — fill every band ────────────────────────
    for (int i = 0; i < 420; i++) {
        float ang = -pi + (static_cast<float>(i) + hash01(i * 2)) / 420.0f * 2.0f * pi;
        float band = hash01(i * 11);
        float r;
        if (band < 0.22f) {
            r = rPark0 - 2.0f + hash01(i) * 10.0f;
        } else if (band < 0.45f) {
            r = rPark0 + 4.0f + hash01(i + 1) * 40.0f;
        } else if (band < 0.72f) {
            r = rPark1 + 3.0f + hash01(i + 2) * 80.0f;
        } else {
            r = rSuburb + hash01(i + 3) * 180.0f;
        }
        // Keep out of fair field
        if (r < L.maxWallR() + 6.0f && std::abs(ang) < L.foulAngleRad() + 0.12f) {
            continue;
        }
        Vector3 base = L.fromHome(r, ang, yG);
        float sc = 0.75f + hash01(i * 19) * 1.6f;
        if (hash01(i * 23) > 0.55f) {
            addTree(m, base, sc, i * 13);
        } else {
            addBush(m, base, 0.7f + hash01(i) * 1.2f, i * 17);
        }
    }
    // Heavy tree wall behind home
    for (int i = 0; i < 100; i++) {
        float u = (static_cast<float>(i) + 0.5f) / 100.0f;
        float ang = pi - 1.4f + u * 2.8f;
        if (ang > pi) {
            ang -= 2.0f * pi;
        }
        float r = rPark1 + 5.0f + (i % 7) * 9.0f + hash01(i) * 20.0f;
        addTree(m, L.fromHome(r, ang, yG), 1.1f + hash01(i + 4) * 1.3f, i * 41);
        if (i % 2 == 0) {
            addBush(
                m, L.fromHome(r - 4.0f, ang + 0.02f, yG), 0.9f + hash01(i + 2), i * 43
            );
        }
    }
    // CF tree belt (HR backdrop)
    for (int i = 0; i < 70; i++) {
        float ang = -0.7f + (static_cast<float>(i) / 70.0f) * 1.4f;
        float r = L.wallRAtAngle(0.0f) + 45.0f + (i % 6) * 10.0f + hash01(i) * 15.0f;
        addTree(m, L.fromHome(r, ang, yG), 1.15f + hash01(i + 7) * 1.25f, i * 29);
    }

    // Fences / hedges around parking (visual fill)
    for (int i = 0; i < segs; i++) {
        if (i % 2 != 0) {
            continue;
        }
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = -pi + t0 * 2.0f * pi;
        float ang1 = -pi + t1 * 2.0f * pi;
        addQuad(
            m, L.fromHome(rPark1 - 0.5f, ang0, yG), L.fromHome(rPark1 - 0.5f, ang1, yG),
            L.fromHome(rPark1 - 0.5f, ang1, yG + 1.4f), L.fromHome(rPark1 - 0.5f, ang0, yG + 1.4f),
            shade(sf::Color(55, 90, 50), 0.95f)
        );
    }

    // Hills (overlapping so no sky gaps on horizon)
    for (int h = 0; h < 36; h++) {
        float ang = -pi + (static_cast<float>(h) + 0.5f) / 36.0f * 2.0f * pi;
        float r = rFar - 30.0f - hash01(h * 3) * 80.0f;
        float hillH = 14.0f + hash01(h * 7) * 40.0f;
        float hillW = 55.0f + hash01(h * 11) * 90.0f;
        Vector3 c = L.fromHome(r, ang, hillH * 0.32f + yG);
        sf::Color hillCol = shade(sf::Color(65, 100, 60), 0.72f + 0.22f * hash01(h));
        addBox(m, c, hillW, hillH, hillW * 0.55f, hillCol);
        addBox(
            m, c + Vector3(0, hillH * 0.32f, 0), hillW * 0.75f, hillH * 0.4f, hillW * 0.4f,
            shade(hillCol, 1.08f)
        );
    }
    for (int i = 0; i < 20; i++) {
        float ang = -pi + (static_cast<float>(i) + 0.5f) / 20.0f * 2.0f * pi;
        float r = rHorizon - 40.0f;
        float h = 22.0f + hash01(i * 5) * 55.0f;
        addBox(
            m, L.fromHome(r, ang, h * 0.5f + yG), 50.0f + hash01(i) * 40.0f, h, 25.0f,
            shade(sf::Color(125, 140, 155), 0.68f + 0.18f * hash01(i + 2))
        );
    }

    // Landmarks
    {
        Vector3 base = L.fromHome(rSuburb + 40.0f, 0.95f, yG);
        addBox(m, base + Vector3(0, 9.0f, 0), 1.3f, 18.0f, 1.3f, facadeGrayColor());
        addBox(m, base + Vector3(0, 19.0f, 0), 6.5f, 4.5f, 6.5f, sf::Color(175, 48, 48));
    }
    {
        // Cell tower behind home
        Vector3 base = L.fromHome(rPark1 + 60.0f, pi * 0.92f, yG);
        addBox(m, base + Vector3(0, 14.0f, 0), 0.8f, 28.0f, 0.8f, sf::Color(120, 120, 125));
        addBox(m, base + Vector3(0, 28.0f, 0), 4.0f, 0.5f, 0.5f, sf::Color(140, 140, 145));
    }

    m.rebuildNormals();
    return m;
}

// Soft sky dome + clouds so deep fly balls still have atmosphere.
Mesh3D buildSkyBackdrop(const Layout& L) {
    Mesh3D m;
    const float R = L.maxWallR() + 520.0f;
    const Vector3 c = L.parkCenter();
    const int rings = 10;
    const int segs = 40;
    for (int j = 0; j < rings; j++) {
        float v0 = static_cast<float>(j) / rings;
        float v1 = static_cast<float>(j + 1) / rings;
        // Hemisphere from horizon (v=0) to zenith (v=1)
        float elev0 = v0 * (pi * 0.48f);
        float elev1 = v1 * (pi * 0.48f);
        float y0 = std::sin(elev0) * R * 0.55f;
        float y1 = std::sin(elev1) * R * 0.55f;
        float rh0 = std::cos(elev0) * R;
        float rh1 = std::cos(elev1) * R;
        sf::Color col0 = shade(skyColor(), 0.85f + 0.2f * v0);
        sf::Color col1 = shade(skyZenithColor(), 0.7f + 0.35f * v1);
        // Blend toward zenith
        sf::Color a(
            static_cast<std::uint8_t>(col0.r * (1.0f - v0) + col1.r * v0),
            static_cast<std::uint8_t>(col0.g * (1.0f - v0) + col1.g * v0),
            static_cast<std::uint8_t>(col0.b * (1.0f - v0) + col1.b * v0)
        );
        for (int i = 0; i < segs; i++) {
            float t0 = static_cast<float>(i) / segs * 2.0f * pi;
            float t1 = static_cast<float>(i + 1) / segs * 2.0f * pi;
            Vector3 p0(c.x + std::cos(t0) * rh0, y0 + 5.0f, c.z + std::sin(t0) * rh0);
            Vector3 p1(c.x + std::cos(t1) * rh0, y0 + 5.0f, c.z + std::sin(t1) * rh0);
            Vector3 p2(c.x + std::cos(t1) * rh1, y1 + 5.0f, c.z + std::sin(t1) * rh1);
            Vector3 p3(c.x + std::cos(t0) * rh1, y1 + 5.0f, c.z + std::sin(t0) * rh1);
            // Inward-facing (camera inside)
            addQuad(m, p0, p3, p2, p1, a);
        }
    }
    // Soft cloud puffs mid-sky
    for (int i = 0; i < 28; i++) {
        float ang = hash01(i * 3) * 2.0f * pi;
        float elev = 0.15f + hash01(i * 5) * 0.35f;
        float rr = R * 0.55f;
        Vector3 p(
            c.x + std::cos(ang) * std::cos(elev) * rr,
            25.0f + std::sin(elev) * rr * 0.5f,
            c.z + std::sin(ang) * std::cos(elev) * rr
        );
        float sx = 28.0f + hash01(i * 7) * 40.0f;
        float sy = 6.0f + hash01(i * 9) * 10.0f;
        float sz = 16.0f + hash01(i * 11) * 28.0f;
        addBox(m, p, sx, sy, sz, sf::Color(245, 248, 255, 180));
        addBox(
            m, p + Vector3(sx * 0.25f, -sy * 0.15f, sz * 0.1f), sx * 0.6f, sy * 0.7f, sz * 0.55f,
            sf::Color(235, 240, 250, 160)
        );
    }
    m.rebuildNormals();
    return m;
}

// ═══════════════════════════════════════════════════════════════════════
// LINES
// ═══════════════════════════════════════════════════════════════════════

Mesh3D buildLines(const Layout& L) {
    Mesh3D m;
    sf::Color chalk(248, 248, 245);
    auto line = [&](Vector3 a, Vector3 b, float hw) {
        addPath(m, a, b, hw, 0.038f, chalk);
    };
    float aL = -L.foulAngleRad(), aR = L.foulAngleRad();
    line(L.home(), L.fromHome(L.wallRAtAngle(aL) + 0.5f, aL, 0), 0.13f);
    line(L.home(), L.fromHome(L.wallRAtAngle(aR) + 0.5f, aR, 0), 0.13f);
    line(L.home(), L.firstBase(), 0.09f);
    line(L.firstBase(), L.secondBase(), 0.09f);
    line(L.secondBase(), L.thirdBase(), 0.09f);
    line(L.thirdBase(), L.home(), 0.09f);
    // Batter box outlines
    for (float cx : {-1.7f, 1.7f}) {
        float zc = L.plateZ() - 0.2f;
        Vector3 a(cx - 1.05f, 0, zc + 1.55f), b(cx + 1.05f, 0, zc + 1.55f);
        Vector3 c(cx + 1.05f, 0, zc - 1.55f), d(cx - 1.05f, 0, zc - 1.55f);
        line(a, b, 0.05f);
        line(b, c, 0.05f);
        line(c, d, 0.05f);
        line(d, a, 0.05f);
    }
    m.rebuildNormals();
    return m;
}

// ═══════════════════════════════════════════════════════════════════════
// CROWDS — dense bowl + OF bleachers
// ═══════════════════════════════════════════════════════════════════════

void addFan(Mesh3D& m, Vector3 feet, float sc, sf::Color shirt, sf::Color skin) {
    // Torso
    addBox(m, feet + Vector3(0, 0.58f * sc, 0), 0.38f * sc, 1.05f * sc, 0.3f * sc, shirt);
    // Head
    addBox(m, feet + Vector3(0, 1.28f * sc, 0), 0.3f * sc, 0.3f * sc, 0.3f * sc, skin);
    // Legs
    addBox(
        m, feet + Vector3(-0.1f * sc, 0.18f * sc, 0), 0.14f * sc, 0.4f * sc, 0.16f * sc,
        shade(shirt, 0.7f)
    );
    addBox(
        m, feet + Vector3(0.1f * sc, 0.18f * sc, 0), 0.14f * sc, 0.4f * sc, 0.16f * sc,
        shade(shirt, 0.7f)
    );
}

sf::Color fanShirt(int id) {
    const sf::Color opts[] = {
        seatBlueColor(), seatRedColor(), sf::Color(245, 245, 248), sf::Color(35, 35, 45),
        sf::Color(25, 110, 65), sf::Color(210, 160, 40), sf::Color(90, 50, 120),
        sf::Color(200, 90, 40), sf::Color(50, 140, 180), sf::Color(160, 40, 70)};
    return opts[static_cast<unsigned>(id) % 10];
}

sf::Color fanSkin(int id) {
    const sf::Color opts[] = {
        sf::Color(225, 185, 150), sf::Color(210, 160, 120), sf::Color(180, 130, 95),
        sf::Color(240, 205, 175), sf::Color(140, 95, 70)};
    return opts[static_cast<unsigned>(id) % 5];
}

std::vector<Mesh3D> buildFanSectors(const Layout& L) {
    std::vector<Mesh3D> sectors(kFanSectorCount);
    const int angSamples = 360;
    const float dRow = 1.35f;
    const float rise = 0.88f;
    int fanId = 0;

    for (int i = 0; i < angSamples; i++) {
        float t = (static_cast<float>(i) + 0.5f) / angSamples;
        float ang = -pi + t * 2.0f * pi;
        if (!inSeatArc(L, ang)) {
            continue;
        }
        int sector = static_cast<int>(t * kFanSectorCount) % kFanSectorCount;
        bool ofBleach = isOfBleacher(L, ang);
        bool club = isClubZone(ang);
        int rows = ofBleach ? 7 : (club ? 18 : 14);
        float r = seatInnerR(L, ang) + 0.5f;
        float y = seatBaseY(L, ang) + 0.65f;

        for (int row = 0; row < rows; row++) {
            // High fill — denser in lower rows and OF
            float fill = ofBleach ? 0.88f : (row < 4 ? 0.92f : 0.78f);
            if (club && row > 14) {
                fill = 0.85f;
            }
            if (hash01(fanId * 13 + row * 3) > fill) {
                fanId++;
                r += dRow;
                y += rise;
                continue;
            }
            // Occasional empty seat
            if (hash01(fanId * 7 + 2) < 0.04f) {
                fanId++;
                r += dRow;
                y += rise;
                continue;
            }
            Vector3 seat = L.fromHome(r + dRow * 0.3f, ang, y);
            seat.x += (hash01(fanId) - 0.5f) * 0.42f;
            seat.z += (hash01(fanId + 3) - 0.5f) * 0.42f;
            float sc = 0.82f + 0.28f * hash01(fanId + 9);
            addFan(sectors[sector], seat, sc, fanShirt(fanId), fanSkin(fanId + 4));
            fanId++;
            r += dRow;
            y += rise;
        }

        // Club upper deck fans
        if (club) {
            float rU = seatInnerR(L, ang) + 22.0f;
            float yU = seatBaseY(L, ang) + 16.0f;
            for (int row = 0; row < 6; row++) {
                if (hash01(fanId * 17 + row) > 0.86f) {
                    fanId++;
                    rU += dRow;
                    yU += rise;
                    continue;
                }
                Vector3 seat = L.fromHome(rU, ang, yU);
                seat.x += (hash01(fanId) - 0.5f) * 0.3f;
                float sc = 0.88f + 0.2f * hash01(fanId + 2);
                addFan(sectors[sector], seat, sc, fanShirt(fanId + 50), fanSkin(fanId));
                fanId++;
                rU += dRow;
                yU += rise;
            }
        }
    }

    // Extra OF bleacher density pass (crowds along outfield)
    for (int i = 0; i < 180; i++) {
        float t = (static_cast<float>(i) + 0.5f) / 180.0f;
        float ang = -L.foulAngleRad() + t * 2.0f * L.foulAngleRad();
        if (std::abs(ang) < 0.12f) {
            continue;
        }
        int sector = static_cast<int>(((ang + pi) / (2.0f * pi)) * kFanSectorCount) %
                     kFanSectorCount;
        if (sector < 0) {
            sector += kFanSectorCount;
        }
        float r0 = seatInnerR(L, ang);
        float y0 = seatBaseY(L, ang);
        for (int row = 0; row < 6; row++) {
            if (hash01(i * 31 + row * 5) > 0.90f) {
                continue;
            }
            float r = r0 + 1.0f + row * dRow;
            float y = y0 + 0.7f + row * rise;
            Vector3 seat = L.fromHome(r, ang, y);
            seat.x += (hash01(i * 3 + row) - 0.5f) * 0.5f;
            float sc = 0.8f + 0.25f * hash01(i + row + 20);
            addFan(sectors[sector], seat, sc, fanShirt(i * 3 + row), fanSkin(i + row));
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
    for (int i = 0; i < kFlagCount; i++) {
        float side = (i % 2 == 0) ? 1.0f : -1.0f;
        float ang = side * (L.foulAngleRad() + 0.12f + 0.1f * static_cast<float>(i / 2));
        float r = L.wallRAtAngle(side * L.foulAngleRad()) + 5.0f + static_cast<float>(i / 2) * 4.0f;
        Vector3 base = L.fromHome(r, ang, 0.0f);
        Mesh3D mesh;
        addBox(mesh, Vector3(0, 4.5f, 0), 0.12f, 9.0f, 0.12f, sf::Color(210, 210, 215));
        sf::Color fc = (i % 3 == 0) ? seatRedColor() : ofWallColor();
        addQuad(
            mesh, Vector3(0.1f, 8.2f, 0), Vector3(3.2f, 8.0f, 0.12f),
            Vector3(3.0f, 5.6f, 0.12f), Vector3(0.1f, 6.0f, 0), fc
        );
        mesh.rebuildNormals();
        flags.push_back(std::move(mesh));
        bases.push_back(base);
    }
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
// Layout API
// ═══════════════════════════════════════════════════════════════════════

float Layout::foulAngleRad() const {
    return foulAngleDegrees * (pi / 180.0f);
}

float Layout::wallFeetAtAngle(float angleRad) const {
    float aDeg = angleRad * (180.0f / pi);
    aDeg = std::clamp(aDeg, -foulAngleDegrees, foulAngleDegrees);
    static const float samples[][2] = {
        {-45.0f, 330.0f}, {-28.0f, 355.0f}, {-15.0f, 375.0f}, {0.0f, 400.0f},
        {15.0f, 375.0f},  {28.0f, 355.0f},  {45.0f, 330.0f},
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

float Layout::wallHeightAtAngle(float /*a*/) const {
    return wallHeightFeet / feetPerUnit;
}

Vector3 Layout::domeCenter() const {
    return Vector3(0.0f, 0.0f, plateZ() - domeCenterOffsetFeet / feetPerUnit);
}

float Layout::domeRoofYAtWorld(float, float) const {
    return 200.0f;
}
float Layout::domeRoofYAtRadius(float) const {
    return 200.0f;
}

float Layout::maxRadiusFromHome(float angleRad) const {
    wrapAng(angleRad);
    float a = std::clamp(angleRad, -foulAngleRad(), foulAngleRad());
    return wallRAtAngle(a) + 70.0f;
}

float Layout::clampRadiusInDome(float, float radius, float) const {
    return std::max(0.0f, radius);
}

bool Layout::isCfScoreboardZone(float angleRad) const {
    wrapAng(angleRad);
    return std::abs(angleRad) < 0.10f;
}

float Layout::seatDeckYAtRadius(float radiusFromHome, float angleRad) const {
    float r0 = bowlInnerRadius(angleRad);
    float past = radiusFromHome - r0;
    if (past < 0.0f) {
        return 0.0f;
    }
    return bowlBaseHeight(angleRad) + past * 0.55f;
}

bool Layout::containInsideDome(Vector3&, Vector3&, float) const {
    return false;
}

Vector3 Layout::fromHome(float radius, float angleRad, float y) const {
    return Vector3(std::sin(angleRad) * radius, y, plateZ() - std::cos(angleRad) * radius);
}

Vector3 Layout::wallPoint(float angleRad, float yFraction) const {
    return fromHome(
        wallRAtAngle(angleRad),
        angleRad,
        wallHeightAtAngle(angleRad) * std::clamp(yFraction, 0.0f, 1.0f)
    );
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
    return Vector3(0.0f, 10.0f, plateZ() - maxWallR() * 0.36f);
}

Vector3 Layout::scoreboardCenter() const {
    return fromHome(wallRAtAngle(0.0f) + 5.5f, 0.0f, 10.0f);
}

void Layout::polarFromHome(const Vector3& worldPos, float& radiusOut, float& angleRadOut) const {
    float dx = worldPos.x;
    float dz = worldPos.z - plateZ();
    radiusOut = std::sqrt(dx * dx + dz * dz);
    angleRadOut = std::atan2(dx, -dz);
}

float Layout::radiusFromHome(const Vector3& worldPos) const {
    float r = 0.0f, a = 0.0f;
    polarFromHome(worldPos, r, a);
    (void)a;
    return r;
}

bool Layout::isSeatingArc(float angleRad) const {
    return inSeatArc(*this, angleRad);
}

float Layout::bowlInnerRadius(float ang) const {
    return seatInnerR(*this, ang);
}

float Layout::bowlBaseHeight(float ang) const {
    return seatBaseY(*this, ang);
}

// ═══════════════════════════════════════════════════════════════════════
// Collision — solid barriers on every path (no free-flight forever)
// ═══════════════════════════════════════════════════════════════════════

namespace {

Vector3 radialOut(float ang) {
    return Vector3(std::sin(ang), 0.0f, -std::cos(ang));
}

void bounceRadial(Vector3& vel, float ang, float rest = 0.55f, float fric = 0.82f) {
    Vector3 n = radialOut(ang);
    float vn = vel.x * n.x + vel.z * n.z;
    if (vn > 0.0f) {
        vel.x -= n.x * vn * (1.0f + rest);
        vel.z -= n.z * vn * (1.0f + rest);
    }
    vel.x *= fric;
    vel.z *= fric;
    vel.y *= 0.88f;
}

void trySettle(Vector3& vel, bool stick, float thresh, BallCollisionHit& hit) {
    if ((stick && vel.magnitude() < thresh + 0.8f) || vel.magnitude() < thresh) {
        vel = Vector3();
        hit.stuck = true;
    }
}

} // namespace

BallCollisionHit collideBall(
    const Layout& layout, Vector3& position, Vector3& velocity, float radius, bool stickOnContact
) {
    BallCollisionHit hit;
    const float groundY = radius + 0.01f;
    const float fa = layout.foulAngleRad();

    auto refreshPolar = [&](float& rOut, float& angOut) {
        layout.polarFromHome(position, rOut, angOut);
    };

    float r = 0.0f, ang = 0.0f;
    refreshPolar(r, ang);
    hit.sprayDeg = ang * (180.0f / pi);
    bool fair = std::abs(ang) <= fa + 0.03f;
    hit.fenceFeet = layout.wallFeetAtAngle(ang);
    hit.wallTopY = layout.wallHeightAtAngle(ang);

    // ── 1. Ground (always first) ──────────────────────────────────────
    bool onGround = false;
    if (position.y < groundY) {
        position.y = groundY;
        onGround = true;
        hit.surface = HitSurface::Ground;
        hit.impactY = groundY;
        if (velocity.y < 0.0f) {
            velocity.y = -velocity.y * 0.36f;
            velocity.x *= 0.84f;
            velocity.z *= 0.84f;
        }
        trySettle(velocity, stickOnContact, 2.8f, hit);
    } else if (position.y < groundY + 0.14f && velocity.y <= 0.2f) {
        if (velocity.magnitude() < 3.4f || (stickOnContact && velocity.magnitude() < 4.6f)) {
            position.y = groundY;
            velocity = Vector3();
            hit.surface = HitSurface::Ground;
            hit.impactY = groundY;
            hit.stuck = true;
            onGround = true;
        }
    }
    if (hit.stuck) {
        return hit;
    }

    refreshPolar(r, ang);
    fair = std::abs(ang) <= fa + 0.03f;
    Vector3 nOut = radialOut(ang);

    // ── 2. Outer world barrier (suburb edge — never fly off forever) ─
    {
        const float rWorld = layout.maxWallR() + 95.0f;
        if (r + radius > rWorld) {
            Vector3 target = layout.fromHome(rWorld - radius - 0.08f, ang, position.y);
            target.y = std::max(target.y, groundY);
            position = target;
            bounceRadial(velocity, ang, 0.35f, 0.75f);
            if (velocity.y > 2.0f) {
                velocity.y *= 0.5f;
            }
            hit.surface = HitSurface::DomeShell;
            hit.impactY = position.y;
            trySettle(velocity, stickOnContact, 3.0f, hit);
            if (hit.stuck) {
                return hit;
            }
            refreshPolar(r, ang);
            fair = std::abs(ang) <= fa + 0.03f;
            nOut = radialOut(ang);
        }
    }

    // ── 3. Ceiling soft clamp (open park — still stop sky rockets) ───
    {
        const float yCeil = 85.0f;
        if (position.y + radius > yCeil) {
            position.y = yCeil - radius;
            if (velocity.y > 0.0f) {
                velocity.y = -velocity.y * 0.25f;
            }
            velocity.x *= 0.9f;
            velocity.z *= 0.9f;
            hit.surface = HitSurface::Roof;
            hit.impactY = position.y;
        }
    }

    // ── 4. Fair OF fence face (only below wall top — no tunneling) ───
    bool clearedFence = false;
    if (fair && r > 1.0f) {
        float wallR = layout.wallRAtAngle(ang);
        float wallH = layout.wallHeightAtAngle(ang);
        hit.wallTopY = wallH;
        hit.fenceFeet = layout.wallFeetAtAngle(ang);
        if (r + radius > wallR) {
            const float clearY = wallH + std::max(radius * 0.85f, 0.28f);
            if (position.y > clearY) {
                // True over-the-wall path — free until stands/deck/ground
                clearedFence = true;
                hit.surface = HitSurface::FenceTopClear;
                hit.impactY = position.y;
            } else if (!onGround) {
                // Solid wall face — bounce back into play
                Vector3 onWall = layout.fromHome(wallR - radius - 0.06f, ang, position.y);
                onWall.y = std::clamp(position.y, groundY, wallH - 0.02f);
                position = onWall;
                bounceRadial(velocity, ang, 0.58f, 0.78f);
                velocity.y = std::min(std::abs(velocity.y) * 0.4f + 1.0f, 4.5f);
                hit.surface = HitSurface::Fence;
                hit.impactY = position.y;
                trySettle(velocity, stickOnContact, 3.2f, hit);
                if (hit.stuck) {
                    return hit;
                }
                refreshPolar(r, ang);
                nOut = radialOut(ang);
            }
        }
    }

    // ── 5. Foul poles ─────────────────────────────────────────────────
    {
        auto poleHit = [&](float poleAng) {
            Vector3 base = layout.wallPoint(poleAng, 0.0f);
            float poleH = layout.wallHeightAtAngle(poleAng) * 3.6f;
            float pr = 0.55f + radius;
            Vector3 d(position.x - base.x, 0.0f, position.z - base.z);
            float dist = std::sqrt(d.x * d.x + d.z * d.z);
            if (position.y < 0.0f || position.y > poleH + 1.5f || dist >= pr || dist < 1e-5f) {
                return;
            }
            Vector3 n = d * (1.0f / dist);
            position.x = base.x + n.x * pr;
            position.z = base.z + n.z * pr;
            float vn = velocity.x * n.x + velocity.z * n.z;
            if (vn > 0.0f) {
                velocity = velocity - n * vn * 1.55f;
            }
            velocity = velocity * 0.55f;
            hit.surface = HitSurface::FoulPole;
            hit.impactY = position.y;
            trySettle(velocity, stickOnContact, 3.0f, hit);
        };
        poleHit(fa);
        poleHit(-fa);
        if (hit.stuck) {
            return hit;
        }
        refreshPolar(r, ang);
    }

    // ── 6. Backstop (behind plate) ────────────────────────────────────
    {
        const float backR = 15.5f;
        const float backH = 12.0f;
        if (std::abs(ang) > fa + 0.15f && r + radius > backR && position.y < backH + radius) {
            if (-std::cos(ang) > 0.30f) {
                Vector3 target = layout.fromHome(backR - radius - 0.06f, ang, position.y);
                target.y = std::clamp(position.y, groundY, backH);
                position = target;
                bounceRadial(velocity, ang, 0.4f, 0.7f);
                hit.surface = HitSurface::Backstop;
                hit.impactY = position.y;
                trySettle(velocity, stickOnContact, 3.0f, hit);
                if (hit.stuck) {
                    return hit;
                }
                refreshPolar(r, ang);
            }
        }
    }

    // ── 7. Stands / OF bleachers — face + deck (every path lands) ─────
    // After clearing the fence OR in foul horseshoe, seats are solid.
    {
        const bool inSeats = layout.isSeatingArc(ang);
        if (inSeats || clearedFence || (fair && r > layout.wallRAtAngle(ang) + 0.2f)) {
            float rBowl = layout.bowlInnerRadius(ang) - 0.2f;
            float yBase = layout.bowlBaseHeight(ang);
            // Seat deck rises with radius past first row
            float pastBowl = std::max(0.0f, r - rBowl);
            float deckY = yBase + std::min(pastBowl * 0.58f, 16.0f) + radius;
            float facadeTop = yBase + 15.5f;

            // Vertical facade (first row face) — bounce if still in front
            if (r + radius > rBowl && position.y < facadeTop && position.y > groundY - 0.05f &&
                pastBowl < 1.2f) {
                Vector3 target = layout.fromHome(rBowl - radius - 0.05f, ang, position.y);
                target.y = std::max(target.y, groundY);
                position = target;
                bounceRadial(velocity, ang, 0.42f, 0.7f);
                hit.surface = HitSurface::Stands;
                hit.impactY = position.y;
                trySettle(velocity, stickOnContact, 3.2f, hit);
                if (hit.stuck) {
                    return hit;
                }
                refreshPolar(r, ang);
                pastBowl = std::max(0.0f, r - rBowl);
                deckY = yBase + std::min(pastBowl * 0.58f, 16.0f) + radius;
            }

            // Horizontal seat deck — land when dropping into the bowl
            if (r > rBowl - 0.5f && position.y < deckY && position.y > yBase - 1.0f) {
                // Only if we're actually over the seating radius
                if (r + radius > rBowl) {
                    position.y = deckY;
                    if (velocity.y < 0.0f) {
                        velocity.y = -velocity.y * 0.28f;
                        velocity.x *= 0.72f;
                        velocity.z *= 0.72f;
                    }
                    // Soft stick on seats
                    if (velocity.magnitude() < 4.5f || stickOnContact) {
                        if (velocity.magnitude() < 5.5f) {
                            velocity = Vector3();
                            hit.stuck = true;
                        }
                    }
                    hit.surface = HitSurface::Stands;
                    hit.impactY = position.y;
                    if (hit.stuck) {
                        return hit;
                    }
                }
            }
        }
    }

    // ── 8. CF scoreboard chassis (solid) ──────────────────────────────
    if (layout.isCfScoreboardZone(ang) || std::abs(ang) < 0.18f) {
        float cfR = layout.wallRAtAngle(0.0f);
        float boardR0 = cfR + 3.5f;
        float boardR1 = cfR + 8.0f;
        float boardY0 = 4.0f;
        float boardY1 = 16.0f;
        if (r + radius > boardR0 && r < boardR1 + radius && position.y > boardY0 - radius &&
            position.y < boardY1 + radius) {
            // Push to front face of board
            Vector3 target = layout.fromHome(boardR0 - radius - 0.05f, ang, position.y);
            target.y = std::clamp(position.y, boardY0, boardY1);
            position = target;
            bounceRadial(velocity, ang, 0.45f, 0.7f);
            hit.surface = HitSurface::Scoreboard;
            hit.impactY = position.y;
            trySettle(velocity, stickOnContact, 3.0f, hit);
            if (hit.stuck) {
                return hit;
            }
        }
    }

    // ── 9. Final ground re-clamp after barrier snaps ──────────────────
    if (position.y < groundY) {
        position.y = groundY;
        if (velocity.y < 0.0f) {
            velocity.y = -velocity.y * 0.3f;
        }
        hit.surface = HitSurface::Ground;
        hit.impactY = groundY;
        trySettle(velocity, stickOnContact, 2.6f, hit);
    }

    return hit;
}

BallCollisionHit collideBallSubsteps(
    const Layout& layout, Vector3& position, Vector3& velocity, float radius, bool stickOnContact,
    int substeps
) {
    BallCollisionHit last;
    substeps = std::max(1, std::min(substeps, 12));
    // Integrate + collide each slice so fast balls can't tunnel a whole barrier.
    const float dt = 1.0f / (60.0f * static_cast<float>(substeps));
    const float dragK = 0.012f;
    const float g = -9.8f;
    for (int i = 0; i < substeps; i++) {
        float sp = velocity.magnitude();
        if (sp > 1e-4f) {
            velocity = velocity + (Vector3(0.0f, g, 0.0f) + velocity * (-dragK * sp)) * dt;
        } else {
            velocity.y += g * dt;
        }
        position = position + velocity * dt;
        last = collideBall(layout, position, velocity, radius, stickOnContact);
        if (last.stuck) {
            break;
        }
    }
    return last;
}

WallClearResult evaluateWallClear(
    const Layout& layout, Vector3 position, Vector3 velocity, float gravityY, float dragK
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
        float r = 0.0f, ang = 0.0f;
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
    float endR = 0.0f, endA = 0.0f;
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
    L.buildingRadiusFeet = 300.0f;
    L.domeCenterOffsetFeet = 150.0f;
    return L;
}

Meshes build(const Layout& layout) {
    Meshes out;
    out.field = buildField(layout);
    out.walls = buildWalls(layout);
    out.stands = buildStands(layout);
    out.lines = buildLines(layout);
    out.city = buildCity(layout);
    out.skyDome = buildSkyBackdrop(layout);
    out.scoreboardScreen = buildScoreboardScreen(layout);
    out.hotel = buildHotel(layout);
    out.structure = buildStructure(layout);
    out.fanSectors = buildFanSectors(layout);
    buildFlags(layout, out.flagMeshes, out.flagBases);
    return out;
}

float recommendedFarPlane(const Layout& layout) {
    // Large enough that HR chase + suburb backdrop never clips.
    return std::max(2800.0f, layout.maxWallR() * 18.0f + 800.0f);
}

float fanCheerOffsetY(int sectorIndex, float timeSec, float boost) {
    float s = static_cast<float>(sectorIndex);
    float seed = hash01(sectorIndex * 47 + 13);
    float b = std::max(0.35f, boost);
    float period = 0.55f + seed * 1.3f;
    float tHop = std::fmod(timeSec + seed * 4.0f + s * 0.31f, period);
    if (tHop < 0.0f) {
        tHop += period;
    }
    float hop = (tHop < 0.13f) ? std::sin((tHop / 0.13f) * pi) : 0.0f;
    float idle = 0.03f * std::sin(timeSec * (2.3f + seed * 2.0f) + s * 1.4f);
    return (idle + hop * (0.12f + seed * 0.1f)) * b;
}

float fanCheerOffsetX(int sectorIndex, float timeSec, float boost) {
    float s = static_cast<float>(sectorIndex);
    float seed = hash01(sectorIndex * 53 + 19);
    return (
        0.04f * std::sin(timeSec * (1.7f + seed * 2.5f) + s * 2.0f) +
        0.02f * std::sin(timeSec * 4.2f + seed * 5.0f)
    ) * std::max(0.35f, boost);
}

float flagSwayYaw(int flagIndex, float timeSec) {
    return 0.22f * std::sin(timeSec * 2.4f + flagIndex * 0.9f) +
           0.08f * std::sin(timeSec * 5.0f + flagIndex);
}

float scoreboardPulse(float timeSec, float excitement) {
    float base = 0.55f + 0.22f * std::sin(timeSec * 3.3f);
    float pop = excitement > 0.01f ? 0.32f * std::sin(timeSec * 11.0f) * excitement : 0.0f;
    return std::clamp(base + pop, 0.28f, 1.0f);
}

} // namespace Stadium3D
