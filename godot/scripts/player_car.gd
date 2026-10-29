class_name PlayerCar
extends VehicleBody3D
## Arcade-tuned car built entirely in code: body, cabin, four VehicleWheel3D.
## Reads keyboard actions plus the touch values published by GameState.touch.

const MAX_STEER_LOW := 0.55     # rad, parking speed
const MAX_STEER_HIGH := 0.20    # rad, at top speed
const STEER_SPEED := 5.5        # rad/s toward target
const ENGINE_FORCE := 5200.0
const REVERSE_FORCE := 2800.0
const BRAKE_FORCE := 70.0
const HANDBRAKE_FORCE := 40.0
const TOP_SPEED_KMH := 165.0

var _wheels: Array = []          # [[wheel, is_front], ...]
var _steer := 0.0


func _ready() -> void:
        mass = 1200.0
        center_of_mass_mode = RigidBody3D.CENTER_OF_MASS_MODE_CUSTOM
        center_of_mass = Vector3(0.0, -0.3, 0.0)
        linear_damp = 0.05
        angular_damp = 1.2
        collision_layer = 2
        collision_mask = 1
        can_sleep = false

        var col := CollisionShape3D.new()
        var box := BoxShape3D.new()
        box.size = Vector3(1.9, 0.9, 4.2)
        col.shape = box
        col.position = Vector3(0, 0.75, 0)
        add_child(col)

        if not CityGen._assets.is_empty():
                var body := MeshInstance3D.new()
                var bm := BoxMesh.new()
                bm.size = Vector3(1.9, 0.7, 4.2)
                body.mesh = bm
                body.material_override = CityGen._assets["car_paint"]
                body.position = Vector3(0, 0.75, 0)
                add_child(body)
                var cabin := MeshInstance3D.new()
                var cm := BoxMesh.new()
                cm.size = Vector3(1.6, 0.55, 2.0)
                cabin.mesh = cm
                var cab_mat := StandardMaterial3D.new()
                cab_mat.albedo_color = Color(0.10, 0.12, 0.16)
                cab_mat.roughness = 0.2
                cabin.material_override = cab_mat
                cabin.position = Vector3(0, 1.35, -0.15)
                add_child(cabin)

        for i in 4:
                var w := VehicleWheel3D.new()
                var front := i < 2
                w.wheel_radius = 0.38
                w.wheel_rest_length = 0.28
                w.suspension_travel = 0.25
                w.suspension_stiffness = 42.0
                w.suspension_max_force = 9000.0
                w.damping_compression = 4.2
                w.damping_relaxation = 4.6
                w.wheel_friction_slip = 3.0 if front else 3.4
                w.wheel_roll_influence = 0.08
                w.use_as_steering = front
                w.use_as_traction = not front
                w.position = Vector3(-0.92 if i % 2 == 0 else 0.92, 0.42, 1.45 if front else -1.45)
                add_child(w)

                var hub := MeshInstance3D.new()
                var tire := CylinderMesh.new()
                tire.top_radius = 0.38
                tire.bottom_radius = 0.38
                tire.height = 0.26
                hub.mesh = tire
                hub.rotation = Vector3(0, 0, PI * 0.5)
                if not CityGen._assets.is_empty():
                        hub.material_override = CityGen._assets["tire"]
                w.add_child(hub)

                _wheels.append([w, front])

        paint(Color(0.85, 0.16, 0.14))


func paint(c: Color) -> void:
        var paint_mat := StandardMaterial3D.new()
        paint_mat.albedo_color = c
        paint_mat.roughness = 0.32
        paint_mat.metallic = 0.25
        for child in get_children():
                if child is MeshInstance3D and child.material_override is StandardMaterial3D:
                        var m: StandardMaterial3D = child.material_override
                        if m.roughness < 0.5 and m.albedo_color != Color(0.10, 0.12, 0.16):
                                child.material_override = paint_mat


func speed_kmh() -> float:
        return linear_velocity.length() * 3.6


func _physics_process(delta: float) -> void:
        var steer_in := Input.get_action_strength("steer_right") - Input.get_action_strength("steer_left")
        steer_in += GameState.touch["steer"]
        steer_in = clampf(steer_in, -1.0, 1.0)

        var throttle := maxf(Input.get_action_strength("accelerate"), GameState.touch["throttle"])
        var brake_in := maxf(Input.get_action_strength("brake"), GameState.touch["brake"])
        var handbrake: bool = Input.is_action_pressed("handbrake") or bool(GameState.touch["hb"])

        var speed := speed_kmh()
        var steer_limit: float = lerpf(MAX_STEER_LOW, MAX_STEER_HIGH, clampf(speed / TOP_SPEED_KMH, 0.0, 1.0))
        _steer = move_toward(_steer, steer_in * steer_limit, STEER_SPEED * delta * steer_limit / MAX_STEER_LOW)
        steering = _steer

        var fwd_speed := -linear_velocity.dot(global_transform.basis.z)

        if handbrake and speed > 4.0:
                brake = HANDBRAKE_FORCE
                for entry in _wheels:
                        if not entry[1]:
                                entry[0].wheel_friction_slip = 1.35
                engine_force = 0.0
        else:
                for entry in _wheels:
                        if not entry[1]:
                                entry[0].wheel_friction_slip = 3.4
                brake = 0.0
                if throttle > 0.02:
                        # Fade engine force near top speed for a soft limiter.
                        var fade: float = clampf(1.0 - speed / TOP_SPEED_KMH, 0.0, 1.0)
                        engine_force = throttle * ENGINE_FORCE * fade
                elif brake_in > 0.02:
                        if fwd_speed > 1.0:
                                engine_force = 0.0
                                brake = brake_in * BRAKE_FORCE
                        else:
                                engine_force = -brake_in * REVERSE_FORCE
                else:
                        engine_force = 0.0
                        # Light rolling resistance so the car coasts to a stop.
                        brake = 2.0


func respawn(road_snap: Vector3) -> void:
        PhysicsServer3D.body_set_state(get_rid(), PhysicsServer3D.BODY_STATE_TRANSFORM,
                        Transform3D(Basis.IDENTITY, road_snap + Vector3(0, 1.2, 0)))
        linear_velocity = Vector3.ZERO
        angular_velocity = Vector3.ZERO
        steering = 0.0
        _steer = 0.0


## Nearest road point: snap the smaller-axis offset to the closest road line.
static func nearest_road_point(p: Vector3) -> Vector3:
        var half: float = CityGen.HALF
        var cell: float = CityGen.CELL
        var snap_x := clampf(roundf((p.x + half) / cell) * cell - half, -half, half)
        var snap_z := clampf(roundf((p.z + half) / cell) * cell - half, -half, half)
        var dx := absf(p.x - snap_x)
        var dz := absf(p.z - snap_z)
        if dx < dz:
                return Vector3(snap_x, 0.5, clampf(p.z, -half - 4.0, half + 4.0))
        return Vector3(clampf(p.x, -half - 4.0, half + 4.0), 0.5, snap_z)
