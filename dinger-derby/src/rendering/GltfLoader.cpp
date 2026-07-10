#include "GltfLoader.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "CharacterModel3D.h"

namespace fs = std::filesystem;

namespace {

// ── tiny JSON (subset) ──────────────────────────────────────────────────

struct Json {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<Json> a;
    std::vector<std::pair<std::string, Json>> o;

    const Json* get(const char* key) const {
        if (type != Object) {
            return nullptr;
        }
        for (const auto& kv : o) {
            if (kv.first == key) {
                return &kv.second;
            }
        }
        return nullptr;
    }
    double num(const char* key, double def = 0.0) const {
        const Json* j = get(key);
        return (j && j->type == Number) ? j->n : def;
    }
    int integer(const char* key, int def = 0) const {
        return static_cast<int>(num(key, def));
    }
    const std::string& str(const char* key, const std::string& def) const {
        const Json* j = get(key);
        if (j && j->type == String) {
            return j->s;
        }
        return def;
    }
    bool has(const char* key) const { return get(key) != nullptr; }
};

struct JsonParser {
    const char* p;
    const char* end;
    std::string err;

    void skip() {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) {
            p++;
        }
    }

    bool parse(Json& out) {
        skip();
        if (p >= end) {
            err = "unexpected end";
            return false;
        }
        if (*p == '{') {
            return parseObject(out);
        }
        if (*p == '[') {
            return parseArray(out);
        }
        if (*p == '"') {
            return parseString(out);
        }
        if (*p == 't' || *p == 'f') {
            return parseBool(out);
        }
        if (*p == 'n') {
            if (end - p >= 4 && std::strncmp(p, "null", 4) == 0) {
                p += 4;
                out = Json();
                return true;
            }
            err = "bad null";
            return false;
        }
        return parseNumber(out);
    }

    bool parseString(Json& out) {
        if (*p != '"') {
            err = "expected string";
            return false;
        }
        p++;
        out.type = Json::String;
        out.s.clear();
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                p++;
                char c = *p++;
                if (c == 'n') {
                    out.s.push_back('\n');
                } else if (c == 't') {
                    out.s.push_back('\t');
                } else if (c == '"') {
                    out.s.push_back('"');
                } else if (c == '\\') {
                    out.s.push_back('\\');
                } else {
                    out.s.push_back(c);
                }
            } else {
                out.s.push_back(*p++);
            }
        }
        if (p >= end || *p != '"') {
            err = "unterminated string";
            return false;
        }
        p++;
        return true;
    }

    bool parseNumber(Json& out) {
        const char* start = p;
        if (*p == '-') {
            p++;
        }
        while (p < end && *p >= '0' && *p <= '9') {
            p++;
        }
        if (p < end && *p == '.') {
            p++;
            while (p < end && *p >= '0' && *p <= '9') {
                p++;
            }
        }
        if (p < end && (*p == 'e' || *p == 'E')) {
            p++;
            if (p < end && (*p == '+' || *p == '-')) {
                p++;
            }
            while (p < end && *p >= '0' && *p <= '9') {
                p++;
            }
        }
        out.type = Json::Number;
        out.n = std::strtod(start, nullptr);
        return true;
    }

    bool parseBool(Json& out) {
        if (end - p >= 4 && std::strncmp(p, "true", 4) == 0) {
            p += 4;
            out.type = Json::Bool;
            out.b = true;
            return true;
        }
        if (end - p >= 5 && std::strncmp(p, "false", 5) == 0) {
            p += 5;
            out.type = Json::Bool;
            out.b = false;
            return true;
        }
        err = "bad bool";
        return false;
    }

    bool parseArray(Json& out) {
        if (*p != '[') {
            return false;
        }
        p++;
        out.type = Json::Array;
        out.a.clear();
        skip();
        if (p < end && *p == ']') {
            p++;
            return true;
        }
        while (p < end) {
            Json item;
            if (!parse(item)) {
                return false;
            }
            out.a.push_back(std::move(item));
            skip();
            if (p < end && *p == ',') {
                p++;
                skip();
                continue;
            }
            if (p < end && *p == ']') {
                p++;
                return true;
            }
            err = "bad array";
            return false;
        }
        err = "unterminated array";
        return false;
    }

    bool parseObject(Json& out) {
        if (*p != '{') {
            return false;
        }
        p++;
        out.type = Json::Object;
        out.o.clear();
        skip();
        if (p < end && *p == '}') {
            p++;
            return true;
        }
        while (p < end) {
            Json keyJ;
            if (!parseString(keyJ)) {
                return false;
            }
            skip();
            if (p >= end || *p != ':') {
                err = "expected :";
                return false;
            }
            p++;
            Json val;
            if (!parse(val)) {
                return false;
            }
            out.o.push_back({keyJ.s, std::move(val)});
            skip();
            if (p < end && *p == ',') {
                p++;
                skip();
                continue;
            }
            if (p < end && *p == '}') {
                p++;
                return true;
            }
            err = "bad object";
            return false;
        }
        err = "unterminated object";
        return false;
    }
};

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool readBin(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekg(0, std::ios::end);
    std::streamoff sz = f.tellg();
    f.seekg(0, std::ios::beg);
    if (sz <= 0) {
        out.clear();
        return true;
    }
    out.resize(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(out.data()), sz);
    return static_cast<bool>(f);
}

