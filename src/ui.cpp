#include "exworld/ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace exworld {
namespace {

using exgine::Material;

Material ui_material(const char* name, float r, float g, float b, float er = 0.f,
                     float eg = 0.f, float eb = 0.f) {
    Material m;
    m.name = name;
    m.base_color = {r, g, b};
    m.roughness = 0.85f;
    m.metallic = 0.f;
    if (er > 0.f || eg > 0.f || eb > 0.f) m.emission = {er, eg, eb};
    return m;
}

// 5x7 pixel font, rows top->bottom, bit4 = leftmost pixel.
struct GlyphDef { char c; std::uint8_t r[7]; };

constexpr GlyphDef kGlyphs[] = {
    {'A', {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'B', {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}},
    {'C', {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}},
    {'D', {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110}},
    {'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},
    {'F', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'G', {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111}},
    {'H', {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'I', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b11111}},
    {'J', {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100}},
    {'K', {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}},
    {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
    {'M', {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
    {'N', {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001}},
    {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'P', {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'Q', {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101}},
    {'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
    {'S', {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'U', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'V', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},
    {'W', {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001}},
    {'X', {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}},
    {'Y', {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'Z', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111}},
    {'0', {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}},
    {'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'2', {0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111}},
    {'3', {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'4', {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}},
    {'5', {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}},
    {'6', {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}},
    {'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}},
    {'8', {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}},
    {'9', {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}},
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {'.', {0, 0, 0, 0, 0, 0b01100, 0b01100}},
    {',', {0, 0, 0, 0, 0b01100, 0b00100, 0b01000}},
    {':', {0, 0b01100, 0b01100, 0, 0b01100, 0b01100, 0}},
    {'!', {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0, 0b00100}},
    {'?', {0b01110, 0b10001, 0b00001, 0b00110, 0b00100, 0, 0b00100}},
    {'\'', {0b00100, 0b00100, 0b01000, 0, 0, 0, 0}},
    {'-', {0, 0, 0, 0b11111, 0, 0, 0}},
    {'+', {0, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0}},
    {'/', {0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000}},
    {'(', {0b00010, 0b00100, 0b01000, 0b01000, 0b01000, 0b00100, 0b00010}},
    {')', {0b01000, 0b00100, 0b00010, 0b00010, 0b00010, 0b00100, 0b01000}},
    {'%', {0b11001, 0b11010, 0b00010, 0b00100, 0b01000, 0b01011, 0b10011}},
    {'=', {0, 0b11111, 0, 0b11111, 0, 0, 0}},
    {'>', {0b01000, 0b00100, 0b00010, 0b00001, 0b00010, 0b00100, 0b01000}},
    {'<', {0b00010, 0b00100, 0b01000, 0b10000, 0b01000, 0b00100, 0b00010}},
};

} // namespace

const char* ui_material_name(UiColor c) {
    switch (c) {
    case UiColor::Panel: return "ui_panel";
    case UiColor::Black: return "ui_black";
    case UiColor::White: return "ui_white";
    case UiColor::Dim: return "ui_dim";
    case UiColor::Gray: return "ui_gray";
    case UiColor::Accent: return "ui_accent";
    case UiColor::Neon: return "ui_neon";
    case UiColor::Cyan: return "ui_cyan";
    case UiColor::Red: return "ui_red";
    case UiColor::Green: return "ui_green";
    case UiColor::Yellow: return "ui_yellow";
    default: return "ui_white";
    }
}

bool ui_glyph(char c, std::array<std::uint8_t, 7>& rows) {
    const char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (const auto& g : kGlyphs) {
        if (g.c == u) {
            std::copy(std::begin(g.r), std::end(g.r), rows.begin());
            return true;
        }
    }
    return false;
}

bool UiRenderer::init(exgine::Runtime& runtime) {
    if (initialized_ && anchor_) return true;

    struct Def { const char* name; UiColor c; float r, g, b, er, eg, eb; };
    static constexpr Def defs[] = {
        {"ui_panel",  UiColor::Panel,  0.043f, 0.050f, 0.078f, 0, 0, 0},
        {"ui_black",  UiColor::Black,  0.008f, 0.008f, 0.012f, 0, 0, 0},
        {"ui_white",  UiColor::White,  0.94f, 0.945f, 0.96f, 0, 0, 0},
        {"ui_dim",    UiColor::Dim,    0.58f, 0.60f, 0.645f, 0, 0, 0},
        {"ui_gray",   UiColor::Gray,   0.34f, 0.36f, 0.40f, 0, 0, 0},
        {"ui_accent", UiColor::Accent, 1.00f, 0.72f, 0.18f, 0.22f, 0.14f, 0.02f},
        {"ui_neon",   UiColor::Neon,   0.95f, 0.20f, 0.72f, 0.45f, 0.06f, 0.32f},
        {"ui_cyan",   UiColor::Cyan,   0.14f, 0.82f, 0.92f, 0.04f, 0.28f, 0.33f},
        {"ui_red",    UiColor::Red,    0.88f, 0.16f, 0.14f, 0.30f, 0.03f, 0.03f},
        {"ui_green",  UiColor::Green,  0.22f, 0.84f, 0.36f, 0.05f, 0.30f, 0.08f},
        {"ui_yellow", UiColor::Yellow, 1.00f, 0.84f, 0.22f, 0.28f, 0.22f, 0.03f},
    };

    auto make_entity = [&](const char* name) -> exgine::EntityId {
        const auto id = runtime.state().entities.create(exgine::NodeKind::Property, name);
        if (!id) return exgine::invalid_entity;
        auto* e = runtime.state().entities.get(id);
        if (!e) return exgine::invalid_entity;
        e->scene_node = runtime.scene().create(runtime.scene().root());
        if (!e->scene_node) {
            runtime.state().entities.destroy(id);
            return exgine::invalid_entity;
        }
        return id;
    };

    for (const auto& d : defs)
        (void)runtime.define_material(ui_material(d.name, d.r, d.g, d.b, d.er, d.eg, d.eb));

    anchor_ = make_entity("UI_Anchor");
    for (std::size_t i = 0; i < buckets_.size(); ++i)
        buckets_[i] = make_entity("UI_Bucket");

    initialized_ = anchor_ && std::all_of(buckets_.begin(), buckets_.end(),
                                          [](exgine::EntityId id) { return id != exgine::invalid_entity; });
    return initialized_;
}

void UiRenderer::begin(const exgine::Camera& cam, float screen_w, float screen_h) {
    aspect_ = screen_h > 0.f ? screen_w / screen_h : 16.f / 9.f;
    canvas_w_ = std::clamp(900.f * aspect_, 1100.f, 2600.f);
    for (auto& b : batch_) b.clear();
    std::fill(dirty_.begin(), dirty_.end(), false);
    hits_.clear();
    (void)cam;
}

float UiRenderer::text_width(float size, std::string_view s) const {
    if (s.empty()) return 0.f;
    const float advance = size * 6.f / 7.f;
    return static_cast<float>(s.size()) * advance - size / 7.f;
}

void UiRenderer::text(float x, float y, float size, std::string_view s, UiColor color,
                      bool center_x, bool center_y) {
    if (s.empty()) return;
    const float advance = size * 6.f / 7.f;
    if (center_x) x -= text_width(size, s) * 0.5f;
    if (center_y) y -= size * 0.5f;
    std::array<std::uint8_t, 7> rows{};
    const float ps = size / 7.f;
    float pen = x;
    for (const char c : s) {
        if (ui_glyph(c, rows)) {
            for (int row = 0; row < 7; ++row) {
                const auto bits = rows[static_cast<std::size_t>(row)];
                for (int col = 0; col < 5; ++col) {
                    if (bits & (0x10u >> col))
                        rect(pen + static_cast<float>(col) * ps, y + static_cast<float>(row) * ps,
                             ps + 0.35f, ps + 0.35f, color);
                }
            }
        }
        pen += advance;
    }
}

void UiRenderer::rect(float x, float y, float w, float h, UiColor color) {
    quad(x + w * 0.5f, y + h * 0.5f, w, h, 0.f, color);
}

void UiRenderer::quad(float cx, float cy, float w, float h, float angle, UiColor color) {
    const float ca = std::cos(angle), sa = std::sin(angle);
    const float hw = w * 0.5f, hh = h * 0.5f;
    const float px[4] = {-hw, hw, hw, -hw};
    const float py[4] = {-hh, -hh, hh, hh};
    Quad q;
    for (int i = 0; i < 4; ++i)
        q.p[static_cast<std::size_t>(i)] = {cx + px[i] * ca - py[i] * sa,
                                            cy + px[i] * sa + py[i] * ca, 0.f};
    push_quad(q, color);
}

void UiRenderer::circle(float cx, float cy, float r, UiColor color, int segments) {
    segments = std::clamp(segments, 8, 64);
    for (int s = 0; s < segments; ++s) {
        const float a0 = 6.2831853f * static_cast<float>(s) / static_cast<float>(segments);
        const float a1 = 6.2831853f * static_cast<float>(s + 1) / static_cast<float>(segments);
        Quad q;
        q.p[0] = {cx, cy, 0.f};
        q.p[1] = {cx + std::cos(a0) * r, cy + std::sin(a0) * r, 0.f};
        q.p[2] = {cx + std::cos(a1) * r, cy + std::sin(a1) * r, 0.f};
        q.p[3] = q.p[2];
        push_quad(q, color);
    }
}

void UiRenderer::ring(float cx, float cy, float r, float thickness, UiColor color, int segments) {
    segments = std::clamp(segments, 8, 64);
    const float r0 = std::max(0.01f, r - thickness * 0.5f);
    const float r1 = r + thickness * 0.5f;
    for (int s = 0; s < segments; ++s) {
        const float a0 = 6.2831853f * static_cast<float>(s) / static_cast<float>(segments);
        const float a1 = 6.2831853f * static_cast<float>(s + 1) / static_cast<float>(segments);
        Quad q;
        q.p[0] = {cx + std::cos(a0) * r0, cy + std::sin(a0) * r0, 0.f};
        q.p[1] = {cx + std::cos(a0) * r1, cy + std::sin(a0) * r1, 0.f};
        q.p[2] = {cx + std::cos(a1) * r1, cy + std::sin(a1) * r1, 0.f};
        q.p[3] = {cx + std::cos(a1) * r0, cy + std::sin(a1) * r0, 0.f};
        push_quad(q, color);
    }
}

void UiRenderer::line(float x1, float y1, float x2, float y2, float thickness, UiColor color) {
    const float dx = x2 - x1, dy = y2 - y1;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.01f) return;
    quad((x1 + x2) * 0.5f, (y1 + y2) * 0.5f, len, thickness, std::atan2(dy, dx), color);
}

void UiRenderer::poly(const std::array<float, 10>& xy, UiColor color) {
    Quad q;
    q.p[0] = {xy[0], xy[1], 0.f};
    q.p[1] = {xy[2], xy[3], 0.f};
    q.p[2] = {xy[4], xy[5], 0.f};
    q.p[3] = {xy[6], xy[7], 0.f};
    push_quad(q, color);
    // second triangle (0, 3, 4)
    Quad t;
    t.p[0] = q.p[0];
    t.p[1] = q.p[3];
    t.p[2] = {xy[8], xy[9], 0.f};
    t.p[3] = t.p[2];
    push_quad(t, color);
}

void UiRenderer::push_quad(const Quad& q, UiColor color) {
    auto& b = batch_[static_cast<std::size_t>(color)];
    b.push_back(q);
    dirty_[static_cast<std::size_t>(color)] = true;
}

int UiRenderer::hit_index(float x, float y) const {
    for (std::size_t i = hits_.size(); i-- > 0;) {
        const auto& r = hits_[i];
        if (x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1)
            return static_cast<int>(i);
    }
    return -1;
}

void UiRenderer::hit_rect(float x0, float y0, float x1, float y1) {
    hits_.push_back({x0, y0, x1, y1});
}

void UiRenderer::end(exgine::Runtime& runtime) {
    if (!initialized_) return;

    const exgine::Camera* cam = nullptr;
    const auto cam_snap = runtime.lighting().main_camera();
    cam = &cam_snap;
    if (!cam->valid()) return;

    const float rx = cam->rotation.x, ry = cam->rotation.y;
    const float cp = std::cos(rx), sp = std::sin(rx);
    const float cy = std::cos(ry), sy = std::sin(ry);
    const exgine::Vec3 fwd{sy * cp, -sp, cy * cp};

    for (std::size_t i = 0; i < buckets_.size(); ++i) {
        const auto id = buckets_[i];
        if (!id) continue;
        auto* e = runtime.state().entities.get(id);
        if (!e || !e->scene_node) continue;

        const auto& quads = batch_[i];
        if (quads.empty()) {
            if (e->active) {
                e->active = false;
                exgine::SceneTransform t{};
                (void)runtime.scene().set_local_transform(e->scene_node, t);
            }
            continue;
        }
        e->active = true;

        const float dist = 2.2f - static_cast<float>(i) * 0.010f;
        const float half_h = dist * std::tan(cam->vertical_fov_degrees * 0.5f * 3.14159265f / 180.f);
        const float half_w = half_h * aspect_;
        const float scale = (2.f * half_h) / 900.f;
        const float cx0 = canvas_w_ * 0.5f;

        exgine::Mesh mesh;
        mesh.vertices.reserve(quads.size() * 4);
        mesh.indices.reserve(quads.size() * 6);
        for (const auto& q : quads) {
            const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
            for (int v = 0; v < 4; ++v)
                mesh.vertices.push_back({{(q.p[static_cast<std::size_t>(v)].x - cx0) * scale,
                                          -(q.p[static_cast<std::size_t>(v)].y - 450.f) * scale,
                                          0.f},
                                         {0.f, 0.f, 1.f},
                                         {0.5f, 0.5f}});
            mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }

        if (dirty_[i]) {
            exgine::MeshAssembly assembly;
            exgine::MeshPart part;
            part.name = "ui";
            part.mesh = std::move(mesh);
            part.material_slot = ui_material_name(static_cast<UiColor>(i));
            assembly.parts.push_back(std::move(part));
            (void)runtime.attach_geometry(id, std::move(assembly));
        }

        exgine::SceneTransform t;
        t.position = {cam->position.x + fwd.x * dist,
                      cam->position.y + fwd.y * dist,
                      cam->position.z + fwd.z * dist};
        t.rotation = cam->rotation;
        t.scale = {1, 1, 1};
        (void)runtime.scene().set_local_transform(e->scene_node, t);
    }
    runtime.scene().update_world_transforms();
}

} // namespace exworld
