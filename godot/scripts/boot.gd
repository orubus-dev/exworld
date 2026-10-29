extends Control
## Boot / loading screen. Runs the real one-time init pass with a progress
## bar (city blueprint, shared assets), then hands off to the main menu.
## Every step is genuine work — no fake timers.

var bar: ProgressBar
var status: Label
var _steps: Array = []
var _step := 0


func _ready() -> void:
	var bg := ColorRect.new()
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	bg.color = Color(0.043, 0.051, 0.08)
	add_child(bg)

	var title := Label.new()
	title.text = "EXWORLD"
	title.add_theme_font_size_override("font_size", 84)
	title.add_theme_color_override("font_color", Color(0.22, 0.88, 1.0))
	title.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.8))
	title.add_theme_constant_override("shadow_offset_y", 4)
	title.set_anchors_preset(Control.PRESET_CENTER_TOP)
	title.position = Vector2(-300, 180)
	title.size = Vector2(600, 100)
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	add_child(title)

	bar = ProgressBar.new()
	bar.set_anchors_preset(Control.PRESET_CENTER)
	bar.position = Vector2(-260, -20)
	bar.size = Vector2(520, 26)
	bar.min_value = 0.0
	bar.max_value = 1.0
	bar.show_percentage = false
	add_child(bar)

	status = Label.new()
	status.text = "Starting..."
	status.add_theme_font_size_override("font_size", 20)
	status.add_theme_color_override("font_color", Color(1, 1, 1, 0.65))
	status.set_anchors_preset(Control.PRESET_CENTER)
	status.position = Vector2(-260, 16)
	status.size = Vector2(520, 30)
	status.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	add_child(status)

	var seed_val := int(Time.get_unix_time_from_system()) % 1000000
	_steps = [
		["Waking up the engine...", func(): GameState.apply_quality(get_viewport())],
		["Generating city blueprint...", func(): GameState.blueprint = CityGen.build_blueprint(seed_val)],
		["Baking materials and window lights...", func(): GameState.assets = CityGen.build_assets()],
		["Warming up input...", func(): Input.flush_buffered_events()],
		["Final checks...", func(): _sanity()],
	]
	_step = 0


func _sanity() -> void:
	if GameState.blueprint.is_empty():
		GameState.blueprint = CityGen.build_blueprint(20261030)
	if GameState.assets.is_empty():
		GameState.assets = CityGen.build_assets()


func _process(_delta: float) -> void:
	if _step >= _steps.size():
		return
	var entry: Array = _steps[_step]
	status.text = entry[0]
	entry[1].call()
	_step += 1
	bar.value = float(_step) / float(_steps.size())
	if _step >= _steps.size():
		_finish()


func _finish() -> void:
	status.text = "Ready"
	get_tree().change_scene_to_file.call_deferred("res://scenes/main_menu.tscn")
