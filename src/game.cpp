#include "exworld/game.hpp"

#include "exgine/android.hpp"
#include "exgine/render.hpp"
#include "exgine/material.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace exworld {

namespace {

exgine::EntityId find_by_name(exgine::Runtime& r, std::string_view name) {
    for (auto id : r.state().entities.ids()) {
        auto* e = r.state().entities.get(id);
        if (e && e->name == name) return id;
    }
    return exgine::invalid_entity;
}

exgine::EntityId find_by_kind(exgine::Runtime& r, exgine::NodeKind kind) {
    for (auto id : r.state().entities.ids()) {
        auto* e = r.state().entities.get(id);
        if (e && e->kind == kind) return id;
    }
    return exgine::invalid_entity;
}

exgine::EntityId find_player_entity(exgine::Runtime& r) {
    static constexpr const char* kNames[] = {"Sebastian", "Player", "Explorer"};
    for (const char* n : kNames) {
        auto id = find_by_name(r, n);
        if (id) return id;
    }
    if (auto id = find_by_kind(r, exgine::NodeKind::Player)) return id;
    return exgine::invalid_entity;
}

bool ensure_render_basics(exgine::Runtime& runtime, const exgine::Vec3& focus,
                          std::uint64_t* out_sun = nullptr) {
    exgine::Camera cam;
    cam.position = {focus.x - 7.f, focus.y + 3.2f, focus.z + 7.f};
    cam.rotation = {-0.28f, 0.55f, 0.f};
    cam.vertical_fov_degrees = 55.f;
    cam.near_plane = 0.1f;
    cam.far_plane = 5000.f;
    if (!runtime.set_main_camera(cam)) {
        std::cerr << "EXWORLD: set_main_camera failed\n";
        return false;
    }

    exgine::Light sun;
    sun.type = exgine::LightType::Directional;
    sun.direction = {-0.45f, -0.78f, -0.2f};
    sun.color = {1.f, 0.92f, 0.78f};
    sun.intensity = 4.f;
    const auto sun_id = runtime.create_light(sun);
    if (out_sun) *out_sun = sun_id;

    auto& settings = runtime.lighting().settings();
    settings.fog.enabled = true;
    settings.fog.color = {0.60f, 0.72f, 0.88f};
    settings.fog.density = 0.0009f;

    (void)runtime.define_material(exgine::make_real_world_material("concrete", 11));
    (void)runtime.define_material(exgine::make_real_world_material("asphalt", 22));
    (void)runtime.define_material(exgine::make_real_world_material("metal", 33));
    (void)runtime.define_material(exgine::make_real_world_material("paint", 44));
    (void)runtime.define_material(exgine::make_real_world_material("glass", 55));
    (void)runtime.define_material(exgine::make_real_world_material("skin", 66));
    (void)runtime.define_material(exgine::make_real_world_material("exworld_surface", 77));
    return true;
}

void set_entity_transform(exgine::Runtime& runtime, exgine::EntityId id,
                          float x, float y, float z, float yaw) {
    auto* e = runtime.state().entities.get(id);
    if (!e) return;
    e->transform.x = x;
    e->transform.y = y;
    e->transform.z = z;
    if (e->scene_node) {
        exgine::SceneTransform t;
        t.position = {x, y, z};
        t.rotation = {0.f, yaw, 0.f};
        t.scale = {1, 1, 1};
        (void)runtime.scene().set_local_transform(e->scene_node, t);
    }
}

} // namespace

ExWorldGame::ExWorldGame(exgine::ProjectSourceLoader loader)
    : loader_(std::move(loader)), engine_({}, loader_) {}

void ExWorldGame::set_input(const PlayerInput& input) noexcept {
    forced_input_ = input;
    use_forced_input_ = true;
}

bool ExWorldGame::open(std::string_view content_root) {
    ready_ = configured_ = false;
    const std::filesystem::path root(content_root);
    std::string manifest;
    if (!loader_((root / "project.exg").string(), manifest) &&
        !loader_("project.exg", manifest) &&
        !loader_("first_light/project.exg", manifest)) {
        std::cerr << "EXWORLD: project.exg not found\n";
        return false;
    }
    return open_from_manifest(manifest);
}

bool ExWorldGame::open_from_manifest(std::string_view manifest_text) {
    ready_ = configured_ = false;
    flow_ = FlowState::Loading;
    flow_time_ = 0.f;
    cinematic_begun_ = false;
    frame_failures_ = 0;

    if (!engine_.open_project(manifest_text)) {
        set_fatal("engine open_project failed");
        std::cerr << "EXWORLD: PlayableGame::open_project failed\n";
        return false;
    }

    if (!configure_systems()) {
        std::cerr << "EXWORLD: configure_systems soft-failed (continuing)\n";
    }
    (void)spawn_world_content();

    auto& runtime = engine_.session().game().runtime();
    // Relocate the player onto the central plaza (city rebuilds around them).
    if (auto pid = find_player_entity(runtime)) {
        set_entity_transform(runtime, pid, 0.f, 1.8f, 12.f, 0.f);
    }
    city_.begin(runtime, 7777u);
    flow_ = FlowState::Loading;
    configured_ = true;
    return true;
}

