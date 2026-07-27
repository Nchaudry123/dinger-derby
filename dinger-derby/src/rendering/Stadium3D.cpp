#include "Stadium3D.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <vector>

#include "GltfLoader.h"

// Crown Jewel retro-modern ballpark — a downtown park. One continuous
// structure: diamond field, forest-green bowl, suite ring + steep upper deck
// behind home, white cantilevered roof, brick + sandstone rotunda, gold
// crown videoboard, tall green LF wall, light towers, and a CBD skyline.

namespace Stadium3D {
namespace {

constexpr float pi = std::numbers::pi_v<float>;

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

// Local-space bounds of a loaded prop mesh, used to derive fit-to-target
// scale factors since props come in from Blender/Meshy at arbitrary export
// scale (meters), not this game's world units.
struct LocalBBox {
    Vector3 mn{1e9f, 1e9f, 1e9f};
    Vector3 mx{-1e9f, -1e9f, -1e9f};
    Vector3 size() const { return Vector3(mx.x - mn.x, mx.y - mn.y, mx.z - mn.z); }
};

LocalBBox computeBBox(const Mesh3D& m) {
    LocalBBox b;
    for (const auto& v : m.vertices) {
        b.mn.x = std::min(b.mn.x, v.x);
        b.mn.y = std::min(b.mn.y, v.y);
        b.mn.z = std::min(b.mn.z, v.z);
        b.mx.x = std::max(b.mx.x, v.x);
        b.mx.y = std::max(b.mx.y, v.y);
        b.mx.z = std::max(b.mx.z, v.z);
    }
    return b;
}

// Appends a scaled/yawed/translated copy of a loaded prop's triangles into
// dst. Per-axis scale lets flat/boxy props (e.g. a scoreboard panel) fit a
// target footprint whose aspect ratio doesn't match the source export.
void appendPropInstance(
    Mesh3D& dst, const Mesh3D& prop, const Vector3& scale, float yawRad, const Vector3& translate
) {
    if (prop.vertices.empty() || prop.triangles.empty()) {
        return;
    }
    int base = static_cast<int>(dst.vertices.size());
    float c = std::cos(yawRad);
    float s = std::sin(yawRad);
    dst.vertices.reserve(dst.vertices.size() + prop.vertices.size());
    for (const auto& v : prop.vertices) {
        Vector3 p(v.x * scale.x, v.y * scale.y, v.z * scale.z);
        Vector3 r(p.x * c + p.z * s, p.y, -p.x * s + p.z * c);
        dst.vertices.push_back(r + translate);
    }
    dst.triangles.reserve(dst.triangles.size() + prop.triangles.size());
    dst.triangleColors.reserve(dst.triangleColors.size() + prop.triangleColors.size());
    for (std::size_t i = 0; i < prop.triangles.size(); i++) {
        const Triangle3D& t = prop.triangles[i];
        dst.triangles.push_back({t.a + base, t.b + base, t.c + base});
        dst.triangleColors.push_back(
            i < prop.triangleColors.size() ? prop.triangleColors[i] : sf::Color::White
        );
    }
}

void addDisk(Mesh3D& m, const Vector3& c, float r, float y, int segs, sf::Color col) {
    for (int i = 0; i < segs; i++) {
        float a0 = (static_cast<float>(i) / segs) * 2.0f * pi;
        float a1 = (static_cast<float>(i + 1) / segs) * 2.0f * pi;
        // Wind so the fan normal faces +Y — the old (p0, p1) order faced
        // downward and every flat disk (mound tiers, base cutouts, home
        // circle, on-deck circles) was back-face culled from above.
        addTri(
            m, Vector3(c.x, y, c.z),
            Vector3(c.x + std::cos(a1) * r, y, c.z + std::sin(a1) * r),
            Vector3(c.x + std::cos(a0) * r, y, c.z + std::sin(a0) * r), col
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

// Sloped side wall between two rings at different heights (mound tiers).
// Wound (p0, p3, p2, p1) so the surface normal points outward/up — the
// natural (p0, p1, p2, p3) order faces inward/down and culls from outside.
void addFrustum(
    Mesh3D& m, const Vector3& c, float r0, float y0, float r1, float y1, int segs,
    sf::Color col
) {
    for (int i = 0; i < segs; i++) {
        float a0 = (static_cast<float>(i) / segs) * 2.0f * pi;
        float a1 = (static_cast<float>(i + 1) / segs) * 2.0f * pi;
        Vector3 p0(c.x + std::cos(a0) * r0, y0, c.z + std::sin(a0) * r0);
        Vector3 p1(c.x + std::cos(a1) * r0, y0, c.z + std::sin(a1) * r0);
        Vector3 p2(c.x + std::cos(a1) * r1, y1, c.z + std::sin(a1) * r1);
        Vector3 p3(c.x + std::cos(a0) * r1, y1, c.z + std::sin(a0) * r1);
        addQuad(m, p0, p3, p2, p1, col);
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

// Full seating arc: the whole 360° wraps with stands — no gap. The
// scoreboard structure sits in front of the CF bleachers rather than
// requiring a hole cut in them; a real gap there was extremely visible
// (trees/city showing straight through) since it sits dead-center in the
// batter/mound camera every single pitch.
bool inSeatArc(const Layout& L, float ang) {
    (void)L;
    (void)ang;
    return true;
}

bool isOfBleacher(const Layout& L, float ang) {
    wrapAng(ang);
    return std::abs(ang) <= L.foulAngleRad() + 0.02f;
}

bool isClubZone(float ang) {
    wrapAng(ang);
    return std::abs(ang) > 1.30f; // wide double-deck arc behind home + dugouts
}

// ═══════════════════════════════════════════════════════════════════════
// FIELD
// ═══════════════════════════════════════════════════════════════════════

Mesh3D buildField(const Layout& L) {
    Mesh3D m;
    const float aL = -L.foulAngleRad();
    const float aR = L.foulAngleRad();
    const Vector3 home = L.home();
    const Vector3 b1 = L.firstBase(), b2 = L.secondBase(), b3 = L.thirdBase();
    const sf::Color grass = grassColor();
    const sf::Color gDk = grassDarkColor();
    const sf::Color dirt = dirtColor();
    const sf::Color dDk(148, 98, 54);
    const sf::Color track = warningTrackColor();
    const float trackW = 3.4f;

    // Fair grass pie, mowed as a pinwheel of wide triangular wedges radiating
    // from home plate (the real, very recognizable MLB crosshatch look),
    // rather than concentric rings.
    //
    // The home-plate dirt circle is colored INTO the inner wedges instead of
    // overlaid as a separate disk: these wedge triangles are ~100 units long,
    // and stacked coplanar overlays lose the depth test to them (screen-space
    // z interpolation error on huge thin tris) no matter the y offset.
    const float homeDirtR = 5.8f;
    const int fairSegs = 96;
    const int mowWedges = 18;
    for (int i = 0; i < fairSegs; i++) {
        float t0 = static_cast<float>(i) / fairSegs;
        float t1 = static_cast<float>(i + 1) / fairSegs;
        float ang0 = aL + (aR - aL) * t0;
        float ang1 = aL + (aR - aL) * t1;
        float rFence0 = L.wallRAtAngle(ang0);
        float rFence1 = L.wallRAtAngle(ang1);
        float rGrass0 = rFence0 - trackW;
        float rGrass1 = rFence1 - trackW;

        // Inner dirt disc (home circle) as part of the same surface.
        addQuad(
            m, L.fromHome(0.02f, ang0, 0.0f), L.fromHome(0.02f, ang1, 0.0f),
            L.fromHome(homeDirtR, ang1, 0.0f), L.fromHome(homeDirtR, ang0, 0.0f),
            dirt
        );

        int wedgeIdx = (i * mowWedges) / fairSegs;
        sf::Color fc = (wedgeIdx % 2 == 0) ? grass : gDk;
        addQuad(
            m, L.fromHome(homeDirtR, ang0, 0.0f), L.fromHome(homeDirtR, ang1, 0.0f),
            L.fromHome(rGrass1, ang1, 0.0f), L.fromHome(rGrass0, ang0, 0.0f), fc
        );

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
        // Start at the plate itself — the fair pie covers r>=0.02 in fair
        // territory, and foul territory must meet it or a bare ring shows
        // through behind home.
        float rIn = 0.02f;
        // Continue the home dirt circle into foul territory on the same
        // surface (no coplanar overlay — see the fair-pie note above).
        float rDirtOut = std::min(homeDirtR + 0.3f, rSeat);
        if (rDirtOut > rIn) {
            addQuad(
                m, L.fromHome(rIn, ang0, 0.002f), L.fromHome(rIn, ang1, 0.002f),
                L.fromHome(rDirtOut, ang1, 0.002f), L.fromHome(rDirtOut, ang0, 0.002f),
                dirt
            );
        }
        if (rSeat > rDirtOut) {
            addQuad(
                m, L.fromHome(rDirtOut, ang0, 0.002f), L.fromHome(rDirtOut, ang1, 0.002f),
                L.fromHome(rSeat, ang1, 0.002f), L.fromHome(rSeat, ang0, 0.002f),
                shade(grass, 0.90f + 0.04f * hash01(i))
            );
        }
    }

    // MLB grass-infield look: the mowed fair grass runs uninterrupted under
    // the whole diamond (Yankee / Dodger / Wrigley style). Dirt shows only as
    // the home circle, the base cutouts, and the mound — no filled dirt
    // diamond or skinned arc band (that minority old-turf-park look read
    // wrong at a glance).
    //
    // (The base "paths" stay grass; only the cutouts are dirt.)

    // Mound, home circle, bases
    // Raised pitcher's mound: two sloped tiers up to a flat table (~10" in
    // MLB scale ≈ 0.42 world units) with the white rubber on top.
    // NOTE: dirt sits a full 0.05 above the grass — anything thinner loses
    // the depth test to the 100-unit mow-wedge triangles (barycentric
    // precision on long thin tris) and green spikes bleed through.
    addDisk(m, L.mound(), 4.4f, 0.05f, 32, shade(dirt, 0.95f));
    addFrustum(m, L.mound(), 4.4f, 0.05f, 3.2f, 0.15f, 30, dirt);
    addDisk(m, L.mound(), 3.2f, 0.15f, 30, dirt);
    addFrustum(m, L.mound(), 3.2f, 0.15f, 2.1f, 0.28f, 26, shade(dirt, 1.02f));
    addDisk(m, L.mound(), 2.1f, 0.28f, 26, shade(dirt, 1.04f));
    addBox(
        m, L.mound() + Vector3(0.0f, 0.30f, 0.55f), 1.45f, 0.055f, 0.45f,
        sf::Color(248, 248, 245)
    );
    // (Home dirt circle is baked into the fair-pie / foul-fill surfaces above.)
    addDisk(m, b1, 2.9f, 0.05f, 20, dirt);
    addDisk(m, b2, 2.9f, 0.05f, 20, dirt);
    addDisk(m, b3, 2.9f, 0.05f, 20, dirt);
    auto bag = [&](Vector3 p) {
        addBox(m, p + Vector3(0, 0.09f, 0), 0.88f, 0.09f, 0.88f, sf::Color(248, 248, 245));
    };
    bag(b1);
    bag(b2);
    bag(b3);
    // Home plate
    {
        float pz = L.plateZ();
        Vector3 tip(0, 0.075f, pz + 0.55f);
        Vector3 bl(-0.55f, 0.075f, pz - 0.35f), br(0.55f, 0.075f, pz - 0.35f);
        Vector3 fl(-0.55f, 0.075f, pz + 0.12f), fr(0.55f, 0.075f, pz + 0.12f);
        addTri(m, tip, fl, fr, sf::Color(252, 252, 250));
        addQuad(m, fl, fr, br, bl, sf::Color(252, 252, 250));
    }
    // Batter boxes
    addBox(m, Vector3(-1.7f, 0.06f, L.plateZ() - 0.2f), 2.1f, 0.02f, 3.1f, shade(dDk, 1.05f));
    addBox(m, Vector3(1.7f, 0.06f, L.plateZ() - 0.2f), 2.1f, 0.02f, 3.1f, shade(dDk, 1.05f));

    // On-deck circles — chalk-rimmed dirt discs behind home, both sides.
    for (float cx : {-7.5f, 7.5f}) {
        Vector3 oc(cx, 0.0f, L.plateZ() + 6.5f);
        addDisk(m, oc, 2.6f, 0.05f, 24, shade(dirt, 0.92f));
        addRing(m, oc, 2.35f, 2.6f, 0.056f, 24, sf::Color(248, 248, 245));
    }

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
    const float baseWallH = L.wallHeightFeet / L.feetPerUnit;
    // Fenway-style dark green for the tall LF wall.
    const sf::Color monsterFace(19, 62, 38);
    const sf::Color monsterTop(32, 82, 50);
    // Stand-ins for the ad-board ribbon lining a real OF wall — flat blocks
    // of color (no text/logos possible on this texture-less renderer), just
    // enough variation to read as "advertising" rather than a blank wall.
    const sf::Color adBoards[] = {
        sf::Color(225, 30, 40),   sf::Color(245, 245, 248), sf::Color(20, 70, 140),
        sf::Color(240, 200, 30),  sf::Color(30, 120, 70),
    };

    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = aL + (aR - aL) * t0;
        float ang1 = aL + (aR - aL) * t1;
        float angM = 0.5f * (ang0 + ang1);
        float r0 = L.wallRAtAngle(ang0);
        float r1 = L.wallRAtAngle(ang1);
        float h0 = L.wallHeightAtAngle(ang0);
        float h1 = L.wallHeightAtAngle(ang1);
        bool monster = L.wallHeightAtAngle(angM) > baseWallH * 1.4f;
        sf::Color faceC = monster ? monsterFace : face;
        sf::Color topC = monster ? monsterTop : top;
        sf::Color fc = shade(faceC, 0.92f + 0.12f * ((i % 4) / 3.0f));
        // Ad-board strip across the lower half of the pad, gapped so a
        // sliver of blue pad still shows top/bottom like a real wall.
        float adY0 = h0 * 0.18f;
        float adY1 = h0 * 0.62f;
        addQuad(
            m, L.fromHome(r0, ang0, 0.0f), L.fromHome(r1, ang1, 0.0f),
            L.fromHome(r1, ang1, adY1), L.fromHome(r0, ang0, adY0), fc
        );
        addQuad(
            m, L.fromHome(r0, ang0, adY0), L.fromHome(r1, ang1, adY1),
            L.fromHome(r1, ang1, h1), L.fromHome(r0, ang0, h0), fc
        );
        // The tall wall stays bare green (no pasted ads) like the real one.
        if (!monster && i % 4 != 0) {
            sf::Color ad = adBoards[(i / 4) % 5];
            addQuad(
                m, L.fromHome(r0 + 0.03f, ang0, adY0 + h0 * 0.05f),
                L.fromHome(r1 + 0.03f, ang1, adY1 + h1 * 0.05f),
                L.fromHome(r1 + 0.03f, ang1, h1 * 0.95f), L.fromHome(r0 + 0.03f, ang0, h0 * 0.95f),
                ad
            );
        }
        // Thickness + outside face (connects to bleachers)
        addQuad(
            m, L.fromHome(r0 + 1.4f, ang0, 0.0f), L.fromHome(r0, ang0, 0.0f),
            L.fromHome(r0, ang0, h0), L.fromHome(r0 + 1.4f, ang0, h0), shade(faceC, 0.8f)
        );
        addQuad(
            m, L.fromHome(r0, ang0, h0), L.fromHome(r1, ang1, h1),
            L.fromHome(r1 + 1.4f, ang1, h1), L.fromHome(r0 + 1.4f, ang0, h0), topC
        );
        // Gold trim stripe capping the pad on the field side — the Crown
        // Jewel signature line running the full length of the OF wall.
        addQuad(
            m, L.fromHome(r0 - 0.04f, ang0, h0 - 0.44f), L.fromHome(r1 - 0.04f, ang1, h1 - 0.44f),
            L.fromHome(r1 - 0.04f, ang1, h1 + 0.02f), L.fromHome(r0 - 0.04f, ang0, h0 + 0.02f),
            seatGoldColor()
        );
        // Padding panel dividers
        if (i % 3 == 0) {
            addBox(
                m, L.fromHome(r0 + 0.08f, ang0, h0 * 0.5f), 0.1f, h0 * 0.92f, 0.1f,
                shade(faceC, 0.75f)
            );
        }
    }

    // Wall-parallel panel helper (matches the winding of the wall face so it
    // shades/culls the same way) for scoreboards and distance markers.
    auto wallPanel = [&](float ang, float halfW, float y0, float y1, float pushOut, sf::Color col) {
        float r = L.wallRAtAngle(ang) + pushOut;
        float dA = halfW / std::max(r, 1.0f);
        addQuad(
            m, L.fromHome(r, ang - dA, y0), L.fromHome(r, ang + dA, y0),
            L.fromHome(r, ang + dA, y1), L.fromHome(r, ang - dA, y1), col
        );
    };

    // Hand-operated scoreboard set low into the tall LF wall — black panel,
    // sparse lit score slots, the signature retro detail.
    {
        const float sbA = -0.42f;
        const float h = L.wallHeightAtAngle(sbA);
        const float y0 = h * 0.10f;
        const float y1 = h * 0.44f;
        wallPanel(sbA, 7.0f, y0, y1, 0.14f, sf::Color(10, 14, 12));
        for (int row = 0; row < 3; row++) {
            float ry0 = y0 + (y1 - y0) * (0.12f + 0.30f * static_cast<float>(row));
            float ry1 = ry0 + (y1 - y0) * 0.16f;
            for (int col = 0; col < 10; col++) {
                float off = (static_cast<float>(col) - 4.5f) * 1.28f;
                bool lit = hash01(row * 31 + col * 7) > 0.62f;
                sf::Color sc = lit ? sf::Color(235, 225, 130) : sf::Color(26, 36, 30);
                wallPanel(sbA + off / L.wallRAtAngle(sbA), 0.5f, ry0, ry1, 0.17f, sc);
            }
        }
    }

    // Distance markers — bright plates with abstract digit bars (LF pole,
    // dead center, RF porch).
    auto distanceMarker = [&](float ang) {
        const float h = L.wallHeightAtAngle(ang);
        const float y0 = h * 0.66f;
        const float y1 = h * 0.92f;
        wallPanel(ang, 1.7f, y0, y1, 0.12f, sf::Color(245, 245, 240));
        for (int d = 0; d < 3; d++) {
            float off = (static_cast<float>(d) - 1.0f) * 0.85f;
            wallPanel(
                ang + off / L.wallRAtAngle(ang), 0.28f, y0 + (y1 - y0) * 0.18f,
                y0 + (y1 - y0) * 0.82f, 0.15f, sf::Color(30, 40, 60)
            );
        }
    };
    distanceMarker(aL + 0.06f);
    distanceMarker(0.0f);
    distanceMarker(aR - 0.06f);

    // Outfield_wall prop tiled along the outside face (behind the pad, out
    // of the primary infield sightline) — the converted Meshy asset.
    {
        std::optional<Mesh3D> wallProp = loadStaticProp("outfield_wall");
        if (wallProp) {
            LocalBBox bb = computeBBox(*wallProp);
            Vector3 sz = bb.size();
            if (sz.y > 1e-4f) {
                const int tiles = 24;
                for (int i = 0; i < tiles; i++) {
                    float t = (static_cast<float>(i) + 0.5f) / tiles;
                    float ang = aL + (aR - aL) * t;
                    float r = L.wallRAtAngle(ang) + 1.6f;
                    float h = L.wallHeightAtAngle(ang);
                    float s = h / sz.y;
                    Vector3 base = L.fromHome(r, ang, -bb.mn.y * s);
                    appendPropInstance(m, *wallProp, Vector3(s, s, s), ang, base);
                }
            }
        }
    }

    // Foul poles (tall, with screen wings)
    auto pole = [&](float ang) {
        float r = L.wallRAtAngle(ang);
        float h = L.wallHeightAtAngle(ang) * 4.0f;
        Vector3 base = L.fromHome(r, ang, 0.0f);
        addBox(m, base + Vector3(0, h * 0.5f, 0), 0.5f, h, 0.5f, foulPoleColor());
        addBox(m, base + Vector3(0, h + 0.5f, 0), 1.4f, 0.55f, 0.18f, foulPoleColor());
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
    sf::Color goldA = seatGoldColor(), goldB = seatGoldAltColor();
    // Dark forest-green risers to match the Crown Jewel bowl palette.
    sf::Color riserBlue(16, 40, 30);
    sf::Color riserGold(120, 95, 35);
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
        bool isAisle = (i % 10) == 0;

        // OF bleachers: fewer rows, low; horseshoe: deep lower bowl
        int rowsLower = ofBleach ? 8 : (club ? 16 : 15);
        float r = rIn;
        float y = yBase;

        for (int row = 0; row < rowsLower; row++) {
            float r1 = r + dRow * 0.92f;
            float y1 = y + rise * 0.82f;
            // Gold reserve-level band tops the last few rows of every
            // section, Dodger Stadium-style, instead of a per-corner team
            // color — the rest of the bowl is a consistent field blue.
            bool useGold = row >= rowsLower - 3;
            sf::Color sc;
            if (isAisle) {
                sc = aisle;
            } else if (useGold) {
                sc = (row + i) % 2 ? goldA : goldB;
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
                useGold ? riserGold : riserBlue
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

        // Crown Jewel grandstand behind home: suite ring (sandstone fascia
        // + glass front + gold mullions), a steep forest-green upper bowl,
        // gold fascia ribbon, and a white cantilevered roof canopy with
        // truss ribs and a warm LED strip under the front lip.
        if (club) {
            const sf::Color roofWhite(238, 238, 236);
            const sf::Color ledWarm(255, 236, 170);
            float rU = rC1 + 0.6f;
            float yU = yC + 3.2f;
            // Sandstone fascia band at the suite level base.
            addQuad(
                m, L.fromHome(rC1, ang0, yC), L.fromHome(rC1, ang1, yC),
                L.fromHome(rC1, ang1, yU + 0.4f), L.fromHome(rC1, ang0, yU + 0.4f),
                facadeGrayColor()
            );
            // Suite ring: continuous glass front.
            addQuad(
                m, L.fromHome(rC1 + 0.15f, ang0, yU + 0.4f), L.fromHome(rC1 + 0.15f, ang1, yU + 0.4f),
                L.fromHome(rC1 + 0.15f, ang1, yU + 3.4f), L.fromHome(rC1 + 0.15f, ang0, yU + 3.4f),
                sf::Color(80, 130, 160, 150)
            );
            // Gold mullion dividers every few bays.
            if (i % 4 == 0) {
                addQuad(
                    m, L.fromHome(rC1 + 0.12f, ang0, yU + 0.35f),
                    L.fromHome(rC1 + 0.12f, ang1, yU + 0.35f),
                    L.fromHome(rC1 + 0.12f, ang1, yU + 3.45f),
                    L.fromHome(rC1 + 0.12f, ang0, yU + 3.45f),
                    seatGoldColor()
                );
            }
            // Steep upper bowl — 9 rows of forest green under the roof.
            float rRow = rU + 0.4f;
            float yRow = yU + 3.6f;
            for (int row = 0; row < 9; row++) {
                float r1 = rRow + dRow * 0.82f;
                float y1 = yRow + rise * 0.72f;
                sf::Color sc = isAisle ? aisle : ((row + i) % 2 ? blueA : blueB);
                sc = shade(sc, 0.93f + 0.07f * hash01(i * 7 + row));
                addQuad(
                    m, L.fromHome(rRow, ang0, y1), L.fromHome(rRow, ang1, y1),
                    L.fromHome(r1, ang1, y1), L.fromHome(r1, ang0, y1), sc
                );
                addQuad(
                    m, L.fromHome(rRow, ang0, yRow), L.fromHome(rRow, ang1, yRow),
                    L.fromHome(rRow, ang1, y1), L.fromHome(rRow, ang0, y1), riserBlue
                );
                rRow = r1 + 0.05f;
                yRow += rise * 0.9f;
            }
            // Gold fascia ribbon capping the upper bowl.
            addQuad(
                m, L.fromHome(rRow, ang0, yRow), L.fromHome(rRow, ang1, yRow),
                L.fromHome(rRow + 0.3f, ang1, yRow + 1.1f), L.fromHome(rRow + 0.3f, ang0, yRow + 1.1f),
                seatGoldColor()
            );
            // White cantilevered roof canopy rising gently outward.
            float roofY0 = yRow + 1.2f;
            addQuad(
                m, L.fromHome(rRow - 0.4f, ang0, roofY0), L.fromHome(rRow - 0.4f, ang1, roofY0),
                L.fromHome(rRow + 6.5f, ang1, roofY0 + 2.2f), L.fromHome(rRow + 6.5f, ang0, roofY0 + 2.2f),
                roofWhite
            );
            // Sloped underside reaching forward over the suite ring.
            addQuad(
                m, L.fromHome(rC1 + 0.5f, ang0, roofY0 - 0.8f), L.fromHome(rC1 + 0.5f, ang1, roofY0 - 0.8f),
                L.fromHome(rRow - 0.4f, ang1, roofY0 - 0.15f), L.fromHome(rRow - 0.4f, ang0, roofY0 - 0.15f),
                shade(roofWhite, 0.82f)
            );
            // Truss rib struts from fascia to the roof's back edge.
            if (i % 5 == 0) {
                addQuad(
                    m, L.fromHome(rRow + 0.1f, ang0, yRow + 1.0f), L.fromHome(rRow + 0.1f, ang1, yRow + 1.0f),
                    L.fromHome(rRow + 6.5f, ang1, roofY0 + 2.1f), L.fromHome(rRow + 6.5f, ang0, roofY0 + 2.1f),
                    shade(roofWhite, 0.7f)
                );
            }
            // Warm LED ribbon tucked under the roof's front edge.
            addQuad(
                m, L.fromHome(rRow - 0.45f, ang0, roofY0 - 0.18f), L.fromHome(rRow - 0.45f, ang1, roofY0 - 0.18f),
                L.fromHome(rRow - 0.45f, ang1, roofY0 + 0.1f), L.fromHome(rRow - 0.45f, ang0, roofY0 + 0.1f),
                ledWarm
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

    // Bleacher_stand prop tiled behind the OF bleacher rows — the converted
    // Meshy asset, additive structure rather than a replacement for the
    // procedural tiered seating built above.
    {
        std::optional<Mesh3D> bleacherProp = loadStaticProp("bleacher_stand");
        if (bleacherProp) {
            LocalBBox bb = computeBBox(*bleacherProp);
            Vector3 sz = bb.size();
            if (sz.y > 1e-4f) {
                const float foulA = L.foulAngleRad();
                const int tiles = 16;
                for (int i = 0; i < tiles; i++) {
                    float t = (static_cast<float>(i) + 0.5f) / tiles;
                    float ang = -foulA + t * 2.0f * foulA;
                    float r = seatInnerR(L, ang) + 8.5f;
                    float yBase = seatBaseY(L, ang);
                    float h = 7.0f;
                    float s = h / sz.y;
                    Vector3 base = L.fromHome(r, ang, yBase - bb.mn.y * s);
                    appendPropInstance(m, *bleacherProp, Vector3(s, s, s), ang, base);
                }
            }
        }
    }

    // Red seat rows perched on top of the tall LF wall (Monster-style).
    {
        const float mA0 = -0.72f, mA1 = -0.26f;
        const int msegs = 22;
        const float baseWallH = L.wallHeightFeet / L.feetPerUnit;
        for (int i = 0; i < msegs; i++) {
            float t0 = static_cast<float>(i) / msegs;
            float t1 = static_cast<float>(i + 1) / msegs;
            float ang0 = mA0 + (mA1 - mA0) * t0;
            float ang1 = mA0 + (mA1 - mA0) * t1;
            float angM = 0.5f * (ang0 + ang1);
            float h = L.wallHeightAtAngle(angM);
            if (h < baseWallH * 1.2f) {
                continue; // only where the wall is actually tall
            }
            float rIn = L.wallRAtAngle(angM) + 0.75f;
            for (int row = 0; row < 3; row++) {
                float r0 = rIn + row * 1.15f;
                float r1 = r0 + 1.0f;
                float y = h + 0.4f + row * 0.85f;
                sf::Color sc = shade(
                    (row + i) % 2 ? seatRedColor() : seatRedAltColor(),
                    0.93f + 0.07f * hash01(i * 3 + row)
                );
                addQuad(
                    m, L.fromHome(r0, ang0, y), L.fromHome(r0, ang1, y),
                    L.fromHome(r1, ang1, y), L.fromHome(r1, ang0, y), sc
                );
                addQuad(
                    m, L.fromHome(r0, ang0, y - 0.85f), L.fromHome(r0, ang1, y - 0.85f),
                    L.fromHome(r0, ang1, y), L.fromHome(r0, ang0, y), shade(sc, 0.7f)
                );
            }
            // Safety rail along the front edge of the perch.
            addQuad(
                m, L.fromHome(rIn, ang0, h + 0.4f), L.fromHome(rIn, ang1, h + 0.4f),
                L.fromHome(rIn, ang1, h + 1.5f), L.fromHome(rIn, ang0, h + 1.5f),
                sf::Color(200, 205, 210, 130)
            );
        }
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
    // ── Crown Jewel rotunda — brick + sandstone entry block behind home:
    // arched bays, clock medallion, cornice, roofline flag row, side wings.
    sf::Color brick = facadeTanColor();   // re-themed brick red-brown
    sf::Color stone = facadeGrayColor();  // re-themed sandstone
    sf::Color glass(46, 62, 74);
    sf::Color gold = seatGoldColor();

    // Central drum + cornice + parapet.
    addBox(m, Vector3(0, 9.0f, z), 34.0f, 18.0f, 16.0f, brick);
    addBox(m, Vector3(0, 18.6f, z), 37.0f, 1.2f, 18.0f, stone);
    addBox(m, Vector3(0, 20.0f, z), 30.0f, 1.6f, 14.0f, shade(brick, 1.05f));
    // Stone base course + corner quoins.
    addBox(m, Vector3(0, 1.2f, z), 35.0f, 2.4f, 17.0f, stone);
    for (float sx : {-16.6f, 16.6f}) {
        addBox(m, Vector3(sx, 9.0f, z - 7.9f), 1.4f, 18.0f, 1.4f, stone);
    }
    // Arched bays across the field-facing facade — glass insets, sandstone
    // arch heads and sills, keystones, pilasters between bays.
    for (int k = -3; k <= 3; k++) {
        float bx = static_cast<float>(k) * 4.6f;
        addBox(m, Vector3(bx, 8.6f, z - 8.05f), 2.6f, 9.5f, 0.3f, glass);
        addBox(m, Vector3(bx, 13.9f, z - 8.1f), 3.2f, 1.4f, 0.5f, stone);
        addBox(m, Vector3(bx, 3.3f, z - 8.1f), 3.2f, 0.7f, 0.5f, stone);
        addBox(m, Vector3(bx, 14.9f, z - 8.15f), 0.7f, 1.1f, 0.6f, shade(stone, 1.1f));
        if (k < 3) {
            addBox(m, Vector3(bx + 2.3f, 8.6f, z - 8.05f), 0.8f, 10.6f, 0.4f, stone);
        }
    }
    // Clock medallion centered above the arches.
    addBox(m, Vector3(0, 16.4f, z - 8.1f), 4.4f, 4.4f, 0.5f, gold);
    addBox(m, Vector3(0, 16.4f, z - 8.25f), 3.4f, 3.4f, 0.55f, sf::Color(248, 246, 238));
    addBox(m, Vector3(0, 16.9f, z - 8.35f), 0.25f, 1.1f, 0.15f, sf::Color(30, 32, 36));
    addBox(m, Vector3(0.5f, 16.4f, z - 8.35f), 1.0f, 0.25f, 0.15f, sf::Color(30, 32, 36));
    // Roofline flag row — alternating gold / red pennants.
    for (int f = -3; f <= 3; f++) {
        float fx = static_cast<float>(f) * 4.8f;
        addBox(m, Vector3(fx, 23.4f, z), 0.18f, 6.4f, 0.18f, sf::Color(210, 210, 215));
        sf::Color flag = (f % 2 == 0) ? gold : sf::Color(190, 45, 40);
        addBox(m, Vector3(fx + 0.9f, 26.0f, z), 1.7f, 1.0f, 0.1f, flag);
    }
    // Brick side wings with stone cornice + glass strips, entry canopies.
    for (float side : {1.0f, -1.0f}) {
        float wx = side * 30.0f;
        addBox(m, Vector3(wx, 7.5f, z - 1.0f), 24.0f, 15.0f, 15.0f, brick);
        addBox(m, Vector3(wx, 15.3f, z - 1.0f), 25.0f, 0.9f, 16.0f, stone);
        for (int row = 0; row < 2; row++) {
            addBox(m, Vector3(wx, 6.4f + row * 4.6f, z - 8.6f), 20.0f, 2.2f, 0.3f, glass);
        }
        addBox(m, Vector3(wx, 4.6f, z + 7.6f), 12.0f, 0.5f, 5.0f, stone);
        addBox(m, Vector3(wx, 4.3f, z + 7.6f), 12.4f, 0.25f, 5.4f, gold);
    }

    m.rebuildNormals();
    return m;
}

// ═══════════════════════════════════════════════════════════════════════
// SCOREBOARD + OF BOARDS
// ═══════════════════════════════════════════════════════════════════════

Mesh3D buildScoreboardScreen(const Layout& L) {
    Mesh3D m;
    // Ribbon board: thin lit strip capping the OF wall, continuous except
    // for the CF videoboard slot.
    const int n = 36;
    for (int i = 0; i < n; i++) {
        float t = (static_cast<float>(i) + 0.5f) / n;
        float ang = -L.foulAngleRad() * 0.94f + t * L.foulAngleRad() * 1.88f;
        if (std::abs(ang) < 0.075f) {
            continue; // CF videoboard slot
        }
        float r = L.wallRAtAngle(ang) + 0.4f;
        float h = L.wallHeightAtAngle(ang);
        addBox(
            m, L.fromHome(r, ang, h + 0.55f), 5.4f, 1.1f, 0.5f,
            shade(sf::Color(14, 24, 58), 0.92f + 0.14f * (i % 3) / 2.0f)
        );
    }

    // One dominant videoboard parked dead-center, placed exactly on
    // scoreboardCenter() so the visual chassis matches the collision solid
    // (isCfScoreboardZone). Camera (batter/mound) sits on the +Z side
    // looking toward -Z, so foreground layers carry increasingly positive z.
    std::optional<Mesh3D> scoreboardProp = loadStaticProp("scoreboard");
    LocalBBox scoreboardBBox;
    if (scoreboardProp) {
        scoreboardBBox = computeBBox(*scoreboardProp);
    }
    {
        Vector3 c = L.scoreboardCenter();
        const float bw = 30.0f; // ~60 ft face
        const float bh = 11.0f; // matches collision half-height 5.5
        if (scoreboardProp && scoreboardBBox.size().x > 1e-4f && scoreboardBBox.size().y > 1e-4f) {
            Vector3 sz = scoreboardBBox.size();
            Vector3 scale(bw / sz.x, bh / sz.y, sz.z > 1e-4f ? 1.5f / sz.z : 1.0f);
            appendPropInstance(m, *scoreboardProp, scale, 0.0f, c);
        } else {
            // Truss backing, furthest back.
            addBox(m, c + Vector3(0, 0, -1.0f), bw + 3.0f, bh + 2.4f, 1.2f, sf::Color(36, 40, 45));
            // Screen face, in front of the frame.
            addBox(m, c, bw, bh, 0.9f, sf::Color(8, 12, 20));
            // "Active content" blocks, in front of the screen.
            addBox(m, c + Vector3(-8.6f, 2.1f, 0.5f), 10.4f, 5.6f, 0.18f, sf::Color(40, 95, 160));
            addBox(m, c + Vector3(7.2f, 1.2f, 0.5f), 11.6f, 4.2f, 0.18f, sf::Color(60, 140, 75));
            addBox(m, c + Vector3(0.5f, -3.0f, 0.5f), 20.0f, 3.4f, 0.18f, sf::Color(190, 55, 45));
        }
        // Crown silhouette capping the board (prop or procedural) — gold
        // base band plus three stepped points, the park's signature mark.
        addBox(m, c + Vector3(0, bh * 0.5f + 1.1f, -0.3f), bw * 0.66f, 1.5f, 1.4f, seatGoldColor());
        for (int pt = -1; pt <= 1; pt++) {
            float px = static_cast<float>(pt) * (bw * 0.22f);
            float ph = (pt == 0) ? 3.4f : 2.2f;
            addBox(
                m, c + Vector3(px, bh * 0.5f + 1.85f + ph * 0.5f, -0.3f), 2.6f, ph, 1.2f,
                shade(seatGoldColor(), pt == 0 ? 1.06f : 0.96f)
            );
        }
        // Triple support masts dropping into the CF bleachers.
        float footY = L.wallHeightAtAngle(0.0f) + 0.3f;
        for (float sx : {-10.5f, 0.0f, 10.5f}) {
            float topY = c.y - bh * 0.5f + 1.0f;
            float postH = std::max(topY - footY, 2.0f);
            addBox(
                m, Vector3(c.x + sx, footY + postH * 0.5f, c.z - 1.6f), 1.0f, postH, 1.0f,
                facadeGrayColor()
            );
        }
    }

    // Batter's eye: dark green backdrop filling the gap between the CF wall
    // top and the underside of the videoboard, so hitters get a clean
    // contrast background.
    {
        float r = L.wallRAtAngle(0.0f) + 0.7f;
        float wallH = L.wallHeightAtAngle(0.0f);
        float dA = 11.5f / r;
        addQuad(
            m, L.fromHome(r, -dA, wallH), L.fromHome(r, dA, wallH),
            L.fromHome(r, dA, 15.8f), L.fromHome(r, -dA, 15.8f), sf::Color(15, 40, 26)
        );
        // Vertical slat shading so the eye reads as louvers, not a flat card.
        for (int k = -5; k <= 5; k++) {
            float a = static_cast<float>(k) * (dA / 5.5f);
            addBox(
                m, L.fromHome(r + 0.1f, a, (wallH + 15.8f) * 0.5f), 0.22f, 15.8f - wallH, 0.12f,
                sf::Color(10, 28, 18)
            );
        }
    }

    // Secondary out-of-town board in right-center.
    {
        float ang = 0.42f;
        float r = L.wallRAtAngle(ang) + 4.0f;
        float baseY = L.wallHeightAtAngle(ang) + 8.0f;
        Vector3 c = L.fromHome(r, ang, baseY);
        addBox(m, c + Vector3(0, 0, -0.6f), 15.5f, 8.0f, 1.0f, sf::Color(36, 40, 45));
        addBox(m, c, 14.0f, 6.8f, 0.7f, sf::Color(10, 14, 24));
        addBox(m, c + Vector3(0, 1.5f, 0.4f), 12.0f, 2.6f, 0.15f, sf::Color(45, 100, 170));
        addBox(m, c + Vector3(0, -1.7f, 0.4f), 12.0f, 2.4f, 0.15f, sf::Color(190, 150, 45));
        float postH = std::max(baseY - 4.0f - 1.0f, 1.0f);
        float postOffY = -(baseY - 4.0f + 1.0f) * 0.5f;
        addBox(m, c + Vector3(-5.4f, postOffY, 0.6f), 0.7f, postH, 0.7f, facadeGrayColor());
        addBox(m, c + Vector3(5.4f, postOffY, 0.6f), 0.7f, postH, 0.7f, facadeGrayColor());
    }

    // Corner sign plates (abstract ad boards, one per foul corner)
    for (float side : {1.0f, -1.0f}) {
        float ang = side * L.foulAngleRad() * 0.75f;
        Vector3 c = L.fromHome(L.wallRAtAngle(ang) + 3.5f, ang, 5.8f);
        addBox(m, c, 11.0f, 5.0f, 0.55f, sf::Color(248, 248, 250));
        addBox(m, c + Vector3(0, 0, -0.35f), 9.0f, 2.4f, 0.15f, seatGoldColor());
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
    sf::Color brace = shade(pole, 0.8f);
    sf::Color lamp(255, 252, 235);

    // Modeled light-tower prop (pole + lamp bank), decimated + vertex-color
    // baked from the Meshy asset. Falls back to a procedural pole cluster
    // if the asset isn't present alongside the build.
    std::optional<Mesh3D> lightPoleProp = loadStaticProp("light_pole");
    LocalBBox lightPoleBBox;
    if (lightPoleProp) {
        lightPoleBBox = computeBBox(*lightPoleProp);
    }

    // Tight cluster of thin poles towering well above the roofline, capped
    // by a big dense lamp grid — the iconic Dodger Stadium light standard
    // silhouette (a handful of very tall towers, not a ring of short ones).
    auto tower = [&](float ang, float r, float h) {
        Vector3 base = L.fromHome(r, ang, 0.0f);
        if (lightPoleProp && lightPoleBBox.size().y > 1e-4f) {
            float s = h / lightPoleBBox.size().y;
            Vector3 translate = base + Vector3(0, -lightPoleBBox.mn.y * s, 0);
            appendPropInstance(m, *lightPoleProp, Vector3(s, s, s), ang, translate);
            return;
        }
        const float spread = 0.65f;
        Vector3 poleOff[5] = {
            Vector3(0, 0, 0), Vector3(-spread, 0, -spread * 0.4f),
            Vector3(spread, 0, -spread * 0.4f), Vector3(-spread * 0.55f, 0, spread * 0.75f),
            Vector3(spread * 0.55f, 0, spread * 0.75f)
        };
        for (const auto& o : poleOff) {
            addBox(m, base + o + Vector3(0, h * 0.5f, 0), 0.24f, h, 0.24f, pole);
        }
        // A couple of collar bands low on the cluster for a hint of bracing.
        for (int ri = 1; ri <= 2; ri++) {
            float ry = h * (static_cast<float>(ri) / 5.0f);
            for (int k = 1; k < 5; k++) {
                Vector3 p0 = base + poleOff[0] + Vector3(0, ry, 0);
                Vector3 p1 = base + poleOff[k] + Vector3(0, ry, 0);
                Vector3 mid = (p0 + p1) * 0.5f;
                float len = (p1 - p0).magnitude();
                bool alongX = std::abs(p1.x - p0.x) > std::abs(p1.z - p0.z);
                addBox(m, mid, alongX ? len : 0.1f, 0.1f, alongX ? 0.1f : len, brace);
            }
        }
        // Big flat lamp-grid panel on top, wide and dense.
        Vector3 panelC = base + Vector3(0, h + 2.2f, 0);
        addBox(m, panelC, 10.0f, 4.4f, 0.4f, facadeGrayColor());
        const int cols = 11;
        const int rowsN = 5;
        for (int cy = 0; cy < rowsN; cy++) {
            for (int cx = 0; cx < cols; cx++) {
                float lx = (static_cast<float>(cx) - (cols - 1) * 0.5f) * 0.86f;
                float ly = (static_cast<float>(cy) - (rowsN - 1) * 0.5f) * 0.78f;
                addBox(m, panelC + Vector3(lx, ly, -0.22f), 0.56f, 0.5f, 0.1f, lamp);
            }
        }
    };

    // Four towers total: two dominant standards flanking the CF videoboard
    // and a shorter pair just inside the foul poles — sparse but massive,
    // leaving the downtown skyline behind center field unobstructed.
    struct TowerSpec {
        float angleRad;
        float reserved;
        float height;
    };
    const TowerSpec towers[] = {
        {.angleRad = 0.55f, .reserved = 0.0f, .height = 92.0f},
        {.angleRad = -0.55f, .reserved = 0.0f, .height = 92.0f},
        {.angleRad = L.foulAngleRad() - 0.12f, .reserved = 0.0f, .height = 66.0f},
        {.angleRad = -(L.foulAngleRad() - 0.12f), .reserved = 0.0f, .height = 66.0f},
    };
    for (const auto& t : towers) {
        tower(t.angleRad, L.wallRAtAngle(t.angleRad) + 28.0f, t.height);
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

    // A couple of small team-facility buildings behind home — real parks
    // have some structure there, but nothing city-scale.
    float pz = L.plateZ();
    addBox(m, Vector3(46.0f, 5.0f, pz + 10.0f), 20.0f, 10.0f, 26.0f, facadeGrayColor());
    addBox(m, Vector3(-46.0f, 5.0f, pz + 10.0f), 20.0f, 10.0f, 26.0f, facadeGrayColor());

    // ── Downtown skyline — the Crown Jewel park sits in the city: a central
    // business district rising behind the outfield, densest dead center so
    // the videoboard plays against towers. Low civic blocks fill the ring
    // behind home so the batter's view stays open.
    const sf::Color towerGlass[] = {
        sf::Color(52, 68, 88),  sf::Color(38, 52, 70),  sf::Color(70, 88, 110),
        sf::Color(88, 96, 108), sf::Color(46, 60, 80),
    };
    const sf::Color towerLit(255, 214, 130);
    const sf::Color crownGold(232, 200, 110);
    const int towers = 46;
    for (int t = 0; t < towers; t++) {
        // Bias placement toward CF (ang ≈ 0) with jitter.
        float ang = (hash01(t * 13 + 5) - 0.5f) * 2.6f + (hash01(t * 7) - 0.5f) * 0.18f;
        float r = rSuburb + 60.0f + hash01(t * 11) * (rFar - rSuburb - 100.0f);
        float tw2 = 26.0f + hash01(t * 3) * 30.0f;
        float th = 55.0f + hash01(t * 17) * 130.0f;
        // Tallest cluster dead-center — the postcard view.
        th *= 1.0f + 0.55f * (1.0f - std::min(std::abs(ang) / 1.3f, 1.0f));
        Vector3 c = L.fromHome(r, ang, yG);
        sf::Color body = shade(towerGlass[t % 5], 0.85f + 0.3f * hash01(t * 23));
        // Main shaft + setback crown — the classic setback-skyscraper stack.
        addBox(m, c + Vector3(0, th * 0.5f, 0), tw2, th, tw2 * 0.85f, body);
        addBox(
            m, c + Vector3(0, th * 0.88f, 0), tw2 * 0.72f, th * 0.24f, tw2 * 0.6f,
            shade(body, 1.08f)
        );
        if (t % 7 == 0) { // spire towers punctuate the skyline
            addBox(m, c + Vector3(0, th + 8.0f, 0), tw2 * 0.16f, 16.0f, tw2 * 0.16f, shade(body, 1.2f));
            addBox(m, c + Vector3(0, th + 16.5f, 0), tw2 * 0.5f, 1.2f, tw2 * 0.5f, crownGold);
        }
        // Sparse warm lit windows on the park-facing side.
        int winRows = 6 + static_cast<int>(th / 26.0f);
        for (int wr = 0; wr < winRows; wr++) {
            float wy = th * (0.18f + 0.72f * static_cast<float>(wr) / static_cast<float>(winRows));
            for (int wc = 0; wc < 5; wc++) {
                if (hash01(t * 101 + wr * 13 + wc * 7) < 0.52f) {
                    continue;
                }
                float off = (static_cast<float>(wc) - 2.0f) * tw2 * 0.17f;
                Vector3 wp = L.fromHome(r - tw2 * 0.44f, ang + off / r, yG + wy);
                addBox(m, wp, 1.6f, 2.2f, 0.3f, shade(towerLit, 0.8f + 0.35f * hash01(t + wr + wc)));
            }
        }
        // Street trees at the tower base soften the parking-to-city edge.
        if (t % 3 == 0) {
            addTree(m, L.fromHome(r - tw2 * 0.8f, ang + 0.01f, yG), 1.2f + hash01(t * 31), t * 17);
        }
    }
    // Low civic blocks filling the ring behind home plate.
    for (int i = 0; i < 18; i++) {
        float ang = pi - 0.9f + (static_cast<float>(i) + 0.5f) / 18.0f * 1.8f;
        float r = rSuburb + 40.0f + hash01(i * 19) * 120.0f;
        float bh = 14.0f + hash01(i * 29) * 22.0f;
        addBox(
            m, L.fromHome(r, ang, yG + bh * 0.5f), 34.0f + hash01(i * 7) * 26.0f, bh,
            24.0f + hash01(i * 11) * 18.0f,
            shade(sf::Color(96, 100, 108), 0.8f + 0.25f * hash01(i * 5))
        );
    }
    // Far horizon silhouette — hazy distant towers blending into the sky.
    for (int i = 0; i < 22; i++) {
        float ang = -pi + (static_cast<float>(i) + 0.5f) / 22.0f * 2.0f * pi;
        float r = rHorizon - 60.0f;
        float h = 40.0f + hash01(i * 5) * 95.0f;
        addBox(
            m, L.fromHome(r, ang, h * 0.5f + yG), 34.0f + hash01(i) * 34.0f, h, 26.0f,
            shade(sf::Color(140, 150, 165), 0.62f + 0.16f * hash01(i + 2))
        );
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
        // Above the 0.05 dirt cutouts (and well above the 0.002 grass) so the
        // chalk never loses the depth test to the huge ground triangles.
        addPath(m, a, b, hw, 0.065f, chalk);
    };
    float aL = -L.foulAngleRad(), aR = L.foulAngleRad();
    line(L.home(), L.fromHome(L.wallRAtAngle(aL) + 0.5f, aL, 0), 0.13f);
    line(L.home(), L.fromHome(L.wallRAtAngle(aR) + 0.5f, aR, 0), 0.13f);
    // No chalk between the bases — real MLB parks only paint the foul lines,
    // the batter's boxes, and the catcher's box.
    // Catcher's box (directly behind home plate)
    {
        float hw = 1.05f;
        float z0 = L.plateZ() + 0.45f;
        float z1 = L.plateZ() + 2.35f;
        Vector3 a(-hw, 0, z0), b(hw, 0, z0), c(hw, 0, z1), d(-hw, 0, z1);
        line(a, b, 0.05f);
        line(b, c, 0.05f);
        line(c, d, 0.05f);
        line(d, a, 0.05f);
    }
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
    // Deliberately avoids seatBlueColor()/seatRedColor() — using the exact
    // riser paint as a shirt color camouflaged ~20% of the crowd against
    // the seat background, especially at a distance/oblique angle where
    // the fan figures were already a handful of pixels.
    const sf::Color opts[] = {
        sf::Color(245, 245, 248), sf::Color(35, 35, 45), sf::Color(25, 110, 65),
        sf::Color(210, 160, 40),  sf::Color(90, 50, 120), sf::Color(200, 90, 40),
        sf::Color(50, 140, 180),  sf::Color(160, 40, 70), sf::Color(230, 210, 40),
        sf::Color(220, 220, 225)};
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
    // Row walk mirrors buildStands EXACTLY (same dRow/rise/row counts and the
    // same concourse → suite → upper-bowl offsets) so every fan sits on a
    // real seat tread: a dense foul-pole-to-foul-pole MLB sellout bowl.
    // Previously fans filled only the front ~9 rows of 15–16-row sections
    // (bare seat backs down both lines) and the upper-deck crowd behind home
    // floated in front of the glass (the "dome of people").
    const int angSamples = 200;
    const float dRow = 1.35f;
    const float rise = 0.88f;
    int fanId = 0;

    for (int i = 0; i < angSamples; i++) {
        float t = (static_cast<float>(i) + 0.5f) / angSamples;
        float ang = -pi + t * 2.0f * pi;
        int sector = static_cast<int>(t * kFanSectorCount) % kFanSectorCount;
        bool ofBleach = isOfBleacher(L, ang);
        bool club = isClubZone(ang);
        // Walkway aisles stay mostly clear, matching the gray aisle paint.
        const float aisleFill = (i % 10 == 0) ? 0.15f : 1.0f;

        auto seatFan = [&](float rRow, float ySeat, float fillBase) {
            if (hash01(fanId * 13 + 7) > fillBase * aisleFill) {
                fanId++;
                return;
            }
            Vector3 seat = L.fromHome(rRow, ang, ySeat);
            seat.x += (hash01(fanId) - 0.5f) * 0.42f;
            seat.z += (hash01(fanId + 3) - 0.5f) * 0.42f;
            float sc = 0.95f + 0.35f * hash01(fanId + 9);
            addFan(sectors[sector], seat, sc, fanShirt(fanId), fanSkin(fanId + 4));
            fanId++;
        };

        float r = seatInnerR(L, ang);
        float y = seatBaseY(L, ang);
        const int rowsLower = ofBleach ? 8 : (club ? 16 : 15);
        for (int row = 0; row < rowsLower; row++) {
            float y1 = y + rise * 0.82f; // tread top, same as the seat quad
            float fill = ofBleach ? 0.88f : (row < 4 ? 0.93f : 0.86f);
            seatFan(r + dRow * 0.45f, y1 + 0.02f, fill);
            r = r + dRow * 0.92f + dRow * 0.05f;
            y += rise;
        }

        if (club) {
            // Upper bowl above the suite glass — same walk as buildStands:
            // concourse (4.2) → fascia (0.6 + 0.4) → 9 steep rows.
            float rC1 = r + 4.2f;
            float yC = y + 0.15f;
            float rRow = rC1 + 0.6f + 0.4f;
            float yRow = yC + 3.2f + 3.6f;
            for (int row = 0; row < 9; row++) {
                float y1 = yRow + rise * 0.72f;
                seatFan(rRow + dRow * 0.4f, y1 + 0.02f, 0.8f);
                rRow = rRow + dRow * 0.82f + 0.05f;
                yRow += rise * 0.9f;
            }
        }
    }

    // Monster-top perch pass — fans in the seats atop the tall LF wall.
    {
        const float baseWallH = L.wallHeightFeet / L.feetPerUnit;
        for (int i = 0; i < 30; i++) {
            float t = (static_cast<float>(i) + 0.5f) / 30.0f;
            float ang = -0.70f + t * 0.42f;
            float h = L.wallHeightAtAngle(ang);
            if (h < baseWallH * 1.2f) {
                continue;
            }
            int sector = static_cast<int>(((ang + pi) / (2.0f * pi)) * kFanSectorCount) %
                         kFanSectorCount;
            if (sector < 0) {
                sector += kFanSectorCount;
            }
            for (int row = 0; row < 3; row++) {
                if (hash01(i * 41 + row * 11) > 0.85f) {
                    continue;
                }
                float r = L.wallRAtAngle(ang) + 0.9f + row * 1.15f;
                float y = h + 0.85f + row * 0.85f;
                Vector3 seat = L.fromHome(r, ang, y);
                seat.x += (hash01(i * 7 + row) - 0.5f) * 0.3f;
                addFan(
                    sectors[sector], seat, 0.9f, fanShirt(i * 5 + row + 20),
                    fanSkin(i + row + 2)
                );
            }
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
    struct FenceSample {
        float angleDeg;
        float feet;
    };
    static const FenceSample samples[] = {
        {.angleDeg = -45.0f, .feet = 328.0f}, {.angleDeg = -28.0f, .feet = 350.0f},
        {.angleDeg = -15.0f, .feet = 370.0f}, {.angleDeg = 0.0f, .feet = 400.0f},
        {.angleDeg = 15.0f, .feet = 368.0f},  {.angleDeg = 28.0f, .feet = 340.0f},
        {.angleDeg = 45.0f, .feet = 322.0f},
    };
    constexpr int n = 7;
    if (aDeg <= samples[0].angleDeg) {
        return samples[0].feet;
    }
    if (aDeg >= samples[n - 1].angleDeg) {
        return samples[n - 1].feet;
    }
    for (int i = 0; i < n - 1; i++) {
        if (aDeg >= samples[i].angleDeg && aDeg <= samples[i + 1].angleDeg) {
            float u = (aDeg - samples[i].angleDeg) / (samples[i + 1].angleDeg - samples[i].angleDeg);
            u = u * u * (3.0f - 2.0f * u);
            return samples[i].feet + (samples[i + 1].feet - samples[i].feet) * u;
        }
    }
    return wallDistanceFeet;
}

float Layout::wallHeightAtAngle(float a) const {
    const float base = wallHeightFeet / feetPerUnit;
    // Tall "Monster"-style wall across left field: full height from the LF
    // foul pole to left-center, then tapering back down to the standard
    // padded-wall height before center field.
    const float monster = 24.0f / feetPerUnit;
    float aDeg = a * (180.0f / pi);
    aDeg = std::clamp(aDeg, -foulAngleDegrees, foulAngleDegrees);
    if (aDeg <= -14.0f) {
        return monster;
    }
    if (aDeg < -6.0f) {
        float u = (aDeg + 14.0f) / 8.0f;
        u = u * u * (3.0f - 2.0f * u);
        return monster + (base - monster) * u;
    }
    return base;
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
    return std::abs(angleRad) < 0.062f;
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
    // Well back and well up from the CF fence — a distant backdrop, not a
    // second wall hiding right behind the first one. A routine fair fly that
    // clears the (short) CF fence is still climbing or at height by the time
    // it reaches this depth, so it passes underneath rather than colliding.
    return fromHome(wallRAtAngle(0.0f) + 42.0f, 0.0f, 22.0f);
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
// Fair infield/outfield play is open air; only real park geometry collides:
//   ground · OF fence · foul poles · backstop · dugouts · foul/OF stands ·
//   CF board · outer world shell · soft sky clamp.
// ═══════════════════════════════════════════════════════════════════════

namespace {

Vector3 radialOut(float ang) {
    return Vector3(std::sin(ang), 0.0f, -std::cos(ang));
}

void bounceRadial(Vector3& vel, float ang, float rest = 0.48f, float fric = 0.78f) {
    Vector3 n = radialOut(ang);
    float vn = vel.x * n.x + vel.z * n.z;
    if (vn > 0.0f) {
        // Reflect only the outward radial component (solid wall impulse).
        vel.x -= n.x * vn * (1.0f + rest);
        vel.z -= n.z * vn * (1.0f + rest);
    }
    // Tangential friction + vertical damping (padded wall / seats).
    vel.x *= fric;
    vel.z *= fric;
    vel.y *= 0.82f;
}

void trySettle(Vector3& vel, bool stick, float thresh, BallCollisionHit& hit) {
    if ((stick && vel.magnitude() < thresh + 0.8f) || vel.magnitude() < thresh) {
        vel = Vector3();
        hit.stuck = true;
    }
}

// True when ball is past the OF fence radius (fair territory only).
bool pastFairFence(const Layout& layout, float r, float ang, float radius, float eps = 0.12f) {
    return r + radius > layout.wallRAtAngle(ang) + eps;
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
    // Tight fair cone — foul strip near the line uses foul geometry (dugouts / seats).
    bool fair = std::abs(ang) <= fa + 0.012f;
    hit.fenceFeet = layout.wallFeetAtAngle(std::clamp(ang, -fa, fa));
    hit.wallTopY = layout.wallHeightAtAngle(std::clamp(ang, -fa, fa));

    // ── 1. Ground (always first) ──────────────────────────────────────
    bool onGround = false;
    if (position.y < groundY) {
        position.y = groundY;
        onGround = true;
        hit.surface = HitSurface::Ground;
        hit.impactY = groundY;
        if (velocity.y < 0.0f) {
            // Grass/dirt bounce: modest restitution, strong spin/skid loss.
            float impact = -velocity.y;
            velocity.y = impact * 0.32f;
            float fric = impact > 6.0f ? 0.78f : 0.86f;
            velocity.x *= fric;
            velocity.z *= fric;
        }
        trySettle(velocity, stickOnContact, 2.6f, hit);
    } else if (position.y < groundY + 0.12f && velocity.y <= 0.15f) {
        if (velocity.magnitude() < 3.0f || (stickOnContact && velocity.magnitude() < 4.2f)) {
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
    fair = std::abs(ang) <= fa + 0.012f;

    // ── 2. Outer world barrier (suburb edge — never fly off forever) ─
    {
        const float rWorld = layout.maxWallR() + 72.0f;
        if (r + radius > rWorld) {
            Vector3 target = layout.fromHome(rWorld - radius - 0.08f, ang, position.y);
            target.y = std::max(target.y, groundY);
            position = target;
            bounceRadial(velocity, ang, 0.28f, 0.70f);
            if (velocity.y > 1.5f) {
                velocity.y *= 0.45f;
            }
            hit.surface = HitSurface::DomeShell;
            hit.impactY = position.y;
            trySettle(velocity, stickOnContact, 2.8f, hit);
            if (hit.stuck) {
                return hit;
            }
            refreshPolar(r, ang);
            fair = std::abs(ang) <= fa + 0.012f;
        }
    }

    // ── 3. Soft sky clamp (open park — kill moonshots that never land) ─
    {
        const float yCeil = 78.0f;
        if (position.y + radius > yCeil) {
            position.y = yCeil - radius;
            if (velocity.y > 0.0f) {
                velocity.y = -velocity.y * 0.18f;
            }
            velocity.x *= 0.92f;
            velocity.z *= 0.92f;
            hit.surface = HitSurface::Roof;
            hit.impactY = position.y;
        }
    }

    // ── 4. Fair OF fence face + top lip ────────────────────────────────
    // Only fair balls inside the foul poles meet the padded wall.
    // Below top → hard bounce into play; above → over-the-wall clear.
    bool clearedFence = false;
    if (fair && r > 2.0f) {
        float wallR = layout.wallRAtAngle(ang);
        float wallH = layout.wallHeightAtAngle(ang);
        hit.wallTopY = wallH;
        hit.fenceFeet = layout.wallFeetAtAngle(ang);
        if (r + radius > wallR) {
            const float clearY = wallH + std::max(radius * 0.75f, 0.22f);
            const float lipLo = wallH - radius * 0.35f;
            if (position.y > clearY) {
                // True over-the-wall path — free until bleachers / ground past fence.
                clearedFence = true;
                hit.surface = HitSurface::FenceTopClear;
                hit.impactY = position.y;
            } else if (position.y >= lipLo && position.y <= clearY && velocity.y > -2.0f) {
                // Top-of-wall clip: knuckle up/out or dribble over.
                float overBias = (position.y - lipLo) / std::max(clearY - lipLo, 0.05f);
                if (overBias > 0.55f && velocity.y > -0.5f) {
                    clearedFence = true;
                    hit.surface = HitSurface::FenceTopClear;
                    hit.impactY = position.y;
                    // Nudge past the face so we don't re-collide next frame.
                    Vector3 past = layout.fromHome(wallR + radius + 0.12f, ang, position.y);
                    position.x = past.x;
                    position.z = past.z;
                    velocity.y = std::max(velocity.y, 0.8f);
                } else if (!onGround) {
                    Vector3 onWall = layout.fromHome(wallR - radius - 0.05f, ang, wallH - 0.04f);
                    position = onWall;
                    bounceRadial(velocity, ang, 0.35f, 0.72f);
                    velocity.y = std::abs(velocity.y) * 0.55f + 1.2f;
                    hit.surface = HitSurface::Fence;
                    hit.impactY = position.y;
                    trySettle(velocity, stickOnContact, 3.0f, hit);
                    if (hit.stuck) {
                        return hit;
                    }
                    refreshPolar(r, ang);
                }
            } else if (!onGround) {
                // Solid wall face — bounce back into fair territory.
                Vector3 onWall = layout.fromHome(wallR - radius - 0.06f, ang, position.y);
                onWall.y = std::clamp(position.y, groundY, wallH - 0.02f);
                position = onWall;
                // Padded wall: moderate restitution, big energy loss.
                bounceRadial(velocity, ang, 0.42f, 0.74f);
                // Slight loft off the pad (not a trampoline).
                velocity.y = std::min(std::abs(velocity.y) * 0.35f + 0.6f, 3.8f);
                hit.surface = HitSurface::Fence;
                hit.impactY = position.y;
                trySettle(velocity, stickOnContact, 3.0f, hit);
                if (hit.stuck) {
                    return hit;
                }
                refreshPolar(r, ang);
            }
        }
    }

    // ── 5. Foul poles (tall yellow poles at fair/foul corners) ─────────
    {
        auto poleHit = [&](float poleAng) {
            Vector3 base = layout.wallPoint(poleAng, 0.0f);
            float poleH = layout.wallHeightAtAngle(poleAng) * 3.8f;
            float pr = 0.48f + radius;
            Vector3 d(position.x - base.x, 0.0f, position.z - base.z);
            float dist = std::sqrt(d.x * d.x + d.z * d.z);
            if (position.y < 0.0f || position.y > poleH + 1.2f || dist >= pr || dist < 1e-5f) {
                return;
            }
            Vector3 n = d * (1.0f / dist);
            position.x = base.x + n.x * pr;
            position.z = base.z + n.z * pr;
            float vn = velocity.x * n.x + velocity.z * n.z;
            if (vn > 0.0f) {
                velocity = velocity - n * vn * 1.62f;
            }
            velocity = velocity * 0.52f;
            hit.surface = HitSurface::FoulPole;
            hit.impactY = position.y;
            trySettle(velocity, stickOnContact, 2.8f, hit);
        };
        poleHit(fa);
        poleHit(-fa);
        if (hit.stuck) {
            return hit;
        }
        refreshPolar(r, ang);
        fair = std::abs(ang) <= fa + 0.012f;
    }

    // ── 6. Backstop / screen behind home (foul only) ──────────────────
    {
        const float backR = 16.0f;
        const float backH = 11.5f;
        // Behind plate: roughly opposite CF (ang near ±π).
        if (!fair && r + radius > backR && position.y < backH + radius) {
            if (-std::cos(ang) > 0.22f) {
                Vector3 target = layout.fromHome(backR - radius - 0.06f, ang, position.y);
                target.y = std::clamp(position.y, groundY, backH);
                position = target;
                bounceRadial(velocity, ang, 0.32f, 0.65f); // soft net-ish
                hit.surface = HitSurface::Backstop;
                hit.impactY = position.y;
                trySettle(velocity, stickOnContact, 2.8f, hit);
                if (hit.stuck) {
                    return hit;
                }
                refreshPolar(r, ang);
            }
        }
    }

    // ── 6b. Dugouts along foul lines (infield foul) ───────────────────
    {
        // Low roofed dugouts sit just outside the baselines, ~45–95 ft from home.
        const float dugR0 = 22.0f;
        const float dugR1 = 48.0f;
        const float dugH = 2.35f;
        float absA = std::abs(ang);
        bool nearLine = absA > fa + 0.04f && absA < fa + 0.55f;
        if (!fair && nearLine && r > dugR0 && r < dugR1 && position.y < dugH + radius) {
            // Facade faces the diamond (push back toward foul grass / line).
            float faceR = dugR0;
            if (r + radius > faceR && r < dugR0 + 2.5f) {
                Vector3 target = layout.fromHome(faceR - radius - 0.04f, ang, position.y);
                target.y = std::clamp(position.y, groundY, dugH);
                position = target;
                bounceRadial(velocity, ang, 0.38f, 0.70f);
                hit.surface = HitSurface::Dugout;
                hit.impactY = position.y;
                trySettle(velocity, stickOnContact, 2.8f, hit);
                if (hit.stuck) {
                    return hit;
                }
                refreshPolar(r, ang);
            } else if (r >= dugR0 + 2.5f && position.y < dugH + radius * 0.5f) {
                // Land on dugout roof.
                position.y = dugH + radius;
                if (velocity.y < 0.0f) {
                    velocity.y = -velocity.y * 0.22f;
                    velocity.x *= 0.70f;
                    velocity.z *= 0.70f;
                }
                hit.surface = HitSurface::Dugout;
                hit.impactY = position.y;
                trySettle(velocity, stickOnContact, 3.0f, hit);
                if (hit.stuck) {
                    return hit;
                }
            }
        }
    }

    // ── 7. Stands / OF bleachers — fair only past fence; foul horseshoe ─
    // Critical: fair infield/outfield flies must NEVER hit floating seat volumes.
    // Fair seats exist only as OF bleachers beyond the wall.
    // Foul seats (horseshoe) are solid at bowlInnerRadius.
    {
        refreshPolar(r, ang);
        fair = std::abs(ang) <= fa + 0.012f;
        const bool foulTerritory = !fair;
        const bool pastFence =
            fair && (clearedFence || pastFairFence(layout, r, ang, radius, 0.05f));
        // Foul horseshoe only where the bowl actually sits (not open CF board gap).
        const bool foulBowl = foulTerritory && layout.isSeatingArc(ang);

        if (foulBowl || pastFence) {
            float rBowl = layout.bowlInnerRadius(ang) - 0.15f;
            float yBase = layout.bowlBaseHeight(ang);
            // Fair OF bleachers sit just outside the fence; never treat infield
            // fair radii as seats even if isSeatingArc is true for that angle.
            if (fair) {
                float wallR = layout.wallRAtAngle(ang);
                rBowl = std::max(rBowl, wallR + 2.4f);
                yBase = std::max(yBase, layout.wallHeightAtAngle(ang) + 0.30f);
            }

            float pastBowl = std::max(0.0f, r - rBowl);
            // Gradual rise: lower bowl rows then steeper upper.
            float rise = pastBowl < 6.0f ? pastBowl * 0.42f
                                         : 6.0f * 0.42f + (pastBowl - 6.0f) * 0.62f;
            float deckY = yBase + std::min(rise, 18.0f) + radius;
            float facadeTop = yBase + (fair ? 12.0f : 14.5f);

            // Vertical facade (first-row face) — only when nearly at the lip.
            if (r + radius > rBowl && position.y < facadeTop && position.y > groundY - 0.05f &&
                pastBowl < 1.4f) {
                Vector3 target = layout.fromHome(rBowl - radius - 0.05f, ang, position.y);
                target.y = std::max(target.y, groundY);
                position = target;
                bounceRadial(velocity, ang, fair ? 0.38f : 0.40f, 0.68f);
                hit.surface = HitSurface::Stands;
                hit.impactY = position.y;
                trySettle(velocity, stickOnContact, 3.0f, hit);
                if (hit.stuck) {
                    return hit;
                }
                refreshPolar(r, ang);
                pastBowl = std::max(0.0f, r - rBowl);
                rise = pastBowl < 6.0f ? pastBowl * 0.42f
                                       : 6.0f * 0.42f + (pastBowl - 6.0f) * 0.62f;
                deckY = yBase + std::min(rise, 18.0f) + radius;
            }

            // Horizontal seat deck — land when dropping into the bowl.
            if (r + radius > rBowl && position.y < deckY && position.y > yBase - 0.8f) {
                position.y = deckY;
                if (velocity.y < 0.0f) {
                    velocity.y = -velocity.y * 0.22f; // soft seats
                    velocity.x *= 0.65f;
                    velocity.z *= 0.65f;
                }
                // Balls die quickly in the seats.
                if (velocity.magnitude() < 4.2f ||
                    (stickOnContact && velocity.magnitude() < 5.2f)) {
                    velocity = Vector3();
                    hit.stuck = true;
                }
                hit.surface = HitSurface::Stands;
                hit.impactY = position.y;
                if (hit.stuck) {
                    return hit;
                }
            }
        }
    }

    // ── 8. CF scoreboard chassis (beyond fence, near center) ──────────
    // Sits well back and well up (matches buildScoreboardScreen's placement
    // via scoreboardCenter()) so it reads as a distant backdrop — a routine
    // fair fly clearing the CF fence must NOT immediately clip a "second
    // wall" hiding right behind it.
    {
        refreshPolar(r, ang);
        if (layout.isCfScoreboardZone(ang)) {
            float cfR = layout.wallRAtAngle(0.0f);
            Vector3 boardC = layout.scoreboardCenter();
            const float boardHalfDepth = 4.0f;
            const float boardHalfHeight = 5.5f;
            float boardR0 = layout.radiusFromHome(boardC) - boardHalfDepth;
            float boardR1 = boardR0 + boardHalfDepth * 2.0f;
            float boardY0 = boardC.y - boardHalfHeight;
            float boardY1 = boardC.y + boardHalfHeight;
            // Only solid once the ball is past the CF fence plane.
            if (r + radius > boardR0 && r < boardR1 + radius && position.y > boardY0 - radius &&
                position.y < boardY1 + radius && r > cfR + 0.5f) {
                Vector3 target = layout.fromHome(boardR0 - radius - 0.05f, ang, position.y);
                target.y = std::clamp(position.y, boardY0, boardY1);
                position = target;
                bounceRadial(velocity, ang, 0.40f, 0.68f);
                hit.surface = HitSurface::Scoreboard;
                hit.impactY = position.y;
                trySettle(velocity, stickOnContact, 2.8f, hit);
                if (hit.stuck) {
                    return hit;
                }
            }
        }
    }

    // ── 9. Final ground re-clamp after barrier snaps ──────────────────
    if (position.y < groundY) {
        position.y = groundY;
        if (velocity.y < 0.0f) {
            velocity.y = -velocity.y * 0.28f;
            velocity.x *= 0.88f;
            velocity.z *= 0.88f;
        }
        hit.surface = HitSurface::Ground;
        hit.impactY = groundY;
        trySettle(velocity, stickOnContact, 2.5f, hit);
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