template <typename T>
bool readAccessor(
    const Json& root,
    const std::vector<unsigned char>& bin,
    int accessorIndex,
    std::vector<T>& out
) {
    const Json* accessors = root.get("accessors");
    const Json* bufferViews = root.get("bufferViews");
    if (!accessors || accessors->type != Json::Array ||
        accessorIndex < 0 || accessorIndex >= static_cast<int>(accessors->a.size())) {
        return false;
    }
    const Json& acc = accessors->a[accessorIndex];
    int count = acc.integer("count");
    int bvIndex = acc.integer("bufferView", -1);
    int accOffset = acc.integer("byteOffset", 0);
    if (!bufferViews || bvIndex < 0 || bvIndex >= static_cast<int>(bufferViews->a.size())) {
        return false;
    }
    const Json& bv = bufferViews->a[bvIndex];
    int bvOffset = bv.integer("byteOffset", 0);
    size_t offset = static_cast<size_t>(bvOffset + accOffset);
    size_t bytes = static_cast<size_t>(count) * sizeof(T);
    if (offset + bytes > bin.size()) {
        return false;
    }
    out.resize(count);
    std::memcpy(out.data(), bin.data() + offset, bytes);
    return true;
}

} // namespace

GltfLoadResult loadGltfFile(const std::string& path) {
    GltfLoadResult result;
    std::string text;
    if (!readFile(path, text)) {
        result.error = "failed to open " + path;
        return result;
    }

    JsonParser parser{text.c_str(), text.c_str() + text.size(), {}};
    Json root;
    if (!parser.parse(root) || root.type != Json::Object) {
        result.error = "JSON parse failed: " + parser.err;
        return result;
    }

    // Buffers
    std::vector<unsigned char> bin;
    const Json* buffers = root.get("buffers");
    if (buffers && buffers->type == Json::Array && !buffers->a.empty()) {
        const Json& b0 = buffers->a[0];
        std::string uri = b0.str("uri", "");
        if (!uri.empty() && uri.find("data:") != 0) {
            fs::path binPath = fs::path(path).parent_path() / uri;
            if (!readBin(binPath.string(), bin)) {
                result.error = "failed to read buffer " + binPath.string();
                return result;
            }
        } else if (uri.find("base64,") != std::string::npos) {
            result.error = "embedded base64 buffers not supported in v1";
            return result;
        }
    }

    SkinnedModel3D& model = result.model;

    // Nodes → joints (simplified: every node is a joint; skins pick subset)
    const Json* nodes = root.get("nodes");
    if (!nodes || nodes->type != Json::Array || nodes->a.empty()) {
        result.error = "no nodes";
        return result;
    }

    const int nodeCount = static_cast<int>(nodes->a.size());
    model.joints.resize(nodeCount);
    std::vector<int> nodeParent(nodeCount, -1);

    for (int i = 0; i < nodeCount; i++) {
        const Json& node = nodes->a[i];
        model.joints[i].name = node.str("name", "node_" + std::to_string(i));
        // translation
        if (const Json* t = node.get("translation"); t && t->type == Json::Array && t->a.size() >= 3) {
            model.joints[i].restTranslation = Vector3(
                static_cast<float>(t->a[0].n),
                static_cast<float>(t->a[1].n),
                static_cast<float>(t->a[2].n)
            );
        }
        if (const Json* r = node.get("rotation"); r && r->type == Json::Array && r->a.size() >= 4) {
            model.joints[i].restRotation = Quaternion::fromXyzw(
                static_cast<float>(r->a[0].n),
                static_cast<float>(r->a[1].n),
                static_cast<float>(r->a[2].n),
                static_cast<float>(r->a[3].n)
            );
        }
        if (const Json* s = node.get("scale"); s && s->type == Json::Array && s->a.size() >= 3) {
            model.joints[i].restScale = Vector3(
                static_cast<float>(s->a[0].n),
                static_cast<float>(s->a[1].n),
                static_cast<float>(s->a[2].n)
            );
        }
        model.joints[i].bakeLocalRest();

        if (const Json* children = node.get("children"); children && children->type == Json::Array) {
            for (const Json& c : children->a) {
                int ci = static_cast<int>(c.n);
                if (ci >= 0 && ci < nodeCount) {
                    nodeParent[ci] = i;
                }
            }
        }
    }
    for (int i = 0; i < nodeCount; i++) {
        model.joints[i].parent = nodeParent[i];
    }

    // Skin inverse binds
    const Json* skins = root.get("skins");
    std::vector<int> skinJoints; // node indices
    if (skins && skins->type == Json::Array && !skins->a.empty()) {
        const Json& skin = skins->a[0];
        if (const Json* js = skin.get("joints"); js && js->type == Json::Array) {
            for (const Json& j : js->a) {
                skinJoints.push_back(static_cast<int>(j.n));
            }
        }
        int ibmAcc = skin.integer("inverseBindMatrices", -1);
        if (ibmAcc >= 0) {
            std::vector<float> mats;
            // 16 floats per matrix
            const Json* accessors = root.get("accessors");
            if (accessors && ibmAcc < static_cast<int>(accessors->a.size())) {
                int count = accessors->a[ibmAcc].integer("count");
                std::vector<float> raw;
                // read as float array of count*16
                // reuse buffer view read with float
                const Json& acc = accessors->a[ibmAcc];
                int bvIndex = acc.integer("bufferView", -1);
                int accOffset = acc.integer("byteOffset", 0);
                const Json* bufferViews = root.get("bufferViews");
                if (bufferViews && bvIndex >= 0) {
                    int bvOffset = bufferViews->a[bvIndex].integer("byteOffset", 0);
                    size_t offset = static_cast<size_t>(bvOffset + accOffset);
                    size_t floats = static_cast<size_t>(count) * 16;
                    if (offset + floats * sizeof(float) <= bin.size()) {
                        raw.resize(floats);
                        std::memcpy(raw.data(), bin.data() + offset, floats * sizeof(float));
                        for (int i = 0; i < count && i < static_cast<int>(skinJoints.size()); i++) {
                            int node = skinJoints[i];
                            if (node < 0 || node >= nodeCount) {
                                continue;
                            }
                            Matrix4 m = Matrix4::identity();
                            // glTF matrices are column-major; our Matrix4 is row-major layout
                            // with transformPoint using row vectors style — convert.
                            for (int c = 0; c < 4; c++) {
                                for (int r = 0; r < 4; r++) {
                                    // column-major input → our row-major values[r*4+c]
                                    m.values[r * 4 + c] = raw[i * 16 + c * 4 + r];
                                }
                            }
                            model.joints[node].inverseBind = m;
                        }
                    }
                }
            }
        }
    }

    // If no skin IBM, derive from rest.
    bool anyIbm = false;
    for (const Joint3D& j : model.joints) {
        // detect non-identity inverseBind roughly
        if (std::fabs(j.inverseBind.values[0] - 1.0f) > 1e-5f ||
            std::fabs(j.inverseBind.values[15] - 1.0f) > 1e-5f ||
            std::fabs(j.inverseBind.values[3]) > 1e-5f) {
            anyIbm = true;
            break;
        }
    }
    if (!anyIbm) {
        model.rebuildInverseBindsFromRest();
    }

    // Mesh: first primitive with POSITION
    const Json* meshes = root.get("meshes");
    if (meshes && meshes->type == Json::Array && !meshes->a.empty()) {
        const Json& mesh = meshes->a[0];
        const Json* prims = mesh.get("primitives");
        if (prims && prims->type == Json::Array && !prims->a.empty()) {
            const Json& prim = prims->a[0];
            const Json* attrs = prim.get("attributes");
            if (attrs) {
                int posAcc = attrs->integer("POSITION", -1);
                int nrmAcc = attrs->integer("NORMAL", -1);
                int jntAcc = attrs->integer("JOINTS_0", -1);
                int wgtAcc = attrs->integer("WEIGHTS_0", -1);
                int idxAcc = prim.integer("indices", -1);

                std::vector<float> positions;
                std::vector<float> normals;
                std::vector<unsigned short> joints0;
                std::vector<float> weights0;
                std::vector<unsigned short> indices;

                if (posAcc >= 0) {
                    readAccessor(root, bin, posAcc, positions);
                }
                if (nrmAcc >= 0) {
                    readAccessor(root, bin, nrmAcc, normals);
                }
                if (jntAcc >= 0) {
                    // JOINTS_0 often UNSIGNED_SHORT VEC4
                    readAccessor(root, bin, jntAcc, joints0);
                }
                if (wgtAcc >= 0) {
                    readAccessor(root, bin, wgtAcc, weights0);
                }
                if (idxAcc >= 0) {
                    readAccessor(root, bin, idxAcc, indices);
                }

                int vertCount = static_cast<int>(positions.size() / 3);
                model.bindVertices.resize(vertCount);
                for (int i = 0; i < vertCount; i++) {
                    SkinVertex& v = model.bindVertices[i];
                    v.position = Vector3(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
                    if (static_cast<int>(normals.size()) >= (i + 1) * 3) {
                        v.normal = Vector3(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);
                    } else {
                        v.normal = Vector3(0.0f, 1.0f, 0.0f);
                    }
                    v.color = sf::Color(230, 230, 235);
                    if (static_cast<int>(joints0.size()) >= (i + 1) * 4 && !skinJoints.empty()) {
                        for (int k = 0; k < 4; k++) {
                            int skinJ = joints0[i * 4 + k];
                            v.joints[k] = (skinJ >= 0 && skinJ < static_cast<int>(skinJoints.size()))
                                ? skinJoints[skinJ]
                                : 0;
                        }
                    } else {
                        v.joints[0] = 0;
                        v.weights[0] = 1.0f;
                    }
                    if (static_cast<int>(weights0.size()) >= (i + 1) * 4) {
                        for (int k = 0; k < 4; k++) {
                            v.weights[k] = weights0[i * 4 + k];
                        }
                    }
                }
                if (!indices.empty()) {
                    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                        model.triangles.push_back({
                            static_cast<int>(indices[i]),
                            static_cast<int>(indices[i + 1]),
                            static_cast<int>(indices[i + 2])
                        });
                    }
                }
            }
        }
    }

    // Animations (LINEAR only)
    const Json* animations = root.get("animations");
    if (animations && animations->type == Json::Array) {
        for (const Json& anim : animations->a) {
            AnimationClip clip;
            clip.name = anim.str("name", "clip");
            const Json* channels = anim.get("channels");
            const Json* samplers = anim.get("samplers");
            if (!channels || !samplers) {
                continue;
            }
            for (const Json& chJ : channels->a) {
                int samplerIndex = chJ.integer("sampler", -1);
                const Json* target = chJ.get("target");
                if (!target || samplerIndex < 0 || samplerIndex >= static_cast<int>(samplers->a.size())) {
                    continue;
                }
                int node = target->integer("node", -1);
                std::string path = target->str("path", "");
                const Json& sampler = samplers->a[samplerIndex];
                int input = sampler.integer("input", -1);
                int output = sampler.integer("output", -1);
                std::vector<float> times;
                std::vector<float> values;
                if (input >= 0) {
                    readAccessor(root, bin, input, times);
                }
                if (output >= 0) {
                    readAccessor(root, bin, output, values);
                }
                AnimChannel ch;
                ch.jointIndex = node;
                ch.times = times;
                ch.values = values;
                ch.interp = AnimChannel::Linear;
                if (path == "translation") {
                    ch.path = AnimChannel::Translation;
                } else if (path == "rotation") {
                    ch.path = AnimChannel::Rotation;
                } else if (path == "scale") {
                    ch.path = AnimChannel::Scale;
                } else {
                    continue;
                }
                for (float t : times) {
                    clip.duration = std::max(clip.duration, t);
                }
                clip.channels.push_back(std::move(ch));
            }
            if (!clip.channels.empty()) {
                model.clips.push_back(std::move(clip));
            }
        }
    }

    if (model.bindVertices.empty()) {
        result.error = "glTF had no mesh vertices";
        return result;
    }

    result.ok = true;
    return result;
}

SkinnedModel3D loadCharacterOrProcedural(
    const std::string& name,
    bool catcher,
    int detail
) {
    std::vector<std::string> candidates = {
        "assets/characters/" + name + ".gltf",
        "../assets/characters/" + name + ".gltf",
        "../../assets/characters/" + name + ".gltf",
        "dinger-derby/assets/characters/" + name + ".gltf"
    };
    for (const std::string& path : candidates) {
        if (!fs::exists(path)) {
            continue;
        }
        GltfLoadResult loaded = loadGltfFile(path);
        if (loaded.ok) {
            return std::move(loaded.model);
        }
    }
    // Prefer the workshop CharacterModel3D athlete (multi-bone arms + throw_preview).
    // Older makeProcedural* humanoids remain available for tests / fallback tooling.
    CharacterModel3D::Detail d = CharacterModel3D::Detail::High;
    if (detail <= 0) {
        d = CharacterModel3D::Detail::Low;
    } else if (detail == 1) {
        d = CharacterModel3D::Detail::Medium;
    }
    return CharacterModel3D::build(
        catcher ? CharacterModel3D::Role::Catcher : CharacterModel3D::Role::Pitcher,
        d
    );
}