bool ExWorldGame::start() noexcept {
    // Loading drives engine start itself once the city is built; desktop
    // callers use start() -> handled inside flow. Keep compatibility: if the
    // city is already done, start the engine now.
    if (flow_ == FlowState::Loading && city_.done()) return start_engine();
    return ready_;
}

bool ExWorldGame::start_engine() {
    if (engine_running_) return ready_;
    ready_ = configured_ && engine_.start();
    engine_running_ = ready_;
    if (!ready_) {
        set_fatal("engine start failed");
        return false;
    }
    if (player_.valid()) {
        auto& runtime = engine_.session().game().runtime();
        camera_.reset(player_.position(runtime));
        (void)runtime.set_main_camera(camera_.camera());
    }
    return ready_;
}

void ExWorldGame::set_fatal(std::string message) {
    fatal_message_ = std::move(message);
    flow_ = FlowState::Fatal;
    std::cerr << "EXWORLD FATAL: " << fatal_message_ << "\n";
}

void ExWorldGame::debug_skip_menu() noexcept {
    if (!configured_) return;
    auto& runtime = engine_.session().game().runtime();
    while (!city_.done()) (void)city_.step();
    register_city_content(runtime);
    if (!start_engine()) return;
    camera_.reset(player_.position(runtime));
    flow_ = FlowState::Playing;
    flow_time_ = 0.f;
}

bool ExWorldGame::configure_systems() {
    auto& runtime = engine_.session().game().runtime();

    auto player_id = find_player_entity(runtime);
    if (!player_id) {
        std::cerr << "EXWORLD: no playable entity (Sebastian/Player)\n";
        set_fatal("no player entity in scene");
        return false;
    }

    exgine::CharacterId cid = exgine::invalid_character;
    const auto& bindings = runtime.character_bindings();
    auto it = bindings.find(player_id);
    if (it != bindings.end()) cid = it->second;

    player_.bind(runtime, player_id, cid);

    (void)load_baked_packages();
    visuals_.set_packages(&packages_);

    const auto pos = player_.position(runtime);
    if (!ensure_render_basics(runtime, pos, &sun_light_)) {
        std::cerr << "EXWORLD: render basics failed\n";
    }

    if (!animation_.initialize(runtime, player_id)) {
        std::cerr << "EXWORLD: AnimationDriver unavailable (ok)\n";
    }

    (void)visuals_.bootstrap(runtime, player_id);
    (void)ui_.init(runtime);

    sound_.initialize(runtime);
    world_.bootstrap(runtime);
    wanted_.reset();
    police_.clear();
    interiors_.clear();
    input_.reset();
    return true;
}

bool ExWorldGame::load_baked_packages() {
    if (!loader_) return false;
    std::string index;
    if (!loader_("baked/index.exg", index) && !loader_("content/baked/index.exg", index)) {
        std::cerr << "EXWORLD: baked/index.exg not found (continuing)\n";
        return false;
    }
    auto file_loader = [this](std::string_view path, std::string& out) -> bool {
        if (!loader_) return false;
        if (loader_(path, out)) return true;
        return loader_(std::string("content/") + std::string(path), out);
    };
    if (!packages_.load_from_text(index, file_loader)) {
        std::cerr << "EXWORLD: baked package index parse failed\n";
        return false;
    }
    std::cerr << "EXWORLD: loaded " << packages_.all().size()
              << " baked packages stream_r=" << packages_.scale().stream_radius << "\n";
    return true;
}

bool ExWorldGame::spawn_world_content() {
    auto& runtime = engine_.session().game().runtime();
    vehicles_.clear();
    world_.clear();
    police_.clear();
    interiors_.clear();

    for (auto id : runtime.state().entities.ids()) {
        auto* e = runtime.state().entities.get(id);
        if (!e) continue;
        if (e->kind == exgine::NodeKind::Building) {
            world_.register_building(
                id, {e->transform.x, e->transform.y, e->transform.z + 2.2f}, 2.4f);
            interiors_.register_building(
                id, 3, 4, {e->transform.x, e->transform.y, e->transform.z});
        }
        if (e->kind == exgine::NodeKind::Vehicle)
            vehicles_.register_vehicle(runtime, id, e->name, {});
        if (e->kind == exgine::NodeKind::NPC) {
            if (e->name.rfind("Cop_", 0) == 0)
                police_.register_unit(id);
        }
    }
    return true;
}

void ExWorldGame::register_city_content(exgine::Runtime& runtime) {
    for (const auto& d : city_.doors()) {
        world_.register_building(d.building, {d.x, 2.2f, d.z}, 2.6f);
        interiors_.register_building(d.building, 2, 3, {d.x, 0.f, d.z});
    }
    for (const auto& v : city_.vehicles())
        vehicles_.register_vehicle(runtime, v.id, v.traffic ? "Traffic" : "Parked", {});
    for (const auto& w : city_.walkers())
        if (const auto* e = runtime.state().entities.get(w.id))
            if (e->name.rfind("Cop_", 0) == 0) police_.register_unit(w.id);
    std::cerr << "EXWORLD: city registered — buildings " << city_.doors().size()
              << " vehicles " << city_.vehicles().size()
              << " walkers " << city_.walkers().size() << "\n";
}

