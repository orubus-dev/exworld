#include "exworld/hud.hpp"

#include <cmath>

namespace exworld {

void Hud::update(const Player& player, const WantedSystem& wanted, float speed_mps,
                 std::string_view region) {
    state_.wanted_stars = static_cast<int>(wanted.level());
    state_.speed_kmh = std::fabs(speed_mps) * 3.6f;
    state_.in_vehicle = player.mode() == PlayerMode::InVehicle ||
                        player.mode() == PlayerMode::EnteringVehicle;
    state_.in_building = player.mode() == PlayerMode::InsideBuilding ||
                         player.mode() == PlayerMode::EnteringBuilding;
    state_.region_name = std::string(region);

    state_.show_prompt = false;
    state_.prompt.clear();
    if (player.mode() == PlayerMode::OnFoot) {
        state_.show_prompt = true;
        state_.prompt = "TAP E - ENTER VEHICLE OR DOOR";
    } else if (state_.in_vehicle) {
        state_.show_prompt = true;
        state_.prompt = "TAP EXIT TO LEAVE VEHICLE";
    } else if (state_.in_building) {
        state_.show_prompt = true;
        state_.prompt = "TAP EXIT TO LEAVE BUILDING";
    }
}

} // namespace exworld
