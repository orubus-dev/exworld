#pragma once

#include "exgine/playable.hpp"
#include "exgine/runtime.hpp"

#include "exworld/player.hpp"
#include "exworld/vehicle.hpp"
#include "exworld/world.hpp"
#include "exworld/animation_driver.hpp"
#include "exworld/sound_director.hpp"
#include "exworld/wanted.hpp"
#include "exworld/camera.hpp"
#include "exworld/citygen.hpp"
#include "exworld/input.hpp"
#include "exworld/police.hpp"
#include "exworld/interior.hpp"
#include "exworld/save.hpp"
#include "exworld/asset_package.hpp"
#include "exworld/hud.hpp"
#include "exworld/ui.hpp"
#include "exworld/visuals.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace exgine {
class AndroidEglPresenter;
struct RenderFrame;
struct RenderResult;
}

namespace exworld {

// Full game flow: loading (city build) -> main menu (cinematic) -> fly-in ->
// gameplay -> pause. Fatal shows an on-screen boot error instead of a black
// screen.
enum class FlowState : std::uint8_t { Loading, Menu, FlyIn, Playing, Paused, Fatal };

class ExWorldGame {
public:
    explicit ExWorldGame(exgine::ProjectSourceLoader loader = {});

    [[nodiscard]] bool open(std::string_view content_root = "content");
    [[nodiscard]] bool open_from_manifest(std::string_view manifest_text);
    [[nodiscard]] bool start() noexcept;

    [[nodiscard]] bool update(double dt) noexcept;
    [[nodiscard]] bool build_frame(exgine::RenderFrame& frame, exgine::RenderResult& result) noexcept;
    [[nodiscard]] bool present(exgine::AndroidEglPresenter& presenter, int width, int height) noexcept;

    InputSystem& input() noexcept { return input_; }
    void set_input(const PlayerInput& input) noexcept;

    [[nodiscard]] std::vector<std::uint8_t> save_game() const;
    [[nodiscard]] bool load_game(const std::vector<std::uint8_t>& bytes);
    [[nodiscard]] bool save_to_file(const std::string& path) const;
    [[nodiscard]] bool load_from_file(const std::string& path);

    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] FlowState flow() const noexcept { return flow_; }
    [[nodiscard]] Player& player() noexcept { return player_; }
    [[nodiscard]] WantedSystem& wanted() noexcept { return wanted_; }
    [[nodiscard]] PoliceAI& police() noexcept { return police_; }
    [[nodiscard]] GameCamera& camera() noexcept { return camera_; }
    [[nodiscard]] InteriorNavigator& interiors() noexcept { return interiors_; }
    [[nodiscard]] AssetPackageRegistry& packages() noexcept { return packages_; }
    [[nodiscard]] Hud& hud() noexcept { return hud_; }
    [[nodiscard]] exgine::PlayableGame& engine() noexcept { return engine_; }

    // Skips loading + menu straight into gameplay (desktop validation builds).
    void debug_skip_menu() noexcept;

private:
    exgine::ProjectSourceLoader loader_;
    exgine::PlayableGame engine_;

    World world_;
    Player player_;
    VehicleController vehicles_;
    AnimationDriver animation_;
    SoundDirector sound_;
    WantedSystem wanted_;
    GameCamera camera_;
    InputSystem input_;
    PoliceAI police_;
    InteriorNavigator interiors_;
    SaveSystem saves_;
    AssetPackageRegistry packages_;
    Hud hud_;
    VisualSystem visuals_;
    UiRenderer ui_;
    CityGenerator city_;

    FlowState flow_ = FlowState::Loading;
    std::string fatal_message_;
    double time_ = 0.0;
    float flow_time_ = 0.f;
    bool ready_ = false;
    bool configured_ = false;
    bool engine_running_ = false;
    bool cinematic_begun_ = false;
    std::uint32_t frame_failures_ = 0;
    // On-screen render diagnostics (bottom-left of every screen).
    bool diag_ok_ = false;
    std::size_t diag_draws_ = 0, diag_skipped_ = 0, diag_culled_ = 0;
    std::uint64_t sun_light_ = 0;
    std::string region_name_ = "CENTRAL PLAZA";
    std::uint32_t screen_w_ = 1280;
    std::uint32_t screen_h_ = 720;
    exgine::Vec3 last_player_pos_{};
    bool last_pos_valid_ = false;
    float player_vel_x_ = 0.f, player_vel_z_ = 0.f;
    float vehicle_yaw_ = 0.f;

    PlayerInput forced_input_{};
    bool use_forced_input_ = false;

    [[nodiscard]] bool configure_systems();
    [[nodiscard]] bool spawn_world_content();
    [[nodiscard]] bool load_baked_packages();
    [[nodiscard]] bool start_engine();
    void register_city_content(exgine::Runtime& runtime);
    void begin_menu();
    void begin_gameplay();
    void set_fatal(std::string message);
    void handle_interactions(float dt, const PlayerInput& in, exgine::Runtime& runtime);
    void update_vehicle_possession(float dt, const PlayerInput& in, exgine::Runtime& runtime);
    void update_sky(exgine::Runtime& runtime);
    void update_camera(float dt, const PlayerInput& in, exgine::Runtime& runtime);
    void update_traffic_and_walkers(float dt, exgine::Runtime& runtime);
    void resolve_player_collisions(exgine::Runtime& runtime);
    void update_region(const exgine::Vec3& pos);
    void draw_ui(exgine::Runtime& runtime);
    void draw_hud(exgine::Runtime& runtime);
    void draw_minimap(exgine::Runtime& runtime, const exgine::Vec3& pos);
    void draw_touch_controls(exgine::Runtime& runtime, bool in_vehicle);
    void draw_star(UiRenderer& ui, float cx, float cy, float r, bool filled);
};

} // namespace exworld
