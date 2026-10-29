// Same heap-AppState fix as android/ path (keep trees in sync)
#include "exworld/game.hpp"

#include "exgine/android.hpp"
#include "exgine/android_audio.hpp"
#include "exgine/mobile.hpp"
#include "exgine/render.hpp"
#include "exgine/asset.hpp"

#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kTag = "EXWORLD";
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kTag, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, kTag, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, kTag, __VA_ARGS__)

std::uint64_t monotonic_time_ns() {
    timespec v{};
    if (clock_gettime(CLOCK_MONOTONIC, &v) != 0) return 0;
    return static_cast<std::uint64_t>(v.tv_sec) * 1000000000ull +
           static_cast<std::uint64_t>(v.tv_nsec);
}

bool read_asset_bytes(AAssetManager* m, std::string_view path, std::vector<std::uint8_t>& out) {
    if (!m) return false;
    AAsset* a = AAssetManager_open(m, std::string(path).c_str(), AASSET_MODE_BUFFER);
    if (!a) return false;
    const auto n = static_cast<std::size_t>(AAsset_getLength64(a));
    out.resize(n);
    const auto got = AAsset_read(a, out.data(), n);
    AAsset_close(a);
    return got >= 0 && static_cast<std::size_t>(got) == n;
}

bool read_asset(AAssetManager* m, std::string_view path, std::string& out) { std::vector<std::uint8_t> bytes; if (!read_asset_bytes(m,path,bytes)) return false; out.assign(reinterpret_cast<const char*>(bytes.data()),bytes.size()); return true; }

std::string normalize_asset_path(std::string_view path) { std::string p(path); while(p.rfind("./",0)==0)p.erase(0,2); if(p.rfind("content/",0)==0)p.erase(0,8); return p; }

bool resolve_and_load(AAssetManager* m, std::string_view path, std::string& out) {
    return resolve_and_load(m, static_cast<const exgine::AssetDatabase*>(nullptr), path, out);
}

bool resolve_and_load(AAssetManager* m, const exgine::AssetDatabase* package, std::string_view path, std::string& out) {
    const std::string normalized = normalize_asset_path(path);
    if (package) { if (const auto* a=package->find_uri(normalized)) { out.assign(reinterpret_cast<const char*>(a->data.data()),a->data.size()); return true; } if (const auto* a=package->find_uri(std::string("content/")+normalized)) { out.assign(reinterpret_cast<const char*>(a->data.data()),a->data.size()); return true; } }
    std::string p(normalized);
    if (p.rfind("./", 0) == 0) p = p.substr(2);
    if (p.rfind("content/", 0) == 0) p = p.substr(8);
    if (p == "City" || p == "Main" || p == "CityScene") p = "scenes/city.scene";
    else if (p == "FirstLight" || p == "FirstLightScene")
        p = "first_light/scenes/first_light.scene";
    if (read_asset(m, p, out)) return true;
    if (read_asset(m, std::string("content/") + p, out)) return true;
    if (read_asset(m, path, out)) return true;
    return false;
}

struct AppState {
    exgine::AndroidEglPresenter presenter;
    exgine::MobileRuntimeBridge mobile;
    std::unique_ptr<exgine::AndroidAudioBackend> audio;
    std::unique_ptr<exworld::ExWorldGame> game;
    AAssetManager* assets = nullptr;
    exgine::AssetDatabase package;
    bool package_loaded = false;
    int width = 1280;
    int height = 720;
    double previous_time = 0;
    bool started = false;
    bool boot_ok = false;
    bool boot_attempted = false;
    std::string boot_error;
};

void feed_touch(AppState* s, const AInputEvent* e, std::size_t i, bool down) {
    if (!s || !s->game) return;
    const float x = AMotionEvent_getX(e, i);
    const float y = AMotionEvent_getY(e, i);
    const float nx = s->width > 0 ? x / static_cast<float>(s->width) : 0.f;
    const float ny = s->height > 0 ? y / static_cast<float>(s->height) : 0.f;
    s->game->input().set_touch(AMotionEvent_getPointerId(e, i), down, nx, ny);
}

