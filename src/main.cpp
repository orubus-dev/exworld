#include "exworld/game.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string read_file(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path content_root =
        argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path("content");

    auto loader = [content_root](std::string_view uri, std::string& out) -> bool {
        std::string u(uri);
        // Same name→path map as Android (engine passes startup_scene token)
        if (u == "City" || u == "CityScene") u = "scenes/city.scene";
        if (u == "FirstLight" || u == "FirstLightScene") u = "first_light/scenes/first_light.scene";

        out = read_file(content_root / u);
        if (out.empty()) out = read_file(content_root / std::string(uri));
        if (out.empty() && u.find('.') == std::string::npos)
            out = read_file(content_root / ("scenes/" + u + ".scene"));
        return !out.empty();
    };

    exworld::ExWorldGame game(loader);

    if (!game.open(content_root.string())) {
        std::cerr << "EXWORLD: failed to open game project\n";
        return 1;
    }
    // Desktop validation: build the whole city, skip menu, drive gameplay.
    game.debug_skip_menu();
    if (!game.ready()) {
        std::cerr << "EXWORLD: failed to start\n";
        return 2;
    }

    std::cout << "EXWORLD open+start OK (Sebastian + procedural city)\n";

    constexpr int kFrames = 300;
    exgine::RenderFrame frame;
    exgine::RenderResult result;
    for (int i = 0; i < kFrames; ++i) {
        exworld::PlayerInput in;
        in.move_z = 0.4f;
        if (i == 100) in.interact = true;
        game.set_input(in);
        if (!game.update(1.0 / 60.0)) {
            std::cerr << "update failed frame " << i << "\n";
            return 3;
        }
        if (!game.build_frame(frame, result) || !result.success) {
            std::cerr << "render failed frame " << i << "\n";
            return 4;
        }
        if (i % 120 == 0)
            std::cout << "frame " << i << " draws=" << result.stats.submitted_draws << "\n";
    }
    std::cout << "EXWORLD validation passed frames=" << kFrames << "\n";
    return 0;
}
