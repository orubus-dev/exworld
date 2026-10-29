class_name ChaseCam
extends Node3D
## Smooth third-person chase camera.
## Rig follows the car with exponential (frame-rate independent) damping,
## yaw blends car facing with velocity direction for natural drift framing,
## a SpringArm keeps buildings out of the lens, FOV kicks up with speed.

var target: PlayerCar = null

var _spring: SpringArm3D
var _cam: Camera3D
var _yaw := 0.0
var _fov := 62.0

const POS_DAMP := 7.0        # rig position smoothing
const YAW_DAMP := 5.0        # rotation smoothing
const ARM_LENGTH := 6.6
const ARM_HEIGHT := 2.6
const BASE_PITCH := -13.0
const BASE_FOV := 62.0
const SPEED_FOV := 16.0


func _ready() -> void:
	_spring = SpringArm3D.new()
	_spring.spring_length = ARM_LENGTH
	_spring.margin = 0.35
	_spring.collision_mask = 1
	_spring.position = Vector3(0, ARM_HEIGHT, 0)
	_spring.rotation_degrees.x = BASE_PITCH
	add_child(_spring)

	_cam = Camera3D.new()
	_cam.fov = BASE_FOV
	_cam.near = 0.3
	_cam.far = 700.0
	_cam.current = true
	_spring.add_child(_cam)


func set_target(car: PlayerCar) -> void:
	target = car
	if target != null:
		add_excluded(target)
		# Snap to the car on first assignment to avoid a whip-pan.
		global_position = target.global_position
		_yaw = target.rotation.y
		rotation.y = _yaw


func add_excluded(car: PlayerCar) -> void:
	_spring.add_excluded_object(car.get_rid())


func _physics_process(delta: float) -> void:
	if target == null:
		return
	var k_pos := 1.0 - exp(-POS_DAMP * delta)
	global_position = global_position.lerp(target.global_position, k_pos)

	var fwd := -target.global_transform.basis.z
	var vel := target.linear_velocity
	vel.y = 0.0
	var look_dir := fwd
	if vel.length() > 5.0:
		look_dir = (fwd * 0.55 + vel.normalized() * 0.45).normalized()
	var desired_yaw := atan2(-look_dir.x, -look_dir.z)
	_yaw = lerp_angle(_yaw, desired_yaw, 1.0 - exp(-YAW_DAMP * delta))
	rotation.y = _yaw

	var target_fov: float = BASE_FOV + SPEED_FOV * clampf(target.speed_kmh() / PlayerCar.TOP_SPEED_KMH, 0.0, 1.0)
	_fov = lerpf(_fov, target_fov, 1.0 - exp(-4.0 * delta))
	_cam.fov = _fov
