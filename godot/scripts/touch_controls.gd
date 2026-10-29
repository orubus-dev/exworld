class_name TouchControls
extends Control
## Multi-touch driving controls, drawn and hit-tested in code.
## Left half: floating steering joystick. Right half: GAS / BRAKE / DRIFT pads.
## Publishes into GameState.touch every frame the car reads.

var joy_center := Vector2.ZERO
var joy_knob := Vector2.ZERO
const JOY_RADIUS := 110.0
const KNOB_RADIUS := 46.0

var gas_rect := Rect2()
var brake_rect := Rect2()
var hb_rect := Rect2()

var _pointers := {}   # index -> zone string: "joy" | "gas" | "brake" | "hb"
var _joy_pointer := -1
var _accent := Color(0.22, 0.88, 1.0, 0.85)
var _dim := Color(1, 1, 1, 0.14)
var _font: Font


func _ready() -> void:
        set_anchors_preset(Control.PRESET_FULL_RECT)
        mouse_filter = Control.MOUSE_FILTER_IGNORE
        process_mode = Node.PROCESS_MODE_ALWAYS
        _font = ThemeDB.fallback_font
        visibility_changed.connect(queue_redraw)
        _layout()


func _layout() -> void:
        var vp := get_viewport_rect().size
        joy_center = Vector2(JOY_RADIUS + 70.0, vp.y - JOY_RADIUS - 40.0)
        joy_knob = joy_center
        var pad := 36.0
        var gas_r := 74.0
        var brake_r := 58.0
        var hb_r := 46.0
        var gas_c := Vector2(vp.x - gas_r - pad, vp.y - gas_r - pad)
        var brake_c := Vector2(gas_c.x - gas_r - brake_r - 26.0, vp.y - brake_r - pad)
        var hb_c := Vector2(gas_c.x - 20.0, gas_c.y - gas_r - hb_r - 30.0)
        gas_rect = Rect2(gas_c - Vector2(gas_r, gas_r), Vector2(gas_r * 2, gas_r * 2))
        brake_rect = Rect2(brake_c - Vector2(brake_r, brake_r), Vector2(brake_r * 2, brake_r * 2))
        hb_rect = Rect2(hb_c - Vector2(hb_r, hb_r), Vector2(hb_r * 2, hb_r * 2))


func _process(_delta: float) -> void:
        _layout()
        var steer := 0.0
        if _joy_pointer != -1:
                steer = clampf((joy_knob.x - joy_center.x) / JOY_RADIUS, -1.0, 1.0)
        GameState.touch["steer"] = steer
        GameState.touch["throttle"] = 1.0 if _pointers.values().has("gas") else 0.0
        GameState.touch["brake"] = 1.0 if _pointers.values().has("brake") else 0.0
        GameState.touch["hb"] = _pointers.values().has("hb")
        queue_redraw()


func _input(event: InputEvent) -> void:
        if not visible:
                return
        if event is InputEventScreenTouch:
                var t := event as InputEventScreenTouch
                if t.pressed:
                        var zone := _zone_at(t.position)
                        if zone != "":
                                _pointers[t.index] = zone
                                if zone == "joy":
                                        _joy_pointer = t.index
                                        joy_center = t.position
                                        joy_knob = t.position
                else:
                        if _pointers.has(t.index):
                                var was_joy: bool = _pointers[t.index] == "joy" and t.index == _joy_pointer
                                _pointers.erase(t.index)
                                if was_joy:
                                        _joy_pointer = -1
                                        joy_knob = joy_center
                queue_redraw()
        elif event is InputEventScreenDrag:
                var d := event as InputEventScreenDrag
                if _pointers.get(d.index, "") == "joy" and d.index == _joy_pointer:
                        var off := d.position - joy_center
                        joy_knob = joy_center + off.limit_length(JOY_RADIUS)
                        queue_redraw()


func _zone_at(p: Vector2) -> String:
        if gas_rect.has_point(p):
                return "gas"
        if brake_rect.has_point(p):
                return "brake"
        if hb_rect.has_point(p):
                return "hb"
        if p.x < get_viewport_rect().size.x * 0.55 and p.y > get_viewport_rect().size.y * 0.35:
                return "joy"
        return ""


func _draw() -> void:
        if not visible:
                return
        # Steering joystick
        draw_circle(joy_center, JOY_RADIUS, Color(1, 1, 1, 0.06))
        draw_arc(joy_center, JOY_RADIUS, 0.0, TAU, 48, _accent, 2.0, true)
        draw_circle(joy_knob, KNOB_RADIUS, Color(_accent.r, _accent.g, _accent.b, 0.30))
        draw_arc(joy_knob, KNOB_RADIUS, 0.0, TAU, 32, _accent, 2.0, true)
        _draw_centered(joy_center, "STEER")

        _draw_pad(gas_rect, "GAS", "gas", Color(0.24, 0.85, 0.4))
        _draw_pad(brake_rect, "BRAKE", "brake", Color(0.95, 0.42, 0.2))
        _draw_pad(hb_rect, "DRIFT", "hb", Color(0.95, 0.75, 0.2))


func _draw_pad(r: Rect2, label: String, zone: String, c: Color) -> void:
        var center := r.get_center()
        var pressed := _pointers.values().has(zone)
        var alpha := 0.32 if pressed else 0.14
        draw_circle(center, r.size.x * 0.5, Color(c.r, c.g, c.b, alpha))
        draw_arc(center, r.size.x * 0.5, 0.0, TAU, 40, Color(c.r, c.g, c.b, 0.8), 2.5, true)
        _draw_centered(center, label, c)


func _draw_centered(c: Vector2, text: String, col: Color = Color(1, 1, 1, 0.75)) -> void:
        var size := 22
        var w := _font.get_string_size(text, HORIZONTAL_ALIGNMENT_CENTER, -1, size).x
        draw_string(_font, Vector2(c.x - w * 0.5, c.y + size * 0.35), text, HORIZONTAL_ALIGNMENT_LEFT, -1, size, col)
