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
// Vertical bands on the fence for a brick/panel texture (no real textures).
sf::Color shadeColor(sf::Color c, float mul) {
    return sf::Color(
        static_cast<std::uint8_t>(std::clamp(c.r * mul, 0.0f, 255.0f)),
        static_cast<std::uint8_t>(std::clamp(c.g * mul, 0.0f, 255.0f)),
        static_cast<std::uint8_t>(std::clamp(c.b * mul, 0.0f, 255.0f)),
        c.a
    );
}

void addAsymmetricOutfieldWall(
    Mesh3D& m,
    const Layout& L,
    float a0,
    float a1,
    int segs,
    sf::Color face,
    sf::Color top
) {
    const int vBands = 4; // horizontal panel rows on wall face
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = a0 + (a1 - a0) * t0;
        float ang1 = a0 + (a1 - a0) * t1;
        float r0 = L.wallRAtAngle(ang0);
        float r1 = L.wallRAtAngle(ang1);
        float h0 = L.wallHeightAtAngle(ang0);
        float h1 = L.wallHeightAtAngle(ang1);
        float faceMul = (i % 2 == 0) ? 1.0f : 0.88f;
        for (int v = 0; v < vBands; v++) {
            float u0 = static_cast<float>(v) / vBands;
            float u1 = static_cast<float>(v + 1) / vBands;
            float rowMul = faceMul * (0.92f + 0.12f * static_cast<float>(v % 2));
            sf::Color rowCol = shadeColor(face, rowMul);
            Vector3 b0 = L.fromHome(r0, ang0, h0 * u0);
            Vector3 b1 = L.fromHome(r1, ang1, h1 * u0);
            Vector3 tA = L.fromHome(r0, ang0, h0 * u1);
            Vector3 tB = L.fromHome(r1, ang1, h1 * u1);
            addQuad(m, b0, b1, tB, tA, rowCol);
            addQuad(m, b1, b0, tA, tB, shadeColor(rowCol, 0.85f));
        }
        Vector3 topA = L.fromHome(r0, ang0, h0);
        Vector3 topB = L.fromHome(r1, ang1, h1);
        Vector3 outer0 = L.fromHome(r0 + 1.8f, ang0, h0);
        Vector3 outer1 = L.fromHome(r1 + 1.8f, ang1, h1);
        addQuad(m, topA, topB, outer1, outer0, top);
        addQuad(m, topB, topA, outer0, outer1, shadeColor(top, 0.9f));
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

// Ring / annulus on the ground (for textured dirt circles).
void addDiskRing(
    Mesh3D& m,
    const Vector3& center,
    float rInner,
    float rOuter,
    float y,
    int n,
    sf::Color col
) {
    Vector3 c(center.x, y, center.z);
    for (int i = 0; i < n; i++) {
        float a0 = (static_cast<float>(i) / n) * pi * 2.0f;
        float a1 = (static_cast<float>(i + 1) / n) * pi * 2.0f;
        Vector3 i0 = c + Vector3(std::cos(a0) * rInner, 0.0f, std::sin(a0) * rInner);
        Vector3 i1 = c + Vector3(std::cos(a1) * rInner, 0.0f, std::sin(a1) * rInner);
        Vector3 o0 = c + Vector3(std::cos(a0) * rOuter, 0.0f, std::sin(a0) * rOuter);
        Vector3 o1 = c + Vector3(std::cos(a1) * rOuter, 0.0f, std::sin(a1) * rOuter);
        addQuad(m, i0, i1, o1, o0, col);
    }
}

// Arc sector of a disk (e.g. home-plate dirt arc toward the mound).
void addDiskSector(
    Mesh3D& m,
    const Vector3& center,
    float radius,
    float y,
    float ang0,
    float ang1,
    int n,
    sf::Color col
) {
    Vector3 c(center.x, y, center.z);
    for (int i = 0; i < n; i++) {
        float t0 = static_cast<float>(i) / n;
        float t1 = static_cast<float>(i + 1) / n;
        float a0 = ang0 + (ang1 - ang0) * t0;
        float a1 = ang0 + (ang1 - ang0) * t1;
        Vector3 p0 = c + Vector3(std::cos(a0) * radius, 0.0f, std::sin(a0) * radius);
        Vector3 p1 = c + Vector3(std::cos(a1) * radius, 0.0f, std::sin(a1) * radius);
        addTri(m, c, p0, p1, col);
    }
}

// Dirt with radial + angular banding (wear rings + pie slices).
void addTexturedDirtDisk(
    Mesh3D& m,
    const Vector3& center,
    float radius,
    float y,
    int rings,
    int segs,
    sf::Color base
) {
    for (int r = 0; r < rings; r++) {
        float u0 = static_cast<float>(r) / rings;
        float u1 = static_cast<float>(r + 1) / rings;
        float r0 = radius * u0;
        float r1 = radius * u1;
        for (int s = 0; s < segs; s++) {
            float a0 = (static_cast<float>(s) / segs) * pi * 2.0f;
            float a1 = (static_cast<float>(s + 1) / segs) * pi * 2.0f;
            // Wear: darker toward center, slight angular mottling
            float mul = 0.88f + 0.14f * u1;
            mul *= 0.94f + 0.10f * static_cast<float>((s + r * 3) % 4) / 3.0f;
            if ((s + r) % 5 == 0) {
                mul *= 0.90f; // random-ish darker patches
            }
            sf::Color col = shadeColor(base, mul);
            Vector3 c(center.x, y, center.z);
            auto pt = [&](float a, float rad) {
                return c + Vector3(std::cos(a) * rad, 0.0f, std::sin(a) * rad);
            };
            if (r0 < 1e-4f) {
                addTri(m, c, pt(a0, r1), pt(a1, r1), col);
            } else {
                addQuad(m, pt(a0, r0), pt(a1, r0), pt(a1, r1), pt(a0, r1), col);
            }
        }
    }
}

// Small darker dirt "wear blotch" (cleats / landing spots).
void addDirtWearBlotch(
    Mesh3D& m,
    const Vector3& center,
    float radius,
    float y,
    sf::Color base,
    int seed
) {
    float mul = 0.78f + 0.08f * static_cast<float>(seed % 5) / 4.0f;
    addDisk(m, center, radius, y, 14, shadeColor(base, mul));
    addDisk(m, center + Vector3(radius * 0.25f, 0, radius * 0.1f), radius * 0.55f, y + 0.001f, 10,
            shadeColor(base, mul * 0.92f));
}

// Point inside diamond UV: (0,0)=home, (1,0)=1B, (1,1)=2B, (0,1)=3B.
Vector3 diamondLerp(
    const Vector3& home,
    const Vector3& b1,
    const Vector3& b2,
    const Vector3& b3,
    float u,
    float v
) {
    Vector3 a = home * (1.0f - u) + b1 * u;
    Vector3 b = b3 * (1.0f - u) + b2 * u;
    Vector3 p = a * (1.0f - v) + b * v;
    p.y = 0.0f;
    return p;
}

// Diagonal / checker mow pattern across the infield grass skin.
void addInfieldMowPattern(
    Mesh3D& m,
    const Vector3& home,
    const Vector3& b1,
    const Vector3& b2,
    const Vector3& b3,
    float scale,
    float y,
    int grid,
    sf::Color grassA,
    sf::Color grassB
) {
    // Scale corners toward diamond center so we stay inside the skin.
    Vector3 c = (home + b1 + b2 + b3) * 0.25f;
    auto sc = [&](const Vector3& p) {
        Vector3 d = p - c;
        d.y = 0.0f;
        return c + d * scale;
    };
    Vector3 H = sc(home);
    Vector3 B1 = sc(b1);
    Vector3 B2 = sc(b2);
    Vector3 B3 = sc(b3);

    for (int j = 0; j < grid; j++) {
        for (int i = 0; i < grid; i++) {
            float u0 = static_cast<float>(i) / grid;
            float u1 = static_cast<float>(i + 1) / grid;
            float v0 = static_cast<float>(j) / grid;
            float v1 = static_cast<float>(j + 1) / grid;
            // Shrink slightly so cells don't fight dirt paths at edges
            const float inset = 0.03f;
            u0 = inset + u0 * (1.0f - 2.0f * inset);
            u1 = inset + u1 * (1.0f - 2.0f * inset);
            v0 = inset + v0 * (1.0f - 2.0f * inset);
            v1 = inset + v1 * (1.0f - 2.0f * inset);

            Vector3 p00 = diamondLerp(H, B1, B2, B3, u0, v0);
            Vector3 p10 = diamondLerp(H, B1, B2, B3, u1, v0);
            Vector3 p11 = diamondLerp(H, B1, B2, B3, u1, v1);
            Vector3 p01 = diamondLerp(H, B1, B2, B3, u0, v1);

            // Diagonal stripes (broadcast-style) with light checker modulation
            int band = static_cast<int>(std::floor((u0 + v0) * grid * 0.85f));
            int check = (i + j) & 1;
            sf::Color col = (band & 1) ? grassA : grassB;
            if (check) {
                col = shadeColor(col, 0.96f);
            }
            // Alternate stripe direction mid-diamond for that "two-way mow" look
            if (u0 + v0 > 1.0f) {
                int band2 = static_cast<int>(std::floor((u0 - v0 + 1.0f) * grid * 0.7f));
                col = (band2 & 1) ? grassB : grassA;
            }
            addFlatQuad(m, p00, p10, p11, p01, y + static_cast<float>((i + j) % 3) * 0.0003f, col);
        }
    }
}

Mesh3D buildField(const Layout& L) {
    Mesh3D m;
    const float aL = -L.foulAngleRad();
    const float aR = L.foulAngleRad();
    const float plateZ = L.plateZ();
    const float bp = L.basePath();
    const int ringSegs = 144; // high-res field disk

    const Vector3 home = L.home();
    const Vector3 b1 = L.firstBase();
    const Vector3 b2 = L.secondBase();
    const Vector3 b3 = L.thirdBase();

    const sf::Color grass = grassColor();
    const sf::Color grassDark = grassDarkColor();
    const sf::Color grassInfield(40, 122, 54);
    const sf::Color dirt = dirtColor();
    const sf::Color dirtDark(140, 98, 55);
    const sf::Color dirtPath(175, 128, 78);
    const sf::Color track = warningTrackColor();
    const sf::Color apron(120, 115, 105); // concrete/walkway between fence and seats

    // ── Fair-territory field only (classic diamond + OF pie) ───────────
    // NOT a 360° heart/blob of grass. Foul territory is concrete apron.
    // Fair angles only: home → wall between the foul lines.
    const int fairSegs = 96;
    for (int i = 0; i < fairSegs; i++) {
        float t0 = static_cast<float>(i) / fairSegs;
        float t1 = static_cast<float>(i + 1) / fairSegs;
        float ang0 = aL + (aR - aL) * t0;
        float ang1 = aL + (aR - aL) * t1;
        float rFence0 = L.wallRAtAngle(ang0) - 0.2f;
        float rFence1 = L.wallRAtAngle(ang1) - 0.2f;

        // Fair grass pie: near home out to warning track.
        const float trackW = 3.8f;
        addQuad(
            m,
            L.fromHome(0.2f, ang0, 0.0f),
            L.fromHome(0.2f, ang1, 0.0f),
            L.fromHome(std::max(0.5f, rFence1 - trackW), ang1, 0.0f),
            L.fromHome(std::max(0.5f, rFence0 - trackW), ang0, 0.0f),
            grass
        );

        // Mowing stripes (OF only).
        float stripeIn = bp * 1.30f;
        for (int s = 0; s < 5; s++) {
            float u0 = 0.42f + s * 0.09f;
            float u1 = u0 + 0.045f;
            float mid0 = std::max(stripeIn, rFence0 * u0);
            float mid1 = std::max(stripeIn, rFence1 * u0);
            float mid0b = std::min(rFence0 - trackW - 0.5f, rFence0 * u1);
            float mid1b = std::min(rFence1 - trackW - 0.5f, rFence1 * u1);
            if (mid0b <= mid0 + 0.4f) {
                continue;
            }
            sf::Color stripe = (s % 2 == 0) ? grassDark : shadeColor(grass, 0.95f);
            addQuad(
                m,
                L.fromHome(mid0, ang0, 0.012f + s * 0.001f),
                L.fromHome(mid1, ang1, 0.012f + s * 0.001f),
                L.fromHome(mid1b, ang1, 0.012f + s * 0.001f),
                L.fromHome(mid0b, ang0, 0.012f + s * 0.001f),
                stripe
            );
        }

        // Warning track inside fence.
        for (int b = 0; b < 3; b++) {
            float u0 = static_cast<float>(b) / 3.0f;
            float u1 = static_cast<float>(b + 1) / 3.0f;
            float ri0 = rFence0 - trackW * (1.0f - u0);
            float ro0 = rFence0 - trackW * (1.0f - u1);
            float ri1 = rFence1 - trackW * (1.0f - u0);
            float ro1 = rFence1 - trackW * (1.0f - u1);
            sf::Color tc = shadeColor(track, (b % 2) ? 0.92f : 1.05f);
            addQuad(
                m,
                L.fromHome(ri0, ang0, 0.018f + b * 0.001f),
                L.fromHome(ri1, ang1, 0.018f + b * 0.001f),
                L.fromHome(ro1, ang1, 0.018f + b * 0.001f),
                L.fromHome(ro0, ang0, 0.018f + b * 0.001f),
                tc
            );
        }

        // Concrete apron: fence → first seat row (fair OF only).
        float wall0 = L.wallRAtAngle(ang0);
        float wall1 = L.wallRAtAngle(ang1);
        float bowl0 = L.bowlInnerRadius(ang0) - 0.15f;
        float bowl1 = L.bowlInnerRadius(ang1) - 0.15f;
        if (bowl0 > wall0 + 0.2f) {
            addQuad(
                m,
                L.fromHome(wall0, ang0, 0.01f),
                L.fromHome(wall1, ang1, 0.01f),
                L.fromHome(bowl1, ang1, 0.01f),
                L.fromHome(bowl0, ang0, 0.01f),
                apron
            );
        }
    }

    // ── Thin foul walkway only (NOT a big beige lobe) ─────────────────
    // Seats sit on the diamond perimeter; only a narrow strip is concrete.
    const int foulSegs = 100;
    for (int i = 0; i < foulSegs; i++) {
        float t0 = static_cast<float>(i) / foulSegs;
        float t1 = static_cast<float>(i + 1) / foulSegs;
        float ang0 = -pi + t0 * 2.0f * pi;
        float ang1 = -pi + t1 * 2.0f * pi;
        float angM = 0.5f * (ang0 + ang1);
        while (angM > pi) {
            angM -= 2.0f * pi;
        }
        while (angM < -pi) {
            angM += 2.0f * pi;
        }
        const bool fair = std::abs(angM) <= L.foulAngleRad() + 0.02f;
        float rSeat0 = std::max(2.0f, L.bowlInnerRadius(ang0) - 0.15f);
        float rSeat1 = std::max(2.0f, L.bowlInnerRadius(ang1) - 0.15f);

        if (fair) {
            // CF board: solid floor under board only (fence → board base).
            if (L.isCfScoreboardZone(angM)) {
                float wall0 = L.wallRAtAngle(ang0);
                float wall1 = L.wallRAtAngle(ang1);
                float rBoard0 = L.clampRadiusInDome(ang0, wall0 + 12.0f, 8.0f);
                float rBoard1 = L.clampRadiusInDome(ang1, wall1 + 12.0f, 8.0f);
                addQuad(
                    m,
                    L.fromHome(wall0, ang0, 0.008f),
                    L.fromHome(wall1, ang1, 0.008f),
                    L.fromHome(rBoard1, ang1, 0.008f),
                    L.fromHome(rBoard0, ang0, 0.008f),
                    apron
                );
            }
            continue;
        }

        // Thin walkway from near the foul line / home out to first seat row.
        // Start radius is just inside the seat line so beige is a ribbon, not a lobe.
        float rIn0 = std::max(1.0f, rSeat0 - 6.5f);
        float rIn1 = std::max(1.0f, rSeat1 - 6.5f);
        addQuad(
            m,
            L.fromHome(rIn0, ang0, 0.005f),
            L.fromHome(rIn1, ang1, 0.005f),
            L.fromHome(rSeat1, ang1, 0.005f),
            L.fromHome(rSeat0, ang0, 0.005f),
            apron
        );
    }

    // Under-seat floor (dark, not beige field) so no sky under the bowl.
    {
        const int underSegs = 96;
        for (int i = 0; i < underSegs; i++) {
            float t0 = static_cast<float>(i) / underSegs;
            float t1 = static_cast<float>(i + 1) / underSegs;
            float ang0 = -pi + t0 * 2.0f * pi;
            float ang1 = -pi + t1 * 2.0f * pi;
            float angM = 0.5f * (ang0 + ang1);
            if (L.isCfScoreboardZone(angM) && std::abs(angM) < L.foulAngleRad()) {
                // board zone handled above
            }
            float r0 = L.bowlInnerRadius(ang0);
            float r1 = L.bowlInnerRadius(ang1);
            float rOut0 = L.clampRadiusInDome(ang0, r0 + 55.0f, 10.0f);
            float rOut1 = L.clampRadiusInDome(ang1, r1 + 55.0f, 10.0f);
            addQuad(
                m,
                L.fromHome(r0, ang0, 0.002f),
                L.fromHome(r1, ang1, 0.002f),
                L.fromHome(rOut1, ang1, 0.002f),
                L.fromHome(rOut0, ang0, 0.002f),
                sf::Color(40, 48, 58) // dark under seats
            );
        }
    }
    // ── Foul-line dirt strips (inside fair side of chalk) ──────────────
    // Thin dirt apron along each foul line from home out past the infield.
    {
        const float lineDirtHw = 0.85f;
        const float lineDirtY = 0.021f;
        // Short dirt aprons only — long strips wash out the plate-cam with brown.
        float foulLen = bp * 1.35f;
        Vector3 flEnd = L.fromHome(foulLen, aL, 0.0f);
        Vector3 frEnd = L.fromHome(foulLen, aR, 0.0f);
        addDirtPath(m, home, flEnd, lineDirtHw, lineDirtY, shadeColor(dirt, 0.96f));
        addDirtPath(m, home, frEnd, lineDirtHw, lineDirtY, shadeColor(dirt, 0.96f));
        // Slightly darker packed edge under chalk
        addDirtPath(m, home, flEnd, lineDirtHw * 0.28f, lineDirtY + 0.002f, dirtDark);
        addDirtPath(m, home, frEnd, lineDirtHw * 0.28f, lineDirtY + 0.002f, dirtDark);
    }

    // ── Classic skinned infield ────────────────────────────────────────
    // Dirt = paths + circles + thin diamond lip (not a giant brown slab).
    // Grass skin fills the interior so the catcher view reads green + chalk.
    auto expandDiamond = [&](float scale) {
        Vector3 c = (home + b1 + b2 + b3) * 0.25f;
        auto push = [&](const Vector3& p) {
            Vector3 d = p - c;
            d.y = 0.0f;
            return c + d * scale;
        };
        return std::array<Vector3, 4>{push(home), push(b1), push(b2), push(b3)};
    };

    // Outer dirt lip only (skinny frame around the diamond).
    {
        auto outer = expandDiamond(1.04f);
        auto inner = expandDiamond(0.92f);
        for (int e = 0; e < 4; e++) {
            int n = (e + 1) % 4;
            // Segment the lip for dirt mottling along each side
            const int segs = 8;
            for (int s = 0; s < segs; s++) {
                float t0 = static_cast<float>(s) / segs;
                float t1 = static_cast<float>(s + 1) / segs;
                auto lerpV = [](const Vector3& a, const Vector3& b, float t) {
                    return a * (1.0f - t) + b * t;
                };
                Vector3 o0 = lerpV(outer[e], outer[n], t0);
                Vector3 o1 = lerpV(outer[e], outer[n], t1);
                Vector3 i0 = lerpV(inner[e], inner[n], t0);
                Vector3 i1 = lerpV(inner[e], inner[n], t1);
                float mul = 0.93f + 0.10f * static_cast<float>((e + s) % 3) / 2.0f;
                if ((e + s) % 4 == 0) {
                    mul *= 0.88f; // wear on the lip
                }
                addFlatQuad(m, o0, o1, i1, i0, 0.020f, shadeColor(dirt, mul));
            }
        }
    }

    // Infield grass skin + diagonal mow pattern (broadcast look)
    {
        auto g = expandDiamond(0.91f);
        addFlatQuad(m, g[0], g[1], g[2], g[3], 0.017f, grassInfield);
        sf::Color mowA = grassInfield;
        sf::Color mowB = shadeColor(grassInfield, 0.88f);
        sf::Color mowC = shadeColor(grassInfield, 1.06f);
        // Dense diagonal grid across the skin
        addInfieldMowPattern(m, home, b1, b2, b3, 0.88f, 0.019f, 14, mowA, mowB);
        // Lighter cross-bands for two-way mow depth
        addInfieldMowPattern(m, home, b1, b2, b3, 0.72f, 0.0205f, 10, mowC, mowB);
    }

    // Base paths — ~6 ft dirt width (1 unit ≈ 2 ft → half-width ≈ 1.5).
    const float pathHw = 1.50f;
    const float pathY = 0.028f;
    auto wearPath = [&](const Vector3& a, const Vector3& b, float hw) {
        addDirtPath(m, a, b, hw, pathY, dirtPath);
        addDirtPath(m, a, b, hw * 0.22f, pathY + 0.002f, dirtDark);
        // Cleat wear blotches along the path
        Vector3 d = b - a;
        d.y = 0.0f;
        float len = d.magnitude();
        if (len < 1e-3f) {
            return;
        }
        d = d * (1.0f / len);
        Vector3 side(-d.z, 0, d.x);
        int spots = std::max(3, static_cast<int>(len / 4.5f));
        for (int s = 0; s < spots; s++) {
            float t = (static_cast<float>(s) + 0.5f) / static_cast<float>(spots);
            float off = ((s % 3) - 1) * hw * 0.35f;
            Vector3 p = a + d * (len * t) + side * off;
            p.y = 0.0f;
            addDirtWearBlotch(m, p, 0.35f + 0.15f * static_cast<float>(s % 3), pathY + 0.003f, dirtDark, s + 7);
        }
    };
    wearPath(home, b1, pathHw);
    wearPath(b1, b2, pathHw);
    wearPath(b2, b3, pathHw);
    wearPath(b3, home, pathHw);
    // Pitcher → 2B cut (narrower) + landing wear
    wearPath(L.mound(), b2, pathHw * 0.55f);

    // Mound circle: MLB 18 ft diameter → radius 9 ft → 4.5 world units.
    const float moundCircleR = 4.5f;
    addTexturedDirtDisk(m, L.mound(), moundCircleR, 0.026f, 7, 40, dirt);
    // Landing zone toward plate (slope starts 6" in front of rubber).
    addDiskRing(
        m,
        L.mound() + Vector3(0, 0, 1.1f),
        0.9f,
        2.2f,
        0.029f,
        24,
        shadeColor(dirtDark, 0.92f)
    );
    addDirtWearBlotch(m, L.mound() + Vector3(0.25f, 0, 1.3f), 0.85f, 0.030f, dirtDark, 3);
    addDirtWearBlotch(m, L.mound() + Vector3(-0.3f, 0, 1.1f), 0.7f, 0.030f, dirtDark, 5);
    {
        // Raised mound: table 5 ft wide, peak 10" above plate (0.417 world units).
        const int n = 32;
        const float tableR = 2.5f; // ~5 ft plateau
        const float peakY = 0.42f; // 10 inches
        for (int ring = 0; ring < 4; ring++) {
            float r0 = tableR * (static_cast<float>(ring) / 4.0f);
            float r1 = tableR * (static_cast<float>(ring + 1) / 4.0f);
            float y0 = 0.05f + (peakY - 0.05f) * (static_cast<float>(ring) / 4.0f);
            float y1 = 0.05f + (peakY - 0.05f) * (static_cast<float>(ring + 1) / 4.0f);
            sf::Color col = shadeColor(dirtDark, 0.90f + ring * 0.035f);
            for (int i = 0; i < n; i++) {
                float a0 = (static_cast<float>(i) / n) * pi * 2.0f;
                float a1 = (static_cast<float>(i + 1) / n) * pi * 2.0f;
                // Oval elongated toward plate (+Z)
                auto pt = [&](float a, float r, float y) {
                    return Vector3(
                        std::cos(a) * r,
                        y,
                        L.moundZ() + std::sin(a) * r * 0.78f
                    );
                };
                if (ring == 0 && r0 < 1e-3f) {
                    addTri(
                        m,
                        Vector3(0, y1, L.moundZ()),
                        pt(a0, r1, y1),
                        pt(a1, r1, y1),
                        col
                    );
                } else {
                    addQuad(m, pt(a0, r0, y0), pt(a1, r0, y0), pt(a1, r1, y1), pt(a0, r1, y1), col);
                }
            }
        }
        // Pitcher's rubber: 24" × 6" (1.0 × 0.25 units), top at ~10".
        addBox(
            m,
            Vector3(0.0f, peakY + 0.02f, L.moundZ()),
            1.0f,
            0.04f,
            0.25f,
            sf::Color(240, 236, 220)
        );
    }

    // Home plate circle: MLB 26 ft diameter → radius 13 ft → 6.5 world units.
    const float homeCircleR = 6.5f;
    addTexturedDirtDisk(m, home, homeCircleR, 0.026f, 6, 40, dirt);
    // Dirt arc from plate toward mound (extends slightly past the circle)
    addDiskSector(
        m,
        home,
        homeCircleR * 1.15f,
        0.027f,
        -pi * 0.5f - 0.85f,
        -pi * 0.5f + 0.85f,
        28,
        shadeColor(dirt, 0.97f)
    );
    // Batter box wear (darker packed dirt)
    addDirtWearBlotch(m, home + Vector3(-1.4f, 0, -0.2f), 0.9f, 0.031f, dirtDark, 11);
    addDirtWearBlotch(m, home + Vector3(1.4f, 0, -0.2f), 0.9f, 0.031f, dirtDark, 12);
    addDirtWearBlotch(m, home + Vector3(0.0f, 0, 0.85f), 0.75f, 0.031f, dirtDark, 13); // catcher
    addDirtWearBlotch(m, home + Vector3(0.15f, 0, -1.9f), 1.0f, 0.030f, dirtDark, 14); // toward mound

    // Base cutouts — ~8–10 ft dirt diameter around bags (radius ~2.2–2.5 units).
    const float baseCutR = 2.4f;
    addTexturedDirtDisk(m, b1, baseCutR, 0.029f, 5, 28, dirt);
    addTexturedDirtDisk(m, b2, baseCutR, 0.029f, 5, 28, dirt);
    addTexturedDirtDisk(m, b3, baseCutR, 0.029f, 5, 28, dirt);
    addDirtWearBlotch(m, b1 + Vector3(-0.3f, 0, 0.25f), 0.55f, 0.032f, dirtDark, 21);
    addDirtWearBlotch(m, b2 + Vector3(0.0f, 0, 0.4f), 0.6f, 0.032f, dirtDark, 22);
    addDirtWearBlotch(m, b3 + Vector3(0.3f, 0, 0.25f), 0.55f, 0.032f, dirtDark, 23);

    // Home plate — MLB 17" pentagon (tip to catcher +Z, flat face to pitcher −Z).
    // 17" ≈ 1.417 ft → 0.708 world units full front width.
    {
        float z = plateZ;
        const float halfFront = 0.354f; // 8.5"
        const float side = 0.354f;      // 8.5" sides along plate
        const float pointLen = 0.50f;   // toward catcher (~12" sides at 45°)
        const float frontDepth = 0.354f;
        float y = 0.055f;
        Vector3 tip(0.0f, y, z + pointLen);                    // catcher tip
        Vector3 bl(-halfFront, y, z);                          // back-left of flat
        Vector3 br(halfFront, y, z);                           // back-right of flat
        Vector3 fl(-halfFront, y, z - frontDepth);             // front-left (pitcher)
        Vector3 fr(halfFront, y, z - frontDepth);              // front-right
        (void)side;
        sf::Color plate(248, 246, 235);
        addTri(m, tip, br, bl, plate);
        addQuad(m, bl, br, fr, fl, plate);
        // Slight bevel shadow under plate
        addBox(m, Vector3(0.0f, 0.04f, z - 0.05f), 0.72f, 0.02f, 0.95f, sf::Color(200, 195, 180));
    }

    // Base bags — MLB 15" square (was 18"; 15" since 2023) → 0.625 ft side ≈ 0.31 units half.
    auto placeBaseBag = [&](const Vector3& p, float yaw) {
        float half = 0.32f; // ~15" / 2 in world units (1u≈2ft)
        float c = std::cos(yaw);
        float sn = std::sin(yaw);
        auto rot = [&](float x, float z) {
            return Vector3(p.x + c * x - sn * z, 0.065f, p.z + sn * x + c * z);
        };
        Vector3 v0 = rot(0.0f, half * 1.35f);
        Vector3 v1 = rot(half * 1.35f, 0.0f);
        Vector3 v2 = rot(0.0f, -half * 1.35f);
        Vector3 v3 = rot(-half * 1.35f, 0.0f);
        addFlatQuad(m, v0, v1, v2, v3, 0.065f, sf::Color(252, 250, 240));
        addBox(m, Vector3(p.x, 0.09f, p.z), 0.72f, 0.045f, 0.72f, sf::Color(250, 248, 235));
    };
    placeBaseBag(b1, L.foulAngleRad() * 0.5f);
    placeBaseBag(b2, 0.0f);
    placeBaseBag(b3, -L.foulAngleRad() * 0.5f);

    // Batter's boxes: regulation 4 ft wide × 6 ft long, 6" from plate edge.
    // 4×6 ft → 2×3 world units. Plate half-width ~0.35; box center at ~1.35.
    {
        sf::Color box = shadeColor(dirtDark, 1.05f);
        const float boxW = 2.0f;  // 4 ft
        const float boxL = 3.0f;  // 6 ft
        const float boxX = 1.35f; // center from plate midline
        const float boxZ = plateZ - 0.15f;
        addBox(m, Vector3(-boxX, 0.035f, boxZ), boxW, 0.03f, boxL, box);
        addBox(m, Vector3(boxX, 0.035f, boxZ), boxW, 0.03f, boxL, box);
        // Catcher's box ~43" wide × 8' (simplified)
        addBox(m, Vector3(0.0f, 0.032f, plateZ + 1.15f), 1.8f, 0.02f, 2.2f, shadeColor(dirt, 0.93f));
    }

    // On-deck circles — 5 ft diameter → radius 2.5 ft → 1.25 world units.
    {
        Vector3 od1 = home + Vector3(7.5f, 0.0f, 2.2f);
        Vector3 od3 = home + Vector3(-7.5f, 0.0f, 2.2f);
        const float odR = 1.25f;
        addDiskRing(m, od1, odR * 0.88f, odR, 0.03f, 20, sf::Color(240, 235, 220));
        addDiskRing(m, od3, odR * 0.88f, odR, 0.03f, 20, sf::Color(240, 235, 220));
        addDisk(m, od1, odR * 0.85f, 0.025f, 16, shadeColor(grass, 0.95f));
        addDisk(m, od3, odR * 0.85f, 0.025f, 16, shadeColor(grass, 0.95f));
    }

    // ── Grass lip at dirt edges (clean cut between dirt and OF grass) ──
    {
        sf::Color lip = shadeColor(grass, 1.08f);
        sf::Color lipDark = shadeColor(grassDark, 1.02f);
        // Around home plate circle
        addDiskRing(m, home, homeCircleR, homeCircleR + 0.35f, 0.016f, 36, lip);
        // Around mound circle
        addDiskRing(m, L.mound(), moundCircleR, moundCircleR + 0.32f, 0.016f, 32, lip);
        // Around base cutouts
        addDiskRing(m, b1, baseCutR, baseCutR + 0.28f, 0.017f, 24, lip);
        addDiskRing(m, b2, baseCutR, baseCutR + 0.28f, 0.017f, 24, lip);
        addDiskRing(m, b3, baseCutR, baseCutR + 0.28f, 0.017f, 24, lip);
        // Thin grass strip outside diamond dirt lip (between lip dirt and OF)
        {
            auto outer = expandDiamond(1.04f);
            auto grassOut = expandDiamond(1.10f);
            for (int e = 0; e < 4; e++) {
                int n = (e + 1) % 4;
                const int segs = 10;
                for (int s = 0; s < segs; s++) {
                    float t0 = static_cast<float>(s) / segs;
                    float t1 = static_cast<float>(s + 1) / segs;
                    auto lerpV = [](const Vector3& a, const Vector3& b, float t) {
                        return a * (1.0f - t) + b * t;
                    };
                    Vector3 i0 = lerpV(outer[e], outer[n], t0);
                    Vector3 i1 = lerpV(outer[e], outer[n], t1);
                    Vector3 o0 = lerpV(grassOut[e], grassOut[n], t0);
                    Vector3 o1 = lerpV(grassOut[e], grassOut[n], t1);
                    sf::Color c = ((e + s) & 1) ? lip : lipDark;
                    addFlatQuad(m, i0, i1, o1, o0, 0.015f, c);
                }
            }
        }
    }

    // ── Coaches' boxes (foul territory along 1B / 3B) ─────────────────
    // Roughly 20×10 ft boxes, ~15 ft off the foul line into foul ground.
    {
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
        // Place mid-baseline, offset into foul territory.
        Vector3 mid1 = home * 0.35f + b1 * 0.65f;
        Vector3 mid3 = home * 0.35f + b3 * 0.65f;
        Vector3 n1 = foulNormal(b1 - home, +1.0f);
        Vector3 n3 = foulNormal(b3 - home, -1.0f);
        Vector3 along1 = (b1 - home);
        along1.y = 0.0f;
        float a1len = along1.magnitude();
        if (a1len > 1e-4f) {
            along1 = along1 * (1.0f / a1len);
        }
        Vector3 along3 = (b3 - home);
        along3.y = 0.0f;
        float a3len = along3.magnitude();
        if (a3len > 1e-4f) {
            along3 = along3 * (1.0f / a3len);
        }
        // 20 ft long × 10 ft deep → 10 × 5 world units; center 15 ft (7.5 u) off line.
        const float boxLen = 10.0f;
        const float boxDepth = 5.0f;
        const float offLine = 7.5f;
        auto placeCoachBox = [&](const Vector3& mid, const Vector3& along, const Vector3& foulN) {
            Vector3 center = mid + foulN * (offLine + boxDepth * 0.5f);
            // Local axes: along baseline, into foul
            Vector3 ax = along * (boxLen * 0.5f);
            Vector3 ay = foulN * (boxDepth * 0.5f);
            Vector3 c0 = center - ax - ay;
            Vector3 c1 = center + ax - ay;
            Vector3 c2 = center + ax + ay;
            Vector3 c3 = center - ax + ay;
            // Slight dirt pad under chalk box
            addFlatQuad(m, c0, c1, c2, c3, 0.018f, shadeColor(dirt, 0.94f));
            // Store corners for chalk in lines mesh via global... just draw chalk here as thin white
            sf::Color chalk(245, 245, 235);
            auto edge = [&](const Vector3& a, const Vector3& b) {
                Vector3 d = b - a;
                d.y = 0.0f;
                float len = d.magnitude();
                if (len < 1e-4f) {
                    return;
                }
                d = d * (1.0f / len);
                Vector3 side(-d.z * 0.07f, 0, d.x * 0.07f);
                Vector3 a0 = a + Vector3(0, 0.074f, 0);
                Vector3 b0 = b + Vector3(0, 0.074f, 0);
                addQuad(m, a0 + side, a0 - side, b0 - side, b0 + side, chalk);
            };
            edge(c0, c1);
            edge(c1, c2);
            edge(c2, c3);
            edge(c3, c0);
        };
        placeCoachBox(mid1, along1, n1);
        placeCoachBox(mid3, along3, n3);
    }

    // Dugouts on foul side of baselines
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
        addBox(m, Vector3(mid1.x, 1.0f, mid1.z) + n1 * 10.0f, 14.0f, 2.0f, 4.0f, sf::Color(50, 60, 70));
        addBox(m, Vector3(mid3.x, 1.0f, mid3.z) + n3 * 10.0f, 14.0f, 2.0f, 4.0f, sf::Color(50, 60, 70));
        // Dugout roofs / rail
        addBox(m, Vector3(mid1.x, 2.15f, mid1.z) + n1 * 10.0f, 14.2f, 0.25f, 4.4f, sf::Color(40, 48, 55));
        addBox(m, Vector3(mid3.x, 2.15f, mid3.z) + n3 * 10.0f, 14.2f, 0.25f, 4.4f, sf::Color(40, 48, 55));
    }

    m.rebuildNormals();
    return m;
}

