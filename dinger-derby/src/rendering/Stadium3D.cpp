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

    // ── Continuous playable surface (full 360°) ────────────────────────
    // Every angle has seamless grass from near home out to fence (fair) or
    // bowl apron (foul). No radial gaps → no wrong-color ground bleeding.
    for (int i = 0; i < ringSegs; i++) {
        float t0 = static_cast<float>(i) / ringSegs;
        float t1 = static_cast<float>(i + 1) / ringSegs;
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
        float rFence0 = fair ? (L.wallRAtAngle(ang0) - 0.35f) : 0.0f;
        float rFence1 = fair ? (L.wallRAtAngle(ang1) - 0.35f) : 0.0f;
        float rBowl0 = L.bowlInnerRadius(ang0) - 0.6f;
        float rBowl1 = L.bowlInnerRadius(ang1) - 0.6f;
        // Play surface outer edge: fence in fair territory, bowl in foul.
        float rOut0 = fair ? rFence0 : rBowl0;
        float rOut1 = fair ? rFence1 : rBowl1;
        rOut0 = std::max(rOut0, 2.0f);
        rOut1 = std::max(rOut1, 2.0f);

        // Base grass (full coverage under dirt diamond).
        addQuad(
            m,
            L.fromHome(0.25f, ang0, 0.0f),
            L.fromHome(0.25f, ang1, 0.0f),
            L.fromHome(rOut1, ang1, 0.0f),
            L.fromHome(rOut0, ang0, 0.0f),
            grass
        );

        if (fair) {
            // Multi-band mowing stripes beyond diamond (vertex "texture").
            float stripeIn = bp * 1.30f;
            for (int s = 0; s < 5; s++) {
                float u0 = 0.42f + s * 0.09f;
                float u1 = u0 + 0.045f;
                float mid0 = std::max(stripeIn, rOut0 * u0);
                float mid1 = std::max(stripeIn, rOut1 * u0);
                float mid0b = std::min(rOut0 - 3.8f, rOut0 * u1);
                float mid1b = std::min(rOut1 - 3.8f, rOut1 * u1);
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
            // Warning track inside fence (banded dirt for read from field)
            {
                const float trackW = 3.8f;
                for (int b = 0; b < 3; b++) {
                    float t0 = static_cast<float>(b) / 3.0f;
                    float t1 = static_cast<float>(b + 1) / 3.0f;
                    float ri0 = rOut0 - trackW + trackW * t0;
                    float ri1 = rOut1 - trackW + trackW * t1;
                    float ro0 = rOut0 - trackW + trackW * t1;
                    float ro1 = rOut1 - trackW + trackW * ((b + 1) / 3.0f);
                    // fix band edges
                    ri0 = rOut0 - trackW * (1.0f - t0);
                    ro0 = rOut0 - trackW * (1.0f - t1);
                    ri1 = rOut1 - trackW * (1.0f - t0);
                    ro1 = rOut1 - trackW * (1.0f - t1);
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
            }
            // Apron: fence → stands (matches bowl, no void)
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
        } else {
            // Foul-territory mow: radial bands so plate/foul views aren't flat green.
            const float foulStripeIn = bp * 0.55f;
            for (int s = 0; s < 6; s++) {
                float u0 = 0.18f + s * 0.12f;
                float u1 = u0 + 0.055f;
                float mid0 = std::max(foulStripeIn, rOut0 * u0);
                float mid1 = std::max(foulStripeIn, rOut1 * u0);
                float mid0b = std::min(rOut0 - 1.2f, rOut0 * u1);
                float mid1b = std::min(rOut1 - 1.2f, rOut1 * u1);
                if (mid0b <= mid0 + 0.35f) {
                    continue;
                }
                // Alternate + slight angular variation
                bool dark = ((s + i) % 2) == 0;
                sf::Color stripe = dark ? grassDark : shadeColor(grass, 0.97f);
                addQuad(
                    m,
                    L.fromHome(mid0, ang0, 0.011f + s * 0.0008f),
                    L.fromHome(mid1, ang1, 0.011f + s * 0.0008f),
                    L.fromHome(mid1b, ang1, 0.011f + s * 0.0008f),
                    L.fromHome(mid0b, ang0, 0.011f + s * 0.0008f),
                    stripe
                );
            }
        }
    }
    // ── Foul-line dirt strips (inside fair side of chalk) ──────────────
    // Thin dirt apron along each foul line from home out past the infield.
    {
        const float lineDirtHw = 0.85f;
        const float lineDirtY = 0.021f;
        float foulLen = L.wallRAtAngle(aR) * 0.42f; // past diamond into OF
        foulLen = std::max(foulLen, bp * 1.6f);
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
    // Heavy wear ring around the rubber / landing zone (front of mound toward plate)
    addDiskRing(
        m,
        L.mound() + Vector3(0, 0, 0.9f),
        0.9f,
        2.1f,
        0.029f,
        24,
        shadeColor(dirtDark, 0.92f)
    );
    addDirtWearBlotch(m, L.mound() + Vector3(0.25f, 0, 1.3f), 0.85f, 0.030f, dirtDark, 3);
    addDirtWearBlotch(m, L.mound() + Vector3(-0.3f, 0, 1.1f), 0.7f, 0.030f, dirtDark, 5);
    {
        Vector3 c(0.0f, 0.10f, L.moundZ());
        const int n = 28;
        // Raised mound table ~18 ft circle already drawn; table top ~5 ft radius feel.
        float mr = 2.6f;
        for (int ring = 0; ring < 3; ring++) {
            float r0 = mr * (static_cast<float>(ring) / 3.0f);
            float r1 = mr * (static_cast<float>(ring + 1) / 3.0f);
            float y0 = 0.04f + ring * 0.045f;
            float y1 = 0.04f + (ring + 1) * 0.045f;
            sf::Color col = shadeColor(dirtDark, 0.92f + ring * 0.04f);
            for (int i = 0; i < n; i++) {
                float a0 = (static_cast<float>(i) / n) * pi * 2.0f;
                float a1 = (static_cast<float>(i + 1) / n) * pi * 2.0f;
                // Slight oval along pitch axis
                auto pt = [&](float a, float r, float y) {
                    return Vector3(std::cos(a) * r, y, L.moundZ() + std::sin(a) * r * 0.72f);
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
        // Rubber
        addBox(m, Vector3(0.0f, 0.20f, L.moundZ()), 1.05f, 0.05f, 0.28f, sf::Color(235, 230, 215));
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

    // Home plate (regulation-ish silhouette, tip to catcher +Z)
    {
        float z = plateZ;
        Vector3 tip(0.0f, 0.055f, z + 0.52f);
        Vector3 bl(-0.52f, 0.055f, z - 0.12f);
        Vector3 br(0.52f, 0.055f, z - 0.12f);
        Vector3 fl(-0.32f, 0.055f, z - 0.52f);
        Vector3 fr(0.32f, 0.055f, z - 0.52f);
        sf::Color plate(245, 242, 230);
        addTri(m, tip, br, bl, plate);
        addQuad(m, bl, br, fr, fl, plate);
    }

    // Base bags — rotated diamond squares (point toward next base)
    auto placeBaseBag = [&](const Vector3& p, float yaw) {
        float s = 0.55f; // half-diagonal-ish
        float c = std::cos(yaw);
        float sn = std::sin(yaw);
        auto rot = [&](float x, float z) {
            return Vector3(p.x + c * x - sn * z, 0.065f, p.z + sn * x + c * z);
        };
        // Diamond outline (rotated square)
        Vector3 v0 = rot(0.0f, s);
        Vector3 v1 = rot(s, 0.0f);
        Vector3 v2 = rot(0.0f, -s);
        Vector3 v3 = rot(-s, 0.0f);
        addFlatQuad(m, v0, v1, v2, v3, 0.065f, sf::Color(250, 248, 235));
        // Slight raised top
        addBox(m, Vector3(p.x, 0.09f, p.z), 0.85f, 0.04f, 0.85f, sf::Color(248, 245, 230));
    };
    // 1B bag aligned along 1B line; 2B faces home; 3B along 3B line
    placeBaseBag(b1, L.foulAngleRad() * 0.5f);
    placeBaseBag(b2, 0.0f);
    placeBaseBag(b3, -L.foulAngleRad() * 0.5f);

    // Batter's boxes (left + right) — flat dirt rectangles, chalk rim via lines mesh
    {
        sf::Color box = shadeColor(dirtDark, 1.05f);
        // Batter's boxes ~4×6 ft → 2×3 world units, centered beside plate.
        addBox(m, Vector3(-1.65f, 0.035f, plateZ - 0.35f), 2.0f, 0.03f, 3.0f, box);
        addBox(m, Vector3(1.65f, 0.035f, plateZ - 0.35f), 2.0f, 0.03f, 3.0f, box);
        // Catcher's box hint (~43 in wide area → ~1.8 units)
        addBox(m, Vector3(0.0f, 0.032f, plateZ + 1.05f), 1.8f, 0.02f, 1.0f, shadeColor(dirt, 0.93f));
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

    // CF scoreboard chassis (screen face is a separate mesh for pulse).
    float cfR = L.wallRAtAngle(0.0f);
    float cfH = L.wallHeightAtAngle(0.0f);
    Vector3 cf = L.fromHome(cfR + 8.0f, 0.0f, cfH + 10.0f);
    addBox(m, cf, 32.0f, 16.0f, 3.0f, sf::Color(30, 35, 45));
    // Ribbon / LED strip under board
    addBox(m, cf + Vector3(0, -9.0f, 0.5f), 30.0f, 1.2f, 0.5f, sf::Color(200, 40, 50));

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
    float cfR = L.wallRAtAngle(0.0f);
    float cfH = L.wallHeightAtAngle(0.0f);
    Vector3 cf = L.fromHome(cfR + 8.0f, 0.0f, cfH + 10.0f);
    // Bright face slightly in front of chassis toward home (+Z from CF wall is toward plate).
    addBox(m, cf + Vector3(0, 0, 1.65f), 28.0f, 12.0f, 0.35f, sf::Color(30, 140, 70));
    // Fake "HR" panel blocks
    addBox(m, cf + Vector3(-8.0f, 2.0f, 1.9f), 8.0f, 3.5f, 0.2f, sf::Color(255, 220, 60));
    addBox(m, cf + Vector3(8.0f, 2.0f, 1.9f), 8.0f, 3.5f, 0.2f, sf::Color(255, 255, 255));
    addBox(m, cf + Vector3(0.0f, -2.5f, 1.9f), 22.0f, 2.2f, 0.2f, sf::Color(40, 200, 90));
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

    // Full 360° lower bowl + upper deck (higher angular + row density)
    const int angSegs = 120;
    const int lowerRows = 18;
    const int upperRows = 12;
    const float rowDepth = 2.15f;
    const float rowRise = 0.92f;
    const float upperGap = 3.2f;

    for (int i = 0; i < angSegs; i++) {
        float t0 = static_cast<float>(i) / angSegs;
        float t1 = static_cast<float>(i + 1) / angSegs;
        float ang0 = -pi + t0 * 2.0f * pi;
        float ang1 = -pi + t1 * 2.0f * pi;
        float angM = 0.5f * (ang0 + ang1);
        float rIn = bowlInnerRadius(L, angM);
        float yBase = bowlBaseHeight(L, angM);

        // Aisle every 8 sectors
        bool isAisle = (i % 8) == 0;
        sf::Color rowSeat = isAisle ? aisle : ((i % 2) ? seat : seatAlt);

        for (int row = 0; row < lowerRows; row++) {
            float r0 = rIn + row * rowDepth;
            float r1 = r0 + rowDepth * 0.92f;
            float y0 = yBase + row * rowRise;
            float y1 = y0 + rowRise * 0.85f;
            // Subtle per-row color variation (fabric texture feel)
            sf::Color seatCol = shadeColor(rowSeat, 0.94f + 0.08f * static_cast<float>((row + i) % 3) / 2.0f);
            addQuad(
                m,
                L.fromHome(r0, ang0, y1),
                L.fromHome(r0, ang1, y1),
                L.fromHome(r1, ang1, y1),
                L.fromHome(r1, ang0, y1),
                seatCol
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

    // Backstop / netting behind home (denser, multi-layer for plate-view realism)
    const float backL = pi - 1.05f;
    const float backR = pi + 1.05f;
    float backInner = 12.5f;
    // Solid lower wall
    addArcWall(
        m, L, backInner, backL, backR, 0.0f, 4.2f, 40,
        sf::Color(70, 78, 88), sf::Color(95, 105, 115)
    );
    // Mid padded band
    addArcWall(
        m, L, backInner + 0.15f, backL + 0.02f, backR - 0.02f, 4.0f, 9.5f, 36,
        sf::Color(55, 70, 95), sf::Color(80, 95, 115)
    );
    // Upper net (semi-transparent)
    addArcWall(
        m, L, backInner + 0.4f, backL + 0.05f, backR - 0.05f, 8.5f, 20.0f, 36,
        sf::Color(200, 210, 220, 75), sf::Color(180, 190, 200, 80)
    );
    // Net grid poles (vertical ribs the batter sees looking up)
    for (int i = 0; i <= 12; i++) {
        float t = static_cast<float>(i) / 12.0f;
        float ang = backL + (backR - backL) * t;
        Vector3 base = L.fromHome(backInner + 0.2f, ang, 0.0f);
        addBox(m, base + Vector3(0, 10.0f, 0), 0.18f, 20.0f, 0.18f, sf::Color(160, 165, 170));
    }
    // Protective rail above catcher
    addBox(m, Vector3(0.0f, 5.5f, plateZ + 11.5f), 10.0f, 0.35f, 0.8f, sf::Color(90, 95, 100));

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
    const int angSamples = 160; // denser angular packing
    const int lowerRows = 18;
    const int upperRows = 12;
    const float rowDepth = 2.15f;
    const float rowRise = 0.92f;
    const float upperGap = 3.2f;

    int fanId = 0;
    for (int i = 0; i < angSamples; i++) {
        float t = (static_cast<float>(i) + 0.5f) / angSamples;
        float ang = -pi + t * 2.0f * pi;
        int sector = static_cast<int>((t * kFanSectorCount)) % kFanSectorCount;
        float rIn = bowlInnerRadius(L, ang);
        float yBase = bowlBaseHeight(L, ang);

        // Lower bowl — denser crowd (~70% seats filled, skip aisles)
        for (int row = 1; row < lowerRows; row += 1) {
            if ((i + row * 2) % 5 == 0) {
                continue; // sparse empty seats
            }
            if (i % 8 == 0) {
                continue; // aisle
            }
            float r = rIn + (row + 0.45f) * rowDepth;
            float y = yBase + (row + 1.0f) * rowRise;
            Vector3 seat = L.fromHome(r, ang, y);
            // Slight seat jitter so the bowl doesn't look grid-perfect.
            seat.x += (hash01(fanId) - 0.5f) * 0.25f;
            seat.z += (hash01(fanId + 3) - 0.5f) * 0.25f;
            addLowPolyFan(
                sectors[sector],
                seat,
                0.92f + 0.18f * hash01(fanId),
                fanShirtColor(fanId),
                fanSkinColor(fanId + 3)
            );
            fanId++;
        }

        // Upper deck (still busy)
        float rLowTop = rIn + lowerRows * rowDepth;
        float yConc = yBase + lowerRows * rowRise + 0.3f;
        float rUp0 = rLowTop + upperGap;
        float yUp0 = yConc + 1.2f;
        for (int row = 0; row < upperRows; row += 1) {
            if ((i + row * 2) % 2 != 0) {
                continue;
            }
            float r = rUp0 + (row + 0.4f) * (rowDepth * 1.15f);
            float y = yUp0 + (row + 1.0f) * (rowRise * 1.15f);
            Vector3 seat = L.fromHome(r, ang, y);
            addLowPolyFan(
                sectors[sector],
                seat,
                0.88f + 0.12f * hash01(fanId + 9),
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

void buildFlags(const Layout& L, std::vector<Mesh3D>& flags, std::vector<Vector3>& bases) {
    flags.clear();
    bases.clear();
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

Vector3 Layout::scoreboardCenter() const {
    float cfR = wallRAtAngle(0.0f);
    float cfH = wallHeightAtAngle(0.0f);
    return fromHome(cfR + 8.0f, 0.0f, cfH + 10.0f);
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
    while (ang > pi) {
        ang -= 2.0f * pi;
    }
    while (ang < -pi) {
        ang += 2.0f * pi;
    }
    float fa = foulAngleRad();
    if (std::abs(ang) <= fa + 0.05f) {
        // Immediately behind OF fence (walkway + first row).
        return wallRAtAngle(ang) + 2.2f;
    }

    float foulPoleR = wallRAtAngle(ang >= 0.0f ? fa : -fa) + 2.2f;
    float backR = 14.0f;
    float bp = basePath();
    float dugoutClear = bp * 0.95f + 22.0f;

    float fromFoul = std::abs(ang) - fa;
    float span = pi - fa;
    float u = std::clamp(fromFoul / std::max(span, 0.01f), 0.0f, 1.0f);
    const float hold = 0.30f;
    float u2 = u < hold ? 0.0f : (u - hold) / (1.0f - hold);
    u2 = u2 * u2 * (3.0f - 2.0f * u2);

    float r = foulPoleR + (backR - foulPoleR) * u2;
    float minClear = backR + (dugoutClear - backR) * (1.0f - u2);
    return std::max(r, minClear);
}

float Layout::bowlBaseHeight(float ang) const {
    float fa = foulAngleRad();
    while (ang > pi) {
        ang -= 2.0f * pi;
    }
    while (ang < -pi) {
        ang += 2.0f * pi;
    }
    if (std::abs(ang) <= fa + 0.05f) {
        return wallHeightAtAngle(ang) + 0.5f;
    }
    float u = std::clamp((std::abs(ang) - fa) / (pi - fa), 0.0f, 1.0f);
    const float hold = 0.30f;
    float u2 = u < hold ? 0.0f : (u - hold) / (1.0f - hold);
    u2 = u2 * u2 * (3.0f - 2.0f * u2);
    return 2.2f + u2 * 5.5f;
}

namespace {

void killOutwardVelocity(Vector3& vel, const Vector3& outwardNormal, bool stick) {
    if (stick) {
        vel = Vector3();
        return;
    }
    float vn = vel.x * outwardNormal.x + vel.y * outwardNormal.y + vel.z * outwardNormal.z;
    if (vn > 0.0f) {
        vel = vel - outwardNormal * (vn * 1.15f);
    }
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

    // ── Ground ────────────────────────────────────────────────────────
    const float groundY = radius + 0.01f;
    if (position.y < groundY) {
        position.y = groundY;
        if (velocity.y < 0.0f) {
            hit.surface = HitSurface::Ground;
            hit.impactY = groundY;
            if (stickOnContact) {
                velocity = Vector3();
                hit.stuck = true;
            } else {
                velocity.y = -velocity.y * 0.15f;
                velocity.x *= 0.7f;
                velocity.z *= 0.7f;
            }
        }
    }

    float r = 0.0f;
    float ang = 0.0f;
    layout.polarFromHome(position, r, ang);
    hit.sprayDeg = ang * (180.0f / pi);
    bool fair = std::abs(ang) <= fa + 0.02f;

    // ── Asymmetric OF fence (solid face below wall top) ────────────────
    if (fair && r > 1.0f) {
        float wallR = layout.wallRAtAngle(ang);
        float wallH = layout.wallHeightAtAngle(ang);
        hit.wallTopY = wallH;
        hit.fenceFeet = layout.wallFeetAtAngle(ang);
        if (r + radius > wallR) {
            if (position.y <= wallH + radius * 0.35f) {
                // Hit fence face — push back into the field.
                Vector3 onWall = layout.fromHome(wallR - radius - 0.02f, ang, position.y);
                onWall.y = std::clamp(position.y, groundY, wallH - 0.02f);
                // Radial outward normal (from home toward ball).
                Vector3 n(std::sin(ang), 0.0f, -std::cos(ang));
                position = onWall;
                killOutwardVelocity(velocity, n, stickOnContact);
                hit.surface = HitSurface::Fence;
                hit.impactY = position.y;
                if (stickOnContact) {
                    velocity = Vector3();
                    hit.stuck = true;
                }
            } else {
                // Over the top — free flight, mark clear.
                hit.surface = HitSurface::FenceTopClear;
                hit.impactY = position.y;
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
            killOutwardVelocity(velocity, n, stickOnContact);
            hit.surface = HitSurface::FoulPole;
            hit.impactY = position.y;
            if (stickOnContact) {
                velocity = Vector3();
                hit.stuck = true;
            }
        }
    };
    collidePole(fa);
    collidePole(-fa);

    // ── Backstop (arc behind home) ─────────────────────────────────────
    {
        float backR = 12.5f;
        float backH = 9.0f;
        // Behind home: angles near ±pi
        float aAbs = std::abs(ang);
        if (aAbs > fa + 0.2f && r + radius > backR && position.y < backH + radius) {
            // Only when mostly behind the plate (ang toward +Z from home = π)
            float behind = -std::cos(ang); // 1 at catcher side
            if (behind > 0.35f) {
                Vector3 n(std::sin(ang), 0.0f, -std::cos(ang));
                position = layout.fromHome(backR - radius - 0.02f, ang, std::min(position.y, backH));
                position.y = std::max(position.y, groundY);
                killOutwardVelocity(velocity, n, stickOnContact);
                hit.surface = HitSurface::Backstop;
                hit.impactY = position.y;
                if (stickOnContact) {
                    velocity = Vector3();
                    hit.stuck = true;
                }
            }
        }
    }

    // ── Lower bowl / stands face (ball cannot enter seating bowl) ──────
    {
        float rBowl = layout.bowlInnerRadius(ang) - 0.4f;
        float yTop = layout.bowlBaseHeight(ang) + 16.0f;
        if (r + radius > rBowl && position.y < yTop && position.y > -0.1f) {
            // In fair territory only beyond the fence (don't clip infield).
            float wallR = fair ? layout.wallRAtAngle(ang) : 0.0f;
            bool pastFence = !fair || r > wallR - 0.5f;
            if (pastFence) {
                Vector3 n(std::sin(ang), 0.0f, -std::cos(ang));
                position = layout.fromHome(rBowl - radius - 0.02f, ang, position.y);
                position.y = std::max(position.y, groundY);
                killOutwardVelocity(velocity, n, stickOnContact);
                hit.surface = HitSurface::Stands;
                hit.impactY = position.y;
                if (stickOnContact) {
                    velocity = Vector3();
                    hit.stuck = true;
                }
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

    // Re-clamp ground after other resolves
    if (position.y < groundY) {
        position.y = groundY;
        if (stickOnContact) {
            velocity = Vector3();
            hit.stuck = true;
            if (hit.surface == HitSurface::None) {
                hit.surface = HitSurface::Ground;
            }
        }
    }

    return hit;
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

Mesh3D buildSkyDome(const Layout& L) {
    Mesh3D m;
    // Large hemisphere centered over the park; vertex colors = zenith → horizon gradient.
    const Vector3 center(0.0f, 0.0f, L.plateZ() - L.maxWallR() * 0.25f);
    const float R = std::max(900.0f, L.maxWallR() * 8.0f);
    const int rings = 18;
    const int segs = 48;
    sf::Color zenith = skyZenithColor();
    sf::Color horizon = skyColor();
    // Slight warm haze near horizon
    sf::Color haze(190, 210, 225);

    auto skyPoint = [&](float elev, float azim) {
        // elev 0 = horizon, elev pi/2 = zenith
        float y = std::sin(elev) * R;
        float rr = std::cos(elev) * R;
        return center + Vector3(std::sin(azim) * rr, y, -std::cos(azim) * rr);
    };
    auto skyCol = [&](float elev) {
        float t = std::clamp(elev / (pi * 0.5f), 0.0f, 1.0f);
        // Soft ease: more haze low, deeper blue high
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
        // Average band color (per-quad solid color — still reads as gradient)
        sf::Color band(
            static_cast<std::uint8_t>((c0.r + c1.r) / 2),
            static_cast<std::uint8_t>((c0.g + c1.g) / 2),
            static_cast<std::uint8_t>((c0.b + c1.b) / 2)
        );
        for (int j = 0; j < segs; j++) {
            float a0 = (static_cast<float>(j) / segs) * pi * 2.0f;
            float a1 = (static_cast<float>(j + 1) / segs) * pi * 2.0f;
            // Inward-facing so we see the dome from inside
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
    out.city = buildCity(layout);
    out.scoreboardScreen = buildScoreboardScreen(layout);
    out.skyDome = buildSkyDome(layout);
    out.clouds = buildClouds(layout);
    out.fanSectors = buildFanSectors(layout);
    buildFlags(layout, out.flagMeshes, out.flagBases);
    return out;
}

float recommendedFarPlane(const Layout& layout) {
    return std::max(3500.0f, layout.maxWallR() * 12.0f + 800.0f);
}

float fanCheerOffsetY(int sectorIndex, float timeSec, float boost) {
    // Traveling wave of fans standing / bouncing — more energetic by default.
    float phase = static_cast<float>(sectorIndex) * 0.55f;
    float wave = std::sin(timeSec * 6.2f + phase);
    float wave2 = std::sin(timeSec * 3.8f + phase * 1.7f);
    float wave3 = std::sin(timeSec * 9.1f + phase * 0.4f);
    float b = std::max(0.4f, boost);
    return (0.10f + 0.28f * std::max(0.0f, wave) + 0.10f * wave2 + 0.05f * std::max(0.0f, wave3)) * b;
}

float fanCheerOffsetX(int sectorIndex, float timeSec, float boost) {
    float phase = static_cast<float>(sectorIndex) * 0.71f;
    float b = std::max(0.4f, boost);
    return 0.08f * std::sin(timeSec * 4.4f + phase) * b;
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
