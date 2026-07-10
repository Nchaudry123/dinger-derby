#include "math/Matrix4.h"
#include "math/Quaternion.h"
#include "math/Vector3.h"

#include <cmath>
#include <iostream>

static int gFails = 0;

static void expect(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        gFails++;
    }
}

int main() {
    Quaternion id = Quaternion::identity();
    expect(std::fabs(id.w - 1.0f) < 1e-5f, "identity w");

    Quaternion q = Quaternion::fromAxisAngle(Vector3(0.0f, 1.0f, 0.0f), 3.14159265f * 0.5f);
    Vector3 v = q.rotate(Vector3(1.0f, 0.0f, 0.0f));
    expect(std::fabs(v.z + 1.0f) < 0.05f || std::fabs(v.z - 1.0f) < 0.05f, "90 deg Y rotate");

    Quaternion a = Quaternion::identity();
    Quaternion b = Quaternion::fromAxisAngle(Vector3(0.0f, 0.0f, 1.0f), 1.0f);
    Quaternion s = Quaternion::slerp(a, b, 0.0f);
    expect(std::fabs(s.w - a.w) < 1e-4f, "slerp t=0");
    s = Quaternion::slerp(a, b, 1.0f);
    expect(std::fabs(s.x - b.normalized().x) < 1e-3f, "slerp t=1");

    Matrix4 m = Matrix4::fromTrs(
        Vector3(1.0f, 2.0f, 3.0f),
        Quaternion::fromEulerXYZ(0.2f, -0.3f, 0.4f),
        Vector3(1.0f, 1.0f, 1.0f)
    );
    Matrix4 inv = m.inverse();
    Matrix4 i = m * inv;
    expect(std::fabs(i.values[0] - 1.0f) < 1e-3f, "inverse diag 0");
    expect(std::fabs(i.values[5] - 1.0f) < 1e-3f, "inverse diag 5");
    expect(std::fabs(i.values[10] - 1.0f) < 1e-3f, "inverse diag 10");
    expect(std::fabs(i.values[15] - 1.0f) < 1e-3f, "inverse diag 15");
    expect(std::fabs(i.values[3]) < 1e-3f, "inverse off T");

    if (gFails == 0) {
        std::cout << "quaternion_test OK\n";
        return 0;
    }
    std::cerr << gFails << " failure(s)\n";
    return 1;
}