void ExWorldGame::begin_menu() {
    flow_ = FlowState::Menu;
    flow_time_ = 0.f;
    cinematic_begun_ = false;
    input_.clear_taps();
}

void ExWorldGame::begin_gameplay() {
    flow_ = FlowState::Playing;
    flow_time_ = 0.f;
    input_.clear_taps();
}

void ExWorldGame::update_region(const exgine::Vec3& pos) {
    if (std::fabs(pos.x) <= CityGenerator::kHalf && std::fabs(pos.z) <= CityGenerator::kHalf) {
        const int bi = std::clamp(static_cast<int>((pos.x + CityGenerator::kHalf) /
                                                   CityGenerator::kPitch), 0, CityGenerator::kGrid - 1);
        const int bj = std::clamp(static_cast<int>((pos.z + CityGenerator::kHalf) /
                                                   CityGenerator::kPitch), 0, CityGenerator::kGrid - 1);
        region_name_ = CityGenerator::district_name(city_.district(bi, bj));
    } else {
        region_name_ = "COUNTRYSIDE";
    }
}

void ExWorldGame::handle_interactions(float, const PlayerInput& in, exgine::Runtime& runtime) {
    if (!player_.valid()) return;
    const auto pos = player_.position(runtime);

    if (in.interact && player_.mode() == PlayerMode::OnFoot) {
        auto veh = vehicles_.nearest_vehicle(pos, 3.5f, runtime);
        if (veh != exgine::invalid_entity) {
            const auto* seat = vehicles_.seat(veh);
            const float dur = seat ? seat->enter_duration : 1.15f;
            if (player_.try_enter_vehicle(veh, dur, runtime)) {
                animation_.play_enter_vehicle(dur);
                sound_.play_vehicle_enter(runtime);
                camera_.add_shake(0.14f);
                wanted_.on_crime(14.f);
                if (wanted_.level() >= WantedLevel::Level1)
                    sound_.play_wanted_alert(runtime);
            }
        } else {
            auto bld = world_.nearest_door(pos, 2.8f);
            if (bld != exgine::invalid_entity && player_.try_enter_building(bld, runtime)) {
                animation_.play_enter_building(0.85f);
                sound_.play_door(true, false, runtime);
                interiors_.activate(bld, runtime);
            }
        }
    }

    if (in.exit) {
        if (player_.mode() == PlayerMode::InVehicle) {
            const auto* seat = vehicles_.seat(player_.current_vehicle());
            const float dur = seat ? seat->exit_duration : 0.95f;
            if (player_.try_exit_vehicle(dur, runtime)) {
                animation_.play_exit_vehicle(dur);
                sound_.play_vehicle_exit(runtime);
                camera_.add_shake(0.10f);
                auto& dyn = runtime.vehicle_dynamics();
                auto it = dyn.find(player_.current_vehicle());
                if (it != dyn.end() && it->second) (void)it->second->possess(false);
            }
        } else if (player_.mode() == PlayerMode::InsideBuilding) {
            if (player_.try_exit_building(runtime)) {
                animation_.play_exit_building(0.75f);
                sound_.play_door(false, false, runtime);
                interiors_.deactivate();
            }
        }
    }
}

void ExWorldGame::update_vehicle_possession(float dt, const PlayerInput& in,
                                            exgine::Runtime& runtime) {
    if (player_.mode() != PlayerMode::InVehicle) return;
    const float throttle = std::max(0.f, in.move_z);
    const float brake = in.move_z < -0.1f ? -in.move_z : (in.crouch ? 1.f : 0.f);
    vehicles_.update_driven(player_.current_vehicle(), throttle, in.move_x, brake, dt, runtime);
}

void ExWorldGame::update_sky(exgine::Runtime& runtime) {
    constexpr float kDayLength = 480.f;
    constexpr float kTwoPi = 6.2831853f;
    const float t = std::fmod(static_cast<float>(time_) / kDayLength, 1.f);
    const float raw = std::sin(t * kTwoPi);
    const float day = std::clamp(raw * 1.6f + 0.1f, 0.f, 1.f);
    const float dawn = std::clamp(1.f - std::abs(day - 0.18f) / 0.18f, 0.f, 1.f);

    const float elev = 0.18f + day * 0.95f;
    const float az = 0.7f + t * 1.5707963f;
    if (sun_light_) {
        if (exgine::Light* sun = runtime.lighting().get_light(sun_light_)) {
            const float ce = std::cos(elev);
            sun->direction = {-std::sin(az) * ce, -std::sin(elev), -std::cos(az) * ce};
            sun->color = {1.f, 0.62f + 0.33f * day + 0.03f * dawn, 0.35f + 0.55f * day};
            sun->intensity = 0.55f + day * 3.45f;
        }
    }

    auto& settings = runtime.lighting().settings();
    const float night_r = 0.02f + dawn * 0.55f;
    const float night_g = 0.03f + dawn * 0.22f;
    const float night_b = 0.07f + dawn * 0.10f;
    settings.environment.sky_color = {
        night_r + (0.47f - night_r) * day,
        night_g + (0.66f - night_g) * day,
        night_b + (0.94f - night_b) * day};
    settings.environment.ambient_intensity = 0.10f + day * 0.30f;
    settings.fog.enabled = true;
    settings.fog.color = settings.environment.sky_color;
    settings.fog.density = 0.00085f + (1.f - day) * 0.0004f;
}

