#include "exworld/input.hpp"

#include <algorithm>
#include <cmath>

namespace exworld {

namespace {

// Canvas is height-locked at 900 units; width follows the device aspect.
constexpr float kCanvasH = 900.f;

// Stick geometry (normalized screen units)
constexpr float kStickZoneMaxX = 0.44f;   // left 44% of the screen
constexpr float kStickMinU = 0.10f, kStickMaxU = 0.90f;
constexpr float kStickRadiusN = 0.1311f;  // 118 canvas px at 900-high canvas
constexpr float kStickDeadzone = 0.14f;

// Look sensitivity: radians of camera turn per full-screen drag
constexpr float kLookYawPerScreen = 4.6f;
constexpr float kLookPitchPerScreen = 2.6f;

struct ButtonDef { TouchAction action; float cx, cy, r; const char* label; };
// cx/cy are expressed relative to canvas W (x) and H (y): x>0 = from right edge.
constexpr ButtonDef kOnFootButtons[] = {
    {TouchAction::Interact, 175.f, 640.f, 96.f, "E"},
    {TouchAction::JumpBrake, 350.f, 700.f, 74.f, "JUMP"},
    {TouchAction::Sprint,   350.f, 540.f, 74.f, "RUN"},
};
constexpr ButtonDef kVehicleButtons[] = {
    {TouchAction::Exit,     175.f, 640.f, 96.f, "EXIT"},
    {TouchAction::JumpBrake, 355.f, 705.f, 84.f, "BRAKE"},
};

constexpr float kPauseCx = 62.f, kPauseCy = 62.f, kPauseR = 46.f;

float button_cx(const ButtonDef& b, float cw) { return b.cx > cw ? b.cx : cw - b.cx; }
float button_cy(const ButtonDef& b) { return kCanvasH - b.cy; }

} // namespace

float InputSystem::canvas_width() const noexcept {
    return std::clamp(kCanvasH * aspect_, 1100.f, 2600.f);
}

void InputSystem::set_screen(float norm_w_aspect) noexcept {
    aspect_ = norm_w_aspect > 0.5f ? norm_w_aspect : 1.7778f;
}

std::vector<InputSystem::ButtonRect> InputSystem::button_layout(bool in_vehicle) const {
    std::vector<ButtonRect> out;
    const float cw = canvas_width();
    const ButtonDef* set = in_vehicle ? kVehicleButtons : kOnFootButtons;
    const std::size_t n = in_vehicle ? std::size(kVehicleButtons) : std::size(kOnFootButtons);
    for (std::size_t i = 0; i < n; ++i) {
        const auto& b = set[i];
        out.push_back({b.action, button_cx(b, cw), button_cy(b), b.r, b.label, true});
    }
    out.push_back({TouchAction::Pause, kPauseCx, kPauseCy, kPauseR, "II", true});
    return out;
}

bool InputSystem::button_held(TouchAction a) const noexcept {
    for (const auto& f : fingers_)
        if (f.down && f.role == a) return true;
    switch (a) {
    case TouchAction::Sprint: return touch_sprint_;
    case TouchAction::Interact: return touch_interact_;
    case TouchAction::Exit: return touch_exit_;
    case TouchAction::JumpBrake: return touch_jump_ || touch_brake_;
    default: return false;
    }
}

void InputSystem::reset() noexcept {
    key_w_ = key_a_ = key_s_ = key_d_ = false;
    key_shift_ = key_space_ = key_e_ = key_f_ = key_ctrl_ = false;
    pad_lx_ = pad_ly_ = pad_rx_ = pad_ry_ = 0;
    pad_a_ = pad_b_ = pad_x_ = pad_y_ = pad_lb_ = pad_rb_ = false;
    for (auto& f : fingers_) f = {};
    stick_active_ = false;
    stick_knodx_ = stick_knody_ = stick_output_x_ = stick_output_y_ = 0;
    touch_sprint_ = touch_interact_ = touch_exit_ = touch_jump_ = false;
    touch_brake_ = false;
    look_dx_ = look_dy_ = 0;
    taps_.clear();
    prev_interact_ = prev_exit_ = false;
    device_ = InputDevice::Keyboard;
}

void InputSystem::set_key(int keycode, bool down) noexcept {
    device_ = InputDevice::Keyboard;
    switch (keycode) {
    case 'W': case 'w': key_w_ = down; break;
    case 'A': case 'a': key_a_ = down; break;
    case 'S': case 's': key_s_ = down; break;
    case 'D': case 'd': key_d_ = down; break;
    case 16:  key_shift_ = down; break;
    case 32:  key_space_ = down; break;
    case 'E': case 'e': key_e_ = down; break;
    case 'F': case 'f': key_f_ = down; break;
    case 17:  key_ctrl_ = down; break;
    default: break;
    }
}

void InputSystem::set_gamepad_axis(float left_x, float left_y, float right_x, float right_y) noexcept {
    device_ = InputDevice::Gamepad;
    pad_lx_ = std::clamp(left_x, -1.f, 1.f);
    pad_ly_ = std::clamp(left_y, -1.f, 1.f);
    pad_rx_ = std::clamp(right_x, -1.f, 1.f);
    pad_ry_ = std::clamp(right_y, -1.f, 1.f);
}

void InputSystem::set_gamepad_button(int button, bool down) noexcept {
    device_ = InputDevice::Gamepad;
    switch (button) {
    case 0: pad_a_ = down; break;
    case 1: pad_b_ = down; break;
    case 2: pad_x_ = down; break;
    case 3: pad_y_ = down; break;
    case 4: pad_lb_ = down; break;
    case 5: pad_rb_ = down; break;
    default: break;
    }
}

TouchAction InputSystem::hit_button(float nx, float ny) const noexcept {
    const float cw = canvas_width();
    const float cx = nx * cw, cy = ny * kCanvasH;
    // Pause first (top-right)
    {
        const float dx = cx - kPauseCx, dy = cy - kPauseCy;
        if (dx * dx + dy * dy < (kPauseR + 22.f) * (kPauseR + 22.f)) return TouchAction::Pause;
    }
    // Buttons are hit-tested against BOTH layouts (on-foot + vehicle sets)
    // so a mode switch mid-touch keeps working.
    for (const auto& b : kOnFootButtons) {
        const float dx = cx - button_cx(b, cw), dy = cy - button_cy(b);
        if (dx * dx + dy * dy < (b.r + 26.f) * (b.r + 26.f)) return b.action;
    }
    for (const auto& b : kVehicleButtons) {
        const float dx = cx - button_cx(b, cw), dy = cy - button_cy(b);
        if (dx * dx + dy * dy < (b.r + 26.f) * (b.r + 26.f)) return b.action;
    }
    return TouchAction::None;
}

void InputSystem::set_touch(int finger_id, bool down, float norm_x, float norm_y) noexcept {
    device_ = InputDevice::Touch;
    norm_x = std::clamp(norm_x, 0.f, 1.f);
    norm_y = std::clamp(norm_y, 0.f, 1.f);

    int slot = -1;
    for (int i = 0; i < 5; ++i)
        if (fingers_[i].down && fingers_[i].id == finger_id) { slot = i; break; }

    if (!down) {
        if (slot < 0) return;
        auto& f = fingers_[slot];
        if (f.role == TouchAction::Stick) {
            stick_active_ = false;
            stick_knodx_ = stick_knody_ = 0;
            stick_output_x_ = stick_output_y_ = 0;
        }
        switch (f.role) {
        case TouchAction::Sprint: touch_sprint_ = false; break;
        case TouchAction::Interact: touch_interact_ = false; break;
        case TouchAction::Exit: touch_exit_ = false; break;
        case TouchAction::JumpBrake: touch_jump_ = false; touch_brake_ = false; break;
        default: break;
        }
        f = {};
        return;
    }

    if (slot < 0) {
        for (int i = 0; i < 5; ++i)
            if (!fingers_[i].down) { slot = i; break; }
        if (slot < 0) return; // more fingers than tracked — ignore

        auto& f = fingers_[slot];
        f.id = finger_id;
        f.down = true;
        f.x = norm_x;
        f.y = norm_y;

        // Role assignment priority: buttons > stick zone > look.
        // Every touch-down also produces a UI tap (menus / pause).
        taps_.push_back({norm_x * canvas_width(), norm_y * kCanvasH});
        const TouchAction hit = hit_button(norm_x, norm_y);
        if (hit != TouchAction::None) {
            f.role = hit;
            switch (hit) {
            case TouchAction::Sprint: touch_sprint_ = true; break;
            case TouchAction::Interact: touch_interact_ = true; break;
            case TouchAction::Exit: touch_exit_ = true; break;
            case TouchAction::JumpBrake: touch_jump_ = true; touch_brake_ = true; break;
            default: break;
            }
            return;
        }
        if (norm_x < kStickZoneMaxX && !stick_active_) {
            f.role = TouchAction::Stick;
            stick_active_ = true;
            stick_base_nx_ = std::clamp(norm_x, kStickMinU * 0.5f, kStickZoneMaxX);
            stick_base_ny_ = std::clamp(norm_y, 0.18f, 0.94f);
            stick_knodx_ = stick_knody_ = 0;
            return;
        }
        f.role = TouchAction::Look;
        return;
    }

    // Move / drag update for an already-tracked finger
    auto& f = fingers_[slot];
    const float dx = norm_x - f.x;
    const float dy = norm_y - f.y;
    f.x = norm_x;
    f.y = norm_y;
    switch (f.role) {
    case TouchAction::Look:
        look_dx_ += dx;
        look_dy_ += dy;
        break;
    case TouchAction::Stick: {
        float kdx = (norm_x - stick_base_nx_) * aspect_; // normalize by height
        float kdy = norm_y - stick_base_ny_;
        const float len = std::sqrt(kdx * kdx + kdy * kdy);
        const float max_len = kStickRadiusN;
        if (len > max_len) {
            kdx *= max_len / len;
            kdy *= max_len / len;
        }
        stick_knodx_ = kdx;
        stick_knody_ = kdy;
        float ox = kdx / max_len, oy = kdy / max_len;
        const float olen = std::sqrt(ox * ox + oy * oy);
        if (olen < kStickDeadzone) {
            ox = oy = 0.f;
        } else if (olen > 1.f) {
            ox /= olen;
            oy /= olen;
        }
        stick_output_x_ = ox;
        stick_output_y_ = oy;
        break;
    }
    default:
        break;
    }
}

PlayerInput InputSystem::poll() noexcept {
    PlayerInput in;

    float mx = 0.f, mz = 0.f;
    if (key_a_) mx -= 1.f;
    if (key_d_) mx += 1.f;
    if (key_w_) mz += 1.f;
    if (key_s_) mz -= 1.f;

    if (std::fabs(pad_lx_) > 0.15f || std::fabs(pad_ly_) > 0.15f) {
        mx = pad_lx_;
        mz = -pad_ly_;
    }

    // Virtual stick overrides keyboard when active
    if (stick_active_) {
        mx = stick_output_x_;
        mz = -stick_output_y_;
    }

    const float len = std::sqrt(mx * mx + mz * mz);
    if (len > 1.f) { mx /= len; mz /= len; }
    in.move_x = mx;
    in.move_z = mz;

    // Camera drag accumulated this frame (normalized units)
    in.look_yaw = look_dx_ * kLookYawPerScreen;
    in.look_pitch = look_dy_ * kLookPitchPerScreen;
    look_dx_ = look_dy_ = 0.f;

    if (std::fabs(pad_rx_) > 0.12f || std::fabs(pad_ry_) > 0.12f) {
        in.look_yaw += pad_rx_ * 2.4f * (1.f / 60.f);
        in.look_pitch += pad_ry_ * 1.6f * (1.f / 60.f);
    }

    in.sprint = key_shift_ || pad_rb_ || touch_sprint_;
    in.jump = key_space_ || pad_y_ || touch_jump_;
    in.crouch = key_ctrl_ || pad_lb_ || touch_brake_;

    const bool interact_held = key_e_ || pad_a_ || touch_interact_;
    const bool exit_held = key_f_ || pad_b_ || touch_exit_;
    in.interact = interact_held && !prev_interact_;
    in.exit = exit_held && !prev_exit_;
    prev_interact_ = interact_held;
    prev_exit_ = exit_held;

    return in;
}

} // namespace exworld
