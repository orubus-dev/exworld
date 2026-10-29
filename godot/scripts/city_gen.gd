class_name CityGen
extends Object
## Procedural open city: blueprint (pure data), shared assets (runtime-built
## textures/materials, no imported binaries), and the scene builder that
## turns the blueprint into meshes, MultiMeshes and collision.
##
## Layout: an N x N grid of city blocks separated by two-way roads.
## Road centerlines sit at multiples of CELL, blocks fill the gaps.

const BLOCK_COUNT := 8
const BLOCK_SIZE := 54.0
const ROAD_WIDTH := 14.0
const CELL := BLOCK_SIZE + ROAD_WIDTH        # 68 m
const EXTENT := CELL * BLOCK_COUNT           # 544 m
const HALF := EXTENT * 0.5

static var _assets: Dictionary = {}


static func road_coord(i: int) -> float:
        return -HALF + float(i) * CELL


## ------------------------------------------------------------------
## Blueprint: deterministic data-only description of the city.
## ------------------------------------------------------------------
static func build_blueprint(seed_val: int) -> Dictionary:
        var rng := RandomNumberGenerator.new()
        rng.seed = seed_val
        var blocks: Array = []
        for bx in BLOCK_COUNT:
                for bz in BLOCK_COUNT:
                        var center := Vector2(road_coord(bx) + CELL * 0.5, road_coord(bz) + CELL * 0.5)
                        var dist_norm: float = clampf(center.length() / HALF, 0.0, 1.0)
                        var block := {"center": center, "kind": "city", "buildings": [], "trees": []}
                        var roll := rng.randf()
                        if roll < 0.10:
                                block["kind"] = "park"
                                for t in rng.randi_range(12, 18):
                                        block["trees"].append(Vector2(
                                                rng.randf_range(-BLOCK_SIZE * 0.5 + 5.0, BLOCK_SIZE * 0.5 - 5.0),
                                                rng.randf_range(-BLOCK_SIZE * 0.5 + 5.0, BLOCK_SIZE * 0.5 - 5.0)))
                        elif roll < 0.15:
                                block["kind"] = "plaza"
                                for t in 4:
                                        var sx := -1.0 if (t % 2 == 0) else 1.0
                                        var sz := -1.0 if (t < 2) else 1.0
                                        block["trees"].append(Vector2(sx * (BLOCK_SIZE * 0.5 - 6.0), sz * (BLOCK_SIZE * 0.5 - 6.0)))
                        else:
                                # Downtown gets taller toward the city center.
                                var h_max := lerpf(88.0, 20.0, dist_norm)
                                for qx in 2:
                                        for qz in 2:
                                                if rng.randf() < 0.80:
                                                        var fw := rng.randf_range(13.0, 21.0)
                                                        var fd := rng.randf_range(13.0, 21.0)
                                                        var h := rng.randf_range(10.0, h_max)
                                                        if h > 60.0:
                                                                fw = rng.randf_range(13.0, 16.5)
                                                                fd = rng.randf_range(13.0, 16.5)
                                                        var off := BLOCK_SIZE * 0.25
                                                        var bpos := Vector2(center.x - off + qx * BLOCK_SIZE * 0.5,
                                                                        center.y - off + qz * BLOCK_SIZE * 0.5)
                                                        block["buildings"].append({
                                                                "pos": bpos,
                                                                "w": fw, "d": fd, "h": h,
                                                                "tint": _building_tint(rng),
                                                        })
                        blocks.append(block)
        return {"blocks": blocks, "seed": seed_val, "extent": EXTENT, "half": HALF}


static func _building_tint(rng: RandomNumberGenerator) -> Color:
        var palette := [
                Color(0.72, 0.71, 0.68),  # concrete
                Color(0.63, 0.59, 0.53),  # warm stone
                Color(0.55, 0.56, 0.60),  # cool steel
                Color(0.60, 0.47, 0.40),  # brick
                Color(0.49, 0.56, 0.57),  # teal glass
                Color(0.70, 0.66, 0.58),  # sand
        ]
        var c: Color = palette[rng.randi_range(0, palette.size() - 1)]
        var j := rng.randf_range(-0.05, 0.05)
        return Color(c.r + j, c.g + j, c.b + j)