void ExWorldGame::update_camera(float dt, const PlayerInput& in, exgine::Runtime& runtime) {
    if (!player_.valid()) return;
    const auto pos = player_.position(runtime);

    if (std::fabs(in.look_yaw) > 1e-6f || std::fabs(in.look_pitch) > 1e-6f)
        camera_.orbit(in.look_yaw, in.look_pitch);

    // Smoothed player velocity estimate
    if (last_pos_valid_ && dt > 1e-4f) {
        const float vx = (pos.x - last_player_pos_.x) / dt;
        const float vz = (pos.z - last_player_pos_.z) / dt;
        const float blend = std::clamp(dt * 12.f, 0.f, 1.f);
        player_vel_x_ += (vx - player_vel_x_) * blend;
        player_vel_z_ += (vz - player_vel_z_) * blend;
    }
    last_player_pos_ = pos;
    last_pos_valid_ = true;

    const bool in_veh = player_.mode() == PlayerMode::InVehicle;
    float speed_frac = 0.f;
    if (in_veh) {
        const float spd = std::sqrt(player_vel_x_ * player_vel_x_ +
                                    player_vel_z_ * player_vel_z_);
        if (spd > 2.f) vehicle_yaw_ = std::atan2(player_vel_x_, player_vel_z_);
        speed_frac = std::clamp(spd / 30.f, 0.f, 1.f);
    }

    camera_.update(dt, pos, {player_vel_x_, 0.f, player_vel_z_}, in_veh, vehicle_yaw_,
                   speed_frac, in.sprint && !in_veh, runtime, &city_.solids());
}

void ExWorldGame::update_traffic_and_walkers(float dt, exgine::Runtime& runtime) {
    const float lim = CityGenerator::kHalf + 26.f;
    for (auto& v : city_.vehicles()) {
        if (!v.traffic) continue;
        v.x += v.dir_x * v.speed * dt;
        v.z += v.dir_z * v.speed * dt;
        if (v.x > lim) v.x = -lim;
        if (v.x < -lim) v.x = lim;
        if (v.z > lim) v.z = -lim;
        if (v.z < -lim) v.z = lim;
        set_entity_transform(runtime, v.id, v.x, 0.06f, v.z, v.yaw);
    }

    for (auto& w : city_.walkers()) {
        const float per = 8.f * w.half;
        w.t = std::fmod(w.t + w.speed * dt, per);
        const int side = static_cast<int>(w.t / (2.f * w.half)) % 4;
        const float p = w.t - static_cast<float>(side) * 2.f * w.half;
        float x = 0, z = 0, dx = 1, dz = 0;
        switch (side) {
        case 0: x = w.cx - w.half + p; z = w.cz - w.half; dx = 1; dz = 0; break;
        case 1: x = w.cx + w.half; z = w.cz - w.half + p; dx = 0; dz = 1; break;
        case 2: x = w.cx + w.half - p; z = w.cz + w.half; dx = -1; dz = 0; break;
        default: x = w.cx - w.half; z = w.cz + w.half - p; dx = 0; dz = -1; break;
        }
        w.yaw = std::atan2(dx, dz);
        set_entity_transform(runtime, w.id, x, 0.05f, z, w.yaw);
    }
}

void ExWorldGame::resolve_player_collisions(exgine::Runtime& runtime) {
    if (!player_.valid() || player_.mode() != PlayerMode::OnFoot) return;
    auto* e = runtime.state().entities.get(player_.entity());
    if (!e) return;

    constexpr float kR = 0.45f;
    const auto& solids = city_.solids();
    for (const auto& s : solids) {
        const float minx = s.min_x - kR, maxx = s.max_x + kR;
        const float minz = s.min_z - kR, maxz = s.max_z + kR;
        if (e->transform.x <= minx || e->transform.x >= maxx ||
            e->transform.z <= minz || e->transform.z >= maxz || s.top < 0.5f)
            continue;
        // Push out along the axis of least penetration
        const float px_left = e->transform.x - minx;
        const float px_right = maxx - e->transform.x;
        const float pz_left = e->transform.z - minz;
        const float pz_right = maxz - e->transform.z;
        const float m = std::min({px_left, px_right, pz_left, pz_right});
        if (m == px_left) e->transform.x = minx;
        else if (m == px_right) e->transform.x = maxx;
        else if (m == pz_left) e->transform.z = minz;
        else e->transform.z = maxz;
        if (e->scene_node) {
            exgine::SceneTransform t;
            t.position = {e->transform.x, e->transform.y, e->transform.z};
            t.scale = {1, 1, 1};
            (void)runtime.scene().set_local_transform(e->scene_node, t);
        }
    }
}

