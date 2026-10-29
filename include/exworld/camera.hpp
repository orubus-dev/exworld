#pragma once

// EXWORLD gameplay camera.
// Third-person follow with critically-damped smoothing, velocity look-ahead,
// vehicle heading alignment, speed-driven FOV, impulse shake, building
// pull-in, plus cinematic (menu) and fly-in modes.

#include "exgine/runtime.hpp"
#include "exworld/citygen.hpp"

#include <vector>

namespace exworld {

class GameCamera {
public:
    void reset(const exgine::Vec3& target);

    // Manual orbit (touch drag / gamepad). Angles in radians.
    void orbit(float dyaw, float dpitch) noexcept;
    void set_orbit(float yaw, float pitch) noexcept;
    void clear_manual_timer() noexcept { manual_timer_ = 0.f; }

    void add_shake(float amount) noexcept;

    // Gameplay follow. velocity = target world velocity (m/s),
    // vehicle_yaw = heading of the driven vehicle, speed_frac = 0..1,
    // solids = building AABBs for pull-in (may be null).
    void update(float dt, const exgine::Vec3& target, const exgine::Vec3& velocity,
                bool in_vehicle, float vehicle_yaw, float speed_frac, bool sprinting,
                exgine::Runtime& runtime, const std::vector<CitySolid>* solids);

    // Cinematic menu orbit around a point. Returns false when superseded.
    void begin_cinematic(const exgine::Vec3& around, float radius = 205.f, float height = 92.f);
    void update_cinematic(float dt);
    [[nodiscard]] bool cinematic() const noexcept { return mode_ == Mode::Cinematic; }

    // Smooth spline from the current pose to gameplay follow behind the player.
    void begin_fly_in(const exgine::Vec3& player);
    [[nodiscard]] bool fly_in(const exgine::Vec3& player, float dt);
    [[nodiscard]] bool flying() const noexcept { return mode_ == Mode::FlyIn; }

    [[nodiscard]] const exgine::Camera& camera() const noexcept { return camera_; }
    [[nodiscard]] float yaw() const noexcept { return yaw_; }
    [[nodiscard]] float pitch() const noexcept { return pitch_; }

private:
    enum class Mode : std::uint8_t { Gameplay, Cinematic, FlyIn };
    void apply(const exgine::Vec3& pos, float fov);
    [[nodiscard]] float pull_in_distance(const exgine::Vec3& from, exgine::Vec3 to,
                                         const std::vector<CitySolid>* solids) const;

    exgine::Camera camera_{};
    Mode mode_ = Mode::Gameplay;

    float yaw_ = 0.55f;
    float pitch_ = -0.24f;
    float distance_ = 4.8f;
    float distance_current_ = 4.8f;
    float fov_ = 58.f;
    float smooth_omega_ = 9.f;

    exgine::Vec3 current_pos_{};
    exgine::Vec3 look_ahead_{};
    bool initialized_ = false;

    float manual_timer_ = 10.f;
    float shake_ = 0.f;
    float shake_time_ = 0.f;

    // cinematic
    exgine::Vec3 cine_center_{};
    float cine_angle_ = 0.f;
    float cine_radius_ = 205.f;
    float cine_height_ = 92.f;

    // fly-in
    exgine::Vec3 fly_from_pos_{};
    exgine::Vec3 fly_goal_pos_{};
    float fly_from_yaw_ = 0.f, fly_from_pitch_ = -0.5f;
    float fly_goal_yaw_ = 0.f, fly_goal_pitch_ = -0.24f;
    float fly_t_ = 0.f;
};

} // namespace exworld