Mesh3D buildWalls(const Layout& L) {
    Mesh3D m;
    const float aL = -L.foulAngleRad();
    const float aR = L.foulAngleRad();
    const int segs = 112; // denser fence silhouette

    // Main fence — Rogers dark navy OF wall (photo).
    sf::Color wall = ofWallColor();
    sf::Color wallTop(28, 55, 95);
    sf::Color pad(14, 32, 60);
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

    // Distance markers on wall — larger white plates + bar "digits" for field read.
    auto mark = [&](float angDeg, int hundreds, int tens, int ones) {
        float ang = angDeg * pi / 180.0f;
        float r = L.wallRAtAngle(ang);
        float h = L.wallHeightAtAngle(ang);
        Vector3 c = L.fromHome(r - 0.55f, ang, h * 0.62f);
        // Plate
        addBox(m, c, 4.2f, 2.6f, 0.45f, sf::Color(252, 252, 248));
        // Dark border frame
        addBox(m, c + Vector3(0, 0, 0.05f), 4.5f, 2.9f, 0.2f, sf::Color(40, 45, 50));
        // Simple 7-seg style bars for three digits (readable as block numerals)
        auto digit = [&](float xOff, int d) {
            Vector3 o = c + Vector3(xOff, 0.0f, 0.28f);
            sf::Color ink(25, 30, 35);
            // Always draw a chunky block "numeral" using horizontal/vertical bars
            // based on digit value patterns (rough but legible at distance).
            bool segs[7] = {false, false, false, false, false, false, false};
            // 0-9 seven-seg
            switch (d) {
                case 0: segs[0]=segs[1]=segs[2]=segs[3]=segs[4]=segs[5]=true; break;
                case 1: segs[1]=segs[2]=true; break;
                case 2: segs[0]=segs[1]=segs[6]=segs[4]=segs[3]=true; break;
                case 3: segs[0]=segs[1]=segs[6]=segs[2]=segs[3]=true; break;
                case 4: segs[5]=segs[6]=segs[1]=segs[2]=true; break;
                case 5: segs[0]=segs[5]=segs[6]=segs[2]=segs[3]=true; break;
                case 6: segs[0]=segs[5]=segs[6]=segs[2]=segs[3]=segs[4]=true; break;
                case 7: segs[0]=segs[1]=segs[2]=true; break;
                case 8: segs[0]=segs[1]=segs[2]=segs[3]=segs[4]=segs[5]=segs[6]=true; break;
                case 9: segs[0]=segs[1]=segs[2]=segs[3]=segs[5]=segs[6]=true; break;
                default: break;
            }
            const float bw = 0.55f;
            const float bh = 0.14f;
            const float bv = 0.55f;
            if (segs[0]) addBox(m, o + Vector3(0, 0.75f, 0), bw, bh, 0.12f, ink);           // top
            if (segs[6]) addBox(m, o + Vector3(0, 0.0f, 0), bw, bh, 0.12f, ink);            // mid
            if (segs[3]) addBox(m, o + Vector3(0, -0.75f, 0), bw, bh, 0.12f, ink);          // bot
            if (segs[5]) addBox(m, o + Vector3(-0.28f, 0.38f, 0), bh, bv, 0.12f, ink);      // UL
            if (segs[1]) addBox(m, o + Vector3(0.28f, 0.38f, 0), bh, bv, 0.12f, ink);       // UR
            if (segs[4]) addBox(m, o + Vector3(-0.28f, -0.38f, 0), bh, bv, 0.12f, ink);     // LL
            if (segs[2]) addBox(m, o + Vector3(0.28f, -0.38f, 0), bh, bv, 0.12f, ink);      // LR
        };
        digit(-1.2f, hundreds);
        digit(0.0f, tens);
        digit(1.2f, ones);
        // Accent bar under plate
        addBox(m, c + Vector3(0, -1.55f, 0.2f), 3.6f, 0.2f, 0.15f, sf::Color(200, 40, 50));
    };
    mark(-45.0f, 3, 3, 0); // LF 330
    mark(-18.0f, 3, 7, 5); // LCF 375
    mark(0.0f, 4, 0, 0);   // CF 400
    mark(22.0f, 3, 5, 5);  // RCF 355
    mark(45.0f, 3, 1, 8);  // RF porch 318

    // CF scoreboard chassis (screen face is a separate mesh for pulse).
    // OF ad panels as solid color blocks only (no logos).
    {
        static const sf::Color ads[] = {
            sf::Color(20, 90, 50),  sf::Color(25, 55, 110), sf::Color(200, 200, 205),
            sf::Color(30, 100, 70), sf::Color(15, 45, 95),  sf::Color(180, 40, 45)
        };
        for (int k = 0; k < 14; k++) {
            float t = (static_cast<float>(k) + 0.5f) / 14.0f;
            float ang = aL + (aR - aL) * t;
            float r = L.wallRAtAngle(ang) - 0.2f;
            float h = L.wallHeightAtAngle(ang) * 0.55f;
            Vector3 c = L.fromHome(r, ang, h);
            addBox(m, c, 5.5f, 2.8f, 0.25f, ads[static_cast<unsigned>(k) % 6]);
        }
    }
    // Batter's-eye / board chassis is built in buildHotel for photo scale.
    float cfR = L.wallRAtAngle(0.0f);
    float cfH = L.wallHeightAtAngle(0.0f);
    Vector3 cf = L.fromHome(cfR + 6.0f, 0.0f, cfH + 6.0f);
    addBox(m, cf, 28.0f, 8.0f, 1.5f, sf::Color(12, 20, 32));

    // Batter's eye — dark green CF wall panel (what the batter looks at)
    {
        const float eyeHalfAng = 0.22f; // ~±12.5° of CF
        const int eyeSegs = 10;
        sf::Color eye(18, 55, 28);
        sf::Color eyeDark(12, 40, 20);
        for (int i = 0; i < eyeSegs; i++) {
            float t0 = static_cast<float>(i) / eyeSegs;
            float t1 = static_cast<float>(i + 1) / eyeSegs;
            float a0 = -eyeHalfAng + 2.0f * eyeHalfAng * t0;
            float a1 = -eyeHalfAng + 2.0f * eyeHalfAng * t1;
            float r0 = L.wallRAtAngle(a0) - 0.15f;
            float r1 = L.wallRAtAngle(a1) - 0.15f;
            float h0 = L.wallHeightAtAngle(a0);
            float h1 = L.wallHeightAtAngle(a1);
            // Raised batter's-eye panel above wall pad height
            float yBot = std::max(h0, h1) * 0.35f;
            float yTop = std::max(h0, h1) + 6.5f;
            sf::Color col = (i % 2) ? eye : eyeDark;
            addQuad(
                m,
                L.fromHome(r0, a0, yBot),
                L.fromHome(r1, a1, yBot),
                L.fromHome(r1, a1, yTop),
                L.fromHome(r0, a0, yTop),
                col
            );
        }
        // Frame rails
        for (float aSign : {-1.0f, 1.0f}) {
            float a = aSign * eyeHalfAng;
            float r = L.wallRAtAngle(a) - 0.1f;
            float h = L.wallHeightAtAngle(a);
            addBox(
                m,
                L.fromHome(r, a, h * 0.5f + 3.0f),
                0.35f,
                h + 6.5f,
                0.35f,
                sf::Color(40, 50, 45)
            );
        }
    }

    m.rebuildNormals();
    return m;
}

