extends Control
## Main menu: title, START GAME, how-to-play, quality + touch settings.

var _howto: PanelContainer


func _ready() -> void:
	var bg := ColorRect.new()
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	bg.color = Color(0.043, 0.051, 0.08)
	add_child(bg)

	# Decorative skyline silhouette (pure rects, cheap).
	var rng := RandomNumberGenerator.new()
	rng.seed = 4242
	var skyline := Control.new()
	skyline.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	skyline.mouse_filter = Control.MOUSE_FILTER_IGNORE
	skyline.custom_minimum_size = Vector2(0, 260)
	add_child(skyline)
	var x := -30.0
	while x < 1400.0:
		var w := rng.randf_range(50.0, 130.0)
		var h := rng.randf_range(60.0, 230.0)
		var rect := ColorRect.new()
		rect.color = Color(0.09, 0.11, 0.16)
		rect.position = Vector2(x, 260.0 - h)
		rect.size = Vector2(w, h)
		rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
		skyline.add_child(rect)
		x += w + rng.randf_range(4.0, 18.0)

	var vbox := VBoxContainer.new()
	vbox.set_anchors_preset(Control.PRESET_CENTER)
	vbox.position = Vector2(-260, -240)
	vbox.custom_minimum_size = Vector2(520, 0)
	vbox.add_theme_constant_override("separation", 16)
	add_child(vbox)

	var title := Label.new()
	title.text = "EXWORLD"
	title.add_theme_font_size_override("font_size", 92)
	title.add_theme_color_override("font_color", Color(0.22, 0.88, 1.0))
	title.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.8))
	title.add_theme_constant_override("shadow_offset_y", 4)
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vbox.add_child(title)

	var subtitle := Label.new()
	subtitle.text = "Open-city driving sandbox  —  v%s" % ProjectSettings.get_setting("application/config/version", "0.7.0")
	subtitle.add_theme_font_size_override("font_size", 22)
	subtitle.add_theme_color_override("font_color", Color(1, 1, 1, 0.6))
	subtitle.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vbox.add_child(subtitle)

	var spacer := Control.new()
	spacer.custom_minimum_size = Vector2(0, 14)
	vbox.add_child(spacer)

	var stats := Label.new()
	var best := GameState.best_time_sec
	stats.text = "Rings collected: %d      Best clear: %s" % [GameState.total_collected,
			"—" if best < 0.0 else "%d:%04.1f" % [int(best) / 60, fmod(best, 60.0)]]
	stats.add_theme_font_size_override("font_size", 19)
	stats.add_theme_color_override("font_color", Color(1, 1, 1, 0.45))
	stats.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vbox.add_child(stats)

	var start := _button("START GAME", Color(0.22, 0.88, 1.0), 64)
	start.pressed.connect(_on_start)
	vbox.add_child(start)

	var howto := _button("HOW TO PLAY", Color(1, 1, 1, 0.85), 56)
	howto.pressed.connect(func(): _howto.visible = not _howto.visible)
	vbox.add_child(howto)

	var quality := _button("QUALITY: HIGH", Color(0.95, 0.75, 0.2), 56)
	quality.text = "QUALITY: %s" % ("HIGH" if GameState.quality_high else "LOW")
	quality.pressed.connect(func():
		GameState.quality_high = not GameState.quality_high
		quality.text = "QUALITY: %s" % ("HIGH" if GameState.quality_high else "LOW")
		GameState.save_save())
	vbox.add_child(quality)

	var touch_btn := _button("TOUCH: AUTO", Color(0.6, 0.9, 0.5), 56)
	touch_btn.text = "TOUCH: %s" % GameState.touch_mode.to_upper()
	touch_btn.pressed.connect(func():
		GameState.touch_mode = {"auto": "on", "on": "off", "off": "auto"}[GameState.touch_mode]
		touch_btn.text = "TOUCH: %s" % GameState.touch_mode.to_upper()
		GameState.save_save())
	vbox.add_child(touch_btn)

	var quit := _button("QUIT", Color(0.9, 0.35, 0.3), 56)
	quit.pressed.connect(func(): get_tree().quit())
	# Hiding quit on mobile: users close apps from the OS.
	if OS.has_feature("android"):
		quit.visible = false
	vbox.add_child(quit)

	_build_howto()


func _build_howto() -> void:
	_howto = PanelContainer.new()
	var sb := StyleBoxFlat.new()
	sb.bg_color = Color(0.07, 0.09, 0.13, 0.97)
	sb.set_corner_radius_all(14)
	sb.set_content_margin_all(26)
	_howto.add_theme_stylebox_override("panel", sb)
	_howto.set_anchors_preset(Control.PRESET_CENTER)
	_howto.custom_minimum_size = Vector2(640, 0)
	_howto.visible = false
	add_child(_howto)

	var l := Label.new()
	l.text = "Drive the open city and collect all 10 glowing rings.\n\nTOUCH: left stick steers, right pedals GAS / BRAKE / DRIFT.\nKEYBOARD: WASD or arrows, SPACE drift, R reset car, ESC pause.\n\nTraffic is ambient for now — it won't crash into you.\nRuns are timed; beat your best clear."
	l.add_theme_font_size_override("font_size", 20)
	l.add_theme_color_override("font_color", Color(1, 1, 1, 0.85))
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_howto.add_child(l)


func _on_start() -> void:
	get_tree().change_scene_to_file("res://scenes/game.tscn")


func _button(text: String, accent: Color, min_w: int) -> Button:
	var b := Button.new()
	b.text = text
	b.custom_minimum_size = Vector2(min_w, 58)
	b.add_theme_font_size_override("font_size", 24)
	b.add_theme_color_override("font_color", accent)
	b.add_theme_color_override("font_hover_color", accent.lightened(0.3))
	var sb := StyleBoxFlat.new()
	sb.bg_color = Color(1, 1, 1, 0.07)
	sb.set_corner_radius_all(12)
	sb.set_content_margin_all(10)
	sb.border_width_bottom = 2
	sb.border_color = Color(accent.r, accent.g, accent.b, 0.6)
	b.add_theme_stylebox_override("normal", sb)
	var sbh := sb.duplicate()
	sbh.bg_color = Color(1, 1, 1, 0.13)
	b.add_theme_stylebox_override("hover", sbh)
	var sbp := sb.duplicate()
	sbp.bg_color = Color(1, 1, 1, 0.2)
	b.add_theme_stylebox_override("pressed", sbp)
	b.add_theme_stylebox_override("focus", StyleBoxEmpty.new())
	return b
