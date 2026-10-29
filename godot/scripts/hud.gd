class_name Hud
extends CanvasLayer
## In-game UI: speed, checkpoint counter, timer, banner, RESET + MENU buttons,
## and the pause overlay. All UI built in code.

signal reset_requested
signal menu_requested

var speed_label: Label
var counter_label: Label
var time_label: Label
var hint_label: Label
var banner: Label
var _panel: PanelContainer
var _dim: ColorRect


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	var root := Control.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(root)

	speed_label = _label(root, 40, Color(1, 1, 1, 0.92))
	speed_label.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
	speed_label.position = Vector2(28, -86)
	speed_label.text = "0 km/h"

	counter_label = _label(root, 30, Color(0.22, 0.88, 1.0))
	counter_label.set_anchors_preset(Control.PRESET_CENTER_TOP)
	counter_label.position = Vector2(-120, 18)
	counter_label.size = Vector2(240, 40)
	counter_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	counter_label.text = "0 / 10"

	time_label = _label(root, 28, Color(1, 1, 1, 0.8))
	time_label.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	time_label.position = Vector2(-190, 20)
	time_label.text = "0:00.0"

	hint_label = _label(root, 20, Color(1, 1, 1, 0.55))
	hint_label.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
	hint_label.position = Vector2(28, -34)
	hint_label.text = "Collect the glowing rings"

	banner = _label(root, 54, Color(1, 1, 1))
	banner.set_anchors_preset(Control.PRESET_CENTER)
	banner.position = Vector2(-400, -80)
	banner.size = Vector2(800, 160)
	banner.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	banner.text = ""

	var reset_btn := _button(root, "RESET", Color(0.95, 0.75, 0.2))
	reset_btn.set_anchors_preset(Control.PRESET_TOP_LEFT)
	reset_btn.position = Vector2(24, 20)
	reset_btn.pressed.connect(func(): reset_requested.emit())

	var menu_btn := _button(root, "MENU", Color(0.22, 0.88, 1.0))
	menu_btn.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	menu_btn.position = Vector2(-152, 20)
	menu_btn.pressed.connect(func(): menu_requested.emit())

	_dim = ColorRect.new()
	_dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	_dim.color = Color(0, 0, 0, 0.55)
	_dim.mouse_filter = Control.MOUSE_FILTER_STOP
	_dim.visible = false
	root.add_child(_dim)

	_panel = PanelContainer.new()
	var sb := StyleBoxFlat.new()
	sb.bg_color = Color(0.07, 0.09, 0.13, 0.96)
	sb.corner_radius_top_left = 14
	sb.corner_radius_top_right = 14
	sb.corner_radius_bottom_left = 14
	sb.corner_radius_bottom_right = 14
	sb.content_margin_left = 40
	sb.content_margin_right = 40
	sb.content_margin_top = 28
	sb.content_margin_bottom = 28
	_panel.add_theme_stylebox_override("panel", sb)
	_panel.set_anchors_preset(Control.PRESET_CENTER)
	_panel.visible = false
	root.add_child(_panel)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 14)
	_panel.add_child(vbox)

	var title := _label(vbox, 34, Color(1, 1, 1))
	title.text = "PAUSED"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER

	var resume := _button(vbox, "RESUME", Color(0.22, 0.88, 1.0))
	resume.pressed.connect(func(): set_paused(false))

	var restart := _button(vbox, "RESTART RUN", Color(0.95, 0.75, 0.2))
	restart.pressed.connect(func():
		set_paused(false)
		reset_requested.emit())

	var quit := _button(vbox, "MAIN MENU", Color(0.9, 0.35, 0.3))
	quit.pressed.connect(func(): menu_requested.emit())


func _label(parent: Node, size: int, color: Color) -> Label:
	var l := Label.new()
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	l.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.7))
	l.add_theme_constant_override("shadow_offset_x", 2)
	l.add_theme_constant_override("shadow_offset_y", 2)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if parent is VBoxContainer:
		parent.add_child(l)
	else:
		parent.add_child(l)
	return l


func _button(parent: Node, text: String, accent: Color) -> Button:
	var b := Button.new()
	b.text = text
	b.custom_minimum_size = Vector2(280, 56)
	b.add_theme_font_size_override("font_size", 24)
	b.add_theme_color_override("font_color", accent)
	b.add_theme_color_override("font_hover_color", accent.lightened(0.3))
	b.add_theme_color_override("font_pressed_color", accent.darkened(0.2))
	var sb := StyleBoxFlat.new()
	sb.bg_color = Color(1, 1, 1, 0.07)
	sb.set_corner_radius_all(10)
	sb.set_content_margin_all(10)
	sb.border_width_bottom = 2
	sb.border_color = Color(accent.r, accent.g, accent.b, 0.6)
	b.add_theme_stylebox_override("normal", sb)
	var sbh := sb.duplicate()
	sbh.bg_color = Color(1, 1, 1, 0.14)
	b.add_theme_stylebox_override("hover", sbh)
	var sbp := sb.duplicate()
	sbp.bg_color = Color(1, 1, 1, 0.2)
	b.add_theme_stylebox_override("pressed", sbp)
	b.add_theme_stylebox_override("focus", StyleBoxEmpty.new())
	if parent is VBoxContainer:
		parent.add_child(b)
	else:
		parent.add_child(b)
	return b


func set_paused(p: bool) -> void:
	get_tree().paused = p
	_panel.visible = p
	_dim.visible = p


func show_banner(text: String) -> void:
	banner.text = text
	banner.modulate = Color(1, 1, 1, 0.0)
	var tw := banner.create_tween()
	tw.tween_property(banner, "modulate:a", 1.0, 0.4)
	tw.tween_interval(2.2)
	tw.tween_property(banner, "modulate:a", 0.0, 0.8)


func update_hud(speed_kmh: float, collected: int, total: int, elapsed: float) -> void:
	speed_label.text = "%d km/h" % int(speed_kmh)
	counter_label.text = "%d / %d" % [collected, total]
	var mins := int(elapsed) / 60
	var secs := fmod(elapsed, 60.0)
	time_label.text = "%d:%04.1f" % [mins, secs]
