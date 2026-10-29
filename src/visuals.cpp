#include "exworld/visuals.hpp"

#include "exgine/building.hpp"
#include "exgine/character.hpp"
#include "exgine/geometry.hpp"
#include "exgine/import.hpp"
#include "exgine/material.hpp"
#include "exgine/vehicle.hpp"

#include <cctype>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace exworld {
namespace {

exgine::MeshAssembly box_mesh(exgine::Vec3 size, std::string material) {
    exgine::MeshAssembly a;
    a.parts.push_back({"box", exgine::make_box({size}), std::move(material), {}, {1, 1, 1}, {}});
    return a;
}

exgine::MeshAssembly quad_mesh(float w, float h, std::string material) {
    exgine::Mesh x;
    x.vertices = {
        {{-w * .5f, -h * .5f, 0}, {0, 0, 1}, {0, 0}},
        {{w * .5f, -h * .5f, 0}, {0, 0, 1}, {1, 0}},
        {{w * .5f, h * .5f, 0}, {0, 0, 1}, {1, 1}},
        {{-w * .5f, h * .5f, 0}, {0, 0, 1}, {0, 1}},
    };
    x.indices = {0, 1, 2, 0, 2, 3};
    exgine::MeshAssembly a;
    a.parts.push_back({"quad", std::move(x), std::move(material), {}, {1, 1, 1}, {}});
    return a;
}

exgine::MeshAssembly detailed_capsule_body(float height, std::string material) {
    const float h = std::max(1.5f, height);
    const float r = 0.28f;
    exgine::MeshAssembly a;
    a.parts.push_back({"body", exgine::make_capsule(r, h - 2.f * r, 24, 10), material,
                       {0, h * 0.5f, 0}, {1, 1, 1}, {}});
    a.parts.push_back({"head", exgine::make_sphere({0.12f * h, 24, 12}), "skin",
                       {0, h * 0.92f, 0}, {1, 1, 1}, {}});
    return a;
}

exgine::EntityId spawn_prop(exgine::Runtime& runtime, const char* name) {
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
}

void set_entity_pos(exgine::Runtime& runtime, exgine::EntityId id, float x, float y, float z) {
    auto* e = runtime.state().entities.get(id);
    if (!e) return;
    e->transform.x = x;
    e->transform.y = y;
    e->transform.z = z;
    if (e->scene_node) {
        exgine::SceneTransform local;
        local.position = {x, y, z};
        local.scale = {1, 1, 1};
        (void)runtime.scene().set_local_transform(e->scene_node, local);
    }
}

bool extract_baked_obj(std::string_view body, std::string& obj, std::string& material) {
    const auto begin = body.find("BEGIN_MESH");
    const auto end = body.find("END_MESH");
    if (begin == std::string_view::npos || end == std::string_view::npos || end <= begin) return false;

    std::istringstream in{std::string(body.substr(0, begin))};
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("material.", 0) == 0) {
            const auto eq = line.find('=');
            if (eq != std::string::npos) material = line.substr(eq + 1);
            while (!material.empty() && std::isspace(static_cast<unsigned char>(material.front()))) material.erase(material.begin());
            while (!material.empty() && std::isspace(static_cast<unsigned char>(material.back()))) material.pop_back();
        }
    }

    obj.assign(body.substr(begin + std::string_view("BEGIN_MESH").size(), end - (begin + std::string_view("BEGIN_MESH").size())));
    return !obj.empty();
}

bool baked_assembly(const BakedPackage* package, std::string_view fallback_material,
                    exgine::MeshAssembly& out) {
    if (!package || package->body.empty()) return false;
    std::string obj;
    std::string material;
    if (!extract_baked_obj(package->body, obj, material)) return false;
    if (material.empty()) material = std::string(fallback_material);

    const auto imported = exgine::import_obj(package->path, obj);
    if (!imported.success || imported.meshes.empty()) return false;

    out = {};
    for (const auto& mesh : imported.meshes) {
        exgine::MeshPart part;
        part.name = package->name;
        part.mesh = mesh.mesh;
        part.material_slot = material;
        out.parts.push_back(std::move(part));
    }
    return out.valid();
}

} // namespace

