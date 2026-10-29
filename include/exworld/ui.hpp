#pragma once

// EXWORLD UI renderer.
// Draws menus, loading screens and the gameplay HUD as merged quad meshes
// anchored in front of the game camera (world-space UI, EXGINE renders it as
// ordinary opaque draws). Canvas is height-locked: 900 units tall, width grows
// with the device aspect ratio so circles stay circular on any phone.

#include "exgine/runtime.hpp"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace exworld {

enum class UiColor : std::uint8_t {
    Panel,   // near-black navy panel
    Black,
    White,
    Dim,     // muted white
    Gray,
    Accent,  // warm amber (EXWORLD brand)
    Neon,    // magenta
    Cyan,
    Red,
    Green,
    Yellow,
    Count
};

class UiRenderer {
public:
    // Creates UI entities + materials once. Safe to call again (no-op).
    bool init(exgine::Runtime& runtime);

    // Begin a frame. Places the UI plane in front of the camera and clears
    // the batch. canvas_w() is derived from the aspect ratio (height = 900).
    void begin(const exgine::Camera& cam, float screen_w, float screen_h);
    void end(exgine::Runtime& runtime);

    [[nodiscard]] float canvas_w() const noexcept { return canvas_w_; }
    static constexpr float canvas_h() noexcept { return 900.f; }

    // --- primitives (canvas coords, y grows downward) ---
    void rect(float x, float y, float w, float h, UiColor color);            // axis aligned
    void quad(float cx, float cy, float w, float h, float angle, UiColor c); // rotated around center
    void circle(float cx, float cy, float r, UiColor color, int segments = 26);
    void ring(float cx, float cy, float r, float thickness, UiColor color, int segments = 36);
    void line(float x1, float y1, float x2, float y2, float thickness, UiColor color);
    void poly(const std::array<float, 10>& xy, UiColor color);               // 5 points (x,y)*5

    // --- text (5x7 pixel font; size = glyph height in canvas units) ---
    void text(float x, float y, float size, std::string_view s, UiColor color,
              bool center_x = false, bool center_y = false);
    [[nodiscard]] float text_width(float size, std::string_view s) const;

    // --- deferred hit rects (buttons), consumed via tap tests in game flow ---
    struct HitRect { float x0, y0, x1, y1; };
    void hit_rect(float x0, float y0, float x1, float y1);
    void clear_hits() noexcept { hits_.clear(); }
    [[nodiscard]] const std::vector<HitRect>& hits() const noexcept { return hits_; }
    [[nodiscard]] int hit_index(float x, float y) const; // -1 = none

private:
    struct Quad { std::array<exgine::Vec3, 4> p; };
    void push_quad(const Quad& q, UiColor color);

    exgine::EntityId anchor_ = exgine::invalid_entity;
    std::array<exgine::EntityId, static_cast<std::size_t>(UiColor::Count)> buckets_{};
    std::array<std::vector<Quad>, static_cast<std::size_t>(UiColor::Count)> batch_{};
    std::array<bool, static_cast<std::size_t>(UiColor::Count)> dirty_{};
    std::vector<HitRect> hits_{};
    float canvas_w_ = 1600.f;
    float aspect_ = 16.f / 9.f;
    bool initialized_ = false;
};

// Shared tiny glyph font (A-Z 0-9 punctuation). Returns false for unknown.
[[nodiscard]] bool ui_glyph(char c, std::array<std::uint8_t, 7>& rows);

// Material slot name backing each UI color (defined by UiRenderer::init).
[[nodiscard]] const char* ui_material_name(UiColor c);

} // namespace exworld
