class_name SmokeNode
extends Node
## End-to-end headless self-test. Added by game.gd only when the scene is
## launched with user arg --smoke:
##   godot --headless --path godot res://scenes/game.tscn ++ --smoke
## Prints SMOKE PASS/FAIL lines; exits 0 on success, 1 on failure.

var _frames := 0
var _car: PlayerCar
var _start_pos := Vector3.ZERO
var _fails := 0


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	print("SMOKE: begin")
	var bp := CityGen.build_blueprint(12345)
	if bp["blocks"].size() == CityGen.BLOCK_COUNT * CityGen.BLOCK_COUNT:
		print("SMOKE PASS: blueprint blocks == 64")
	else:
		print("SMOKE FAIL: blueprint blocks")
		_fails += 1
	var n := 0
	for b in bp["blocks"]:
		n += b["buildings"].size()
	if n > 100:
		print("SMOKE PASS: blueprint has %d buildings" % n)
	else:
		print("SMOKE FAIL: only %d buildings" % n)
		_fails += 1
	var a := CityGen.build_assets()
	if a.has("building") and a.has("ring") and a.has("ground"):
		print("SMOKE PASS: assets build")
	else:
		print("SMOKE FAIL: assets incomplete")
		_fails += 1


func _physics_process(_delta: float) -> void:
	_frames += 1
	var game := get_parent()
	if _frames == 10:
		_car = game.get_node_or_null("PlayerCar") as PlayerCar
		if _car == null:
			print("SMOKE FAIL: no PlayerCar in game scene")
			_finish()
			return
		_start_pos = _car.global_position
		Input.action_press("accelerate")
		print("SMOKE: car spawned at %s, simulating throttle" % _start_pos)
	elif _frames == 150:
		var moved := _car.global_position.distance_to(_start_pos)
		print("SMOKE: car moved %.2f m after ~2.3s of throttle" % moved)
		if moved < 2.0:
			print("SMOKE FAIL: car did not move")
			_fails += 1
		else:
			print("SMOKE PASS: car drives under input")
		var rings := 0
		for child in game.get_children():
			if child is CheckpointRing:
				rings += 1
		print("SMOKE: rings spawned = %d" % rings)
		if rings == 10:
			print("SMOKE PASS: 10 checkpoint rings")
		else:
			print("SMOKE FAIL: expected 10 rings, got %d" % rings)
			_fails += 1
		var traffic: Traffic = game.get_node_or_null("Traffic") as Traffic
		if traffic != null and traffic.get_child_count() == 10:
			print("SMOKE PASS: 10 traffic cars")
		else:
			print("SMOKE FAIL: traffic cars missing")
			_fails += 1
		_finish()


func _finish() -> void:
	if _fails == 0:
		print("SMOKE: ALL PASS")
		get_tree().quit(0)
	else:
		print("SMOKE: %d FAILURES" % _fails)
		get_tree().quit(1)