Mesh3D buildScoreboardScreen(const Layout& L) {
    Mesh3D m;
    // Dark CF videoboard face (no logos) — plate-cam silhouette.
    float cfR = L.wallRAtAngle(0.0f);
    float cfH = L.wallHeightAtAngle(0.0f);
    float rBoard = std::min(cfR + 9.0f, L.maxRadiusFromHome(0.0f) - 22.0f);
    Vector3 cf = L.fromHome(rBoard, 0.0f, cfH + 13.5f);
    addBox(m, cf + Vector3(0, 0, -1.4f), 46.0f, 14.0f, 0.45f, sf::Color(18, 36, 68));
    // Soft panel grid (abstract, not branding).
    for (int col = -5; col <= 5; col++) {
        for (int row = -2; row <= 2; row++) {
            addBox(
                m,
                cf + Vector3(static_cast<float>(col) * 3.7f, static_cast<float>(row) * 2.3f, -1.6f),
                3.1f, 1.9f, 0.12f,
                sf::Color(28, 58, 105)
            );
        }
    }
    m.rebuildNormals();
    return m;
}

// Deterministic pseudo-random in [0,1) from integer seed.
float hash01(int n) {
    unsigned x = static_cast<unsigned>(n) * 1664525u + 1013904223u;
    return static_cast<float>(x & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

// Free-function wrappers used inside this TU (Layout methods are the API).
float bowlInnerRadius(const Layout& L, float ang) {
    return L.bowlInnerRadius(ang);
}
float bowlBaseHeight(const Layout& L, float ang) {
    return L.bowlBaseHeight(ang);
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
    // Rogers crowd — heavy Blue Jays navy/royal + white/grey home mix.
    static const sf::Color shirts[] = {
        sf::Color(12, 35, 64),    // Jays navy
        sf::Color(28, 78, 168),   // royal blue
        sf::Color(240, 242, 245), // white
        sf::Color(200, 16, 46),   // Canada red accent
        sf::Color(20, 20, 25),    // black
        sf::Color(180, 185, 190), // grey
        sf::Color(30, 60, 140),   // blue alt
        sf::Color(12, 35, 64),    // navy again (weight)
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
    // Photo reference: deep navy three-tier bowl wrapping the field.
    sf::Color seat0 = seatBlueColor();
    sf::Color seat0b = seatBlueAltColor();
    sf::Color seat1 = seatMidColor();
    sf::Color seat1b = shadeColor(seat1, 0.92f);
    sf::Color seat2 = seatUpperColor();
    sf::Color seat2b = shadeColor(seat2, 0.90f);
    sf::Color riser(22, 32, 52);
    sf::Color aisle(55, 62, 78);
    sf::Color concourse = concourseColor();
    sf::Color rail(200, 205, 210);

    // Full horseshoe bowl around the diamond (except narrow CF board cutout).
    // Moderate rise + enough radial depth so overhead views show continuous rings.
    const int angSegs = 160;
    const int rows0 = 16; // field level
    const int rows1 = 12; // mid
    const int rows2 = 14; // upper
    const float dRow = 1.65f;
    const float rise0 = 1.15f;
    const float rise1 = 1.25f;
    const float rise2 = 1.35f;
    const float gap01 = 2.8f;
    const float gap12 = 3.0f;
    const float facadeH01 = 4.2f;
    const float facadeH12 = 4.8f;

    for (int i = 0; i < angSegs; i++) {
        float t0 = static_cast<float>(i) / angSegs;
        float t1 = static_cast<float>(i + 1) / angSegs;
        float ang0 = -pi + t0 * 2.0f * pi;
        float ang1 = -pi + t1 * 2.0f * pi;
        float angM = 0.5f * (ang0 + ang1);

        // No seating at all behind CF scoreboard / hotel.
        if (L.isCfScoreboardZone(angM)) {
            continue;
        }

        float rIn = bowlInnerRadius(L, angM);
        float yBase = bowlBaseHeight(L, angM);
        const float rCap =
            L.closedDome ? L.clampRadiusInDome(angM, L.maxRadiusFromHome(angM), 12.0f) : 1.0e6f;
        bool isAisle = (i % 8) == 0;

        auto emitTier = [&](int rows, float rStart, float yStart, float rise, sf::Color a,
                            sf::Color b, float& rOut, float& yOut) {
            float rCursor = rStart;
            float yCursor = yStart;
            for (int row = 0; row < rows; row++) {
                if (rCursor > rCap - dRow) {
                    break;
                }
                float r0 = L.clampRadiusInDome(angM, rCursor, 12.0f);
                float r1 = L.clampRadiusInDome(angM, r0 + dRow * 0.90f, 12.0f);
                if (r1 <= r0 + 0.05f) {
                    break;
                }
                float y0 = yCursor;
                float y1 = y0 + rise * 0.88f;
                sf::Color sc = isAisle ? aisle : ((row + i) % 2 ? a : b);
                sc = shadeColor(sc, 0.94f + 0.06f * static_cast<float>((row + i) % 3) / 2.0f);
                // Seat tread
                addQuad(
                    m, L.fromHome(r0, ang0, y1), L.fromHome(r0, ang1, y1), L.fromHome(r1, ang1, y1),
                    L.fromHome(r1, ang0, y1), sc
                );
                // Riser face (what plate-cam mostly sees)
                addQuad(
                    m, L.fromHome(r0, ang0, y0), L.fromHome(r0, ang1, y0), L.fromHome(r0, ang1, y1),
                    L.fromHome(r0, ang0, y1), riser
                );
                rCursor = r1 + dRow * 0.06f;
                yCursor = y0 + rise;
            }
            rOut = rCursor;
            yOut = yCursor;
        };

        // ── Level 0: field bowl ──────────────────────────────────────
        float rAfter0 = rIn;
        float yAfter0 = yBase;
        emitTier(rows0, rIn, yBase, rise0, seat0, seat0b, rAfter0, yAfter0);

        // Tall mid-deck facade
        float yMidBase = yAfter0 + facadeH01;
        float rMidIn = L.clampRadiusInDome(angM, rAfter0 + 0.35f, 12.0f);
        addQuad(
            m, L.fromHome(rAfter0, ang0, yAfter0), L.fromHome(rAfter0, ang1, yAfter0),
            L.fromHome(rAfter0, ang1, yMidBase), L.fromHome(rAfter0, ang0, yMidBase),
            shadeColor(riser, 0.85f)
        );
        float rC0 = L.clampRadiusInDome(angM, rMidIn + gap01, 12.0f);
        if (rC0 > rMidIn + 0.3f) {
            addQuad(
                m, L.fromHome(rMidIn, ang0, yMidBase), L.fromHome(rMidIn, ang1, yMidBase),
                L.fromHome(rC0, ang1, yMidBase), L.fromHome(rC0, ang0, yMidBase), concourse
            );
            addQuad(
                m, L.fromHome(rMidIn, ang0, yMidBase + 0.7f),
                L.fromHome(rMidIn, ang1, yMidBase + 0.7f),
                L.fromHome(rMidIn + 0.18f, ang1, yMidBase + 0.7f),
                L.fromHome(rMidIn + 0.18f, ang0, yMidBase + 0.7f), rail
            );
        }

        // ── Level 1: mid ─────────────────────────────────────────────
        float rAfter1 = rC0;
        float yAfter1 = yMidBase + 0.9f;
        emitTier(rows1, rC0, yMidBase + 0.9f, rise1, seat1, seat1b, rAfter1, yAfter1);

        float yUpBase = yAfter1 + facadeH12;
        float rUpIn = L.clampRadiusInDome(angM, rAfter1 + 0.35f, 12.0f);
        addQuad(
            m, L.fromHome(rAfter1, ang0, yAfter1), L.fromHome(rAfter1, ang1, yAfter1),
            L.fromHome(rAfter1, ang1, yUpBase), L.fromHome(rAfter1, ang0, yUpBase),
            shadeColor(riser, 0.78f)
        );
        float rC1 = L.clampRadiusInDome(angM, rUpIn + gap12, 12.0f);
        if (rC1 > rUpIn + 0.3f) {
            addQuad(
                m, L.fromHome(rUpIn, ang0, yUpBase), L.fromHome(rUpIn, ang1, yUpBase),
                L.fromHome(rC1, ang1, yUpBase), L.fromHome(rC1, ang0, yUpBase), concourse
            );
        }

        // ── Level 2: upper deck ──────────────────────────────────────
        float rAfter2 = rC1;
        float yAfter2 = yUpBase + 1.0f;
        emitTier(rows2, rC1, yUpBase + 1.0f, rise2, seat2, seat2b, rAfter2, yAfter2);
        addQuad(
            m, L.fromHome(rAfter2, ang0, yAfter2 + 0.15f),
            L.fromHome(rAfter2, ang1, yAfter2 + 0.15f),
            L.fromHome(rAfter2, ang1, yAfter2 + 1.1f),
            L.fromHome(rAfter2, ang0, yAfter2 + 1.1f), rail
        );
    }

    // Backstop / netting behind home
    const float backL = pi - 1.05f;
    const float backR = pi + 1.05f;
    float backInner = 12.5f;
    addArcWall(
        m, L, backInner, backL, backR, 0.0f, 4.2f, 40, sf::Color(70, 78, 88),
        sf::Color(95, 105, 115)
    );
    addArcWall(
        m, L, backInner + 0.15f, backL + 0.02f, backR - 0.02f, 4.0f, 9.5f, 36,
        sf::Color(55, 70, 95), sf::Color(80, 95, 115)
    );
    addArcWall(
        m, L, backInner + 0.4f, backL + 0.05f, backR - 0.05f, 8.5f, 18.0f, 36,
        sf::Color(200, 210, 220, 75), sf::Color(180, 190, 200, 80)
    );
    for (int i = 0; i <= 12; i++) {
        float t = static_cast<float>(i) / 12.0f;
        float ang = backL + (backR - backL) * t;
        Vector3 base = L.fromHome(backInner + 0.2f, ang, 0.0f);
        addBox(m, base + Vector3(0, 9.0f, 0), 0.18f, 18.0f, 0.18f, sf::Color(160, 165, 170));
    }
    addBox(m, Vector3(0.0f, 5.5f, plateZ + 11.5f), 10.0f, 0.35f, 0.8f, sf::Color(90, 95, 100));

    // Press / club glass behind home (in-shell)
    addBox(m, Vector3(0.0f, 16.0f, plateZ + 16.0f), 24.0f, 5.5f, 5.5f, sf::Color(50, 60, 75));
    addBox(m, Vector3(0.0f, 16.0f, plateZ + 13.2f), 22.0f, 4.2f, 0.35f, sf::Color(110, 160, 200, 150));

    // Suite boxes 1B/3B
    addBox(m, Vector3(18.0f, 7.0f, plateZ - 6.0f), 8.0f, 3.5f, 5.0f, sf::Color(50, 60, 80));
    addBox(m, Vector3(-18.0f, 7.0f, plateZ - 6.0f), 8.0f, 3.5f, 5.0f, sf::Color(50, 60, 80));

    // Catwalk lights under roof
    sf::Color steel(160, 168, 178);
    sf::Color lamp(255, 248, 220);
    for (int i = 0; i < 18; i++) {
        float ang = -pi + (static_cast<float>(i) / 18.0f) * 2.0f * pi;
        float r = std::min(bowlInnerRadius(L, ang) + 12.0f, L.maxRadiusFromHome(ang) - 10.0f);
        Vector3 p = L.fromHome(r, ang, 0.0f);
        float y = L.domeRoofYAtWorld(p.x, p.z) * 0.80f;
        Vector3 base = L.fromHome(r, ang, y);
        addBox(m, base, 1.0f, 0.45f, 2.2f, steel);
        addBox(m, base + Vector3(0.0f, -0.45f, 0.0f), 2.0f, 0.28f, 1.1f, lamp);
    }

    m.rebuildNormals();
    return m;
}

// CF hotel + videoboard stack (photo reference, no logos).
Mesh3D buildHotel(const Layout& L) {
    Mesh3D m;
    if (!L.closedDome) {
        return m;
    }
    float cfR = L.wallRAtAngle(0.0f);
    float cfH = L.wallHeightAtAngle(0.0f);
    float rBoard = std::min(cfR + 9.0f, L.maxRadiusFromHome(0.0f) - 22.0f);
    float rHotel = std::min(cfR + 17.0f, L.maxRadiusFromHome(0.0f) - 12.0f);

    // Large dark videoboard face (solid blocks, no branding).
    Vector3 board = L.fromHome(rBoard, 0.0f, cfH + 13.5f);
    addBox(m, board, 48.0f, 16.0f, 2.2f, boardChassisColor());
    addBox(m, board + Vector3(0, 0, -1.3f), 44.0f, 13.5f, 0.4f, sf::Color(20, 40, 70));
    // Abstract panel grid on the board (not logos).
    for (int col = -5; col <= 5; col++) {
        for (int row = -2; row <= 2; row++) {
            addBox(
                m,
                board + Vector3(static_cast<float>(col) * 3.6f, static_cast<float>(row) * 2.4f, -1.5f),
                3.0f, 2.0f, 0.15f,
                sf::Color(30, 70, 120)
            );
        }
    }

    // White hotel facade with window grid above/behind the board.
    Vector3 hotel = L.fromHome(rHotel, 0.0f, cfH + 24.0f);
    sf::Color facade = hotelFacadeColor();
    sf::Color facadeDark(200, 195, 185);
    sf::Color glass(70, 110, 150, 210);
    sf::Color frame(80, 85, 92);
    addBox(m, hotel, 46.0f, 30.0f, 12.0f, facade);
    addBox(m, hotel + Vector3(-16.0f, -2.0f, 1.0f), 14.0f, 24.0f, 8.0f, facadeDark);
    addBox(m, hotel + Vector3(16.0f, -2.0f, 1.0f), 14.0f, 24.0f, 8.0f, facadeDark);
    for (int row = 0; row < 7; row++) {
        for (int col = -5; col <= 5; col++) {
            if ((row + col) % 4 == 0) {
                continue;
            }
            Vector3 w = hotel + Vector3(
                static_cast<float>(col) * 3.2f, -11.0f + static_cast<float>(row) * 3.4f, -6.2f
            );
            addBox(m, w, 2.4f, 2.2f, 0.28f, glass);
            addBox(m, w + Vector3(0, 0, 0.12f), 2.55f, 2.35f, 0.08f, frame);
        }
    }
    addBox(m, hotel + Vector3(0.0f, 16.0f, 0.0f), 48.0f, 1.6f, 13.0f, frame);
    // Simple flag poles (no logos) along hotel roof.
    for (int f = -3; f <= 3; f++) {
        Vector3 fp = hotel + Vector3(static_cast<float>(f) * 5.5f, 18.0f, -5.0f);
        addBox(m, fp, 0.12f, 3.5f, 0.12f, sf::Color(180, 185, 190));
        addBox(m, fp + Vector3(0.7f, 1.2f, 0), 1.4f, 0.7f, 0.08f, sf::Color(200, 40, 50));
    }
    m.rebuildNormals();
    return m;
}

// Arched steel lattice roof (photo: dense truss under membrane).
Mesh3D buildStructure(const Layout& L) {
    Mesh3D m;
    if (!L.closedDome) {
        return m;
    }
    const Vector3 c = L.domeCenter();
    const float Rh = L.domeHorizR() * 0.96f;
    sf::Color truss = domeRibColor();
    sf::Color trussHi(70, 82, 98);
    sf::Color trussLite(110, 120, 135);

    // Primary arch ribs — outer ring so the diamond stays visible from above.
    const int nArches = 28;
    for (int i = 0; i < nArches; i++) {
        float ang = -pi + (static_cast<float>(i) / nArches) * 2.0f * pi;
        for (int s = 0; s < 7; s++) {
            float t0 = 0.55f + 0.45f * (static_cast<float>(s) / 7.0f);
            float t1 = 0.55f + 0.45f * (static_cast<float>(s + 1) / 7.0f);
            float r0 = t0 * Rh;
            float r1 = t1 * Rh;
            auto pt = [&](float r) {
                float x = c.x + std::sin(ang) * r;
                float z = c.z - std::cos(ang) * r;
                float y = L.domeRoofYAtWorld(x, z) - 1.8f;
                return Vector3(x, y, z);
            };
            Vector3 a = pt(r0);
            Vector3 b = pt(r1);
            Vector3 mid = (a + b) * 0.5f;
            float len = (b - a).magnitude();
            if (len < 0.3f) {
                continue;
            }
            float thick = (i % 4 == 0) ? 0.72f : 0.42f;
            addBox(m, mid, thick, thick * 0.7f, len * 0.92f, (i % 4 == 0) ? trussHi : truss);
        }
    }

    // Circumferential ring trusses (outer bowl only).
    for (int ring = 0; ring < 4; ring++) {
        float r = Rh * (0.60f + 0.10f * static_cast<float>(ring));
        int segs = 48 + ring * 4;
        for (int j = 0; j < segs; j++) {
            float a0 = -pi + (static_cast<float>(j) / segs) * 2.0f * pi;
            float a1 = -pi + (static_cast<float>(j + 1) / segs) * 2.0f * pi;
            auto pt = [&](float a) {
                float x = c.x + std::sin(a) * r;
                float z = c.z - std::cos(a) * r;
                return Vector3(x, L.domeRoofYAtWorld(x, z) - 2.0f, z);
            };
            Vector3 p0 = pt(a0);
            Vector3 p1 = pt(a1);
            Vector3 mid = (p0 + p1) * 0.5f;
            float chord = (p1 - p0).magnitude();
            addBox(m, mid, chord * 0.94f, 0.32f, 0.42f, (ring % 2) ? trussLite : truss);
        }
    }

    // Cross braces between arches (outer ring only).
    for (int i = 0; i < nArches; i += 2) {
        float ang0 = -pi + (static_cast<float>(i) / nArches) * 2.0f * pi;
        float ang1 = -pi + (static_cast<float>(i + 1) / nArches) * 2.0f * pi;
        for (int s = 6; s < 10; s += 2) {
            float r = (static_cast<float>(s) / 10.0f) * Rh;
            auto pt = [&](float ang) {
                float x = c.x + std::sin(ang) * r;
                float z = c.z - std::cos(ang) * r;
                return Vector3(x, L.domeRoofYAtWorld(x, z) - 2.1f, z);
            };
            Vector3 p0 = pt(ang0);
            Vector3 p1 = pt(ang1);
            Vector3 mid = (p0 + p1) * 0.5f;
            float chord = (p1 - p0).magnitude();
            addBox(m, mid, chord * 0.9f, 0.22f, 0.28f, trussLite);
        }
    }
    m.rebuildNormals();
    return m;
}

// Build low-poly fans in angular wedges. Skips CF scoreboard / hotel zone.
std::vector<Mesh3D> buildFanSectors(const Layout& L) {
    std::vector<Mesh3D> sectors(kFanSectorCount);
    // Match continuous 3-tier bowl — fans all around the diamond except CF board.
    const int angSamples = 200;
    const int rows0 = 14;
    const int rows1 = 10;
    const int rows2 = 12;
    const float dRow = 1.65f;
    const float rise0 = 1.15f;
    const float rise1 = 1.25f;
    const float rise2 = 1.35f;
    const float facadeH01 = 4.2f;
    const float facadeH12 = 4.8f;
    int fanId = 0;
    auto maybePlace = [&](Mesh3D& sec, float r, float ang, float y, float fillChance) {
        float h = hash01(fanId * 17 + 3);
        if (h > fillChance) {
            fanId++;
            return;
        }
        if (hash01(fanId * 9 + 1) < 0.08f) {
            fanId++;
            return;
        }
        r = L.clampRadiusInDome(ang, r, 14.0f);
        Vector3 seat = L.fromHome(r, ang, y);
        {
            Vector3 c = L.domeCenter();
            float hh = std::sqrt((seat.x - c.x) * (seat.x - c.x) + (seat.z - c.z) * (seat.z - c.z));
            if (hh > L.domeHorizR() - 10.0f) {
                fanId++;
                return;
            }
        }
        seat.x += (hash01(fanId) - 0.5f) * 0.45f;
        seat.z += (hash01(fanId + 3) - 0.5f) * 0.45f;
        seat.y += (hash01(fanId + 5) - 0.5f) * 0.08f;
        float scale = 0.82f + 0.28f * hash01(fanId + 11);
        addLowPolyFan(sec, seat, scale, fanShirtColor(fanId), fanSkinColor(fanId + 3));
        fanId++;
    };

    for (int i = 0; i < angSamples; i++) {
        float t = (static_cast<float>(i) + 0.5f) / angSamples;
        float ang = -pi + t * 2.0f * pi;
        int sector = static_cast<int>((t * kFanSectorCount)) % kFanSectorCount;
        float rIn = bowlInnerRadius(L, ang);
        float yBase = bowlBaseHeight(L, ang);
        const float rCap =
            L.closedDome ? L.clampRadiusInDome(ang, L.maxRadiusFromHome(ang), 14.0f) : 1.0e6f;

        // CF board zone: only ONE elevated fan row above the scoreboard.
        if (L.isCfScoreboardZone(ang)) {
            float wallR = L.wallRAtAngle(ang);
            float yBoardTop = L.wallHeightAtAngle(ang) + 22.0f;
            float rFan = L.clampRadiusInDome(ang, wallR + 20.0f, 14.0f);
            float fill = 0.75f + 0.2f * hash01(i * 11);
            maybePlace(sectors[sector], rFan, ang, yBoardTop, fill);
            continue;
        }

        // Field level
        float r = rIn;
        float y = yBase;
        for (int row = 0; row < rows0; row++) {
            if (r > rCap - dRow) {
                break;
            }
            float fill = 0.70f + 0.25f * hash01(i * 3 + row);
            maybePlace(sectors[sector], r + dRow * 0.4f, ang, y + rise0 * 0.7f, fill);
            r += dRow;
            y += rise0;
        }

        // Mid level
        float yMid = y + facadeH01 + 0.9f;
        float rMid = r + 2.0f;
        for (int row = 0; row < rows1; row++) {
            if (rMid > rCap - dRow) {
                break;
            }
            float fill = 0.65f + 0.28f * hash01(i * 5 + row + 40);
            maybePlace(sectors[sector], rMid + dRow * 0.35f, ang, yMid + rise1 * 0.7f, fill);
            rMid += dRow;
            yMid += rise1;
        }

        // Upper level
        float yUp = yMid + facadeH12 + 1.0f;
        float rUp = rMid + 2.2f;
        for (int row = 0; row < rows2; row++) {
            if (rUp > rCap - dRow) {
                break;
            }
            float fill = 0.60f + 0.30f * hash01(i * 7 + row + 90);
            maybePlace(sectors[sector], rUp + dRow * 0.35f, ang, yUp + rise2 * 0.7f, fill);
            rUp += dRow;
            yUp += rise2;
        }
    }

    // Straight row of fans directly behind the batter (first perimeter row).
    {
        const int nBack = 32;
        const float rBack = std::max(15.5f, L.bowlInnerRadius(pi) + 0.5f);
        const float yBack = L.bowlBaseHeight(pi) + 1.6f;
        for (int k = 0; k < nBack; k++) {
            float u = (static_cast<float>(k) + 0.5f) / static_cast<float>(nBack);
            float x = (u - 0.5f) * 20.0f; // straight line across behind plate
            Vector3 seat(x, yBack, L.plateZ() + rBack);
            if (hash01(k * 13 + 2) > 0.88f) {
                continue;
            }
            seat.x += (hash01(k * 3) - 0.5f) * 0.15f;
            int sector = static_cast<int>(((std::atan2(seat.x, -(seat.z - L.plateZ())) + pi) /
                                           (2.0f * pi)) *
                                          kFanSectorCount) %
                         kFanSectorCount;
            if (sector < 0) {
                sector += kFanSectorCount;
            }
            float scale = 0.88f + 0.2f * hash01(k + 50);
            addLowPolyFan(
                sectors[sector], seat, scale, fanShirtColor(k + 200), fanSkinColor(k + 40)
            );
        }
    }

    for (auto& sec : sectors) {
        sec.rebuildNormals();
    }
    return sectors;
}

void buildFlags(const Layout& L, std::vector<Mesh3D>& flags, std::vector<Vector3>& bases) {
    flags.clear();
    bases.clear();
    // Closed dome = fully interior park; no exterior flag poles outside the shell.
    if (L.closedDome) {
        return;
    }
    flags.reserve(kFlagCount);
    bases.reserve(kFlagCount);

    sf::Color poleCol(180, 185, 190);
    sf::Color flagCols[] = {
        sf::Color(200, 40, 50),
        sf::Color(30, 60, 160),
        sf::Color(240, 240, 245),
        sf::Color(230, 160, 40),
        sf::Color(40, 120, 70),
    };

    for (int i = 0; i < kFlagCount; i++) {
        float t = (static_cast<float>(i) + 0.5f) / kFlagCount;
        float ang = -pi + t * 2.0f * pi;
        float r = L.bowlInnerRadius(ang) + 6.0f + hash01(i * 5) * 4.0f;
        Vector3 base = L.fromHome(r, ang, 0.0f);
        base.y = L.bowlBaseHeight(ang) + 2.0f;

        Mesh3D m;
        // Pole local: origin at base
        addBox(m, Vector3(0, 5.0f, 0), 0.18f, 10.0f, 0.18f, poleCol);
        // Flag cloth hanging in +X from pole top
        sf::Color fc = flagCols[static_cast<unsigned>(i) % 5];
        addQuad(
            m,
            Vector3(0.1f, 9.2f, 0),
            Vector3(3.8f, 9.0f, 0.15f),
            Vector3(3.6f, 6.6f, 0.15f),
            Vector3(0.1f, 7.0f, 0),
            fc
        );
        addQuad(
            m,
            Vector3(0.1f, 9.2f, 0),
            Vector3(0.1f, 7.0f, 0),
            Vector3(3.6f, 6.6f, -0.05f),
            Vector3(3.8f, 9.0f, -0.05f),
            sf::Color(
                static_cast<std::uint8_t>(fc.r * 0.85f),
                static_cast<std::uint8_t>(fc.g * 0.85f),
                static_cast<std::uint8_t>(fc.b * 0.85f)
            )
        );
        m.rebuildNormals();
        flags.push_back(std::move(m));
        bases.push_back(base);
    }
}

Mesh3D buildCity(const Layout& L) {
    Mesh3D m;
    // Rogers Centre closed = no exterior city / suburbs outside the dome.
    if (L.closedDome) {
        return m;
    }
    const float plateZ = L.plateZ();
    const float baseR = L.maxWallR();
    // Match in-park grass so the suburb ring doesn't show a color seam.
    sf::Color grassFar = grassColor();
    sf::Color grassFarDark = grassDarkColor();
    sf::Color road(55, 55, 58);
    sf::Color sidewalk(120, 118, 110);
    sf::Color roof(90, 55, 45);
    sf::Color brick(140, 95, 75);
    sf::Color stucco(190, 175, 150);
    sf::Color glass(90, 130, 160);
    sf::Color tree(30, 90, 40);
    sf::Color trunk(70, 50, 30);

    // Suburb ground starts just outside the max fence (overlaps stand apron slightly).
    addAnnulusSector(m, L, baseR + 1.0f, baseR + 220.0f, -pi, pi, -0.02f, 96, grassFar);
    // Subtle outer grass darkening for depth
    addAnnulusSector(m, L, baseR + 160.0f, baseR + 220.0f, -pi, pi, -0.01f, 64, grassFarDark);
    // Outer ring road
    addAnnulusSector(
        m, L, baseR + 55.0f, baseR + 62.0f, -pi, pi, 0.01f, 80, road
    );
    addAnnulusSector(
        m, L, baseR + 120.0f, baseR + 128.0f, -pi, pi, 0.01f, 80, road
    );

    // Buildings full 360° so every camera sees a skyline.
    const int buildingCount = 140;
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

    // Parked cars on the ring roads (static traffic — park feels populated).
    for (int i = 0; i < 48; i++) {
        float t = static_cast<float>(i) / 48.0f;
        float ang = -pi + t * 2.0f * pi;
        float r = (i % 2 == 0) ? (baseR + 58.0f) : (baseR + 123.0f);
        Vector3 c = L.fromHome(r, ang, 0.55f);
        sf::Color body(
            static_cast<std::uint8_t>(40 + hash01(i) * 180),
            static_cast<std::uint8_t>(40 + hash01(i + 2) * 120),
            static_cast<std::uint8_t>(40 + hash01(i + 4) * 160)
        );
        addBox(m, c, 2.4f, 0.9f, 1.2f, body);
        addBox(m, c + Vector3(0, 0.55f, 0), 1.4f, 0.55f, 1.05f, sf::Color(60, 90, 120, 180));
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
        thickLine(L.home(), L.fromHome(len, ang, 0.0f), 0.16f, 0.072f);
    };
    foulLine(-L.foulAngleRad());
    foulLine(L.foulAngleRad());
    // Diamond chalk: base paths (inside edge of dirt)
    thickLine(L.home(), L.firstBase(), 0.10f, 0.078f);
    thickLine(L.firstBase(), L.secondBase(), 0.10f, 0.078f);
    thickLine(L.secondBase(), L.thirdBase(), 0.10f, 0.078f);
    thickLine(L.thirdBase(), L.home(), 0.10f, 0.078f);
    // Batter's box chalk outlines (match ~4×6 ft dirt boxes)
    {
        auto boxOutline = [&](float cx) {
            const float hw = 1.0f;
            const float hz = 1.5f;
            const float zc = L.plateZ() - 0.35f;
            Vector3 a(cx - hw, 0, zc + hz);
            Vector3 b(cx + hw, 0, zc + hz);
            Vector3 c(cx + hw, 0, zc - hz);
            Vector3 d(cx - hw, 0, zc - hz);
            thickLine(a, b, 0.06f, 0.076f);
            thickLine(b, c, 0.06f, 0.076f);
            thickLine(c, d, 0.06f, 0.076f);
            thickLine(d, a, 0.06f, 0.076f);
        };
        boxOutline(-1.65f);
        boxOutline(1.65f);
    }
    m.rebuildNormals();
    return m;
}

} // namespace

float Layout::foulAngleRad() const {
    return foulAngleDegrees * (pi / 180.0f);
}

// Official Rogers Centre OF fence (feet) vs angle from CF.
// LF 328 · LCF 368 · LCF alley 381 · CF 400 · RCF alley 372 · RCF 359 · RF 328.
float Layout::wallFeetAtAngle(float angleRad) const {
    float aDeg = angleRad * (180.0f / pi);
    aDeg = std::clamp(aDeg, -foulAngleDegrees, foulAngleDegrees);

    static const float samples[][2] = {
        {-45.0f, 328.0f}, // LF foul pole
        {-32.0f, 368.0f}, // left-centre
        {-22.0f, 381.0f}, // LCF power alley
        {-8.0f, 395.0f},
        {0.0f, 400.0f},   // dead CF
        {10.0f, 385.0f},
        {20.0f, 372.0f},  // RCF power alley
        {32.0f, 359.0f},  // right-centre
        {45.0f, 328.0f},  // RF foul pole
    };
    constexpr int n = static_cast<int>(sizeof(samples) / sizeof(samples[0]));
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
    // Rogers OF wall is a relatively uniform ~10 ft face.
    return wallHeightFeet / feetPerUnit;
}

Vector3 Layout::domeCenter() const {
    // Circular building center is shifted toward CF from home.
    return Vector3(0.0f, 0.0f, plateZ() - domeCenterOffsetFeet / feetPerUnit);
}

float Layout::domeRoofYAtWorld(float worldX, float worldZ) const {
    // Ellipsoid roof from the circular building center (matches drawn shell).
    // ((x-cx)/Rh)^2 + ((z-cz)/Rh)^2 + (y/H)^2 = 1, y >= 0.
    const Vector3 c = domeCenter();
    const float Rh = std::max(domeHorizR(), 1.0f);
    const float H = std::max(roofPeakY(), wallH() + 30.0f);
    float dx = worldX - c.x;
    float dz = worldZ - c.z;
    float u2 = (dx * dx + dz * dz) / (Rh * Rh);
    if (u2 >= 0.999f) {
        // Side shell stays high enough that the upper deck is never crushed.
        return std::max(H * 0.22f, 38.0f);
    }
    float y = H * std::sqrt(std::max(0.0f, 1.0f - u2));
    // Floor the roof so mid/upper seating always has headroom under the shell.
    return std::max(y, 36.0f + (1.0f - std::sqrt(u2)) * 40.0f);
}

float Layout::domeRoofYAtRadius(float radiusFromHome) const {
    // Approximate using CF ray (for callers that only have polar-from-home).
    Vector3 p = fromHome(radiusFromHome, 0.0f, 0.0f);
    return domeRoofYAtWorld(p.x, p.z);
}

float Layout::maxRadiusFromHome(float angleRad) const {
    // Ray from home: P(r) = home + r * dir, dir = (sin a, 0, -cos a).
    // Intersect with circle (x-cx)^2 + (z-cz)^2 = Rh^2 centered at domeCenter.
    const Vector3 c = domeCenter();
    const float Rh = domeHorizR();
    const float hx = 0.0f;
    const float hz = plateZ();
    const float dx = std::sin(angleRad);
    const float dz = -std::cos(angleRad);
    const float ox = hx - c.x;
    const float oz = hz - c.z;
    const float A = dx * dx + dz * dz; // = 1
    const float B = 2.0f * (ox * dx + oz * dz);
    const float C = ox * ox + oz * oz - Rh * Rh;
    float disc = B * B - 4.0f * A * C;
    if (disc < 0.0f) {
        return maxWallR() * 0.5f;
    }
    float r = (-B + std::sqrt(disc)) / (2.0f * A); // outward positive root
    return std::max(r, wallRAtAngle(angleRad) + 2.0f);
}

float Layout::clampRadiusInDome(float angleRad, float radius, float margin) const {
    // Keep seating / geometry safely inside the circular building shell.
    float rMax = maxRadiusFromHome(angleRad) - margin;
    float r = std::min(std::max(radius, 0.0f), std::max(rMax, 1.0f));
    const Vector3 c = domeCenter();
    const float maxH = std::max(domeHorizR() - margin, 1.0f);
    // Binary search so the actual world point is inside the circle.
    float lo = 0.0f;
    float hi = r;
    for (int i = 0; i < 14; i++) {
        float mid = 0.5f * (lo + hi);
        Vector3 p = fromHome(mid, angleRad, 0.0f);
        float h = std::sqrt((p.x - c.x) * (p.x - c.x) + (p.z - c.z) * (p.z - c.z));
        if (h <= maxH) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

bool Layout::isCfScoreboardZone(float angleRad) const {
    // CF board / hotel wedge (seats skipped; one fan row allowed above board).
    while (angleRad > pi) {
        angleRad -= 2.0f * pi;
    }
    while (angleRad < -pi) {
        angleRad += 2.0f * pi;
    }
    return std::abs(angleRad) < 0.28f; // ~±16°
}

float Layout::seatDeckYAtRadius(float radiusFromHome, float angleRad) const {
    // Three steep decks so plate-cam reads full field / mid / upper layers.
    float r0 = bowlInnerRadius(angleRad);
    float past = radiusFromHome - r0;
    if (past < 0.0f) {
        return 0.0f;
    }
    float yBase = bowlBaseHeight(angleRad);
    // Match buildStands vertical profile (steep, short radial depth).
    const float d0 = 18.0f;
    const float d1 = 12.0f;
    const float rise0 = 0.72f;
    const float rise1 = 0.78f;
    const float rise2 = 0.82f;
    if (past < d0) {
        return yBase + past * rise0;
    }
    float y1 = yBase + d0 * rise0 + 4.5f; // tall mid facade
    float p1 = past - d0;
    if (p1 < d1) {
        return y1 + p1 * rise1;
    }
    float y2 = y1 + d1 * rise1 + 5.0f; // tall upper facade
    return y2 + (p1 - d1) * rise2;
}

bool Layout::containInsideDome(Vector3& position, Vector3& velocity, float radius) const {
    if (!closedDome) {
        return false;
    }
    bool hit = false;
    const Vector3 c = domeCenter();
    const float Rh = domeHorizR() - radius - 0.05f;
    float dx = position.x - c.x;
    float dz = position.z - c.z;
    float horiz = std::sqrt(dx * dx + dz * dz);
    if (horiz > Rh && horiz > 1e-5f) {
        float s = Rh / horiz;
        position.x = c.x + dx * s;
        position.z = c.z + dz * s;
        // Bounce off circular shell (reflect outward component) — do not freeze.
        Vector3 n(dx / horiz, 0.0f, dz / horiz);
        float vn = velocity.x * n.x + velocity.z * n.z;
        if (vn > 0.0f) {
            velocity.x -= n.x * vn * 1.55f;
            velocity.z -= n.z * vn * 1.55f;
        }
        velocity.x *= 0.72f;
        velocity.z *= 0.72f;
        velocity.y *= 0.85f;
        hit = true;
    }
    float roofY = domeRoofYAtWorld(position.x, position.z) - radius - 0.04f;
    roofY = std::max(roofY, radius + 0.5f);
    if (position.y > roofY) {
        position.y = roofY;
        // Soft roof bounce downward — carom, don't stick.
        if (velocity.y > 0.0f) {
            velocity.y = -velocity.y * 0.38f;
        }
        velocity.x *= 0.70f;
        velocity.z *= 0.70f;
        hit = true;
    }
    if (position.y < radius + 0.01f) {
        position.y = radius + 0.01f;
        if (velocity.y < 0.0f) {
            velocity.y = -velocity.y * 0.30f;
            velocity.x *= 0.88f;
            velocity.z *= 0.88f;
        }
    }
    return hit;
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

Vector3 Layout::scoreboardCenter() const {
    float cfR = wallRAtAngle(0.0f);
    float cfH = wallHeightAtAngle(0.0f);
    float rBoard = std::min(cfR + 9.0f, maxRadiusFromHome(0.0f) - 22.0f);
    return fromHome(rBoard, 0.0f, cfH + 13.5f);
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

float Layout::bowlInnerRadius(float ang) const {
    // First seat row hugs the FIELD perimeter (not a huge foul apron).
    // Fair OF: just past the fence. Foul: parallel to the foul line a few
    // units out, joining the OF seats at the poles and wrapping tight behind home.
    while (ang > pi) {
        ang -= 2.0f * pi;
    }
    while (ang < -pi) {
        ang += 2.0f * pi;
    }
    float fa = foulAngleRad();
    float absA = std::abs(ang);
    float r = 0.0f;
    if (absA <= fa + 0.02f) {
        // Fair: seats right behind the OF wall / fence.
        r = wallRAtAngle(ang) + 1.6f;
    } else {
        // Distance past the foul line (home-polar): r * sin(delta) = clearance.
        const float clearance = 7.5f; // thin strip only — beige becomes seats
        float delta = std::max(absA - fa, 0.025f);
        float rAlong = clearance / std::sin(delta);
        float foulPoleR = wallRAtAngle(ang >= 0.0f ? fa : -fa) + 1.6f;
        // Near the foul pole, join the OF seating line.
        if (delta < 0.40f) {
            float t = delta / 0.40f;
            t = t * t * (3.0f - 2.0f * t);
            r = foulPoleR * (1.0f - t) + std::min(rAlong, foulPoleR) * t;
        } else {
            r = rAlong;
        }
        // Behind home: keep a tight straight-ish first row.
        r = std::clamp(r, 15.0f, foulPoleR);
    }
    if (closedDome) {
        r = clampRadiusInDome(ang, r, 12.0f);
    }
    return r;
}

float Layout::bowlBaseHeight(float ang) const {
    // Lower bowl starts at wall height in fair; low apron in foul (tight wrap).
    float fa = foulAngleRad();
    while (ang > pi) {
        ang -= 2.0f * pi;
    }
    while (ang < -pi) {
        ang += 2.0f * pi;
    }
    if (std::abs(ang) <= fa + 0.05f) {
        return wallHeightAtAngle(ang) + 0.35f;
    }
    float u = std::clamp((std::abs(ang) - fa) / (pi - fa), 0.0f, 1.0f);
    float u2 = u * u * (3.0f - 2.0f * u);
    return 1.0f + u2 * 3.2f;
}

namespace {

void killOutwardVelocity(Vector3& vel, const Vector3& outwardNormal, bool stick) {
    if (stick) {
        // Soft stick: kill outward component and most speed — don't zero if
        // we only grazed (caller may still zero when truly settled).
        float vn = vel.x * outwardNormal.x + vel.y * outwardNormal.y + vel.z * outwardNormal.z;
        if (vn > 0.0f) {
            vel = vel - outwardNormal * vn;
        }
        vel = vel * 0.15f;
        if (vel.magnitude() < 1.5f) {
            vel = Vector3();
        }
        return;
    }
    float vn = vel.x * outwardNormal.x + vel.y * outwardNormal.y + vel.z * outwardNormal.z;
    if (vn > 0.0f) {
        vel = vel - outwardNormal * (vn * 1.15f);
    }
}

// Cap how far a single collision may snap the ball (prevents teleports).
void softSnapPosition(Vector3& position, const Vector3& target, float maxStep = 1.25f) {
    Vector3 d = target - position;
    float len = d.magnitude();
    if (len <= maxStep || len < 1e-6f) {
        position = target;
        return;
    }
    position = position + d * (maxStep / len);
}

bool resolveAabbSphere(
    Vector3& pos,
    Vector3& vel,
    float radius,
    const Vector3& boxCenter,
    const Vector3& halfExtents,
    bool stick,
    BallCollisionHit& hit,
    HitSurface surface
) {
    Vector3 local = pos - boxCenter;
    Vector3 closest(
        std::clamp(local.x, -halfExtents.x, halfExtents.x),
        std::clamp(local.y, -halfExtents.y, halfExtents.y),
        std::clamp(local.z, -halfExtents.z, halfExtents.z)
    );
    Vector3 delta = local - closest;
    float d2 = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    if (d2 > radius * radius && d2 > 1e-10f) {
        return false;
    }
    // Inside or penetrating
    Vector3 n;
    if (d2 < 1e-8f) {
        // Deep inside — push out along smallest axis penetration.
        float px = halfExtents.x - std::abs(local.x);
        float py = halfExtents.y - std::abs(local.y);
        float pz = halfExtents.z - std::abs(local.z);
        if (px <= py && px <= pz) {
            n = Vector3(local.x >= 0.0f ? 1.0f : -1.0f, 0, 0);
            pos.x = boxCenter.x + n.x * (halfExtents.x + radius);
        } else if (py <= pz) {
            n = Vector3(0, local.y >= 0.0f ? 1.0f : -1.0f, 0);
            pos.y = boxCenter.y + n.y * (halfExtents.y + radius);
        } else {
            n = Vector3(0, 0, local.z >= 0.0f ? 1.0f : -1.0f);
            pos.z = boxCenter.z + n.z * (halfExtents.z + radius);
        }
    } else {
        float d = std::sqrt(d2);
        n = delta * (1.0f / d);
        pos = boxCenter + closest + n * radius;
    }
    killOutwardVelocity(vel, n, stick);
    hit.surface = surface;
    hit.impactY = pos.y;
    if (stick) {
        hit.stuck = true;
        vel = Vector3();
    }
    return true;
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
    const float plateZ = layout.plateZ();
    const float fa = layout.foulAngleRad();

    // ── Ground: bounce + friction; only settle when nearly dead ───────
    const float groundY = radius + 0.01f;
    bool onGround = false;
    auto speedXZ = [&]() {
        return std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    };
    if (position.y < groundY) {
        position.y = groundY;
        onGround = true;
        hit.surface = HitSurface::Ground;
        hit.impactY = groundY;
        if (velocity.y < 0.0f) {
            // Restitution bounce — ball caroms instead of freezing.
            velocity.y = -velocity.y * 0.38f;
            velocity.x *= 0.86f;
            velocity.z *= 0.86f;
        }
        // Settle only when truly slow (or caller requested stick + already soft).
        if ((stickOnContact && velocity.magnitude() < 3.8f) ||
            velocity.magnitude() < 2.6f) {
            velocity = Vector3();
            hit.stuck = true;
        }
    } else if (position.y < groundY + 0.12f && velocity.y <= 0.15f) {
        onGround = true;
        if (velocity.magnitude() < 3.2f || (stickOnContact && velocity.magnitude() < 4.5f)) {
            position.y = groundY;
            velocity = Vector3();
            hit.surface = HitSurface::Ground;
            hit.impactY = groundY;
            hit.stuck = true;
        }
    }
    (void)speedXZ;

    float r = 0.0f;
    float ang = 0.0f;
    layout.polarFromHome(position, r, ang);
    hit.sprayDeg = ang * (180.0f / pi);
    bool fair = std::abs(ang) <= fa + 0.02f;

    // ── Asymmetric OF fence (solid face below wall top) ────────────────
    // IMPORTANT: once the ball is past the fence (over the top OR already deep
    // beyond the plane), do NOT pull it back to the wall face when it drops.
    // That caused "line drive over the crowd → teleport to wall → Wall Ball".
    // Also: never overwrite a Ground landing with FenceTopClear — that blocked
    // settle and froze HRs in chase cam while the ball sank through the dirt.
    bool clearedFenceTop = false;
    if (fair && r > 1.0f) {
        float wallR = layout.wallRAtAngle(ang);
        float wallH = layout.wallHeightAtAngle(ang);
        hit.wallTopY = wallH;
        hit.fenceFeet = layout.wallFeetAtAngle(ang);
        if (r + radius > wallR) {
            const float clearY = wallH + std::max(radius, 0.35f);
            // pastDepth in world units (~2 ft/unit): how far beyond the fence plane.
            const float pastDepth = r - wallR;
            // Radial outward speed: positive = flying further into seats/street.
            Vector3 nOut(std::sin(ang), 0.0f, -std::cos(ang));
            const float vOut =
                velocity.x * nOut.x + velocity.y * nOut.y + velocity.z * nOut.z;

            if (onGround || hit.stuck) {
                // Already landed — keep Ground. Track clear geometrically only.
                if (pastDepth > 0.15f || position.y > clearY) {
                    clearedFenceTop = true;
                }
            } else if (position.y > clearY) {
                // Over the top — free flight (HR path).
                clearedFenceTop = true;
                hit.surface = HitSurface::FenceTopClear;
                hit.impactY = position.y;
            } else if (pastDepth > 0.75f) {
                // Already past the wall plane (seats / street air / just cleared).
                // Free flight even when the ball has dropped below wall height —
                // never teleport back onto the fence face.
                clearedFenceTop = true;
                hit.surface = HitSurface::FenceTopClear;
                hit.impactY = position.y;
                hit.wallTopY = wallH;
            } else if (pastDepth > 0.15f && vOut >= -1.0f) {
                // Thin tunnel past the plane while still flying outward — let it
                // finish going out rather than yanking back to the wall.
                clearedFenceTop = true;
                hit.surface = HitSurface::FenceTopClear;
                hit.impactY = position.y;
                hit.wallTopY = wallH;
            } else {
                // Near the fence plane and at/below wall top — solid face carom.
                Vector3 onWall = layout.fromHome(wallR - radius - 0.05f, ang, position.y);
                onWall.y = std::clamp(position.y, groundY, wallH - 0.02f);
                softSnapPosition(position, onWall, 0.55f);
                // Reflect outward component so wall balls bounce back into play.
                float vn = velocity.x * nOut.x + velocity.z * nOut.z;
                if (vn > 0.0f) {
                    velocity.x -= nOut.x * vn * 1.65f;
                    velocity.z -= nOut.z * vn * 1.65f;
                }
                velocity = velocity * 0.62f;
                velocity.y = std::min(velocity.y * 0.85f + 1.2f, 4.5f); // slight kick up
                hit.surface = HitSurface::Fence;
                hit.impactY = position.y;
                if (stickOnContact && velocity.magnitude() < 3.5f) {
                    velocity = Vector3();
                    hit.stuck = true;
                }
            }
        }
    }

    // ── Foul poles (thin vertical cylinders) ───────────────────────────
    auto collidePole = [&](float poleAng) {
        Vector3 poleBase = layout.wallPoint(poleAng, 0.0f);
        float poleH = layout.wallHeightAtAngle(poleAng) * 3.4f;
        float pr = 0.55f;
        Vector3 d(position.x - poleBase.x, 0.0f, position.z - poleBase.z);
        float dist = std::sqrt(d.x * d.x + d.z * d.z);
        if (position.y < 0.0f || position.y > poleH + 2.0f) {
            return;
        }
        if (dist < pr + radius && dist > 1e-5f) {
            Vector3 n = d * (1.0f / dist);
            position.x = poleBase.x + n.x * (pr + radius);
            position.z = poleBase.z + n.z * (pr + radius);
            killOutwardVelocity(velocity, n, false);
            velocity = velocity * 0.55f;
            hit.surface = HitSurface::FoulPole;
            hit.impactY = position.y;
            if (stickOnContact && velocity.magnitude() < 3.5f) {
                velocity = Vector3();
                hit.stuck = true;
            }
        }
    };
    collidePole(fa);
    collidePole(-fa);

    // ── Backstop (arc behind home) ─────────────────────────────────────
    {
        float backR = 14.0f;
        float backH = 10.0f;
        float aAbs = std::abs(ang);
        if (aAbs > fa + 0.2f && r + radius > backR && position.y < backH + radius) {
            float behind = -std::cos(ang); // 1 at catcher side
            if (behind > 0.35f) {
                Vector3 n(std::sin(ang), 0.0f, -std::cos(ang));
                Vector3 target = layout.fromHome(
                    backR - radius - 0.05f, ang, std::min(position.y, backH)
                );
                target.y = std::max(target.y, groundY);
                softSnapPosition(position, target, 0.80f);
                killOutwardVelocity(velocity, n, false);
                velocity = velocity * 0.50f;
                hit.surface = HitSurface::Backstop;
                hit.impactY = position.y;
                if (stickOnContact && velocity.magnitude() < 3.5f) {
                    velocity = Vector3();
                    hit.stuck = true;
                }
            }
        }
    }

    // ── Lower bowl / stands face ───────────────────────────────────────
    {
        float rBowl = layout.bowlInnerRadius(ang) - 0.4f;
        float yTop = layout.bowlBaseHeight(ang) + 4.0f;
        float wallR = fair ? layout.wallRAtAngle(ang) : layout.bowlInnerRadius(ang) * 0.5f;
        bool pastFence = !fair || r > wallR - 0.5f;
        bool highClear =
            clearedFenceTop ||
            (fair && r > wallR + 0.75f) ||
            (fair && r > wallR && position.y > layout.wallHeightAtAngle(ang) + 1.0f);
        if (!highClear && pastFence && r + radius > rBowl && position.y < yTop && position.y > -0.1f) {
            Vector3 n(std::sin(ang), 0.0f, -std::cos(ang));
            Vector3 target = layout.fromHome(rBowl - radius - 0.05f, ang, position.y);
            target.y = std::max(target.y, groundY);
            softSnapPosition(position, target, 0.55f);
            float vn = velocity.x * n.x + velocity.z * n.z;
            if (vn > 0.0f) {
                velocity.x -= n.x * vn * 1.4f;
                velocity.z -= n.z * vn * 1.4f;
            }
            velocity = velocity * 0.50f;
            hit.surface = HitSurface::Stands;
            hit.impactY = position.y;
            if (stickOnContact && velocity.magnitude() < 3.2f && position.y < groundY + 1.5f) {
                velocity = Vector3();
                hit.stuck = true;
            }
        }
    }

    // ── Dugouts (foul-side boxes along baselines) ──────────────────────
    {
        Vector3 home = layout.home();
        Vector3 b1 = layout.firstBase();
        Vector3 b3 = layout.thirdBase();
        Vector3 mid1 = (home + b1) * 0.5f;
        Vector3 mid3 = (home + b3) * 0.5f;
        auto foulN = [](const Vector3& along, float preferX) {
            Vector3 n(-along.z, 0.0f, along.x);
            float nm = n.magnitude();
            if (nm > 1e-4f) {
                n = n * (1.0f / nm);
            }
            if (n.x * preferX < 0.0f) {
                n = n * -1.0f;
            }
            return n;
        };
        Vector3 n1 = foulN(b1 - home, +1.0f);
        Vector3 n3 = foulN(b3 - home, -1.0f);
        Vector3 c1 = Vector3(mid1.x, 1.0f, mid1.z) + n1 * 10.0f;
        Vector3 c3 = Vector3(mid3.x, 1.0f, mid3.z) + n3 * 10.0f;
        resolveAabbSphere(
            position, velocity, radius, c1, Vector3(7.0f, 1.1f, 2.2f), stickOnContact, hit,
            HitSurface::Dugout
        );
        resolveAabbSphere(
            position, velocity, radius, c3, Vector3(7.0f, 1.1f, 2.2f), stickOnContact, hit,
            HitSurface::Dugout
        );
    }

    // ── CF scoreboard chassis ──────────────────────────────────────────
    {
        float cfR = layout.wallRAtAngle(0.0f);
        float cfH = layout.wallHeightAtAngle(0.0f);
        Vector3 cf = layout.fromHome(cfR + 8.0f, 0.0f, cfH + 10.0f);
        resolveAabbSphere(
            position, velocity, radius, cf, Vector3(16.5f, 8.5f, 2.0f), stickOnContact, hit,
            HitSurface::Scoreboard
        );
    }

    // ── Closed dome: seat decks + hotel + absolute ellipsoid shell ─────
    if (layout.closedDome) {
        layout.polarFromHome(position, r, ang);
        fair = std::abs(ang) <= fa + 0.02f;

        // Seating-bowl floors past the fence (tiered decks) — soft bounce.
        if (fair || r > layout.wallRAtAngle(0.0f) * 0.5f) {
            float wallR = fair ? layout.wallRAtAngle(ang) : layout.bowlInnerRadius(ang) * 0.55f;
            if (r > wallR + 0.4f) {
                float deckY = layout.seatDeckYAtRadius(r, ang) + radius + 0.02f;
                if (position.y < deckY && position.y > deckY - 6.0f) {
                    if (velocity.y <= 0.5f || position.y < deckY - 0.05f) {
                        position.y = deckY;
                        if (velocity.y < 0.0f) {
                            velocity.y = -velocity.y * 0.22f;
                        }
                        velocity.x *= 0.70f;
                        velocity.z *= 0.70f;
                        hit.surface = HitSurface::Stands;
                        hit.impactY = position.y;
                        if (stickOnContact && velocity.magnitude() < 3.5f) {
                            velocity = Vector3();
                            hit.stuck = true;
                        }
                    }
                }
            }
        }

        // CF hotel + board chassis (photo-scale, inside shell).
        {
            float cfR = layout.wallRAtAngle(0.0f);
            float cfH = layout.wallHeightAtAngle(0.0f);
            float rBoard = std::min(cfR + 10.0f, layout.maxRadiusFromHome(0.0f) - 20.0f);
            float rHotel = std::min(cfR + 18.0f, layout.maxRadiusFromHome(0.0f) - 12.0f);
            Vector3 board = layout.fromHome(rBoard, 0.0f, cfH + 14.0f);
            Vector3 hotel = layout.fromHome(rHotel, 0.0f, cfH + 22.0f);
            resolveAabbSphere(
                position, velocity, radius, board, Vector3(22.0f, 8.0f, 1.8f), false, hit,
                HitSurface::Scoreboard
            );
            resolveAabbSphere(
                position, velocity, radius, hotel, Vector3(20.0f, 16.0f, 6.0f), false, hit,
                HitSurface::Scoreboard
            );
        }

        // Hard ellipsoid shell — bounce, never leave the building.
        float yBefore = position.y;
        if (layout.containInsideDome(position, velocity, radius)) {
            float roofY = layout.domeRoofYAtWorld(position.x, position.z);
            if (yBefore + radius > roofY - 0.05f) {
                hit.surface = HitSurface::Roof;
            } else {
                hit.surface = HitSurface::DomeShell;
            }
            hit.impactY = position.y;
            if (stickOnContact && velocity.magnitude() < 3.5f) {
                velocity = Vector3();
                hit.stuck = true;
            }
        }
    }

    // Re-clamp ground after other resolves.
    if (position.y < groundY) {
        position.y = groundY;
        if (velocity.y < 0.0f) {
            velocity.y = -velocity.y * 0.30f;
            velocity.x *= 0.85f;
            velocity.z *= 0.85f;
        }
        if (stickOnContact && velocity.magnitude() < 3.5f) {
            velocity = Vector3();
            hit.stuck = true;
            if (hit.surface == HitSurface::None ||
                hit.surface == HitSurface::FenceTopClear) {
                hit.surface = HitSurface::Ground;
            }
        }
    }

    // Final hard containment (never leave the building).
    if (layout.closedDome) {
        layout.containInsideDome(position, velocity, radius);
        if (position.y < groundY) {
            position.y = groundY;
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
    // Multi-pass geometric resolve (pair with sub-stepped integration in the demo).
    substeps = std::clamp(substeps, 1, 12);
    BallCollisionHit last;
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
    float r0 = 0.0f;
    float ang0 = 0.0f;
    layout.polarFromHome(position, r0, ang0);
    out.sprayDeg = ang0 * (180.0f / pi);
    out.fair = std::abs(out.sprayDeg) <= layout.foulAngleDegrees + 0.5f;
    out.fenceFeet = layout.wallFeetAtAngle(ang0);
    out.wallTopY = layout.wallHeightAtAngle(ang0);

    // Always integrate for landing distance (fair and foul). Distance is always ≥ 0.
    const float dt = 1.0f / 180.0f;
    const int maxSteps = 4000;
    const float groundY = 0.05f;
    float prevR = r0;
    Vector3 pos = position;
    Vector3 vel = velocity;
    // Sanity-cap insane exit speeds so projection stays stable.
    {
        float spd0 = vel.magnitude();
        const float maxSpd = 52.0f; // ~116 mph world units/s
        if (spd0 > maxSpd) {
            vel = vel * (maxSpd / spd0);
        }
    }

    for (int i = 0; i < maxSteps; i++) {
        Vector3 prevPos = pos;

        // Quadratic drag + gravity.
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
        bool stepFair = std::abs(ang * (180.0f / pi)) <= layout.foulAngleDegrees + 0.5f;
        float wallR = stepFair ? layout.wallRAtAngle(ang) : layout.maxWallR() * 1.5f;
        float wallH = layout.wallHeightAtAngle(ang);

        // Crossed fence radius this step (outward) — fair territory only.
        if (stepFair && prevR < wallR && r >= wallR) {
            float u = (wallR - prevR) / std::max(r - prevR, 1e-5f);
            u = std::clamp(u, 0.0f, 1.0f);
            float yFence = prevPos.y + (pos.y - prevPos.y) * u;

            out.sprayDeg = ang * (180.0f / pi);
            out.fair = true;
            out.fenceFeet = layout.wallFeetAtAngle(ang);
            out.wallTopY = wallH;
            out.heightAtFence = yFence;
            out.marginFeet = (yFence - wallH) * layout.feetPerUnit;

            constexpr float kClearMargin = 0.35f; // world units
            if (yFence >= wallH + kClearMargin) {
                out.clearsWall = true;
                // Keep integrating for true landing past the wall.
            } else {
                out.hitsWallFace = true;
                out.clearsWall = false;
                out.landFeet = std::max(0.0f, out.fenceFeet);
                return out;
            }
        }

        if (pos.y <= groundY && vel.y <= 0.0f) {
            pos.y = groundY;
            float landR = 0.0f;
            float landA = 0.0f;
            layout.polarFromHome(pos, landR, landA);
            out.landFeet = std::max(0.0f, landR * layout.feetPerUnit);
            out.sprayDeg = landA * (180.0f / pi);
            out.fair = std::abs(out.sprayDeg) <= layout.foulAngleDegrees + 0.5f;
            if (out.fair) {
                float wallAtLand = layout.wallRAtAngle(landA);
                if (landR < wallAtLand - 0.5f) {
                    out.clearsWall = false;
                    out.hitsWallFace = false;
                    out.marginFeet = 0.0f;
                }
            } else {
                out.clearsWall = false;
                out.hitsWallFace = false;
            }
            return out;
        }

        // Escaped far past park without landing.
        if (r > layout.maxWallR() + 80.0f) {
            out.landFeet = std::max(0.0f, r * layout.feetPerUnit);
            out.sprayDeg = ang * (180.0f / pi);
            out.fair = std::abs(out.sprayDeg) <= layout.foulAngleDegrees + 0.5f;
            return out;
        }

        prevR = r;
    }

    float endR = 0.0f;
    float endA = 0.0f;
    layout.polarFromHome(pos, endR, endA);
    if (out.landFeet <= 0.0f) {
        out.landFeet = std::max(0.0f, endR * layout.feetPerUnit);
    }
    out.sprayDeg = endA * (180.0f / pi);
    out.fair = std::abs(out.sprayDeg) <= layout.foulAngleDegrees + 0.5f;
    return out;
}

Layout defaultPlayLayout() {
    // Rogers Centre (Toronto) — large shell so full seating bowl fits inside.
    Layout L;
    L.wallDistanceFeet = 400.0f;
    L.wallHeightFeet = 10.0f;
    L.closedDome = true;
    L.roofPeakFeet = 300.0f;
    L.buildingRadiusFeet = 430.0f;
    L.domeCenterOffsetFeet = 200.0f;
    return L;
}

Mesh3D buildSkyDome(const Layout& L) {
    Mesh3D m;
    if (L.closedDome) {
        // Membrane panels on the ellipsoid (trusses drawn separately).
        // Leave the apex band brighter so the roof reads open like the photo.
        const Vector3 c = L.domeCenter();
        const float Rh = L.domeHorizR();
        const int rings = 16;
        const int segs = 56;
        sf::Color panel = domePanelColor();
        sf::Color panelAlt(205, 214, 226);
        sf::Color rib = domeRibColor();
        sf::Color skyWash(140, 175, 215);

        auto roofPt = [&](float rFrac, float ang) {
            float r = rFrac * Rh;
            float x = c.x + std::sin(ang) * r;
            float z = c.z - std::cos(ang) * r;
            return Vector3(x, L.domeRoofYAtWorld(x, z), z);
        };

        // Outer roof ring only (rFrac 0.58–1.0). Wide open center so orbit/top
        // views show the full diamond and continuous seating bowl.
        for (int i = 0; i < rings; i++) {
            float u0 = static_cast<float>(i) / rings;
            float u1 = static_cast<float>(i + 1) / rings;
            float r0 = 0.58f + u0 * 0.42f;
            float r1 = 0.58f + u1 * 0.42f;
            for (int j = 0; j < segs; j++) {
                float a0 = -pi + (static_cast<float>(j) / segs) * 2.0f * pi;
                float a1 = -pi + (static_cast<float>(j + 1) / segs) * 2.0f * pi;
                bool alt = ((i + j) % 2) == 0;
                float skyT = std::clamp((1.0f - r0) * 1.2f, 0.0f, 0.55f);
                sf::Color base = alt ? panel : panelAlt;
                sf::Color col(
                    static_cast<std::uint8_t>(base.r * (1.0f - skyT) + skyWash.r * skyT),
                    static_cast<std::uint8_t>(base.g * (1.0f - skyT) + skyWash.g * skyT),
                    static_cast<std::uint8_t>(base.b * (1.0f - skyT) + skyWash.b * skyT)
                );
                addQuad(
                    m, roofPt(r1, a0), roofPt(r1, a1), roofPt(r0, a1), roofPt(r0, a0), col
                );
            }
        }
        // Circular outer ring wall.
        const float wallTop = L.domeRoofYAtWorld(c.x, c.z - Rh * 0.995f);
        for (int j = 0; j < segs; j++) {
            float a0 = -pi + (static_cast<float>(j) / segs) * 2.0f * pi;
            float a1 = -pi + (static_cast<float>(j + 1) / segs) * 2.0f * pi;
            Vector3 b0(c.x + std::sin(a0) * Rh, 0.0f, c.z - std::cos(a0) * Rh);
            Vector3 b1(c.x + std::sin(a1) * Rh, 0.0f, c.z - std::cos(a1) * Rh);
            Vector3 t0(c.x + std::sin(a0) * Rh, wallTop, c.z - std::cos(a0) * Rh);
            Vector3 t1(c.x + std::sin(a1) * Rh, wallTop, c.z - std::cos(a1) * Rh);
            sf::Color wallCol = (j % 2) ? sf::Color(70, 88, 108) : rib;
            addQuad(m, b0, b1, t1, t0, wallCol);
            addQuad(m, b1, b0, t0, t1, wallCol);
        }
        m.rebuildNormals();
        return m;
    }

    // Open-air fallback: large hemisphere gradient sky.
    const Vector3 center(0.0f, 0.0f, L.plateZ() - L.maxWallR() * 0.25f);
    const float R = std::max(900.0f, L.maxWallR() * 8.0f);
    const int rings = 18;
    const int segs = 48;
    sf::Color zenith = skyZenithColor();
    sf::Color horizon = skyColor();
    sf::Color haze(190, 210, 225);

    auto skyPoint = [&](float elev, float azim) {
        float y = std::sin(elev) * R;
        float rr = std::cos(elev) * R;
        return center + Vector3(std::sin(azim) * rr, y, -std::cos(azim) * rr);
    };
    auto skyCol = [&](float elev) {
        float t = std::clamp(elev / (pi * 0.5f), 0.0f, 1.0f);
        float u = t * t * (3.0f - 2.0f * t);
        sf::Color lo = (t < 0.25f) ? haze : horizon;
        float v = (t < 0.25f) ? (t / 0.25f) : ((t - 0.25f) / 0.75f);
        if (t < 0.25f) {
            return sf::Color(
                static_cast<std::uint8_t>(lo.r + (horizon.r - lo.r) * v),
                static_cast<std::uint8_t>(lo.g + (horizon.g - lo.g) * v),
                static_cast<std::uint8_t>(lo.b + (horizon.b - lo.b) * v)
            );
        }
        return sf::Color(
            static_cast<std::uint8_t>(horizon.r + (zenith.r - horizon.r) * u),
            static_cast<std::uint8_t>(horizon.g + (zenith.g - horizon.g) * u),
            static_cast<std::uint8_t>(horizon.b + (zenith.b - horizon.b) * u)
        );
    };

    for (int i = 0; i < rings; i++) {
        float e0 = (static_cast<float>(i) / rings) * (pi * 0.5f);
        float e1 = (static_cast<float>(i + 1) / rings) * (pi * 0.5f);
        sf::Color c0 = skyCol(e0);
        sf::Color c1 = skyCol(e1);
        sf::Color band(
            static_cast<std::uint8_t>((c0.r + c1.r) / 2),
            static_cast<std::uint8_t>((c0.g + c1.g) / 2),
            static_cast<std::uint8_t>((c0.b + c1.b) / 2)
        );
        for (int j = 0; j < segs; j++) {
            float a0 = (static_cast<float>(j) / segs) * pi * 2.0f;
            float a1 = (static_cast<float>(j + 1) / segs) * pi * 2.0f;
            addQuad(
                m,
                skyPoint(e0, a0),
                skyPoint(e0, a1),
                skyPoint(e1, a1),
                skyPoint(e1, a0),
                band
            );
        }
    }
    m.rebuildNormals();
    return m;
}

Mesh3D buildClouds(const Layout& L) {
    Mesh3D m;
    // No outdoor clouds under a closed retractable roof.
    if (L.closedDome) {
        return m;
    }
    const Vector3 park = L.parkCenter();
    const float baseR = L.maxWallR();

    auto addPuff = [&](const Vector3& c, float sx, float sy, float sz, sf::Color col) {
        // Soft stack of flattened boxes = cumulus without textures.
        addBox(m, c, sx, sy, sz, col);
        addBox(m, c + Vector3(sx * 0.35f, sy * 0.15f, sz * 0.1f), sx * 0.7f, sy * 0.85f, sz * 0.75f, col);
        addBox(m, c + Vector3(-sx * 0.3f, sy * 0.2f, -sz * 0.15f), sx * 0.65f, sy * 0.75f, sz * 0.7f, col);
        addBox(m, c + Vector3(sx * 0.05f, sy * 0.45f, 0.0f), sx * 0.5f, sy * 0.55f, sz * 0.55f, col);
    };

    // Layered cloud field around / above the park.
    const int cloudCount = 28;
    for (int i = 0; i < cloudCount; i++) {
        float t = static_cast<float>(i) / cloudCount;
        float ang = -pi + t * 2.0f * pi + hash01(i * 3) * 0.4f;
        float r = baseR * (1.8f + hash01(i * 7) * 3.5f);
        float y = 55.0f + hash01(i * 11) * 70.0f;
        Vector3 c = L.fromHome(r, ang, y);
        // Bias a few over CF / behind home for catcher-view interest.
        if (hash01(i + 50) > 0.7f) {
            c = park + Vector3(
                (hash01(i + 1) - 0.5f) * 120.0f,
                70.0f + hash01(i + 2) * 50.0f,
                -40.0f - hash01(i + 3) * 80.0f
            );
        }
        float sx = 18.0f + hash01(i * 13) * 28.0f;
        float sy = 4.0f + hash01(i * 17) * 6.0f;
        float sz = 12.0f + hash01(i * 19) * 22.0f;
        // Bright white with slight grey variation for depth.
        float shade = 0.88f + hash01(i * 23) * 0.12f;
        sf::Color col(
            static_cast<std::uint8_t>(255 * shade),
            static_cast<std::uint8_t>(255 * shade),
            static_cast<std::uint8_t>(250 * shade),
            210
        );
        addPuff(c, sx, sy, sz, col);
    }

    // Thin high cirrus streaks
    for (int i = 0; i < 10; i++) {
        float ang = hash01(i + 300) * pi * 2.0f;
        float r = baseR * (2.5f + hash01(i + 310) * 2.0f);
        Vector3 c = L.fromHome(r, ang, 110.0f + hash01(i + 320) * 30.0f);
        addBox(
            m,
            c,
            40.0f + hash01(i) * 50.0f,
            1.8f,
            6.0f + hash01(i + 5) * 8.0f,
            sf::Color(245, 248, 255, 160)
        );
    }

    m.rebuildNormals();
    return m;
}

Meshes build(const Layout& layout) {
    Meshes out;
    out.field = buildField(layout);
    out.walls = buildWalls(layout);
    out.stands = buildStands(layout);
    out.lines = buildLines(layout);
    out.city = buildCity(layout); // empty under closed dome
    out.scoreboardScreen = buildScoreboardScreen(layout);
    out.skyDome = buildSkyDome(layout);
    out.clouds = buildClouds(layout); // empty under closed dome
    out.hotel = buildHotel(layout);
    out.structure = buildStructure(layout);
    out.fanSectors = buildFanSectors(layout);
    buildFlags(layout, out.flagMeshes, out.flagBases); // empty under closed dome
    return out;
}

float recommendedFarPlane(const Layout& layout) {
    // Tight far plane for indoor dome — no distant suburbs.
    if (layout.closedDome) {
        return std::max(900.0f, layout.roofShellR() * 3.5f + 120.0f);
    }
    return std::max(3500.0f, layout.maxWallR() * 12.0f + 800.0f);
}

float fanCheerOffsetY(int sectorIndex, float timeSec, float boost) {
    // Desynchronized crowd: each sector hops on its own clock (not a unison wave).
    float s = static_cast<float>(sectorIndex);
    float seed = hash01(sectorIndex * 47 + 13);
    float seed2 = hash01(sectorIndex * 91 + 7);
    float b = std::max(0.35f, boost);

    // Independent hop periods (0.55s–1.9s) and phase offsets.
    float period = 0.55f + seed * 1.35f;
    float phase = seed2 * period * 3.7f + s * 0.37f;
    float tHop = std::fmod(timeSec + phase, period);
    if (tHop < 0.0f) {
        tHop += period;
    }
    // Short jump pulse (not everyone airborne at once).
    float hopDur = 0.12f + seed * 0.14f;
    float hop = 0.0f;
    if (tHop < hopDur) {
        hop = std::sin((tHop / hopDur) * 3.14159265f);
    }
    // Second rarer hop layer (some groups double-bounce).
    float period2 = 1.1f + seed2 * 2.0f;
    float t2 = std::fmod(timeSec * 0.85f + seed * 5.0f, period2);
    float hop2 = 0.0f;
    if (t2 < 0.10f + seed * 0.08f) {
        hop2 = std::sin((t2 / (0.10f + seed * 0.08f)) * 3.14159265f) * 0.55f;
    }
    // Tiny idle fidget unique per sector.
    float idle =
        0.035f * std::sin(timeSec * (2.1f + seed * 3.4f) + s * 1.7f) +
        0.02f * std::sin(timeSec * (5.5f + seed2 * 2.0f) + seed * 8.0f);

    float amp = 0.14f + seed * 0.16f; // some sections jump higher
    return (idle + hop * amp + hop2 * amp * 0.7f) * b;
}

float fanCheerOffsetX(int sectorIndex, float timeSec, float boost) {
    float s = static_cast<float>(sectorIndex);
    float seed = hash01(sectorIndex * 53 + 19);
    float b = std::max(0.35f, boost);
    // Independent lateral sway — different freqs so the bowl shimmers, not waves.
    return (
        0.05f * std::sin(timeSec * (1.7f + seed * 2.8f) + s * 2.1f) +
        0.03f * std::sin(timeSec * (4.0f + seed * 1.5f) + seed * 6.0f)
    ) * b;
}

float flagSwayYaw(int flagIndex, float timeSec) {
    float phase = static_cast<float>(flagIndex) * 0.9f;
    return 0.22f * std::sin(timeSec * 2.4f + phase) + 0.08f * std::sin(timeSec * 5.1f + phase * 1.3f);
}

float scoreboardPulse(float timeSec, float excitement) {
    float base = 0.55f + 0.25f * std::sin(timeSec * 3.5f);
    float pop = excitement > 0.01f ? (0.35f * std::sin(timeSec * 12.0f) * excitement) : 0.0f;
    return std::clamp(base + pop, 0.25f, 1.0f);
}

} // namespace Stadium3D