int32_t handle_input(android_app* app, AInputEvent* input) {
    auto* s = static_cast<AppState*>(app->userData);
    if (!s || !input || !s->game) return 0;
    if (AInputEvent_getType(input) == AINPUT_EVENT_TYPE_KEY) {
        const int action = AKeyEvent_getAction(input);
        if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_UP) return 0;
        const int code = AKeyEvent_getKeyCode(input);
        const bool down = action == AKEY_EVENT_ACTION_DOWN;
        int mapped = 0;
        switch (code) {
        case AKEYCODE_W: mapped = 'W'; break;
        case AKEYCODE_A: mapped = 'A'; break;
        case AKEYCODE_S: mapped = 'S'; break;
        case AKEYCODE_D: mapped = 'D'; break;
        case AKEYCODE_E: mapped = 'E'; break;
        case AKEYCODE_F: mapped = 'F'; break;
        case AKEYCODE_SPACE: mapped = 32; break;
        case AKEYCODE_SHIFT_LEFT:
        case AKEYCODE_SHIFT_RIGHT: mapped = 16; break;
        case AKEYCODE_BUTTON_A: s->game->input().set_gamepad_button(0, down); return 1;
        case AKEYCODE_BUTTON_B: s->game->input().set_gamepad_button(1, down); return 1;
        default: break;
        }
        if (mapped) { s->game->input().set_key(mapped, down); return 1; }
        return 0;
    }
    if (AInputEvent_getType(input) != AINPUT_EVENT_TYPE_MOTION) return 0;
    if ((AInputEvent_getSource(input) & AINPUT_SOURCE_CLASS_POINTER) == 0) return 0;
    const int32_t action = AMotionEvent_getAction(input);
    const int32_t type = action & AMOTION_EVENT_ACTION_MASK;
    const std::size_t idx = static_cast<std::size_t>(
        (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    const std::size_t count = AMotionEvent_getPointerCount(input);
    if (type == AMOTION_EVENT_ACTION_DOWN || type == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        if (idx < count) feed_touch(s, input, idx, true); return 1;
    }
    if (type == AMOTION_EVENT_ACTION_UP || type == AMOTION_EVENT_ACTION_POINTER_UP) {
        if (idx < count) feed_touch(s, input, idx, false); return 1;
    }
    if (type == AMOTION_EVENT_ACTION_MOVE) {
        for (std::size_t i = 0; i < count; ++i) feed_touch(s, input, i, true);
        return 1;
    }
    return 0;
}

bool boot_game(AppState& state) {
    try {
        std::vector<std::uint8_t> package_bytes;
        if (read_asset_bytes(state.assets, "world.exg", package_bytes)) {
            const auto loaded = state.package.unpack(package_bytes);
            state.package_loaded = loaded.success;
            if (loaded.success) LOGI("world.exg loaded: %zu assets", state.package.size());
            else LOGW("world.exg rejected: %s", loaded.error.c_str());
        } else LOGW("world.exg missing; using direct assets");
        auto loader = [&state](std::string_view path, std::string& out) -> bool {
            return resolve_and_load(state.assets, state.package_loaded ? &state.package : nullptr, path, out);
        };
        state.game = std::make_unique<exworld::ExWorldGame>(loader);
        std::string manifest;
        if (!resolve_and_load(state.assets, "project.exg", manifest) &&
            !resolve_and_load(state.assets, "first_light/project.exg", manifest)) {
            state.boot_error = "project.exg missing";
            return false;
        }
        if (!state.game->open_from_manifest(manifest)) {
            state.boot_error = "open_from_manifest failed";
            return false;
        }
        return true;
    } catch (...) {
        state.boot_error = "boot exception";
        return false;
    }
}

void handle_cmd(android_app* app, int32_t cmd) {
    auto* s = static_cast<AppState*>(app->userData);
    if (!s) return;
    switch (cmd) {
    case APP_CMD_START: s->mobile.on_start(); break;
    case APP_CMD_RESUME: s->mobile.on_resume(); break;
    case APP_CMD_PAUSE: s->mobile.on_pause(); break;
    case APP_CMD_STOP:
        s->mobile.on_stop();
        if (s->audio) s->audio->stop();
        s->started = false;
        break;
    case APP_CMD_INIT_WINDOW:
        if (app->window && s->presenter.attach(app->window)) {
            s->mobile.on_surface_available();
            s->width = ANativeWindow_getWidth(app->window);
            s->height = ANativeWindow_getHeight(app->window);
        }
        break;
    case APP_CMD_TERM_WINDOW:
        s->mobile.on_surface_lost();
        s->presenter.detach();
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONTENT_RECT_CHANGED:
        (void)s->presenter.resize();
        if (app->window) {
            s->width = ANativeWindow_getWidth(app->window);
            s->height = ANativeWindow_getHeight(app->window);
        }
        break;
    default: break;
    }
}

} // namespace

void android_main(android_app* app) {
    LOGI("android_main enter (heap)");
    auto* state = new (std::nothrow) AppState();
    if (!state) return;
    state->assets = app->activity ? app->activity->assetManager : nullptr;
    app->userData = state;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;
    state->audio = std::make_unique<exgine::AndroidAudioBackend>();
    if (!state->audio->start()) state->audio.reset();

    for (;;) {
        int ident = 0, events = 0;
        android_poll_source* source = nullptr;
        while ((ident = ALooper_pollOnce(state->mobile.renderable() ? 0 : -1, nullptr, &events,
                                         reinterpret_cast<void**>(&source))) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) {
                state->mobile.on_destroy();
                state->mobile.on_surface_lost();
                if (state->audio) state->audio->stop();
                state->presenter.detach();
                delete state;
                app->userData = nullptr;
                return;
            }
        }

        if (!state->boot_attempted && state->mobile.renderable() && state->presenter.ready()) {
            state->boot_attempted = true;
            state->boot_ok = boot_game(*state);
        }
        if (!state->mobile.renderable() || !state->presenter.ready()) continue;

        const double now = static_cast<double>(monotonic_time_ns()) / 1e9;
        const double dt = state->previous_time > 0
                              ? std::clamp(now - state->previous_time, 0.0, 0.05) : 1.0 / 60.0;
        state->previous_time = now;
        if (state->mobile.begin_frame(now).state == exgine::MobileFrameState::Paused) continue;
        if (!state->boot_ok) continue;

        if (!state->started) {
            try { state->started = state->game && state->game->start(); }
            catch (...) { state->started = false; }
            if (!state->started) continue;
            LOGI("game started");
        }
        try {
            if (state->game && state->game->update(dt))
                (void)state->game->present(state->presenter, state->width, state->height);
        } catch (...) {}
    }
}