bool ExWorldGame::update(double dt) noexcept {
    if (flow_ == FlowState::Fatal) {
        // Keep the error screen alive; presenter still swaps frames.
        return true;
    }
    if (dt < 0.0) return false;
    time_ += dt;
    flow_time_ += static_cast<float>(dt);
    auto& runtime = engine_.session().game().runtime();
    update_sky(runtime);

    switch (flow_) {
    case FlowState::Loading: {
        (void)city_.step();
        if (!cinematic_begun_) {
            cinematic_begun_ = true;
            camera_.begin_cinematic({0.f, 0.f, 0.f}, 130.f, 72.f);
        }
        camera_.update_cinematic(static_cast<float>(dt));
        (void)runtime.set_main_camera(camera().camera());
        if (city_.done()) {
            register_city_content(runtime);
            if (!start_engine()) break; // fatal set inside
            begin_menu();
        }
        break;
    }
    case FlowState::Menu: {
        if (!cinematic_begun_) {
            cinematic_begun_ = true;
            camera_.begin_cinematic({0.f, 0.f, 0.f});
        }
        camera_.update_cinematic(static_cast<float>(dt));
        (void)runtime.set_main_camera(camera().camera());
        if (flow_time_ > 0.8f && !input_.taps().empty()) {
            input_.clear_taps();
            if (player_.valid()) {
                camera_.begin_fly_in(player_.position(runtime));
                flow_ = FlowState::FlyIn;
                flow_time_ = 0.f;
            } else {
                begin_gameplay();
            }
        }
        break;
    }
    case FlowState::FlyIn: {
        if (player_.valid()) {
            const bool still = camera_.fly_in(player_.position(runtime), static_cast<float>(dt));
            (void)runtime.set_main_camera(camera().camera());
            if (!still) begin_gameplay();
        } else {
            begin_gameplay();
        }
        break;
    }
    case FlowState::Playing: {
        // Pause button (hit rect index matches draw order: 0 = pause)
        bool paused = false;
        for (const auto& tap : input_.taps()) {
            if (ui_.hit_index(tap.x, tap.y) == 0) paused = true;
        }
        input_.clear_taps();
        if (paused) {
            (void)engine_.pause();
            flow_ = FlowState::Paused;
            flow_time_ = 0.f;
            break;
        }

        PlayerInput in = use_forced_input_ ? forced_input_ : input_.poll();
        use_forced_input_ = false;

        handle_interactions(static_cast<float>(dt), in, runtime);
        update_vehicle_possession(static_cast<float>(dt), in, runtime);

        if (player_.mode() == PlayerMode::InsideBuilding && interiors_.active()) {
            interiors_.update(static_cast<float>(dt), in.move_x, in.move_z,
                              player_.entity(), runtime);
        } else {
            player_.update(in, static_cast<float>(dt), runtime);
        }
        resolve_player_collisions(runtime);

        animation_.update(static_cast<float>(dt), player_.motion(), runtime);

        const auto pos = player_.position(runtime);
        const bool in_vehicle = player_.mode() == PlayerMode::InVehicle;
        sound_.update(static_cast<float>(dt), pos, player_.motion().speed, in_vehicle,
                      in_vehicle ? 2200.f : 0.f, runtime);
        world_.set_stream_focus(pos, runtime);
        police_.update(static_cast<float>(dt), pos, wanted_.level(), runtime);
        update_traffic_and_walkers(static_cast<float>(dt), runtime);
        update_camera(static_cast<float>(dt), in, runtime);
        update_region(pos);
        hud_.update(player_, wanted_, player_.motion().speed, region_name_);

        if (!engine_.update(dt)) {
            ++frame_failures_;
            if (frame_failures_ > 120) set_fatal("engine update kept failing");
        } else {
            frame_failures_ = 0;
        }
        break;
    }
    case FlowState::Paused: {
        // Resume = hit rect 0, main menu = hit rect 1 (draw order below)
        int choice = -1;
        for (const auto& tap : input_.taps()) {
            const int idx = ui_.hit_index(tap.x, tap.y);
            if (idx == 0 || idx == 1) choice = idx;
        }
        input_.clear_taps();
        if (choice == 0) {
            (void)engine_.resume();
            flow_ = FlowState::Playing;
            flow_time_ = 0.f;
        } else if (choice == 1) {
            (void)engine_.resume();
            begin_menu();
        }
        break;
    }
    case FlowState::Fatal:
    default:
        break;
    }

    draw_ui(runtime);
    return true;
}

bool ExWorldGame::build_frame(exgine::RenderFrame& frame, exgine::RenderResult& result) noexcept {
    if (flow_ == FlowState::Fatal && fatal_message_.empty()) return false;
    screen_w_ = 1280;
    screen_h_ = 720;
    exgine::Renderer renderer({exgine::RenderBackend::Headless, 1280, 720, true, true, 256, 128});
    if (!renderer.build_frame(engine_.session().game().runtime(), frame)) return false;
    result = renderer.submit(frame);
    return result.success;
}

