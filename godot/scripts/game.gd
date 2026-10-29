extends Node3D
## Game orchestrator: builds environment + city, spawns the car, camera,
## HUD, touch controls, traffic and checkpoint rings; runs the loop.

var car: PlayerCar
var cam_rig: ChaseCam
var hud: Hud
var touch: TouchControls
var traffic: Traffic

var rings: Array = []
var collected := 0
var elapsed := 0.0
var running := false


func _ready() -> void:
        GameState.apply_quality(get_viewport())
        _build_environment()

        if GameState.blueprint.is_empty():
                GameState.blueprint = CityGen.build_blueprint(randi())
        if GameState.assets.is_empty():
                GameState.assets = CityGen.build_assets()

        add_child(CityGen.build_scene(GameState.blueprint))

        car = PlayerCar.new()
        car.name = "PlayerCar"
        var start_z: float = CityGen.HALF - 24.0
        car.position = Vector3(0.0, 1.0, start_z)
        car.rotation.y = PI   # face -Z, into the city
        add_child(car)

        cam_rig = ChaseCam.new()
        cam_rig.name = "ChaseCam"
        add_child(cam_rig)
        cam_rig.set_target(car)

        traffic = Traffic.new()
        traffic.name = "Traffic"
        add_child(traffic)
        var trng := RandomNumberGenerator.new()
        trng.seed = int(GameState.blueprint["seed"]) + 99
        traffic.setup(10, trng)

        hud = Hud.new()
        hud.name = "Hud"
        add_child(hud)
        hud.reset_requested.connect(_reset_run)
        hud.menu_requested.connect(_go_menu)

        touch = TouchControls.new()
        touch.name = "TouchControls"
        touch.visible = GameState.touchscreen_wanted()
        add_child(touch)

        _spawn_rings()
        running = true

        if OS.get_cmdline_user_args().has("--smoke"):
                add_child(SmokeNode.new())


func _build_environment() -> void:
        var env := Environment.new()
        env.background_mode = Environment.BG_SKY
        var sky := Sky.new()
        var sky_mat := ProceduralSkyMaterial.new()
        sky_mat.sky_top_color = Color(0.24, 0.42, 0.72)
        sky_mat.sky_horizon_color = Color(0.62, 0.70, 0.80)
        sky_mat.ground_bottom_color = Color(0.12, 0.13, 0.15)
        sky_mat.ground_horizon_color = Color(0.55, 0.62, 0.70)
        sky.sky_material = sky_mat
        env.sky = sky
        env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
        env.ambient_light_energy = 1.1
        env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
        env.fog_enabled = true
        env.fog_density = 0.0011
        env.fog_light_color = Color(0.60, 0.68, 0.78)

        var we := WorldEnvironment.new()
        we.environment = env
        add_child(we)

        var sun := DirectionalLight3D.new()
        sun.rotation_degrees = Vector3(-52.0, -35.0, 0.0)
        sun.light_energy = 1.35
        sun.light_color = Color(1.0, 0.96, 0.88)
        sun.shadow_enabled = GameState.quality_high
        sun.directional_shadow_max_distance = 140.0
        add_child(sun)


func _spawn_rings() -> void:
        for r in rings:
                r.queue_free()
        rings.clear()
        collected = 0

        var rng := RandomNumberGenerator.new()
        rng.seed = randi()
        var spots: Array = []
        var attempts := 0
        while spots.size() < 10 and attempts < 400:
                attempts += 1
                var rx := rng.randi_range(0, CityGen.BLOCK_COUNT)
                var rz := rng.randi_range(0, CityGen.BLOCK_COUNT)
                var p := Vector3(CityGen.road_coord(rx), 0.0, CityGen.road_coord(rz))
                var start := Vector3(0.0, 0.0, CityGen.HALF - 24.0)
                if p.distance_to(start) < 60.0:
                        continue
                var ok := true
                for s in spots:
                        if p.distance_to(s) < 80.0:
                                ok = false
                                break
                if ok:
                        spots.append(p)

        for p in spots:
                var ring := CheckpointRing.new()
                ring.position = p
                ring.collected.connect(_on_ring_collected)
                add_child(ring)
                rings.append(ring)


func _on_ring_collected(_ring: CheckpointRing) -> void:
        if not running:
                return
        collected += 1
        GameState.total_collected += 1
        if collected >= rings.size():
                running = false
                if GameState.best_time_sec < 0.0 or elapsed < GameState.best_time_sec:
                        GameState.best_time_sec = elapsed
                GameState.save_save()
                hud.show_banner("CITY CLEARED!\n%d / %d  —  %s\nHold GAS and tap RESET to run again" % [collected, rings.size(), _fmt_time(elapsed)])
        else:
                hud.show_banner("%d / %d" % [collected, rings.size()])


func _reset_run() -> void:
        car.respawn(Vector3(0.0, 0.5, CityGen.HALF - 24.0))
        elapsed = 0.0
        running = true
        _spawn_rings()


func _process(delta: float) -> void:
        if running and not get_tree().paused:
                elapsed += delta
        if hud != null:
                hud.update_hud(car.speed_kmh() if car != null else 0.0, collected, rings.size(), elapsed)
                if Input.is_action_just_pressed("pause"):
                        hud.set_paused(not get_tree().paused)
                if Input.is_action_just_pressed("reset_car"):
                        _reset_run()


func _go_menu() -> void:
        get_tree().paused = false
        get_tree().change_scene_to_file("res://scenes/main_menu.tscn")


func _fmt_time(t: float) -> String:
        return "%d:%04.1f" % [int(t) / 60, fmod(t, 60.0)]
