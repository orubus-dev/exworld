#include "exworld/camera.hpp"

#include <algorithm>
#include <cmath>

namespace exworld {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float approach(float current, float target, float omega, float dt) {
    return current + (target - current) * (1.f - std::exp(-omega * dt));
}

float wrap_angle(float a) {
    while (a > kPi) a -= 2.f * kPi;
    while (a < -kPi) a += 2.f * kPi;
    return a;
}

float ease_in_out(float t) {
    t = std::clamp(t, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

} // namespace

void GameCamera::reset(const exgine::Vec3& target) {
    current_pos_ = {target.x - std::sin(yaw_) * std::cos(pitch_) * distance_,
                    target.y - std::sin(pitch_) * distance_ + 2.0f,
                    target.z - std::cos(yaw_) * std::cos(pitch_) * distance_};
    initialized_ = true;
    look_ahead_ = {};
    apply(current_pos_, fov_);
    mode_ = Mode::Gameplay;
}

void GameCamera::orbit(float dyaw, float dpitch) noexcept {
    yaw_ = wrap_angle(yaw_ + dyaw);
    pitch_ = std::clamp(pitch_ + dpitch, -1.05f, 0.55f);
    manual_timer_ = 0.f;
}

void GameCamera::set_orbit(float yaw, float pitch) noexcept {
    yaw_ = wrap_angle(yaw);
    pitch_ = std::clamp(pitch, -1.05f, 0.55f);
}

void GameCamera::add_shake(float amount) noexcept {
    shake_ = std::clamp(shake_ + amount, 0.f, 0.6f);
}

void GameCamera::apply(const exgine::Vec3& pos, float fov) {
    camera_.position = pos;
    // EXGINE camera looks along -Z in camera space; gameplay yaw 0 points +Z.
    camera_.rotation = {-pitch_, yaw_ + kPi, 0.f};
    camera_.vertical_fov_degrees = fov;
    camera_.near_plane = 0.1f;
    camera_.far_plane = 5000.f;
}

float GameCamera::pull_in_distance(const exgine::Vec3& from, exgine::Vec3 to,
                                   const std::vector<CitySolid>* solids) const {
    if (!solids || solids->empty()) return distance_current_;
    exgine::Vec3 best_to = to;
    float best_frac = 1.f;
    for (int step = 12; step >= 1; --step) {
        const float t = static_cast<float>(step) / 12.f;
        const exgine::Vec3 p{from.x + (to.x - from.x) * t,
                             from.y + (to.y - from.y) * t,
                             from.z + (to.z - from.z) * t};
        bool hit = false;
        for (const auto& s : *solids) {
            if (p.x > s.min_x - 0.35f && p.x < s.max_x + 0.35f &&
                p.z > s.min_z - 0.35f && p.z < s.max_z + 0.35f &&
                p.y < s.top + 0.35f) {
                hit = true;
                break;
            }
        }
        if (!hit) {
            // first unobstructed sample walking from the target outward
            best_frac = t;
            best_to = p;
            break;
        }
    }
    if (best_frac >= 1.f) return distance_current_;
    (void)best_to;
    const float full = std::sqrt((to.x - from.x) * (to.x - from.x) +
                                 (to.z - from.z) * (to.z - from.z));
    const float clipped = full * best_frac;
    return std::max(1.4f, std::min(distance_current_, clipped));
}

void GameCamera::update(float dt, const exgine::Vec3& target, const exgine::Vec3& velocity,
                        bool in_vehicle, float vehicle_yaw, float speed_frac, bool sprinting,
                        exgine::Runtime& runtime, const std::vector<CitySolid>* solids) {
    if (!initialized_) reset(target);
    manual_timer_ += dt;

    // --- yaw behavior -------------------------------------------------------
    if (in_vehicle && manual_timer_ > 1.1f) {
        // Ease behind the vehicle heading; drag overrides temporarily.
        const float diff = wrap_angle(vehicle_yaw - yaw_);
        yaw_ = wrap_angle(yaw_ + diff * (1.f - std::exp(-4.2f * dt)));
        pitch_ = approach(pitch_, -0.20f + 0.12f * speed_frac, 3.0f, dt);
    } else if (!in_vehicle && manual_timer_ > 1.4f) {
        const float speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
        if (speed > 0.6f) {
            const float move_yaw = std::atan2(velocity.x, velocity.z);
            const float diff = wrap_angle(move_yaw - yaw_);
            yaw_ = wrap_angle(yaw_ + diff * (1.f - std::exp(-1.6f * dt)));
        }
    }

    // --- follow target with velocity look-ahead ------------------------------
    const float lead = in_vehicle ? 0.55f : 0.28f;
    exgine::Vec3 ahead{velocity.x * lead, 0.f, velocity.z * lead};
    const float lead_len = std::sqrt(ahead.x * ahead.x + ahead.z * ahead.z);
    const float lead_max = in_vehicle ? 7.f : 2.2f;
    if (lead_len > lead_max) {
        ahead.x *= lead_max / lead_len;
        ahead.z *= lead_max / lead_len;
    }
    look_ahead_ = {approach(look_ahead_.x, ahead.x, 5.f, dt),
                   0.f,
                   approach(look_ahead_.z, ahead.z, 5.f, dt)};

    const exgine::Vec3 pivot{target.x + look_ahead_.x, target.y, target.z + look_ahead_.z};

    // --- distance & fov ------------------------------------------------------
    const float dist_target = in_vehicle ? 7.6f + 2.6f * speed_frac
                                         : (sprinting ? 5.4f : 4.8f);
    distance_current_ = approach(distance_current_, dist_target, 3.5f, dt);
    const float fov_target = in_vehicle ? 58.f + 16.f * speed_frac
                                        : 58.f + (sprinting ? 6.f : 0.f);
    fov_ = approach(fov_, fov_target, 2.6f, dt);

    // --- desired position ----------------------------------------------------
    const float height = in_vehicle ? 2.5f : 1.85f;
    const float cy = std::cos(yaw_), sy = std::sin(yaw_);
    const float cp = std::cos(pitch_), sp = std::sin(pitch_);
    exgine::Vec3 desired{pivot.x - sy * cp * distance_current_,
                         pivot.y - sp * distance_current_ + height,
                         pivot.z - cy * cp * distance_current_};

    // Building pull-in: shorten the boom if a solid blocks the view.
    if (solids) {
        const float clipped = pull_in_distance(pivot, desired, solids);
        if (clipped < distance_current_ - 0.05f) {
            const float frac = distance_current_ > 0.01f ? clipped / distance_current_ : 1.f;
            desired.x = pivot.x + (desired.x - pivot.x) * frac;
            desired.y = pivot.y + (desired.y - pivot.y) * frac;
            desired.z = pivot.z + (desired.z - pivot.z) * frac;
            distance_current_ = clipped;
        }
    }

    // Ground clamp (city plate at y=0)
    desired.y = std::max(desired.y, 0.55f);

    // --- critically damped smoothing (frame-rate independent) ----------------
    const float omega = in_vehicle ? 6.8f : 8.5f;
    current_pos_.x = approach(current_pos_.x, desired.x, omega, dt);
    current_pos_.y = approach(current_pos_.y, desired.y, omega + 2.f, dt);
    current_pos_.z = approach(current_pos_.z, desired.z, omega, dt);

    // --- shake ---------------------------------------------------------------
    shake_time_ += dt;
    shake_ *= std::exp(-5.5f * dt);
    exgine::Vec3 shaken = current_pos_;
    if (shake_ > 0.002f) {
        const float t = shake_time_;
        shaken.x += std::sin(t * 47.f) * shake_ * 0.35f;
        shaken.y += std::sin(t * 53.f + 1.7f) * shake_ * 0.30f;
        shaken.z += std::cos(t * 41.f + 0.9f) * shake_ * 0.35f;
    }

    apply(shaken, fov_);
    (void)runtime.set_main_camera(camera_);
    mode_ = Mode::Gameplay;
}

void GameCamera::begin_cinematic(const exgine::Vec3& around, float radius, float height) {
    mode_ = Mode::Cinematic;
    cine_center_ = around;
    cine_angle_ = 0.9f;
    cine_radius_ = radius;
    cine_height_ = height;
    distance_current_ = 8.f;
}

void GameCamera::update_cinematic(float dt) {
    cine_angle_ += dt * 0.055f;
    const float radius = cine_radius_;
    const float height = cine_height_ + std::sin(cine_angle_ * 0.7f) * 10.f;
    const exgine::Vec3 pos{cine_center_.x + std::cos(cine_angle_) * radius,
                           height,
                           cine_center_.z + std::sin(cine_angle_) * radius};
    // Look at the plaza from above.
    const exgine::Vec3 to{cine_center_.x - pos.x, -height + 14.f, cine_center_.z - pos.z};
    const float len = std::sqrt(to.x * to.x + to.y * to.y + to.z * to.z);
    pitch_ = std::asin(std::clamp(-to.y / std::max(len, 0.001f), -1.f, 1.f));
    yaw_ = std::atan2(to.x, to.z);

    fov_ = approach(fov_, 52.f, 1.5f, dt);
    current_pos_ = pos;
    apply(pos, fov_);
}

void GameCamera::begin_fly_in(const exgine::Vec3& player) {
    fly_from_pos_ = current_pos_;
    fly_from_yaw_ = yaw_;
    fly_from_pitch_ = pitch_;
    // Goal pose: standard gameplay boom behind the player.
    fly_goal_yaw_ = wrap_angle(yaw_);
    fly_goal_pitch_ = -0.24f;
    const float cy = std::cos(fly_goal_yaw_), sy = std::sin(fly_goal_yaw_);
    const float cp = std::cos(fly_goal_pitch_), sp = std::sin(fly_goal_pitch_);
    fly_goal_pos_ = {player.x - sy * cp * distance_,
                     player.y - sp * distance_ + 1.85f,
                     player.z - cy * cp * distance_};
    fly_goal_pos_.y = std::max(fly_goal_pos_.y, 0.55f);
    fly_t_ = 0.f;
    mode_ = Mode::FlyIn;
}

bool GameCamera::fly_in(const exgine::Vec3& player, float dt) {
    fly_t_ += dt;
    const float t = ease_in_out(fly_t_ / 2.6f);
    (void)player;
    if (t >= 1.f) {
        yaw_ = fly_goal_yaw_;
        pitch_ = fly_goal_pitch_;
        current_pos_ = fly_goal_pos_;
        apply(current_pos_, fov_);
        mode_ = Mode::Gameplay;
        return false;
    }
    const exgine::Vec3 pos{fly_from_pos_.x + (fly_goal_pos_.x - fly_from_pos_.x) * t,
                           fly_from_pos_.y + (fly_goal_pos_.y - fly_from_pos_.y) * t,
                           fly_from_pos_.z + (fly_goal_pos_.z - fly_from_pos_.z) * t};
    yaw_ = fly_from_yaw_ + wrap_angle(fly_goal_yaw_ - fly_from_yaw_) * t;
    pitch_ = fly_from_pitch_ + (fly_goal_pitch_ - fly_from_pitch_) * t;
    current_pos_ = pos;
    apply(pos, fov_);
    return true;
}

} // namespace exworld
