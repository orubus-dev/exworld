#include "exworld/vehicle.hpp"

#include <algorithm>
#include <cmath>

namespace exworld {

void VehicleController::clear() { vehicles_.clear(); }

void VehicleController::register_vehicle(exgine::Runtime& /*runtime*/, exgine::EntityId id,
                                         std::string name, VehicleSeat seat) {
    vehicles_.push_back({id, std::move(name), seat});
}

exgine::EntityId VehicleController::nearest_vehicle(const exgine::Vec3& pos, float max_distance,
                                                    exgine::Runtime& runtime) const {
    exgine::EntityId best = exgine::invalid_entity;
    float best_d = max_distance;

    for (const auto& v : vehicles_) {
        auto* e = runtime.state().entities.get(v.id);
        if (!e) continue;
        const float dx = e->transform.x - pos.x;
        const float dy = e->transform.y - pos.y;
        const float dz = e->transform.z - pos.z;
        const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d < best_d && d <= v.seat.interact_radius) {
            best_d = d;
            best = v.id;
        }
    }
    return best;
}

void VehicleController::update_driven(exgine::EntityId vehicle, float throttle, float steer,
                                      float brake, float dt, exgine::Runtime& runtime) {
    auto* e = runtime.state().entities.get(vehicle);
    if (!e) return;

    // Full VehicleDynamicsController possession when available
    auto& dyn_map = runtime.vehicle_dynamics();
    auto it = dyn_map.find(vehicle);
    if (it != dyn_map.end() && it->second) {
        exgine::VehicleInput vin;
        vin.throttle = std::clamp(throttle, 0.f, 1.f);
        vin.brake = std::clamp(brake, 0.f, 1.f);
        vin.steering = std::clamp(steer, -1.f, 1.f);
        vin.handbrake = false;

        (void)it->second->possess(true);
        (void)it->second->set_input(vin);
        (void)it->second->update(dt);

        // Sync entity transform from physics body if possible
        if (auto* phys = runtime.physics()) {
            const auto body = it->second->state().body;
            if (body) {
                if (auto t = phys->body_transform(body)) {
                    e->transform.x = t->position.x;
                    e->transform.y = t->position.y;
                    e->transform.z = t->position.z;
                }
            }
        }
    } else {
        // Kinematic fallback
        const float speed = (throttle - brake * 0.8f) * 16.0f;
        const float yaw = steer * 1.2f;
        e->transform.x += std::sin(yaw) * speed * dt;
        e->transform.z += std::cos(yaw) * speed * dt;
    }

    if (e->scene_node) {
        (void)runtime.scene().set_local_transform(
            e->scene_node,
            {{e->transform.x, e->transform.y, e->transform.z}, {}, {1, 1, 1}});
    }
}

const VehicleSeat* VehicleController::seat(exgine::EntityId id) const {
    for (const auto& v : vehicles_)
        if (v.id == id) return &v.seat;
    return nullptr;
}

} // namespace exworld