## ------------------------------------------------------------------
## Shared assets: textures and materials built in code (tiny APK,
## nothing to import, works headless).
## ------------------------------------------------------------------
static func build_assets() -> Dictionary:
        if not _assets.is_empty():
                return _assets
        var rng := RandomNumberGenerator.new()
        rng.seed = 20261030

        var win_img := Image.create(64, 64, false, Image.FORMAT_RGB8)
        win_img.fill(Color(0.13, 0.14, 0.16))
        # 2x2 windows per tile: x in {6, 38}, y in {8, 40}, each 20x14 px.
        for wy in 2:
                for wx in 2:
                        var rect := Rect2(6 + wx * 32, 8 + wy * 32, 20, 14)
                        var glass := Color(0.95, 0.87, 0.62) if rng.randf() < 0.30 else Color(0.18, 0.23, 0.29)
                        win_img.fill_rect(rect, glass)
        var win_tex := ImageTexture.create_from_image(win_img)

        var bmat := StandardMaterial3D.new()
        bmat.albedo_texture = win_tex
        bmat.albedo_color = Color(1, 1, 1)
        bmat.vertex_color_use_as_albedo = true
        bmat.uv1_triplanar = true
        bmat.uv1_world_triplanar = true
        bmat.uv1_scale = Vector3(0.16, 0.2, 0.16)
        bmat.roughness = 0.85
        _assets["building"] = bmat

        _assets["roof"] = _flat(Color(0.16, 0.17, 0.19), 0.95)
        _assets["ground"] = _flat(Color(0.16, 0.16, 0.17), 0.95)
        _assets["sidewalk"] = _flat(Color(0.47, 0.47, 0.46), 0.9)
        _assets["marking"] = _flat(Color(0.88, 0.88, 0.84), 0.8)

        var leaf := _flat(Color(1, 1, 1), 0.9)
        leaf.vertex_color_use_as_albedo = true
        _assets["leaf"] = leaf

        var trunk := _flat(Color(0.35, 0.26, 0.18), 0.95)
        _assets["trunk"] = trunk

        var car_paint := _flat(Color(1, 1, 1), 0.35)
        car_paint.vertex_color_use_as_albedo = true
        _assets["car_paint"] = car_paint

        var tire := _flat(Color(0.08, 0.08, 0.09), 0.95)
        _assets["tire"] = tire

        var ring := _flat(Color(0.13, 0.85, 1.0), 0.4)
        ring.emission_enabled = true
        ring.emission = Color(0.13, 0.85, 1.0)
        ring.emission_energy_multiplier = 1.6
        _assets["ring"] = ring
        return _assets


static func _flat(c: Color, rough: float) -> StandardMaterial3D:
        var m := StandardMaterial3D.new()
        m.albedo_color = c
        m.roughness = rough
        return m


