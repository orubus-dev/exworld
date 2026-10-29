#pragma once

// EXWORLD input.
// Desktop: keyboard + gamepad. Android: multi-touch with a dynamic virtual
// joystick (spawns where the thumb lands), full-right-side camera drag and
// labeled action buttons. Touch roles are locked per pointer so a camera
// drag never hijacks the stick and vice versa.

#include "exworld/player.hpp"

#include <cstdint>
#include <vector>

namespace exworld {

enum class InputDevice : std::uint8_t { Keyboard, Gamepad, Touch, Combined };

// Touch actions (screen-anchored buttons, canvas coordinates 1600x900-based).
enum class TouchAction : std::uint8_t {
    None = 0,
    Pause,
    Interact,   // E — enter vehicle / building (or context action)
    Exit,       // F — exit vehicle / building
    Sprint,
    JumpBrake,  // jump on foot, brake in vehicle
    Stick,
    Look
};

struct TouchFinger {
    bool down = false;
    float x = 0.f; // 0..1, y grows downward (Android-native)
    float y = 0.f;
    std::int32_t id = -1;
    TouchAction role = TouchAction::None;
    // stick state
    float base_x = 0.f, base_y = 0.f; // normalized stick base
};

struct UiTap {
    float x = 0.f; // canvas coords
    float y = 0.f;
};

class InputSystem {
public:
    void reset() noexcept;

    // Desktop / test injection
    void set_key(int keycode, bool down) noexcept;
    void set_gamepad_axis(float left_x, float left_y, float right_x, float right_y) noexcept;
    void set_gamepad_button(int button, bool down) noexcept;

    // Android touch
    void set_touch(int finger_id, bool down, float norm_x, float norm_y) noexcept;

    // Build the PlayerInput consumed by the game once per frame.
    [[nodiscard]] PlayerInput poll() noexcept;

    // Virtual stick visuals (normalized screen coords; valid while active).
    [[nodiscard]] bool stick_active() const noexcept { return stick_active_; }
    [[nodiscard]] float stick_base_x() const noexcept { return stick_base_nx_; }
    [[nodiscard]] float stick_base_y() const noexcept { return stick_base_ny_; }
    [[nodiscard]] float stick_knob_dx() const noexcept { return stick_knodx_; }
    [[nodiscard]] float stick_knob_dy() const noexcept { return stick_knody_; }

    // Button pressed states for HUD drawing.
    [[nodiscard]] bool button_held(TouchAction a) const noexcept;
    // Button hit rects in canvas coords (queried by the UI to draw them).
    struct ButtonRect { TouchAction action; float cx, cy, r; const char* label; bool round; };
    [[nodiscard]] std::vector<ButtonRect> button_layout(bool in_vehicle) const;

    // One-shot UI taps (menu / pause), consumed by game flow each frame.
    [[nodiscard]] const std::vector<UiTap>& taps() const noexcept { return taps_; }
    void clear_taps() noexcept { taps_.clear(); }

    [[nodiscard]] InputDevice last_device() const noexcept { return device_; }

    // Button geometry shared by input hit-testing and HUD drawing.
    // Canvas is height-locked 900; width derived from aspect at set_screen.
    void set_screen(float norm_w_aspect) noexcept; // aspect = width/height
    [[nodiscard]] float canvas_width() const noexcept;

private:
    [[nodiscard]] TouchAction hit_button(float nx, float ny) const noexcept;

    bool key_w_ = false, key_a_ = false, key_s_ = false, key_d_ = false;
    bool key_shift_ = false, key_space_ = false, key_e_ = false, key_f_ = false;
    bool key_ctrl_ = false;

    float pad_lx_ = 0, pad_ly_ = 0, pad_rx_ = 0, pad_ry_ = 0;
    bool pad_a_ = false, pad_b_ = false, pad_x_ = false, pad_y_ = false;
    bool pad_lb_ = false, pad_rb_ = false;

    TouchFinger fingers_[5]{};
    InputDevice device_ = InputDevice::Keyboard;

    // touch state
    bool stick_active_ = false;
    float stick_base_nx_ = 0.f, stick_base_ny_ = 0.f;
    float stick_knodx_ = 0.f, stick_knody_ = 0.f;
    float stick_output_x_ = 0.f, stick_output_y_ = 0.f;
    bool touch_sprint_ = false, touch_interact_ = false, touch_exit_ = false;
    bool touch_jump_ = false;
    bool touch_brake_ = false;
    float look_dx_ = 0.f, look_dy_ = 0.f; // accumulated drag, normalized units
    float aspect_ = 16.f / 9.f;

    std::vector<UiTap> taps_{};

    bool prev_interact_ = false;
    bool prev_exit_ = false;
};

} // namespace exworld
