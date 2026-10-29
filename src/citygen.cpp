#include "exworld/citygen.hpp"

#include "exgine/building.hpp"
#include "exgine/character.hpp"
#include "exgine/geometry.hpp"
#include "exgine/material.hpp"
#include "exgine/vehicle.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace exworld {
namespace {

constexpr float kPi = 3.14159265358979323846f;

std::uint64_t mix64(std::uint64_t v) {
    v += 0x9e3779b97f4a7c15ull;
    v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ull;
    v = (v ^ (v >> 27)) * 0x94d049bb133111ebull;
    return v ^ (v >> 31);
}

struct MaterialDef {
    const char* name;
    float r, g, b;
    float rough, metal;
    float er, eg, eb;
};

constexpr MaterialDef kMaterials[] = {
    {"road_asphalt", 0.085f, 0.090f, 0.100f, 0.95f, 0.f, 0, 0, 0},
    {"mark_white", 0.85f, 0.85f, 0.82f, 0.8f, 0.f, 0, 0, 0},
    {"mark_yellow", 0.90f, 0.74f, 0.15f, 0.8f, 0.f, 0, 0, 0},
    {"sidewalk_concrete", 0.55f, 0.55f, 0.565f, 0.9f, 0.f, 0, 0, 0},
    {"lot_concrete", 0.40f, 0.405f, 0.42f, 0.92f, 0.f, 0, 0, 0},
    {"stone_plaza", 0.62f, 0.60f, 0.565f, 0.85f, 0.f, 0, 0, 0},
    {"park_grass", 0.16f, 0.40f, 0.135f, 0.95f, 0.f, 0, 0, 0},
    {"tree_bark", 0.30f, 0.22f, 0.14f, 0.9f, 0.f, 0, 0, 0},
    {"tree_leaves", 0.13f, 0.37f, 0.12f, 0.9f, 0.f, 0, 0, 0},
    {"steel_dark", 0.18f, 0.19f, 0.21f, 0.5f, 0.6f, 0, 0, 0},
    {"lamp_head", 0.90f, 0.85f, 0.70f, 0.6f, 0.f, 0.95f, 0.82f, 0.55f},
    {"wood_bench", 0.35f, 0.24f, 0.13f, 0.85f, 0.f, 0, 0, 0},
    {"brick_red", 0.47f, 0.20f, 0.15f, 0.85f, 0.f, 0, 0, 0},
    {"brick_brown", 0.40f, 0.28f, 0.18f, 0.85f, 0.f, 0, 0, 0},
    {"concrete_light", 0.66f, 0.65f, 0.62f, 0.85f, 0.f, 0, 0, 0},
    {"concrete_dark", 0.33f, 0.34f, 0.36f, 0.85f, 0.f, 0, 0, 0},
    {"glass_tower", 0.25f, 0.42f, 0.55f, 0.25f, 0.4f, 0, 0, 0},
    {"glass_window", 0.50f, 0.68f, 0.80f, 0.3f, 0.2f, 0.10f, 0.14f, 0.18f},
    {"roof_dark", 0.15f, 0.15f, 0.16f, 0.9f, 0.f, 0, 0, 0},
    {"door_wood", 0.30f, 0.20f, 0.11f, 0.85f, 0.f, 0, 0, 0},
    {"billboard_neon", 1.0f, 0.25f, 0.80f, 0.6f, 0.f, 0.95f, 0.15f, 0.72f},
    {"billboard_cyan", 0.20f, 0.90f, 1.0f, 0.6f, 0.f, 0.10f, 0.72f, 0.85f},
    {"fountain_water", 0.15f, 0.45f, 0.65f, 0.15f, 0.f, 0.02f, 0.10f, 0.16f},
    {"house_cream", 0.82f, 0.78f, 0.68f, 0.85f, 0.f, 0, 0, 0},
    {"house_sage", 0.55f, 0.66f, 0.52f, 0.85f, 0.f, 0, 0, 0},
    {"house_terracotta", 0.75f, 0.45f, 0.32f, 0.85f, 0.f, 0, 0, 0},
    {"house_sky", 0.58f, 0.70f, 0.80f, 0.85f, 0.f, 0, 0, 0},
};

void push_quad_y(exgine::Mesh& mesh, float cx, float y, float cz, float hx, float hz) {
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    const exgine::Vec3 n{0, 1, 0};
    mesh.vertices.push_back({{cx - hx, y, cz - hz}, n, {0, 0}});
    mesh.vertices.push_back({{cx + hx, y, cz - hz}, n, {1, 0}});
    mesh.vertices.push_back({{cx + hx, y, cz + hz}, n, {1, 1}});
    mesh.vertices.push_back({{cx - hx, y, cz + hz}, n, {0, 1}});
    mesh.indices.insert(mesh.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
}

void push_box(exgine::Mesh& mesh, float cx, float cy, float cz, float hx, float hy, float hz) {
    const float ext[3] = {hx, hy, hz};
    const float cen[3] = {cx, cy, cz};
    for (int axis = 0; axis < 3; ++axis) {
        for (int side = 0; side < 2; ++side) {
            const float sgn = side == 0 ? -1.f : 1.f;
            exgine::Vec3 nv;
            float* n = &nv.x;
            n[axis] = sgn;
            const int u_axis = (axis + 1) % 3;
            const int v_axis = (axis + 2) % 3;
            const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
            const float su[4] = {-1.f, 1.f, 1.f, -1.f};
            const float sv[4] = {-1.f, -1.f, 1.f, 1.f};
            for (int i = 0; i < 4; ++i) {
                float p[3] = {cen[0], cen[1], cen[2]};
                p[axis] += sgn * ext[axis];
                p[u_axis] += su[i] * ext[u_axis];
                p[v_axis] += sv[i] * ext[v_axis];
                mesh.vertices.push_back({{p[0], p[1], p[2]}, nv, {0.5f, 0.5f}});
            }
            mesh.indices.insert(mesh.indices.end(),
                                {base, base + 1, base + 2, base, base + 2, base + 3});
        }
    }
}

void push_cyl(exgine::Mesh& mesh, float cx, float cz, float r, float y0, float y1, int seg) {
    seg = std::clamp(seg, 4, 24);
    for (int s = 0; s < seg; ++s) {
        const float a0 = 2.f * kPi * static_cast<float>(s) / static_cast<float>(seg);
        const float a1 = 2.f * kPi * static_cast<float>(s + 1) / static_cast<float>(seg);
        const float c0x = std::cos(a0) * r, c0z = std::sin(a0) * r;
        const float c1x = std::cos(a1) * r, c1z = std::sin(a1) * r;
        const exgine::Vec3 n0{std::cos(a0), 0, std::sin(a0)};
        const exgine::Vec3 n1{std::cos(a1), 0, std::sin(a1)};
        const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({{cx + c0x, y0, cz + c0z}, n0, {0, 0}});
        mesh.vertices.push_back({{cx + c1x, y0, cz + c1z}, n1, {1, 0}});
        mesh.vertices.push_back({{cx + c1x, y1, cz + c1z}, n1, {1, 1}});
        mesh.vertices.push_back({{cx + c0x, y1, cz + c0z}, n0, {0, 1}});
        mesh.indices.insert(mesh.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
    }
    // cap
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    const exgine::Vec3 up{0, 1, 0};
    mesh.vertices.push_back({{cx, y1, cz}, up, {0.5f, 0.5f}});
    for (int s = 0; s <= seg; ++s) {
        const float a = 2.f * kPi * static_cast<float>(s) / static_cast<float>(seg);
        mesh.vertices.push_back({{cx + std::cos(a) * r, y1, cz + std::sin(a) * r}, up, {0.5f, 0.5f}});
    }
    for (int s = 1; s <= seg; ++s) {
        const auto b = static_cast<std::uint32_t>(base);
        mesh.indices.insert(mesh.indices.end(), {b, b + static_cast<std::uint32_t>(s) + 1u,
                                                 b + static_cast<std::uint32_t>(s)});
    }
}

} // namespace

std::uint64_t CityGenerator::next_u64() {
    rng_ = mix64(rng_);
    return rng_;
}

float CityGenerator::next_float() {
    return static_cast<float>(next_u64() >> 40) / static_cast<float>(1 << 24);
}

float CityGenerator::next_range(float a, float b) {
    return a + (b - a) * next_float();
}

void CityGenerator::seed_for(int bi, int bj) {
    rng_ = mix64(seed_ ^ (0x51'7c'c1'b7ull * static_cast<std::uint64_t>(bi + 1)) ^
                 (0x25'45'49'13ull * static_cast<std::uint64_t>(bj + 7)));
}

exgine::Mesh& CityGenerator::acc(std::string_view material) {
    for (auto& [name, mesh] : accs_)
        if (name == material) return mesh;
    accs_.emplace_back(std::string(material), exgine::Mesh{});
    return accs_.back().second;
}

void CityGenerator::acc_quad_y(std::string_view material, float cx, float y, float cz,
                               float sx, float sz) {
    push_quad_y(acc(material), cx, y, cz, sx * 0.5f, sz * 0.5f);
}

void CityGenerator::acc_box(std::string_view material, float cx, float cy, float cz,
                            float sx, float sy, float sz) {
    push_box(acc(material), cx, cy, cz, sx * 0.5f, sy * 0.5f, sz * 0.5f);
}

void CityGenerator::acc_cyl(std::string_view material, float cx, float cz, float r,
                            float y0, float y1, int seg) {
    push_cyl(acc(material), cx, cz, r, y0, y1, seg);
}

CityGenerator::District CityGenerator::district(int bi, int bj) const {
    const int c = kGrid / 2;
    const int dc = std::max(std::abs(bi - c), std::abs(bj - c));
    if (bi == c && bj == c) return District::Plaza;
    if (dc == 1) return District::Downtown;
    if (dc == 2 || dc == 3) return District::Commercial;
    if (dc == 4) return District::Residential;
    // Outskirts: deterministic mix of park / parking / houses / empty
    const auto h = mix64(seed_ ^ (0xabcdefull * static_cast<std::uint64_t>(bi * 31 + bj))) % 100;
    if (h < 38) return District::Residential;
    if (h < 60) return District::Park;
    if (h < 75) return District::Parking;
    return District::Outskirts;
}

const char* CityGenerator::district_name(District d) {
    switch (d) {
    case District::Plaza: return "CENTRAL PLAZA";
    case District::Downtown: return "DOWNTOWN";
    case District::Commercial: return "MARKET DISTRICT";
    case District::Residential: return "RESIDENTIAL";
    case District::Park: return "GREEN BELT";
    case District::Parking: return "OUTSKIRTS";
    case District::Outskirts: return "OUTSKIRTS";
    }
    return "EXWORLD CITY";
}

exgine::EntityId CityGenerator::spawn_entity(exgine::NodeKind kind, const char* name,
                                             float x, float y, float z, float yaw) {
    const auto id = rt_->state().entities.create(kind, name);
    if (!id) return exgine::invalid_entity;
    auto* e = rt_->state().entities.get(id);
    if (!e) return exgine::invalid_entity;
    e->scene_node = rt_->scene().create(rt_->scene().root());
    if (!e->scene_node) {
        rt_->state().entities.destroy(id);
        return exgine::invalid_entity;
    }
    e->transform.x = x;
    e->transform.y = y;
    e->transform.z = z;
    exgine::SceneTransform t;
    t.position = {x, y, z};
    t.rotation = {0.f, yaw, 0.f};
    t.scale = {1, 1, 1};
    (void)rt_->scene().set_local_transform(e->scene_node, t);
    return id;
}

void CityGenerator::begin(exgine::Runtime& runtime, std::uint64_t seed) {
    rt_ = &runtime;
    seed_ = seed;
    phase_ = Phase::Ground;
    block_index_ = 0;
    extras_index_ = 0;
    units_done_ = 0.f;
    units_total_ = 1.f + static_cast<float>(kGrid * kGrid) + 3.f;
    accs_.clear();
    solids_.clear();
    doors_.clear();
    vehicles_.clear();
    walkers_.clear();
    rng_ = mix64(seed_);

    for (const auto& m : kMaterials) {
        exgine::Material mat;
        mat.name = m.name;
        mat.base_color = {m.r, m.g, m.b};
        mat.roughness = m.rough;
        mat.metallic = m.metal;
        if (m.er > 0.f) mat.emission = {m.er, m.eg, m.eb};
        (void)rt_->define_material(mat);
    }
}

float CityGenerator::step() {
    if (phase_ == Phase::Done) return 1.f;
    switch (phase_) {
    case Phase::Ground:
        build_ground();
        phase_ = Phase::Blocks;
        break;
    case Phase::Blocks:
        if (block_index_ < kGrid * kGrid) {
            const int bi = block_index_ % kGrid;
            const int bj = block_index_ / kGrid;
            seed_for(bi, bj);
            build_block(bi, bj);
            ++block_index_;
        } else {
            phase_ = Phase::Props;
        }
        break;
    case Phase::Props:
        if (extras_index_ == 0) build_props();
        else if (extras_index_ == 1) flush_accumulators();
        else if (extras_index_ >= 2) {
            build_extras();
            phase_ = Phase::Done;
            accs_.clear();
        }
        ++extras_index_;
        break;
    default:
        break;
    }
    units_done_ += 1.f;
    return std::clamp(units_done_ / units_total_, 0.f, 1.f);
}

void CityGenerator::build_ground() {
    const float ext = kHalf + 30.f;

    // X roads (continuous), Z roads (segmented between crossings)
    for (int k = 0; k <= kGrid; ++k) {
        const float line = road_line(k);
        acc_quad_y("road_asphalt", 0.f, 0.05f, line, ext * 2.f, kRoadW);
        for (int j = -1; j <= kGrid; ++j) {
            const float z0 = j < 0 ? -ext : road_line(j) + kRoadW * 0.5f;
            const float z1 = j >= kGrid ? ext : road_line(j + 1) - kRoadW * 0.5f;
            if (z1 <= z0) continue;
            acc_quad_y("road_asphalt", line, 0.05f, (z0 + z1) * 0.5f, kRoadW, z1 - z0);
        }
    }

    // Center dashes
    for (int k = 0; k <= kGrid; ++k) {
        const float line = road_line(k);
        const char* mat = is_avenue(k) ? "mark_yellow" : "mark_white";
        for (float p = -ext; p < ext; p += 9.f)
            acc_quad_y(mat, p + 1.8f, 0.075f, line, 3.6f, 0.28f);
        for (int j = -1; j <= kGrid; ++j) {
            const float z0 = j < 0 ? -ext : road_line(j) + kRoadW * 0.5f;
            const float z1 = j >= kGrid ? ext : road_line(j + 1) - kRoadW * 0.5f;
            for (float z = z0 + 3.f; z < z1 - 2.f; z += 9.f)
                acc_quad_y(mat, line, 0.075f, z + 1.8f, 0.28f, 3.6f);
        }
    }

    // Crosswalks near the center (roads k = 3..8)
    for (int kx = 3; kx <= 8; ++kx) {
        for (int kz = 3; kz <= 8; ++kz) {
            const float ix = road_line(kx), iz = road_line(kz);
            for (int i = -2; i <= 2; ++i) {
                acc_quad_y("mark_white", ix + static_cast<float>(i) * 1.8f, 0.075f, iz + 4.9f, 1.1f, 2.0f);
                acc_quad_y("mark_white", ix + static_cast<float>(i) * 1.8f, 0.075f, iz - 4.9f, 1.1f, 2.0f);
                acc_quad_y("mark_white", ix + 4.9f, 0.075f, iz + static_cast<float>(i) * 1.8f, 2.0f, 1.1f);
                acc_quad_y("mark_white", ix - 4.9f, 0.075f, iz + static_cast<float>(i) * 1.8f, 2.0f, 1.1f);
            }
        }
    }

    // Sidewalk rings around every block
    for (int bi = 0; bi < kGrid; ++bi) {
        for (int bj = 0; bj < kGrid; ++bj) {
            const float bx = road_line(bi) + kPitch * 0.5f;
            const float bz = road_line(bj) + kPitch * 0.5f;
            const float hb = kBlock * 0.5f;
            const float rw = 3.2f;
            acc_box("sidewalk_concrete", bx, 0.08f, bz - hb + rw * 0.5f, kBlock, 0.16f, rw);
            acc_box("sidewalk_concrete", bx, 0.08f, bz + hb - rw * 0.5f, kBlock, 0.16f, rw);
            acc_box("sidewalk_concrete", bx - hb + rw * 0.5f, 0.08f, bz, rw, 0.16f, kBlock - rw * 2.f);
            acc_box("sidewalk_concrete", bx + hb - rw * 0.5f, 0.08f, bz, rw, 0.16f, kBlock - rw * 2.f);
        }
    }
}

void CityGenerator::spawn_shell_block(int bi, int bj, District dist) {
    const float bx = road_line(bi) + kPitch * 0.5f;
    const float bz = road_line(bj) + kPitch * 0.5f;
    const int count = dist == District::Residential ? 4 + static_cast<int>(next_float() * 3.f)
                                                    : 2 + static_cast<int>(next_float() * 2.f);
    static const char* paints[] = {"house_cream", "house_sage", "house_terracotta", "house_sky",
                                   "brick_red", "brick_brown", "concrete_light"};
    for (int i = 0; i < count; ++i) {
        const float w = next_range(9.f, 13.f);
        const float d = next_range(8.f, 11.f);
        const float h = dist == District::Commercial ? next_range(5.5f, 8.f) : next_range(3.8f, 5.4f);
        const float ox = next_range(-19.f, 19.f);
        const float oz = next_range(-17.f, 17.f);
        const float x = bx + ox, z = bz + oz;
        const char* wall = paints[next_u64() % 7];

        acc_box(wall, x, 0.05f + h * 0.5f, z, w, h, d);
        acc_box("roof_dark", x, 0.05f + h + 0.18f, z, w + 0.7f, 0.36f, d + 0.7f);
        acc_box("door_wood", x + next_range(-2.f, 2.f), 1.25f, z + d * 0.5f + 0.06f, 1.1f, 2.3f, 0.12f);
        // window strips on front/back
        const int strips = 1 + static_cast<int>(h / 2.4f);
        for (int s = 0; s < strips; ++s) {
            const float wy = 1.7f + static_cast<float>(s) * 2.5f;
            if (wy > h - 0.4f) break;
            acc_box("glass_window", x, wy, z + d * 0.5f + 0.05f, w * 0.62f, 1.05f, 0.1f);
            acc_box("glass_window", x, wy, z - d * 0.5f - 0.05f, w * 0.62f, 1.05f, 0.1f);
        }

        const auto id = spawn_entity(exgine::NodeKind::Building, "ShellBuilding", x, 0.05f, z);
        if (id) {
            solids_.push_back({x - w * 0.5f, z - d * 0.5f, x + w * 0.5f, z + d * 0.5f, 0.05f + h});
            doors_.push_back({id, x, z + d * 0.5f + 1.1f});
        }
        if (next_float() < 0.45f && vehicles_.size() < 46 && dist != District::Commercial) {
            const float pyaw = static_cast<float>(next_u64() % 4) * kPi * 0.5f + next_range(-0.12f, 0.12f);
            const float pz = bz + (next_float() < 0.5f ? -28.4f : 28.4f);
            const float px = bx + next_range(-22.f, 22.f);
            if (const auto vid = spawn_vehicle_entity(px, pz, pyaw,
                                                      static_cast<int>(next_u64() % 4), "ParkedCar"))
                vehicles_.push_back({vid, px, pz, 0, 0, pyaw, false});
        }
    }
    if (dist == District::Residential) {
        for (int t = 0; t < 3; ++t)
            acc_cyl("tree_bark", bx + next_range(-20.f, 20.f), bz + next_range(-20.f, 20.f),
                    0.2f, 0.1f, 2.4f, 5);
        for (int t = 0; t < 3; ++t)
            acc_cyl("tree_leaves", bx + next_range(-20.f, 20.f), bz + next_range(-20.f, 20.f),
                    next_range(1.3f, 1.9f), 2.1f, 4.4f, 7);
    }
}

exgine::EntityId CityGenerator::spawn_full_building(float x, float z, float w, float d,
                                                    int floors, std::uint64_t bseed,
                                                    const char* wall) {
    const auto id = spawn_entity(exgine::NodeKind::Building, "CityBuilding", x, 0.05f, z);
    if (!id) return exgine::invalid_entity;
    exgine::BuildingConfig cfg;
    cfg.width = w;
    cfg.depth = d;
    cfg.floors = static_cast<std::uint32_t>(floors);
    cfg.floor_height = 3.15f;
    cfg.rooms_per_floor = 2;
    cfg.windows_per_wall = 1;
    cfg.furniture_density = 0;
    cfg.seed = bseed;
    cfg.wall_material = wall;
    cfg.floor_material = "concrete_light";
    cfg.glass_material = "glass_window";
    cfg.door_material = "door_wood";
    cfg.furniture_material = "wood_bench";
    if (!rt_->generate_building(id, cfg)) {
        rt_->state().entities.destroy(id);
        return exgine::invalid_entity;
    }
    solids_.push_back({x - w * 0.5f, z - d * 0.5f, x + w * 0.5f, z + d * 0.5f,
                       0.05f + static_cast<float>(floors) * 3.15f});
    doors_.push_back({id, x, z + d * 0.5f + 1.2f});
    return id;
}

exgine::EntityId CityGenerator::spawn_vehicle_entity(float x, float z, float yaw,
                                                     int type_pick, const char* name) {
    const auto id = spawn_entity(exgine::NodeKind::Vehicle, name, x, 0.06f, z, yaw);
    if (!id) return exgine::invalid_entity;
    exgine::VehicleType type = exgine::VehicleType::Car;
    if (type_pick == 1) type = exgine::VehicleType::SUV;
    else if (type_pick == 2) type = exgine::VehicleType::Pickup;
    else if (type_pick == 3) type = exgine::VehicleType::SportsCar;
    auto cfg = exgine::make_vehicle_config(type);
    cfg.seed = next_u64();
    if (!rt_->generate_vehicle(id, cfg)) {
        rt_->state().entities.destroy(id);
        return exgine::invalid_entity;
    }
    return id;
}

exgine::EntityId CityGenerator::spawn_character_entity(float x, float z, bool cop,
                                                       const char* name) {
    const auto id = spawn_entity(exgine::NodeKind::NPC, name, x, 0.05f, z);
    if (!id) return exgine::invalid_entity;
    exgine::CharacterDefinition def;
    def.type = exgine::CharacterType::NPC;
    def.appearance.height = 1.68f + next_float() * 0.14f;
    def.appearance.seed = next_u64();
    def.appearance.archetype = cop ? exgine::CharacterArchetype::Police
                                   : exgine::CharacterArchetype::Civilian;
    auto assembly = exgine::generate_character(def);
    if (assembly.valid() && !rt_->attach_geometry(id, std::move(assembly))) {
        rt_->state().entities.destroy(id);
        return exgine::invalid_entity;
    }
    return id;
}

void CityGenerator::build_block(int bi, int bj) {
    const float bx = road_line(bi) + kPitch * 0.5f;
    const float bz = road_line(bj) + kPitch * 0.5f;
    const District dist = district(bi, bj);
    const float in = kBlock - 6.4f; // inner area (inside sidewalk ring)

    switch (dist) {
    case District::Plaza: {
        acc_quad_y("stone_plaza", bx, 0.17f, bz, in, in);
        // fountain (world origin area) — player relocates to plaza south
        acc_cyl("stone_plaza", bx, bz - 2.f, 4.2f, 0.17f, 0.75f, 20);
        acc_quad_y("fountain_water", bx, 0.68f, bz - 2.f, 7.6f, 7.6f);
        acc_cyl("stone_plaza", bx, bz - 2.f, 1.5f, 0.68f, 2.1f, 14);
        acc_cyl("fountain_water", bx, bz - 2.f, 1.15f, 2.05f, 2.2f, 12);
        for (int i = 0; i < 8; ++i) {
            const float a = static_cast<float>(i) * kPi * 0.25f;
            acc_box("wood_bench", bx + std::cos(a) * 9.5f, 0.55f,
                    bz - 2.f + std::sin(a) * 9.5f, 1.7f, 0.55f, 0.6f);
        }
        for (int i = 0; i < 4; ++i)
            acc_cyl("tree_leaves", bx + (i % 2 == 0 ? -16.f : 16.f),
                    bz - 2.f + (i < 2 ? -12.f : 12.f), 1.8f, 2.2f, 4.8f, 8);
        break;
    }
    case District::Downtown: {
        const float w = next_range(26.f, 33.f);
        const float d = next_range(24.f, 29.f);
        const int floors = 8 + static_cast<int>(next_float() * 8.f);
        static const char* walls[] = {"glass_tower", "concrete_light", "concrete_dark", "brick_red"};
        const char* wall = walls[next_u64() % 4];
        const float x = bx + next_range(-3.f, 3.f);
        const float z = bz + next_range(-3.f, 3.f);
        (void)spawn_full_building(x, z, w, d, floors, next_u64(), wall);
        acc_cyl("tree_leaves", bx - 20.f, bz + 20.f, 1.7f, 2.2f, 4.6f, 7);
        acc_cyl("tree_leaves", bx + 20.f, bz - 20.f, 1.7f, 2.2f, 4.6f, 7);
        break;
    }
    case District::Commercial: {
        acc_quad_y("lot_concrete", bx, 0.06f, bz, in, in);
        if ((bi + bj) % 2 == 0) {
            const float w = next_range(19.f, 23.f);
            const float d = next_range(21.f, 25.f);
            (void)spawn_full_building(bx - 10.f, bz - 8.f, w, d,
                                      3 + static_cast<int>(next_float() * 3.f), next_u64(),
                                      next_u64() % 2 ? "brick_red" : "concrete_light");
            (void)spawn_full_building(bx + 12.f, bz + 9.f, next_range(18.f, 22.f),
                                      next_range(19.f, 23.f),
                                      3 + static_cast<int>(next_float() * 3.f), next_u64(),
                                      next_u64() % 2 ? "brick_brown" : "concrete_dark");
        } else {
            spawn_shell_block(bi, bj, District::Commercial);
        }
        // parked cars along the block edge
        for (int p = 0; p < 2; ++p) {
            if (vehicles_.size() >= 34) break;
            const float px = bx + next_range(-20.f, 20.f);
            const float pz = bz + (p == 0 ? -28.4f : 28.4f);
            if (const auto vid = spawn_vehicle_entity(px, pz,
                                                      next_float() < 0.5f ? 0.f : kPi,
                                                      static_cast<int>(next_u64() % 4), "ParkedCar"))
                vehicles_.push_back({vid, px, pz, 0, 0, 0.f, false});
        }
        break;
    }
    case District::Residential:
    case District::Outskirts: {
        acc_quad_y("park_grass", bx, 0.055f, bz, in, in);
        spawn_shell_block(bi, bj, District::Residential);
        break;
    }
    case District::Park: {
        acc_quad_y("park_grass", bx, 0.055f, bz, in, in);
        acc_quad_y("stone_plaza", bx, 0.07f, bz, 4.f, in);        // path N-S
        acc_quad_y("stone_plaza", bx, 0.07f, bz, in, 4.f);        // path E-W
        const int trees = 9 + static_cast<int>(next_float() * 5.f);
        for (int t = 0; t < trees; ++t) {
            const float tx = bx + next_range(-19.f, 19.f);
            const float tz = bz + next_range(-19.f, 19.f);
            if (std::fabs(tx - bx) < 3.4f || std::fabs(tz - bz) < 3.4f) continue;
            acc_cyl("tree_bark", tx, tz, 0.22f, 0.1f, 2.6f, 5);
            acc_cyl("tree_leaves", tx, tz, next_range(1.5f, 2.2f), 2.3f, 5.0f, 7);
        }
        for (int b = 0; b < 3; ++b)
            acc_box("wood_bench", bx + next_range(-14.f, 14.f), 0.55f,
                    bz + next_range(-14.f, 14.f), 1.7f, 0.55f, 0.6f);
        break;
    }
    case District::Parking: {
        acc_quad_y("lot_concrete", bx, 0.06f, bz, in, in);
        for (int row = 0; row < 2; ++row) {
            for (int s = 0; s < 6; ++s)
                acc_quad_y("mark_white", bx - 17.f + static_cast<float>(s) * 6.8f, 0.09f,
                           bz + (row == 0 ? -9.f : 9.f), 0.25f, 5.6f);
        }
        for (int p = 0; p < 6; ++p) {
            if (vehicles_.size() >= 34) break;
            const float px = bx - 17.f + static_cast<float>(p % 6) * 6.8f + 2.4f;
            const float pz = bz + (p < 3 ? -9.f : 9.f);
            if (const auto vid = spawn_vehicle_entity(px, pz, kPi * 0.5f,
                                                      static_cast<int>(next_u64() % 4), "ParkedCar"))
                vehicles_.push_back({vid, px, pz, 0, 0, kPi * 0.5f, false});
        }
        break;
    }
    }
}

void CityGenerator::build_props() {
    // Streetlights: two per block on opposite sidewalk corners, arm toward road
    for (int bi = 0; bi < kGrid; ++bi) {
        for (int bj = 0; bj < kGrid; ++bj) {
            const float bx = road_line(bi) + kPitch * 0.5f;
            const float bz = road_line(bj) + kPitch * 0.5f;
            const float hb = kBlock * 0.5f - 1.6f;
            const float lx[2] = {bx - hb, bx + hb};
            const float lz[2] = {bz - hb, bz + hb};
            for (int c = 0; c < 2; ++c) {
                const float px = lx[c], pz = lz[1 - c];
                const float sgn_x = c == 0 ? -1.f : 1.f;
                const float sgn_z = c == 0 ? -1.f : 1.f;
                acc_cyl("steel_dark", px, pz, 0.13f, 0.16f, 7.1f, 6);
                acc_box("steel_dark", px + sgn_x * 0.85f, 7.05f, pz, 1.7f, 0.13f, 0.13f);
                acc_box("lamp_head", px + sgn_x * 1.6f, 6.93f, pz, 0.75f, 0.2f, 0.4f);
                (void)sgn_z;
            }
        }
    }

    // Billboards downtown corners
    for (int i = 0; i < 8; ++i) {
        const float a = static_cast<float>(i) * kPi * 0.25f + 0.4f;
        const float px = std::cos(a) * 88.f;
        const float pz = std::sin(a) * 88.f;
        acc_box("steel_dark", px, 3.4f, pz, 0.45f, 6.8f, 0.45f);
        acc_box(i % 2 == 0 ? "billboard_neon" : "billboard_cyan",
                px, 7.6f, pz, 11.f, 0.35f, 5.2f);
    }
}

void CityGenerator::flush_accumulators() {
    for (auto& [material, mesh] : accs_) {
        if (!mesh.valid()) continue;
        const auto id = spawn_entity(exgine::NodeKind::Property, "CityStatic", 0.f, 0.f, 0.f);
        if (!id) continue;
        exgine::MeshAssembly assembly;
        exgine::MeshPart part;
        part.name = material;
        part.mesh = std::move(mesh);
        part.material_slot = material;
        assembly.parts.push_back(std::move(part));
        (void)rt_->attach_geometry(id, std::move(assembly));
        mesh = {};
    }
}

void CityGenerator::build_extras() {
    // Moving traffic on avenues (k = 3 and 8, both axes)
    int traffic_id = 0;
    for (int axis = 0; axis < 2; ++axis) {
        for (int k : {3, 8}) {
            const float line = road_line(k);
            for (int lane = 0; lane < 2; ++lane) {
                const float off = lane == 0 ? 2.9f : -2.9f;
                for (int n = 0; n < 2; ++n) {
                    const float p = -kHalf + 60.f +
                                    static_cast<float>(traffic_id % 3) * 220.f +
                                    static_cast<float>(n) * 140.f;
                    const float x = axis == 0 ? p : line + off;
                    const float z = axis == 0 ? line + off : p;
                    const float sgn = lane == 0 ? 1.f : -1.f;
                    const float dx = axis == 0 ? sgn : 0.f;
                    const float dz = axis == 0 ? 0.f : -sgn;
                    char name[32];
                    std::snprintf(name, sizeof(name), "Traffic_%02d", traffic_id);
                    const float yaw = std::atan2(dx, dz);
                    if (const auto vid = spawn_vehicle_entity(x, z, yaw,
                                                              traffic_id % 3 == 0 ? 1 : 0, name))
                        vehicles_.push_back({vid, x, z, dx, dz, next_range(9.f, 12.5f), yaw, true});
                    ++traffic_id;
                }
            }
        }
    }

    // Pedestrians on downtown/commercial sidewalk loops + police presence
    int walker_id = 0;
    const int c = kGrid / 2;
    for (int bi = 2; bi <= 8 && walker_id < 24; ++bi) {
        for (int bj = 2; bj <= 8 && walker_id < 24; ++bj) {
            const int dc = std::max(std::abs(bi - c), std::abs(bj - c));
            if (dc > 3) continue;
            const bool cop = (bi + bj) % 5 == 0 && walker_id % 4 == 0 && dc <= 2;
            char name[32];
            std::snprintf(name, sizeof(name), cop ? "Cop_City_%02d" : "CityPed_%02d", walker_id);
            const float cx = road_line(bi) + kPitch * 0.5f;
            const float cz = road_line(bj) + kPitch * 0.5f;
            if (const auto id = spawn_character_entity(cx - 24.4f, cz - 24.4f, cop, name))
                walkers_.push_back({id, cx, cz, 24.4f,
                                    next_float() * 8.f * 24.4f,
                                    next_range(1.1f, 1.9f), 0.f});
            ++walker_id;
        }
    }

    // Warm point lights at downtown street corners (night life)
    for (int i = 0; i < 10; ++i) {
        const float a = static_cast<float>(i) * kPi * 0.2f + 0.3f;
        exgine::Light l;
        l.type = exgine::LightType::Point;
        l.position = {std::cos(a) * 46.f, 6.8f, std::sin(a) * 46.f};
        l.color = {1.f, 0.72f, 0.42f};
        l.intensity = 6.f;
        l.range = 30.f;
        (void)rt_->create_light(l);
    }
    exgine::Light plaza1;
    plaza1.type = exgine::LightType::Point;
    plaza1.position = {0.f, 5.5f, -2.f};
    plaza1.color = {0.9f, 0.85f, 0.7f};
    plaza1.intensity = 8.f;
    plaza1.range = 34.f;
    (void)rt_->create_light(plaza1);
}

} // namespace exworld
