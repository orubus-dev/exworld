// EXWORLD Android NativeActivity
// CRITICAL: AppState MUST be heap-allocated. AndroidAudioBackend contains
// std::array<float,262144> (~1MB). Stack placement overflows the native
// thread stack and produces SIGSEGV ("keeps stopping") before any game code runs.

#include "exworld/game.hpp"

#include "exgine/android.hpp"
#include "exgine/android_audio.hpp"
#include "exgine/engine.hpp"
#include "exgine/mobile.hpp"
#include "exgine/render.hpp"

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

bool read_asset(AAssetManager* m, std::string_view path, std::string& out) {
    if (!m) return false;
    AAsset* a = AAssetManager_open(m, std::string(path).c_str(), AASSET_MODE_BUFFER);
    if (!a) return false;
    const auto n = static_cast<std::size_t>(AAsset_getLength64(a));
    out.resize(n);
    const auto got = AAsset_read(a, out.data(), n);
    AAsset_close(a);
    return got >= 0 && static_cast<std::size_t>(got) == n;
}

bool resolve_and_load(AAssetManager* m, std::string_view path, std::string& out) {
    std::string p(path);
    if (p.rfind("./", 0) == 0) p = p.substr(2);
    if (p.rfind("content/", 0) == 0) p = p.substr(8);
    if (p == "City" || p == "Main" || p == "CityScene") p = "scenes/city.scene";
    else if (p == "FirstLight" || p == "FirstLightScene")
        p = "first_light/scenes/first_light.scene";

    if (read_asset(m, p, out)) return true;
    if (read_asset(m, std::string("content/") + p, out)) return true;
    if (read_asset(m, std::string("first_light/") + p, out)) return true;
    if (read_asset(m, path, out)) return true;
    if (p.find('.') == std::string::npos) {
        if (read_asset(m, p + ".scene", out)) return true;
        if (read_asset(m, std::string("scenes/") + p + ".scene", out)) return true;
    }
    return false;
}

// On-device error screen used when boot fails — NEVER a silent black screen.
struct FatalScreen {
    std::unique_ptr<exgine::Engine> engine;
    exworld::UiRenderer ui;
    bool ready = false;

    bool init() {
        const std::string dsl =
            "game EXWORLD { world Fatal { terrain T { amplitude = 0 seed = 1 } } }";
        engine = std::make_unique<exgine::Engine>();
        if (!engine->load(exgine::SourceText{dsl})) return false;
        auto& rt = engine->runtime();
        exgine::Camera cam;
        cam.position = {0, 0, 0};
        cam.rotation = {0, 0, 0};
        cam.vertical_fov_degrees = 58.f;
        cam.near_plane = 0.1f;
        cam.far_plane = 100.f;
        if (!rt.set_main_camera(cam)) return false;
        ready = ui.init(rt);
        return ready;
    }

    void draw(exgine::AndroidEglPresenter& presenter, const std::string& msg,
              float width, float height) {
        if (!ready && !init()) {
            // Last resort: an empty frame swap still keeps the app alive.
            exgine::RenderFrame frame;
            frame.config = {exgine::RenderBackend::OpenGLES,
                            static_cast<std::uint32_t>(width),
                            static_cast<std::uint32_t>(height), true, true, 256, 128};
            (void)presenter.present(frame);
            return;
        }
        auto& rt = engine->runtime();
        ui.begin(rt.lighting().main_camera(), width, height);
        const float cw = ui.canvas_w(), ch = ui.canvas_h();
        ui.rect(0.f, 0.f, cw, ch, exworld::UiColor::Black);
        ui.text(cw * 0.5f, 300.f, 60.f, "BOOT ERROR", exworld::UiColor::Red, true, true);
        ui.text(cw * 0.5f, 430.f, 26.f, msg, exworld::UiColor::White, true, true);
        ui.text(cw * 0.5f, 520.f, 24.f, "CLOSE AND REOPEN THE APP", exworld::UiColor::Dim,
                true, true);
        ui.end(rt);

        exgine::Renderer renderer({exgine::RenderBackend::OpenGLES,
                                   static_cast<std::uint32_t>(width),
                                   static_cast<std::uint32_t>(height), true, true, 256, 128});
        exgine::RenderFrame frame;
        if (renderer.build_frame(rt, frame) && renderer.validate(frame))
            (void)presenter.present(frame);
    }
};

struct AppState {
    exgine::AndroidEglPresenter presenter;
    exgine::MobileRuntimeBridge mobile;
    std::unique_ptr<exgine::AndroidAudioBackend> audio;
    std::unique_ptr<exworld::ExWorldGame> game;
    FatalScreen fatal;
    AAssetManager* assets = nullptr;
    int width = 1280;
    int height = 720;
    double previous_time = 0;
    bool boot_ok = false;
    bool boot_attempted = false;
    std::string boot_error;
};