bool VisualSystem::ensure_materials(exgine::Runtime& runtime) {
    (void)runtime.define_material(exgine::make_real_world_material("skin", 1001));
    (void)runtime.define_material(exgine::make_real_world_material("hair", 1002));
    (void)runtime.define_material(exgine::make_real_world_material("fabric", 1003));
    (void)runtime.define_material(exgine::make_real_world_material("denim", 1004));
    (void)runtime.define_material(exgine::make_real_world_material("leather", 1005));
    (void)runtime.define_material(exgine::make_real_world_material("concrete", 11));
    (void)runtime.define_material(exgine::make_real_world_material("asphalt", 22));
    (void)runtime.define_material(exgine::make_real_world_material("metal", 33));
    (void)runtime.define_material(exgine::make_real_world_material("steel", 34));
    (void)runtime.define_material(exgine::make_real_world_material("aluminium", 35));
    (void)runtime.define_material(exgine::make_real_world_material("paint", 44));
    (void)runtime.define_material(exgine::make_real_world_material("glass", 55));
    (void)runtime.define_material(exgine::make_real_world_material("wood", 66));
    (void)runtime.define_material(exgine::make_real_world_material("grass", 77));
    (void)runtime.define_material(exgine::make_real_world_material("water", 88));
    (void)runtime.define_material(exgine::make_real_world_material("terrain", 89));
    (void)runtime.define_material(exgine::make_real_world_material("hud", 99));
    (void)runtime.define_material(exgine::make_real_world_material("exworld_surface", 120));
    (void)runtime.define_material(exgine::make_real_world_material("rubber", 130));
    (void)runtime.define_material(exgine::make_real_world_material("watch_metal", 140));
    (void)runtime.define_material(exgine::make_real_world_material("backpack_fabric", 150));
    return true;
}

bool VisualSystem::ensure_character(exgine::Runtime& runtime, exgine::EntityId id, bool is_player) {
    auto* e = runtime.state().entities.get(id);
    if (!e) return false;
    if (e->geometry && e->geometry->valid() && e->geometry->parts.size() >= 6) return true;

    exgine::CharacterDefinition def;
    def.type = is_player ? exgine::CharacterType::Player : exgine::CharacterType::NPC;
    def.appearance.height = is_player ? 1.75f : 1.70f;
    def.appearance.build = is_player ? 1.05f : 1.0f;
    def.appearance.seed = is_player ? 4242ull : (1000ull + static_cast<std::uint64_t>(id));
    def.appearance.archetype = exgine::CharacterArchetype::Civilian;
    def.appearance.watch = is_player;
    def.appearance.backpack = false;
    if (e->name.rfind("Cop_", 0) == 0) def.appearance.archetype = exgine::CharacterArchetype::Police;

    auto assembly = exgine::generate_character(def);
    if (!assembly.valid() || assembly.parts.size() < 6)
        assembly = detailed_capsule_body(def.appearance.height, "skin");

    if (!runtime.attach_geometry(id, std::move(assembly))) {
        std::cerr << "EXWORLD: attach_geometry failed for " << e->name << "\n";
        return false;
    }
    return true;
}

bool VisualSystem::ensure_ground(exgine::Runtime& runtime, const exgine::Vec3& at) {
    if (ground_ == exgine::invalid_entity) ground_ = spawn_prop(runtime, "GroundPlate");
    if (!ground_) return false;
    set_entity_pos(runtime, ground_, at.x, -0.08f, at.z);

    bool attached_baked = false;
    if (packages_) {
        exgine::MeshAssembly baked;
        attached_baked = baked_assembly(packages_->get("ground"), "asphalt", baked) &&
                         runtime.attach_geometry(ground_, std::move(baked));
    }
    if (!attached_baked && !runtime.attach_geometry(ground_, box_mesh({800.f, 0.16f, 800.f}, "asphalt")))
        return false;

    if (ground_grass_ == exgine::invalid_entity) ground_grass_ = spawn_prop(runtime, "GroundGrass");
    if (ground_grass_) {
        set_entity_pos(runtime, ground_grass_, at.x + 220.f, -0.06f, at.z + 180.f);
        (void)runtime.attach_geometry(ground_grass_, box_mesh({420.f, 0.12f, 420.f}, "grass"));
    }

    if (ground_water_ == exgine::invalid_entity) ground_water_ = spawn_prop(runtime, "GroundWater");
    if (ground_water_) {
        set_entity_pos(runtime, ground_water_, at.x - 280.f, -0.4f, at.z - 200.f);
        (void)runtime.attach_geometry(ground_water_, box_mesh({300.f, 0.4f, 300.f}, "water"));
    }
    return true;
}