bool ExWorldGame::present(exgine::AndroidEglPresenter& presenter, int width, int height) noexcept {
    if (width > 0) screen_w_ = static_cast<std::uint32_t>(width);
    if (height > 0) screen_h_ = static_cast<std::uint32_t>(height);
    if (height > 0) input_.set_screen(static_cast<float>(width) / static_cast<float>(height));
    const std::uint32_t w = width > 0 ? static_cast<std::uint32_t>(width) : 1280u;
    const std::uint32_t h = height > 0 ? static_cast<std::uint32_t>(height) : 720u;
    exgine::RenderFrame frame;
    exgine::Renderer renderer({exgine::RenderBackend::OpenGLES, w, h, true, true, 256, 128});
    auto& rt = engine_.session().game().runtime();
    if (!renderer.build_frame(rt, frame)) {
        (void)ensure_render_basics(rt, player_.valid() ? player_.position(rt) : exgine::Vec3{0, 2, 0},
                                   &sun_light_);
        if (!renderer.build_frame(rt, frame)) return false;
    }
    if (!renderer.validate(frame)) return false;
    return presenter.present(frame);
}

std::vector<std::uint8_t> ExWorldGame::save_game() const {
    return saves_.save(player_, wanted_, time_, engine_.session().game().runtime());
}

bool ExWorldGame::load_game(const std::vector<std::uint8_t>& bytes) {
    return saves_.load(bytes, player_, wanted_, time_, engine_.session().game().runtime());
}

bool ExWorldGame::save_to_file(const std::string& path) const {
    return saves_.save_to_file(path, player_, wanted_, time_, engine_.session().game().runtime());
}

bool ExWorldGame::load_from_file(const std::string& path) {
    return saves_.load_from_file(path, player_, wanted_, time_, engine_.session().game().runtime());
}

// ---------------------------------------------------------------------------
// UI drawing
// ---------------------------------------------------------------------------

void ExWorldGame::draw_star(UiRenderer& ui, float cx, float cy, float r, bool filled) {
    const auto color = filled ? UiColor::Accent : UiColor::Gray;
    float rim[10][2];
    for (int i = 0; i < 10; ++i) {
        const float a = -1.5707963f + static_cast<float>(i) * 0.31415926f;
        const float rad = i % 2 == 0 ? r : r * 0.45f;
        rim[i][0] = cx + std::cos(a) * rad;
        rim[i][1] = cy + std::sin(a) * rad;
    }
    for (int i = 0; i < 5; ++i) {
        const auto& a = rim[i * 2];
        const auto& b = rim[i * 2 + 1];
        std::array<float, 10> tri = {cx, cy, a[0], a[1], b[0], b[1], b[0], b[1], b[0], b[1]};
        ui.poly(tri, color);
    }
}

void ExWorldGame::draw_minimap(exgine::Runtime& runtime, const exgine::Vec3& pos) {
    auto& ui = ui_;
    const float px = 44.f, py = 44.f, size = 250.f;
    const float cx = px + size * 0.5f, cy = py + size * 0.5f;

    ui.rect(px - 3.f, py - 3.f, size + 6.f, size + 6.f, UiColor::Black);
    ui.rect(px, py, size, size, UiColor::Panel);

    const float yaw = camera_.yaw();
    const float cs = std::cos(yaw), sn = std::sin(yaw);
    const float scale = 0.40f;
    const float max_r = size * 0.5f - 6.f;

    auto to_map = [&](float wx, float wz, float& mx, float& my) {
        const float dx = wx - pos.x, dz = wz - pos.z;
        mx = (dx * cs - dz * sn) * scale;
        my = -(dx * sn + dz * cs) * scale;
    };
    auto clipped_line = [&](float wx1, float wz1, float wx2, float wz2, float th, UiColor col) {
        float x1, y1, x2, y2;
        to_map(wx1, wz1, x1, y1);
        to_map(wx2, wz2, x2, y2);
        x1 += cx; y1 += cy; x2 += cx; y2 += cy;
        const float dx = x2 - x1, dy = y2 - y1;
        const float len2 = dx * dx + dy * dy;
        if (len2 < 1.f) return;
        float t0 = 0.f, t1 = 1.f;
        // clip against circle (center cx,cy radius max_r)
        const float fx = x1 - cx, fy = y1 - cy;
        const float A = dx * dx + dy * dy;
        const float B = 2.f * (fx * dx + fy * dy);
        const float C = fx * fx + fy * fy - max_r * max_r;
        const float disc = B * B - 4.f * A * C;
        if (disc < 0.f) return; // segment entirely outside
        const float sq = std::sqrt(disc);
        const float ta = (-B - sq) / (2.f * A);
        const float tb = (-B + sq) / (2.f * A);
        if (C > 0.f) { t0 = ta; t1 = tb; } // starts outside: enter/exit
        else { t1 = tb; }
        if (t1 <= t0) return;
        ui.line(x1 + dx * t0, y1 + dy * t0, x1 + dx * t1, y1 + dy * t1, th, col);
    };

    // street grid
    for (int k = 0; k <= CityGenerator::kGrid; ++k) {
        const float line = CityGenerator::road_line(k);
        const float ext = CityGenerator::kHalf + 30.f;
        const bool ave = CityGenerator::is_avenue(k);
        clipped_line(-ext, line, ext, line, ave ? 5.f : 3.f, ave ? UiColor::Accent : UiColor::Dim);
        clipped_line(line, -ext, line, ext, ave ? 5.f : 3.f, ave ? UiColor::Accent : UiColor::Dim);
    }

    // vehicles (parked + traffic)
    for (const auto& v : city_.vehicles()) {
        float mx, my;
        to_map(v.x, v.z, mx, my);
        if (mx * mx + my * my > max_r * max_r) continue;
        ui.circle(cx + mx, cy + my, v.traffic ? 4.f : 3.f,
                  v.traffic ? UiColor::White : UiColor::Gray, 8);
    }
    // pedestrians + cops
    for (const auto& w : city_.walkers()) {
        auto* e = runtime.state().entities.get(w.id);
        if (!e) continue;
        const bool cop = e->name.rfind("Cop_", 0) == 0;
        float mx, my;
        to_map(e->transform.x, e->transform.z, mx, my);
        if (mx * mx + my * my > max_r * max_r) continue;
        ui.circle(cx + mx, cy + my, cop ? 5.f : 3.f, cop ? UiColor::Red : UiColor::Dim, 8);
    }

    // player arrow (always up — the map rotates)
    std::array<float, 10> arrow = {cx, cy - 11.f, cx - 8.f, cy + 9.f, cx + 8.f, cy + 9.f,
                                   cx + 8.f, cy + 9.f, cx + 8.f, cy + 9.f};
    ui.poly(arrow, UiColor::White);

    // border ring + north marker
    ui.ring(cx, cy, size * 0.5f + 1.f, 3.f, UiColor::Accent, 40);
    float nx, ny;
    to_map(pos.x, pos.z - 1000.f, nx, ny);
    const float nl = std::sqrt(nx * nx + ny * ny);
    if (nl > 1.f) {
        nx = nx / nl * (max_r - 12.f);
        ny = ny / nl * (max_r - 12.f);
        ui.text(cx + nx, cy + ny + 8.f, 22.f, "N", UiColor::Accent, true, true);
    }
}

