#pragma once

#include "exgine/runtime.hpp"

namespace exworld {

class GameCamera {
public:
    void reset(const exgine::Vec3& target);

    void set_look(float yaw, float pitch) noexcept;
    void set_distance(float d) noexcept { distance_ = d; }

    void update(float dt, const exgine::Vec3& target, bool in_vehicle,
                exgine::Runtime& runtime);

    [[nodiscard]] const exgine::Camera& camera() const noexcept { return camera_; }
    [[nodiscard]] float yaw() const noexcept { return yaw_; }
    [[nodiscard]] float pitch() const noexcept { return pitch_; }

private:
    exgine::Camera camera_{};
    float yaw_ = 0.55f;
    float pitch_ = -0.28f;
    float distance_ = 10.f;
    float vehicle_distance_ = 16.f;
    float smooth_ = 9.f;
    exgine::Vec3 current_pos_{};
    bool initialized_ = false;
};

} // namespace exworld