## ------------------------------------------------------------------
## Scene builder.
## ------------------------------------------------------------------
static func build_scene(blueprint: Dictionary) -> Node3D:
        if _assets.is_empty():
                build_assets()
        var city := Node3D.new()
        city.name = "City"
        var ext := float(blueprint["extent"])
        var half := float(blueprint["half"])

        # Ground: one big slab; its top face is y = 0.
        var ground := StaticBody3D.new()
        ground.name = "Ground"
        ground.collision_layer = 1
        var gshape := CollisionShape3D.new()
        var gbox := BoxShape3D.new()
        gbox.size = Vector3(ext + 140.0, 1.0, ext + 140.0)
        gshape.shape = gbox
        gshape.position.y = -0.5
        ground.add_child(gshape)
        var gmesh := MeshInstance3D.new()
        var plane := PlaneMesh.new()
        plane.size = Vector2(ext + 140.0, ext + 140.0)
        gmesh.mesh = plane
        gmesh.material_override = _assets["ground"]
        ground.add_child(gmesh)
        city.add_child(ground)

        # Invisible boundary walls so the player cannot drive off the map.
        var wall_h := 40.0
        var wall_defs := [
                [Vector3(0, wall_h * 0.5, -half - 6.0), Vector3(ext + 40.0, wall_h, 2.0)],
                [Vector3(0, wall_h * 0.5, half + 6.0), Vector3(ext + 40.0, wall_h, 2.0)],
                [Vector3(-half - 6.0, wall_h * 0.5, 0), Vector3(2.0, wall_h, ext + 40.0)],
                [Vector3(half + 6.0, wall_h * 0.5, 0), Vector3(2.0, wall_h, ext + 40.0)],
        ]
        for wd in wall_defs:
                var wb := StaticBody3D.new()
                wb.collision_layer = 1
                var wsh := CollisionShape3D.new()
                var wbox := BoxShape3D.new()
                wbox.size = wd[1]
                wsh.shape = wbox
                wsh.position = wd[0]
                wb.add_child(wsh)
                city.add_child(wb)

        # --- collect instance batches -------------------------------------
        var lot_xf: Array = []
        var lot_col: Array = []
        var bld_xf: Array = []
        var bld_col: Array = []
        var roof_xf: Array = []
        var roof_col: Array = []
        var trunk_xf: Array = []
        var leaf_xf: Array = []
        var leaf_col: Array = []
        var dash_xf: Array = []

        for block in blueprint["blocks"]:
                var c: Vector2 = block["center"]
                var kind: String = block["kind"]
                var lot_color := Color(0.47, 0.47, 0.46)
                if kind == "park":
                        lot_color = Color(0.30, 0.52, 0.28)
                elif kind == "plaza":
                        lot_color = Color(0.55, 0.54, 0.50)
                lot_xf.append(Transform3D(Basis.IDENTITY, Vector3(c.x, 0.09, c.y)))
                lot_col.append(lot_color)

                for b in block["buildings"]:
                        var h: float = b["h"]
                        var pos: Vector2 = b["pos"]
                        bld_xf.append(Transform3D(Basis.IDENTITY, Vector3(pos.x, 0.18 + h * 0.5, pos.y)))
                        bld_col.append(b["tint"])
                        roof_xf.append(Transform3D(Basis.IDENTITY, Vector3(pos.x, 0.18 + h + 0.3, pos.y)))
                        roof_col.append(Color(0.16, 0.17, 0.19))

                for t in block["trees"]:
                        var base := Vector3(c.x + t.x, 0.18, c.y + t.y)
                        trunk_xf.append(Transform3D(Basis.IDENTITY, base + Vector3(0, 2.0, 0)))
                        leaf_xf.append(Transform3D(Basis.IDENTITY, base + Vector3(0, 5.0, 0)))
                        var g := rng_static().randf_range(0.85, 1.1)
                        leaf_col.append(Color(0.22 * g, 0.45 * g, 0.20 * g))

        # Road dashes along every road line, both axes.
        for r in BLOCK_COUNT + 1:
                var line := road_coord(r)
                var d := -half + 4.0
                while d < half - 4.0:
                        # Skip dashes across intersections.
                        var near_cross := false
                        for rc in BLOCK_COUNT + 1:
                                if absf(d - road_coord(rc)) < ROAD_WIDTH * 0.5 + 3.0:
                                        near_cross = true
                                        break
                        if not near_cross:
                                dash_xf.append(Transform3D(Basis.IDENTITY, Vector3(line, 0.02, d)))
                                dash_xf.append(Transform3D(Basis.from_euler(Vector3(0, PI * 0.5, 0)), Vector3(d, 0.02, line)))
                        d += 8.0

        # --- MultiMeshes ---------------------------------------------------
        city.add_child(_make_mm(BoxMesh.new(), lot_xf, lot_col, _assets["sidewalk"]))

        var bmesh := BoxMesh.new()
        city.add_child(_make_mm(bmesh, bld_xf, bld_col, _assets["building"]))

        var rmesh := BoxMesh.new()
        rmesh.size = Vector3(1.06, 0.6, 1.06)
        city.add_child(_make_mm(rmesh, roof_xf, roof_col, _assets["roof"]))

        var trunk_mesh := CylinderMesh.new()
        trunk_mesh.top_radius = 0.3
        trunk_mesh.bottom_radius = 0.42
        trunk_mesh.height = 4.0
        var plain_trunk: Array = []
        for i in trunk_xf.size():
                plain_trunk.append(Color(1, 1, 1))
        city.add_child(_make_mm(trunk_mesh, trunk_xf, plain_trunk, _assets["trunk"]))

        var leaf_mesh := SphereMesh.new()
        leaf_mesh.radius = 2.4
        leaf_mesh.height = 4.6
        city.add_child(_make_mm(leaf_mesh, leaf_xf, leaf_col, _assets["leaf"]))

        var dash_mesh := BoxMesh.new()
        dash_mesh.size = Vector3(0.28, 0.03, 3.6)
        var white_dashes: Array = []
        for i in dash_xf.size():
                white_dashes.append(Color(1, 1, 1))
        city.add_child(_make_mm(dash_mesh, dash_xf, white_dashes, _assets["marking"]))

        # --- building collision -------------------------------------------
        var bodies := Node3D.new()
        bodies.name = "BuildingBodies"
        for block in blueprint["blocks"]:
                for b in block["buildings"]:
                        var sb := StaticBody3D.new()
                        sb.collision_layer = 1
                        var cs := CollisionShape3D.new()
                        var bs := BoxShape3D.new()
                        bs.size = Vector3(b["w"], b["h"], b["d"])
                        cs.shape = bs
                        cs.position = Vector3(b["pos"].x, 0.18 + b["h"] * 0.5, b["pos"].y)
                        sb.add_child(cs)
                        bodies.add_child(sb)
        city.add_child(bodies)
        return city


static func _rng_holder() -> RandomNumberGenerator:
        var r := RandomNumberGenerator.new()
        r.seed = 7
        return r

static var _rng_cache: RandomNumberGenerator = null

static func rng_static() -> RandomNumberGenerator:
        if _rng_cache == null:
                _rng_cache = _rng_holder()
        return _rng_cache


static func _make_mm(mesh: Mesh, xforms: Array, colors: Array, mat: Material) -> MultiMeshInstance3D:
        var mm := MultiMesh.new()
        mm.transform_format = MultiMesh.TRANSFORM_3D
        mm.use_colors = true
        mm.mesh = mesh
        mm.instance_count = xforms.size()
        for i in xforms.size():
                mm.set_instance_transform(i, xforms[i])
                mm.set_instance_color(i, colors[i])
        var mmi := MultiMeshInstance3D.new()
        mmi.multimesh = mm
        mmi.material_override = mat
        return mmi