void ExWorldGame::draw_touch_controls(exgine::Runtime&, bool in_vehicle) {
    auto& ui = ui_;
    // Pause (hit rect index 0 in Playing)
    ui.circle(62.f, 62.f, 46.f, UiColor::Panel);
    ui.ring(62.f, 62.f, 46.f, 3.f, UiColor::Dim, 28);
    ui.text(62.f, 62.f, 30.f, "II", UiColor::White, true, true);
    ui.hit_rect(62.f - 60.f, 62.f - 60.f, 62.f + 60.f, 62.f + 60.f);

    // Action buttons
    for (const auto& b : input_.button_layout(in_vehicle)) {
        if (b.action == TouchAction::Pause) continue;
        const bool held = input_.button_held(b.action);
        ui.circle(b.cx, b.cy, b.r, UiColor::Panel);
        ui.ring(b.cx, b.cy, b.r, held ? 6.f : 3.f, held ? UiColor::Accent : UiColor::Dim, 32);
        ui.text(b.cx, b.cy, b.r > 80.f ? 30.f : 24.f, b.label,
                held ? UiColor::Accent : UiColor::White, true, true);
        ui.hit_rect(b.cx - b.r - 24.f, b.cy - b.r - 24.f, b.cx + b.r + 24.f, b.cy + b.r + 24.f);
    }

    // Virtual joystick
    if (input_.stick_active()) {
        const float bx = input_.stick_base_x() * ui.canvas_w();
        const float by = input_.stick_base_y() * ui.canvas_h();
        const float kx = input_.stick_knob_dx() * ui.canvas_h();
        const float ky = input_.stick_knob_dy() * ui.canvas_h();
        ui.circle(bx, by, 118.f, UiColor::Panel);
        ui.ring(bx, by, 118.f, 4.f, UiColor::Dim, 36);
        ui.circle(bx + kx, by + ky, 52.f, UiColor::Accent);
        ui.ring(bx + kx, by + ky, 52.f, 3.f, UiColor::White, 24);
    } else {
        // faint hint where the stick can spawn
        const float hx = ui.canvas_w() * 0.20f, hy = ui.canvas_h() * 0.76f;
        ui.ring(hx, hy, 96.f, 2.5f, UiColor::Gray, 32);
        ui.text(hx, hy, 20.f, "MOVE", UiColor::Gray, true, true);
    }
}

void ExWorldGame::draw_hud(exgine::Runtime& runtime) {
    auto& ui = ui_;
    const auto& hs = hud_.state();
    const bool in_vehicle = hs.in_vehicle;
    const auto pos = player_.position(runtime);

    draw_minimap(runtime, pos);
    ui.text(316.f, 62.f, 34.f, hs.region_name, UiColor::Accent, false, true);

    // Wanted stars (top center-right)
    const float stars_x = ui.canvas_w() - 150.f;
    for (int i = 0; i < 5; ++i)
        draw_star(ui, stars_x - static_cast<float>(i) * 44.f, 62.f, 17.f, i < hs.wanted_stars);

    // Speed (in vehicle)
    if (in_vehicle) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d KM/H", static_cast<int>(hs.speed_kmh + 0.5f));
        ui.text(ui.canvas_w() - 40.f, ui.canvas_h() - 240.f, 46.f, buf, UiColor::White,
                true, true);
    }

    // Context prompt
    if (hs.show_prompt && !hs.prompt.empty())
        ui.text(ui.canvas_w() * 0.5f, ui.canvas_h() - 150.f, 30.f, hs.prompt,
                UiColor::White, true, true);

    draw_touch_controls(runtime, in_vehicle);
}