void feed_touch(AppState* s, const AInputEvent* e, std::size_t i, bool down) {
    if (!s || !s->game) return;
    const float x = AMotionEvent_getX(e, i);
    const float y = AMotionEvent_getY(e, i);
    const float nx = s->height > 0 ? x / static_cast<float>(s->height) : 0.f;
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
        if (idx < count) feed_touch(s, input, idx, true);
        return 1;
    }
    if (type == AMOTION_EVENT_ACTION_UP || type == AMOTION_EVENT_ACTION_POINTER_UP) {
        if (idx < count) feed_touch(s, input, idx, false);
        return 1;
    }
    if (type == AMOTION_EVENT_ACTION_MOVE) {
        for (std::size_t i = 0; i < count; ++i) feed_touch(s, input, i, true);
        return 1;
    }
    return 0;
}

bool boot_game(AppState& state) {
    try {
        auto loader = [&state](std::string_view path, std::string& out) -> bool {
            const bool ok = resolve_and_load(state.assets, path, out);
            if (!ok) LOGW("asset [%.*s] MISS", (int)path.size(), path.data());
            return ok;
        };

        state.game = std::make_unique<exworld::ExWorldGame>(loader);

        std::string manifest;
        if (!resolve_and_load(state.assets, "project.exg", manifest) &&
            !resolve_and_load(state.assets, "first_light/project.exg", manifest)) {
            state.boot_error = "project.exg missing from APK assets";
            LOGE("%s", state.boot_error.c_str());
            return false;
        }
        LOGI("manifest %zu bytes", manifest.size());

        if (!state.game->open_from_manifest(manifest)) {
            state.boot_error = "open_from_manifest failed";
            LOGE("%s", state.boot_error.c_str());
            return false;
        }
        LOGI("boot OK (loading flow started)");
        return true;
    } catch (const std::exception& e) {
        state.boot_error = std::string("exception: ") + e.what();
        LOGE("%s", state.boot_error.c_str());
        return false;
    } catch (...) {
        state.boot_error = "unknown exception during boot";
        LOGE("%s", state.boot_error.c_str());
        return false;
    }
}

void handle_cmd(android_app* app, int32_t cmd) {
    auto* s = static_cast<AppState*>(app->userData);
    if (!s) return;
    switch (cmd) {
    case APP_CMD_START:
        LOGI("APP_CMD_START");
        s->mobile.on_start();
        break;
    case APP_CMD_RESUME:
        LOGI("APP_CMD_RESUME");
        s->mobile.on_resume();
        break;
    case APP_CMD_PAUSE:
        LOGI("APP_CMD_PAUSE");
        s->mobile.on_pause();
        break;
    case APP_CMD_STOP:
        LOGI("APP_CMD_STOP");
        s->mobile.on_stop();
        if (s->audio) s->audio->stop();
        break;
    case APP_CMD_INIT_WINDOW:
        LOGI("APP_CMD_INIT_WINDOW");
        if (app->window && s->presenter.attach(app->window)) {
            s->mobile.on_surface_available();
            s->width = ANativeWindow_getWidth(app->window);
            s->height = ANativeWindow_getHeight(app->window);
            LOGI("surface %dx%d attached", s->width, s->height);
        } else {
            LOGE("presenter.attach failed: %s", s->presenter.last_error().c_str());
        }
        break;
    case APP_CMD_TERM_WINDOW:
        LOGI("APP_CMD_TERM_WINDOW");
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
    default:
        break;
    }
}

} // namespace

void android_main(android_app* app) {
    LOGI("android_main enter (heap AppState)");

    auto* state = new (std::nothrow) AppState();
    if (!state) {
        LOGE("failed to allocate AppState");
        return;
    }
    state->assets = app->activity ? app->activity->assetManager : nullptr;

    app->userData = state;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;

    state->audio = std::make_unique<exgine::AndroidAudioBackend>();
    if (!state->audio->start()) {
        LOGW("AAudio unavailable");
        state->audio.reset();
    }

    for (;;) {
        int ident = 0, events = 0;
        android_poll_source* source = nullptr;
        while ((ident = ALooper_pollOnce(state->mobile.renderable() ? 0 : -1,
                                         nullptr, &events,
                                         reinterpret_cast<void**>(&source))) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) {
                LOGI("destroyRequested — exit clean");
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
            if (!state->boot_ok)
                LOGE("Boot failed: %s — showing error screen", state->boot_error.c_str());
        }

        if (!state->mobile.renderable() || !state->presenter.ready()) continue;

        const double now = static_cast<double>(monotonic_time_ns()) / 1e9;
        const double dt = state->previous_time > 0
                              ? std::clamp(now - state->previous_time, 0.0, 0.05)
                              : 1.0 / 60.0;
        state->previous_time = now;

        if (state->mobile.begin_frame(now).state == exgine::MobileFrameState::Paused) continue;

        const float wf = static_cast<float>(state->width > 0 ? state->width : 1280);
        const float hf = static_cast<float>(state->height > 0 ? state->height : 720);

        if (state->boot_ok && state->game) {
            try {
                if (state->game->update(dt))
                    (void)state->game->present(state->presenter, state->width, state->height);
            } catch (const std::exception& e) {
                LOGE("frame exception: %s", e.what());
            } catch (...) {
                LOGE("frame unknown exception");
            }
        } else {
            state->fatal.draw(state->presenter, state->boot_error, wf, hf);
        }
    }
}
