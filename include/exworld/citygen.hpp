#pragma once

// EXWORLD procedural city generator.
// Builds a full GTA-scale street grid on device: road network with markings
// and crosswalks, districted blocks (downtown towers, commercial, residential,
// parks, plaza), streetlights, trees, billboards, parked + moving traffic,
// pedestrians and police. Merged-mesh ground layer keeps draw calls low.
// Generation is deterministic (seeded per block) and time-sliced via step()
// so the loading screen can show real progress.

#include "exgine/runtime.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace exworld {

struct CitySolid {
    float min_x = 0, min_z = 0, max_x = 0, max_z = 0, top = 0;
};

struct CityVehicleUnit {
    exgine::EntityId id = exgine::invalid_entity;
    float x = 0, z = 0;
    float dir_x = 1, dir_z = 0;
    float speed = 10.f;
    float yaw = 0.f;
    bool traffic = false;
};

struct CityWalkerUnit {
    exgine::EntityId id = exgine::invalid_entity;
    float cx = 0, cz = 0;
    float half = 24.f;
    float t = 0.f;
    float speed = 1.4f;
    float yaw = 0.f;
};

struct CityDoorRef {
    exgine::EntityId building = exgine::invalid_entity;
    float x = 0, z = 0;
};

class CityGenerator {
public:
    void begin(exgine::Runtime& runtime, std::uint64_t seed);
    float step(); // one unit of work per call; returns progress 0..1
    [[nodiscard]] bool done() const noexcept { return phase_ == Phase::Done; }
    [[nodiscard]] float progress() const noexcept {
        return std::clamp(units_done_ / units_total_, 0.f, 1.f);
    }

    [[nodiscard]] const std::vector<CitySolid>& solids() const noexcept { return solids_; }
    [[nodiscard]] const std::vector<CityDoorRef>& doors() const noexcept { return doors_; }
    [[nodiscard]] std::vector<CityVehicleUnit>& vehicles() noexcept { return vehicles_; }
    [[nodiscard]] std::vector<CityWalkerUnit>& walkers() noexcept { return walkers_; }

    static constexpr int kGrid = 11;
    static constexpr float kPitch = 64.f;
    static constexpr float kRoadW = 12.f;
    static constexpr float kBlock = kPitch - kRoadW;
    static constexpr float kHalf = kGrid * kPitch * 0.5f;

    [[nodiscard]] static float road_line(int k) {
        return (static_cast<float>(k) - static_cast<float>(kGrid) * 0.5f) * kPitch;
    }
    [[nodiscard]] static bool is_avenue(int k) { return k == 3 || k == 8; }

    enum class District { Plaza, Downtown, Commercial, Residential, Park, Parking, Outskirts };
    [[nodiscard]] District district(int bi, int bj) const;
    [[nodiscard]] static const char* district_name(District d);

private:
    enum class Phase { Ground, Blocks, Props, Extras, Done };

    void build_ground();
    void build_block(int bi, int bj);
    void build_props();
    void build_extras();
    void flush_accumulators();

    exgine::Runtime* rt_ = nullptr;
    std::uint64_t seed_ = 7777;
    std::uint64_t rng_ = 0;

    std::uint64_t next_u64();
    float next_float();
    float next_range(float a, float b);
    void seed_for(int bi, int bj);

    // merged geometry accumulators (material -> mesh), flushed as few entities
    std::vector<std::pair<std::string, exgine::Mesh>> accs_;
    exgine::Mesh& acc(std::string_view material);
    void acc_box(std::string_view material, float cx, float cy, float cz,
                 float sx, float sy, float sz);
    void acc_quad_y(std::string_view material, float cx, float y, float cz,
                    float sx, float sz);
    void acc_cyl(std::string_view material, float cx, float cz, float r,
                 float y0, float y1, int seg);

    exgine::EntityId spawn_entity(exgine::NodeKind kind, const char* name,
                                  float x, float y, float z, float yaw = 0.f);
    exgine::EntityId spawn_full_building(float x, float z, float w, float d,
                                         int floors, std::uint64_t bseed,
                                         const char* wall);
    void spawn_shell_block(int bi, int bj, District dist);
    exgine::EntityId spawn_vehicle_entity(float x, float z, float yaw, int type_pick,
                                          const char* name);
    exgine::EntityId spawn_character_entity(float x, float z, bool cop, const char* name);

    Phase phase_ = Phase::Ground;
    int block_index_ = 0;
    int extras_index_ = 0;
    float units_total_ = 1.f;
    float units_done_ = 0.f;

    std::vector<CitySolid> solids_;
    std::vector<CityDoorRef> doors_;
    std::vector<CityVehicleUnit> vehicles_;
    std::vector<CityWalkerUnit> walkers_;
};

} // namespace exworld