void ExWorldGame::draw_ui(exgine::Runtime& runtime) {
    auto& ui = ui_;
    ui.begin(camera_.camera(), static_cast<float>(screen_w_), static_cast<float>(screen_h_));

    switch (flow_) {
    case FlowState::Loading: {
        const float cw = ui.canvas_w();
        ui.rect(0.f, 0.f, cw, ui.canvas_h(), UiColor::Black);
        const float prog = city_.progress();

        ui.text(cw * 0.5f, 300.f, 110.f, "EXWORLD", UiColor::Accent, true, true);
        ui.text(cw * 0.5f, 385.f, 30.f, "ORUBUS OPEN WORLD", UiColor::Dim, true, true);

        const float bar_w = std::min(cw * 0.62f, 900.f);
        ui.rect(cw * 0.5f - bar_w * 0.5f - 3.f, 487.f, bar_w + 6.f, 26.f, UiColor::Panel);
        ui.rect(cw * 0.5f - bar_w * 0.5f, 490.f, bar_w * std::clamp(prog, 0.02f, 1.f), 20.f,
                UiColor::Accent);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "BUILDING CITY %d%%",
                      static_cast<int>(std::clamp(prog, 0.f, 1.f) * 100.f));
        ui.text(cw * 0.5f, 560.f, 26.f, buf, UiColor::White, true, true);
        ui.text(cw * 0.5f, ui.canvas_h() - 60.f, 20.f, "STREAMING DISTRICTS - TRAFFIC - PEDESTRIANS",
                UiColor::Gray, true, true);
        break;
    }
    case FlowState::Menu: {
        const float cw = ui.canvas_w();
        const float ch = ui.canvas_h();
        // letterbox bands
        ui.rect(0.f, 0.f, cw, 120.f, UiColor::Black);
        ui.rect(0.f, ch - 120.f, cw, 120.f, UiColor::Black);

        ui.text(cw * 0.5f, 250.f, 130.f, "EXWORLD", UiColor::Accent, true, true);
        ui.text(cw * 0.5f, 350.f, 32.f, "ORUBUS OPEN WORLD", UiColor::Dim, true, true);

        const bool pulse = std::fmod(flow_time_, 1.4f) < 1.0f;
        ui.text(cw * 0.5f, ch - 210.f, 40.f, "TAP TO START",
                pulse ? UiColor::White : UiColor::Dim, true, true);
        ui.text(cw * 0.5f, ch - 150.f, 22.f, "V0.5.0 - PROCEDURAL CITY BUILD 7777",
                UiColor::Gray, true, true);
        ui.text(40.f, ch - 60.f, 20.f, "ORUBUS DEV", UiColor::Dim);
        ui.text(cw - 40.f, ch - 60.f, 20.f, "GLES3 - 60FPS", UiColor::Dim, true, false);
        break;
    }
    case FlowState::FlyIn:
        // clean transition, no UI
        break;
    case FlowState::Playing:
        draw_hud(runtime);
        break;
    case FlowState::Paused: {
        const float cw = ui.canvas_w();
        const float ch = ui.canvas_h();
        ui.rect(0.f, 0.f, cw, ch, UiColor::Black);
        ui.text(cw * 0.5f, 280.f, 80.f, "PAUSED", UiColor::White, true, true);

        const float bw = 520.f, bh = 96.f;
        ui.rect(cw * 0.5f - bw * 0.5f, 400.f, bw, bh, UiColor::Panel);
        ui.text(cw * 0.5f, 448.f, 36.f, "RESUME", UiColor::Accent, true, true);
        ui.hit_rect(cw * 0.5f - bw * 0.5f, 400.f, cw * 0.5f + bw * 0.5f, 400.f + bh);

        ui.rect(cw * 0.5f - bw * 0.5f, 530.f, bw, bh, UiColor::Panel);
        ui.text(cw * 0.5f, 578.f, 36.f, "MAIN MENU", UiColor::White, true, true);
        ui.hit_rect(cw * 0.5f - bw * 0.5f, 530.f, cw * 0.5f + bw * 0.5f, 530.f + bh);
        break;
    }
    case FlowState::Fatal: {
        const float cw = ui.canvas_w();
        ui.rect(0.f, 0.f, cw, ui.canvas_h(), UiColor::Black);
        ui.text(cw * 0.5f, 300.f, 60.f, "BOOT ERROR", UiColor::Red, true, true);
        ui.text(cw * 0.5f, 420.f, 26.f, fatal_message_, UiColor::White, true, true);
        ui.text(cw * 0.5f, 500.f, 24.f, "CLOSE AND REOPEN THE APP", UiColor::Dim, true, true);
        break;
    }
    }

    ui.end(runtime);
}

} // namespace exworld
