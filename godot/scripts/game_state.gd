extends Node
## Autoload singleton: input actions, settings, persistent save data,
## and shared resources produced during the boot loading pass.

const SAVE_PATH := "user://exworld_save.cfg"

var quality_high: bool = true
var touch_mode: String = "auto"  # "auto" | "on" | "off"

# Populated by the boot loading pass, consumed by the game scene.
var blueprint: Dictionary = {}
var assets: Dictionary = {}

# Persistent stats.
var total_collected: int = 0
var best_time_sec: float = -1.0

# Live touch input written by TouchControls, read by PlayerCar.
var touch := {"steer": 0.0, "throttle": 0.0, "brake": 0.0, "hb": false}


func _ready() -> void:
	_register_actions()
	load_save()


func _register_actions() -> void:
	_add_key_action("accelerate", [KEY_W, KEY_UP])
	_add_key_action("brake", [KEY_S, KEY_DOWN])
	_add_key_action("steer_left", [KEY_A, KEY_LEFT])
	_add_key_action("steer_right", [KEY_D, KEY_RIGHT])
	_add_key_action("handbrake", [KEY_SPACE])
	_add_key_action("pause", [KEY_ESCAPE])
	_add_key_action("reset_car", [KEY_R])


func _add_key_action(action: String, keys: Array) -> void:
	if not InputMap.has_action(action):
		InputMap.add_action(action)
	for k in keys:
		var ev := InputEventKey.new()
		ev.physical_keycode = k
		InputMap.action_add_event(action, ev)


func touchscreen_wanted() -> bool:
	if touch_mode == "on":
		return true
	if touch_mode == "off":
		return false
	return DisplayServer.is_touchscreen_available() or OS.has_feature("android")


func apply_quality(viewport: Viewport) -> void:
	if quality_high:
		viewport.msaa_3d = Viewport.MSAA_2X
	else:
		viewport.msaa_3d = Viewport.MSAA_DISABLED


func save_save() -> void:
	var cf := ConfigFile.new()
	cf.set_value("stats", "total_collected", total_collected)
	cf.set_value("stats", "best_time_sec", best_time_sec)
	cf.set_value("settings", "quality_high", quality_high)
	cf.set_value("settings", "touch_mode", touch_mode)
	cf.save(SAVE_PATH)


func load_save() -> void:
	var cf := ConfigFile.new()
	if cf.load(SAVE_PATH) != OK:
		return
	total_collected = int(cf.get_value("stats", "total_collected", 0))
	best_time_sec = float(cf.get_value("stats", "best_time_sec", -1.0))
	quality_high = bool(cf.get_value("settings", "quality_high", true))
	touch_mode = String(cf.get_value("settings", "touch_mode", "auto"))