bool VisualSystem::ensure_hud_entities(exgine::Runtime& runtime) {
    auto make = [&](exgine::EntityId& slot, const char* name, float w, float h) {
        if (slot == exgine::invalid_entity) slot = spawn_prop(runtime, name);
        if (!slot) return false;
        return runtime.attach_geometry(slot, quad_mesh(w, h, "hud"));
    };
    return make(hud_move_, "HUD_MoveStick", 0.55f, 0.55f) &&
           make(hud_look_, "HUD_LookStick", 0.50f, 0.50f) &&
           make(hud_interact_, "HUD_Interact", 0.42f, 0.28f) &&
           make(hud_exit_, "HUD_Exit", 0.42f, 0.28f);
}

bool VisualSystem::ensure_buildings_vehicles(exgine::Runtime& runtime) {
    for (auto id : runtime.state().entities.ids()) {
        auto* e = runtime.state().entities.get(id);
        if (!e) continue;
        // Buildings keep their hydrated procedural geometry (per-seed wall palette,
        // door gaps, window glass, floors). The old code overwrote every building
        // with one shared baked box, which is why the whole city rendered identical.
        if (e->kind == exgine::NodeKind::Building && (!e->geometry || !e->geometry->valid())) {
            exgine::BuildingConfig cfg;
            cfg.floors = 5;
            cfg.floor_height = 3.0f;
            cfg.width = 14.f;
            cfg.depth = 12.f;
            cfg.seed = 1000ull + static_cast<std::uint64_t>(id);
            (void)runtime.generate_building(id, cfg);
        }
        if (e->kind == exgine::NodeKind::Vehicle && (!e->geometry || !e->geometry->valid())) {
            auto cfg = exgine::make_vehicle_config(exgine::VehicleType::Car);
            cfg.seed = 2000ull + static_cast<std::uint64_t>(id);
            (void)runtime.generate_vehicle(id, cfg);
        }
    }
    return true;
}

bool VisualSystem::bootstrap(exgine::Runtime& runtime, exgine::EntityId player) {
    ensure_materials(runtime);
    if (player && !ensure_character(runtime, player, true))
        std::cerr << "EXWORLD: player mesh failed\n";

    for (auto id : runtime.state().entities.ids()) {
        auto* e = runtime.state().entities.get(id);
        if (!e) continue;
        if (e->kind == exgine::NodeKind::NPC ||
            (e->kind == exgine::NodeKind::Player && id != player))
            (void)ensure_character(runtime, id, false);
    }

    exgine::Vec3 at{};
    if (player) {
        if (auto* e = runtime.state().entities.get(player))
            at = {e->transform.x, e->transform.y, e->transform.z};
    }
    (void)ensure_ground(runtime, at);
    (void)ensure_buildings_vehicles(runtime);
    (void)ensure_hud_entities(runtime);
    runtime.scene().update_world_transforms();
    ready_ = true;
    std::cerr << "EXWORLD: visuals bootstrap OK; baked package geometry preferred over generated fallback\n";
    return true;
}

void VisualSystem::update_hud_markers(exgine::Runtime& runtime, const exgine::Camera& cam,
                                      const HudLayout& /*layout*/, int /*sw*/, int /*sh*/) {
    if (!ready_) return;

    const float dist = 2.6f;
    const float cy = std::cos(cam.rotation.y);
    const float sy = std::sin(cam.rotation.y);
    const float cp = std::cos(cam.rotation.x);
    const float sp = std::sin(cam.rotation.x);
    const float fx = sy * cp;
    const float fy = -sp;
    const float fz = cy * cp;
    const float rx = cy;
    const float rz = -sy;
    const float ux = -sy * sp;
    const float uy = cp;
    const float uz = -cy * sp;

    auto place = [&](exgine::EntityId id, float ndc_x, float ndc_y) {
        if (!id) return;
        const float ox = ndc_x * 1.25f;
        const float oy = ndc_y * 0.78f;
        set_entity_pos(runtime, id,
                       cam.position.x + fx * dist + rx * ox + ux * oy,
                       cam.position.y + fy * dist + uy * oy,
                       cam.position.z + fz * dist + rz * ox + uz * oy);
    };

    place(hud_move_, -0.58f, -0.48f);
    place(hud_look_, 0.48f, -0.18f);
    place(hud_interact_, 0.75f, 0.58f);
    place(hud_exit_, 0.75f, -0.58f);
    runtime.scene().update_world_transforms();
}

} // namespace exworld
