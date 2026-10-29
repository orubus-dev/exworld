class_name Traffic
extends Node3D
## Cosmetic AI cars cruising rectangular loops around city blocks.
## No physics bodies: purely ambient, zero gameplay cost.

var _cars := []   # {node, wps: Array[Vector3], idx, speed}


func setup(count: int, rng: RandomNumberGenerator) -> void:
        for i in count:
                var rx := rng.randi_range(1, CityGen.BLOCK_COUNT - 1)
                var rz := rng.randi_range(1, CityGen.BLOCK_COUNT - 1)
                var cx := CityGen.road_coord(rx)
                var cz := CityGen.road_coord(rz)
                # Rectangular loop around the crossing at (cx, cz).
                var raw := [
                        Vector3(cx - CityGen.CELL, 0.0, cz - CityGen.CELL),
                        Vector3(cx + CityGen.CELL, 0.0, cz - CityGen.CELL),
                        Vector3(cx + CityGen.CELL, 0.0, cz + CityGen.CELL),
                        Vector3(cx - CityGen.CELL, 0.0, cz + CityGen.CELL),
                ]
                # Pull each corner toward the loop center to keep cars in the right lane.
                var wps: Array = []
                for p: Vector3 in raw:
                        var dir := (Vector3(cx, 0.0, cz) - p).normalized()
                        wps.append(p + dir * 3.5)
                var car := _make_car(rng)
                car.position = wps[0]
                add_child(car)
                _cars.append({"node": car, "wps": wps, "idx": 1, "speed": rng.randf_range(8.0, 13.0)})


func _physics_process(delta: float) -> void:
        for car in _cars:
                var node: Node3D = car["node"]
                var wps: Array = car["wps"]
                var idx: int = car["idx"]
                var target: Vector3 = wps[idx]
                var to_target := target - node.position
                to_target.y = 0.0
                var dist := to_target.length()
                if dist < 2.0:
                        car["idx"] = (idx + 1) % wps.size()
                        continue
                var dir := to_target / dist
                node.position += dir * car["speed"] * delta
                var desired_yaw := atan2(-dir.x, -dir.z)
                node.rotation.y = lerp_angle(node.rotation.y, desired_yaw, 1.0 - exp(-5.0 * delta))


func _make_car(rng: RandomNumberGenerator) -> Node3D:
        var root := Node3D.new()
        var colors := [
                Color(0.85, 0.82, 0.78), Color(0.2, 0.32, 0.55), Color(0.55, 0.56, 0.58),
                Color(0.75, 0.25, 0.2), Color(0.16, 0.45, 0.3), Color(0.9, 0.7, 0.2),
        ]
        var paint := StandardMaterial3D.new()
        paint.albedo_color = colors[rng.randi_range(0, colors.size() - 1)]
        paint.roughness = 0.4

        var body := MeshInstance3D.new()
        var bm := BoxMesh.new()
        bm.size = Vector3(1.9, 0.7, 4.2)
        body.mesh = bm
        body.material_override = paint
        body.position.y = 0.75
        root.add_child(body)

        var cabin := MeshInstance3D.new()
        var cm := BoxMesh.new()
        cm.size = Vector3(1.6, 0.55, 2.0)
        cabin.mesh = cm
        var glass := StandardMaterial3D.new()
        glass.albedo_color = Color(0.1, 0.12, 0.16)
        glass.roughness = 0.2
        cabin.material_override = glass
        cabin.position.y = 1.35
        root.add_child(cabin)
        return root
