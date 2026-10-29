class_name CheckpointRing
extends Area3D
## A glowing collectible ring placed at a road intersection.

signal collected(ring: CheckpointRing)

var taken := false
var _mesh: MeshInstance3D
var _spin := 0.0


func _init() -> void:
	collision_layer = 0
	collision_mask = 2   # player car
	monitoring = true

	var shape := CollisionShape3D.new()
	var cyl := CylinderShape3D.new()
	cyl.radius = 3.4
	cyl.height = 7.0
	shape.shape = cyl
	shape.position.y = 2.5
	add_child(shape)

	_mesh = MeshInstance3D.new()
	var torus := TorusMesh.new()
	torus.inner_radius = 2.6
	torus.outer_radius = 3.6
	_mesh.mesh = torus
	if not CityGen._assets.is_empty():
		_mesh.material_override = CityGen._assets["ring"]
	_mesh.position.y = 3.0
	add_child(_mesh)

	body_entered.connect(_on_body_entered)


func _process(delta: float) -> void:
	if taken:
		return
	_spin += delta * 1.4
	_mesh.rotation.z = _spin
	_mesh.position.y = 3.0 + sin(_spin * 0.9) * 0.35


func _on_body_entered(body: Node3D) -> void:
	if taken or not (body is PlayerCar):
		return
	taken = true
	visible = false
	set_deferred("monitoring", false)
	collected.emit(self)
