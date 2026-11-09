"""라이브 Blender에서 MCP로 부르는 작업 모음.

MCP의 execute_blender_code에서 이 모듈을 불러 도면을 장면에 만들고, look으로 보고, 도면이나 규칙을 고쳐 다시 만든다.
장면을 손으로 고치지 않는다. 고칠 내용은 도면과 코드에 남겨야 다음 건물에도 적용된다.
"""
import importlib
import json
import math
import sys
from collections import deque
from pathlib import Path

import bpy
from mathutils import Vector

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parents[1]
TEXTURE_DIR = PROJECT_DIR / "Saved" / "MapDesign" / "textures"

ROUTE_SAMPLES_PER_CELL = 4
ROUTE_RAY_ABOVE_M = 1.5
ROUTE_RAY_BELOW_M = 3.0
# 경계석과 계단 턱만큼은 도면 높이와 달라도 된다.
ROUTE_HEIGHT_TOLERANCE_M = 0.3
# 계단 턱보다 높고 캡슐 허리보다 낮은 곳에서 앞이 막혔는지 본다.
ROUTE_KNEE_HEIGHT_M = 0.7

# 소품 불빛의 Blender 미리보기 밝기. 언리얼 해(8 lux)와 미리보기 해(4 W/m²)의 비율로 candela를 W로 바꾼다
# (W = 4π × candela × 4 / 8). 밝기 비율만 비슷하면 되는 대략의 값이고, 언리얼 밝기는 candela 그대로 manifest로 넘긴다.
PREVIEW_LIGHT_W_PER_CANDELA = 6.3

COLLECTIONS = ("MD_Site", "MD_Buildings", "MD_Props", "MD_Effects", "MD_Lights", "MD_Preview")
MODULES = ("mesh_kit", "map_plan", "site_builder", "building_builder", "props_builder", "texture_kit")


def _module(name):
    """코드를 고친 뒤 Blender를 다시 켜지 않아도 새 내용을 쓰도록 매번 다시 읽는다."""
    module = sys.modules.get(name)
    return importlib.reload(module) if module else importlib.import_module(name)


def _reload_builders():
    return {name: _module(name) for name in MODULES}


def make_textures():
    return _module("texture_kit").build_all(TEXTURE_DIR)


def _collection(name):
    collection = bpy.data.collections.get(name)
    if collection is None:
        collection = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(collection)
    return collection


def clear(names=COLLECTIONS):
    """이 작업이 만든 물체만 지운다. 장면의 다른 물체는 건드리지 않는다."""
    for name in names:
        collection = bpy.data.collections.get(name)
        if collection:
            for obj in list(collection.objects):
                data = obj.data
                bpy.data.objects.remove(obj)
                if data and data.users == 0:
                    (bpy.data.lights if isinstance(data, bpy.types.Light) else bpy.data.meshes).remove(data)


def view(eye, target, lens=24.0):
    """장면 카메라를 옮긴다. 도면의 cameras와 같은 시점을 언리얼에서도 찍어 비교한다."""
    camera = bpy.data.objects["Camera"]
    camera.data.lens = lens
    camera.location = eye
    camera.rotation_euler = (Vector(target) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = camera


def set_visible(collection_name, visible):
    bpy.data.collections[collection_name].hide_viewport = not visible
    bpy.data.collections[collection_name].hide_render = not visible


def _load_image(path):
    return bpy.data.images.load(str(path), check_existing=True) if path.exists() else None


def _make_material(name, spec):
    """언리얼에서 만들 재질과 같은 입력(색, 노멀, 거칠기)만 쓴다. 그래야 미리보기와 게임이 비슷하다."""
    material_name = f"MD_{name}"
    existing = bpy.data.materials.get(material_name)
    if existing:
        bpy.data.materials.remove(existing)
    material = bpy.data.materials.new(material_name)
    material.use_nodes = True
    tree = material.node_tree
    shader = next(node for node in tree.nodes if node.type == "BSDF_PRINCIPLED")
    shader.inputs["Roughness"].default_value = spec["roughness"]

    albedo = _load_image(TEXTURE_DIR / f"T_MD_{spec['texture']}_B.png")
    tint = (*spec["tint"], 1.0)
    if albedo:
        image = tree.nodes.new("ShaderNodeTexImage")
        image.image = albedo
        multiply = tree.nodes.new("ShaderNodeMix")
        multiply.data_type = "RGBA"
        multiply.blend_type = "MULTIPLY"
        multiply.inputs["Factor"].default_value = 1.0
        multiply.inputs["B"].default_value = tint
        tree.links.new(image.outputs["Color"], multiply.inputs["A"])
        tree.links.new(multiply.outputs["Result"], shader.inputs["Base Color"])
    else:
        shader.inputs["Base Color"].default_value = tint

    normal_image = _load_image(TEXTURE_DIR / f"T_MD_{spec['texture']}_N.png")
    if normal_image:
        normal_image.colorspace_settings.name = "Non-Color"
        image = tree.nodes.new("ShaderNodeTexImage")
        image.image = normal_image
        normal = tree.nodes.new("ShaderNodeNormalMap")
        tree.links.new(image.outputs["Color"], normal.inputs["Color"])
        tree.links.new(normal.outputs["Normal"], shader.inputs["Normal"])

    if "emissive" in spec:
        shader.inputs["Emission Color"].default_value = tint
        shader.inputs["Emission Strength"].default_value = spec["emissive"]
    if spec.get("shader") == "flow" and albedo:
        _show_streaks_as_alpha(material, albedo, spec["opacity"], tint)
    return material


def _show_streaks_as_alpha(material, streaks, opacity, tint):
    """흐르는 물은 언리얼처럼 결 그림의 흑백 값을 불투명도로 써서 결 사이가 비치게 보여 준다. 색은 tint만 쓴다."""
    tree = material.node_tree
    shader = next(node for node in tree.nodes if node.type == "BSDF_PRINCIPLED")
    image = next(node for node in tree.nodes if node.type == "TEX_IMAGE" and node.image == streaks)
    for link in list(shader.inputs["Base Color"].links):
        tree.links.remove(link)
    shader.inputs["Base Color"].default_value = tint
    alpha = tree.nodes.new("ShaderNodeMath")
    alpha.operation = "MULTIPLY"
    alpha.inputs[1].default_value = opacity
    tree.links.new(image.outputs["Color"], alpha.inputs[0])
    tree.links.new(alpha.outputs["Value"], shader.inputs["Alpha"])
    material.surface_render_method = "BLENDED"
    material.use_backface_culling = False


def load_materials():
    kit = _module("mesh_kit")
    return {name: _make_material(name, spec) for name, spec in kit.MATERIALS.items()}


def _material(name):
    material = bpy.data.materials.get(f"MD_{name}")
    if material is None:
        load_materials()
        material = bpy.data.materials[f"MD_{name}"]
    return material


def to_object(mesh_data, collection_name, location=(0.0, 0.0, 0.0), yaw=0.0):
    """MeshData를 장면의 물체로 만든다. 같은 이름이 있으면 교체한다."""
    old = bpy.data.objects.get(mesh_data.name)
    if old:
        old_mesh = old.data
        bpy.data.objects.remove(old)
        if old_mesh and old_mesh.users == 0:
            bpy.data.meshes.remove(old_mesh)
    faces = [list(range(poly.start, poly.start + poly.count)) for poly in mesh_data.polys]
    mesh = bpy.data.meshes.new(mesh_data.name)
    mesh.from_pydata(mesh_data.verts, [], faces)
    slots = {}
    for poly in mesh_data.polys:
        if poly.material not in slots:
            slots[poly.material] = len(slots)
            mesh.materials.append(_material(poly.material))
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for polygon, poly in zip(mesh.polygons, mesh_data.polys):
        polygon.material_index = slots[poly.material]
        polygon.use_smooth = poly.smooth
        for loop_index, uv in zip(polygon.loop_indices, poly.uvs):
            uv_layer.data[loop_index].uv = uv
    mesh.update()
    obj = bpy.data.objects.new(mesh_data.name, mesh)
    _collection(collection_name).objects.link(obj)
    obj.location = location
    obj.rotation_euler = (0.0, 0.0, math.radians(yaw))
    return obj


def _load_plan(plan_path):
    modules = _reload_builders()
    return modules, modules["map_plan"].load_plan(SCRIPT_DIR / plan_path)


def show_site(plan_path):
    """토목만 만든다. 건물을 얹기 전에 땅이 맞는지 본다."""
    modules, plan = _load_plan(plan_path)
    errors = [str(issue) for issue in modules["map_plan"].validate(plan) if issue.level == "error"]
    if errors:
        return errors
    clear(("MD_Site",))
    meshes = modules["site_builder"].build_site(plan)
    for mesh_data in meshes:
        obj = to_object(mesh_data, "MD_Site")
        if mesh_data.name == "Bounds":
            obj.hide_viewport = True
            obj.hide_render = True
    return [mesh.name for mesh in meshes]


def show_building(plan_path, building_id, in_place=False):
    """건물 하나를 만든다. in_place가 아니면 원점에 세워 앞·옆·뒤를 살핀다."""
    modules, plan = _load_plan(plan_path)
    spec = next(entry for entry in plan.buildings if entry["id"] == building_id)
    builder = modules["building_builder"]
    x, y, _, _ = spec["rect"]
    location, yaw = builder.placement(spec, plan.cell(x, y).z) if in_place else ((0.0, 0.0, 0.0), 0.0)
    return to_object(builder.build_building(spec), "MD_Buildings", location, yaw).name


def show_kit(name, location=(0.0, 0.0, 0.0)):
    kit = _module("props_builder")
    _module("mesh_kit")
    if name in kit.EFFECTS:
        to_object(kit.EFFECTS[name](), "MD_Preview", location)
    for spec in kit.LIGHTS.get(name, []):
        _add_light(spec, "MD_Preview", location, 0.0)
    return to_object(kit.KITS[name](), "MD_Preview", location).name


def _add_light(spec, collection_name, location, yaw):
    """소품에 딸린 불빛을 놓는다. 언리얼에서 쓸 값은 물체 속성으로 붙여 manifest로 넘긴다."""
    collection = _collection(collection_name)
    name = f"MD_Light_{len(collection.objects)}"
    angle = math.radians(yaw)
    dx, dy, dz = spec["offset"]
    data = bpy.data.lights.new(name, "POINT")
    data.color = spec["color"]
    data.energy = spec["candela"] * PREVIEW_LIGHT_W_PER_CANDELA
    data.shadow_soft_size = spec["source_radius_m"]
    obj = bpy.data.objects.new(name, data)
    obj.location = (location[0] + dx * math.cos(angle) - dy * math.sin(angle),
                    location[1] + dx * math.sin(angle) + dy * math.cos(angle),
                    location[2] + dz)
    for key in ("candela", "radius_m", "source_radius_m"):
        obj[key] = spec[key]
    collection.objects.link(obj)
    return obj


def _place(built, build, collection_name, location, yaw):
    """같은 소품은 처음 만든 메시를 같이 쓴다."""
    key = (collection_name, build)
    if key not in built:
        built[key] = to_object(build(), collection_name, location, yaw)
        return built[key]
    copy = built[key].copy()
    copy.location = location
    copy.rotation_euler = (0.0, 0.0, math.radians(yaw))
    _collection(collection_name).objects.link(copy)
    return copy


def _prop_instances(plan, modules):
    """소품 하나를 한 번만 만들고 나머지는 같은 메시를 공유한다. 언리얼에서도 같은 메시를 쓴다.

    소품에 딸린 효과(물줄기, 등불 유리)는 MD_Effects에 같은 자리로 놓는다. 언리얼에서 충돌과 그림자를 끈다.
    소품에 딸린 불빛은 MD_Lights에 놓는다.
    """
    kits = modules["props_builder"]
    built = {}
    for prop in plan.props:
        name = prop["kit"]
        at = prop["at"]
        location = (at[0], at[1], plan.ground_height(at[0], at[1]))
        yaw = prop.get("yaw", 0.0)
        _place(built, kits.KITS[name], "MD_Props", location, yaw)
        if name in kits.EFFECTS:
            _place(built, kits.EFFECTS[name], "MD_Effects", location, yaw)
        for spec in kits.LIGHTS.get(name, []):
            _add_light(spec, "MD_Lights", location, yaw)


def show_all(plan_path):
    """도면 전체를 장면에 만든다."""
    result = show_site(plan_path)
    if result and str(result[0]).startswith("["):
        return result
    modules, plan = _load_plan(plan_path)
    clear(("MD_Buildings", "MD_Props", "MD_Effects", "MD_Lights", "MD_Preview"))
    builder = modules["building_builder"]
    for spec in plan.buildings:
        x, y, _, _ = spec["rect"]
        location, yaw = builder.placement(spec, plan.cell(x, y).z)
        to_object(builder.build_building(spec), "MD_Buildings", location, yaw)
    _prop_instances(plan, modules)
    return len(bpy.data.collections["MD_Buildings"].objects), len(bpy.data.collections["MD_Props"].objects)


def _find_route(plan, directions, start, goal):
    previous = {start: None}
    queue = deque([start])
    while queue:
        cell = queue.popleft()
        if cell == goal:
            break
        for direction, (dx, dy) in directions.items():
            neighbor = (cell[0] + dx, cell[1] + dy)
            if neighbor not in previous and plan.can_cross(cell[0], cell[1], direction):
                previous[neighbor] = cell
                queue.append(neighbor)
    route = []
    cell = goal
    while cell is not None:
        route.append(cell)
        cell = previous.get(cell)
    return route[::-1]


def _walk(plan, max_step, route, ray):
    """경로를 잘게 나누어 바닥이 있는지, 턱이 높지 않은지, 앞이 막히지 않았는지 잰다."""
    problems = []
    last = None
    for (ax, ay), (bx, by) in zip(route, route[1:]):
        for step in range(ROUTE_SAMPLES_PER_CELL):
            t = step / ROUTE_SAMPLES_PER_CELL
            x, y = ax + 0.5 + (bx - ax) * t, ay + 0.5 + (by - ay) * t
            expected = plan.ground_height(x, y)
            hit, point, _, _, _, _ = ray(Vector((x, y, expected + ROUTE_RAY_ABOVE_M)), Vector((0, 0, -1)), ROUTE_RAY_ABOVE_M + ROUTE_RAY_BELOW_M)
            if not hit:
                problems.append(f"no floor at ({x:.2f}, {y:.2f})")
                continue
            z = point.z
            if abs(z - expected) > ROUTE_HEIGHT_TOLERANCE_M:
                problems.append(f"height {z:.2f} vs plan {expected:.2f} at ({x:.2f}, {y:.2f})")
            if last is not None:
                if abs(z - last[2]) > max_step:
                    problems.append(f"step {z - last[2]:+.2f} m at ({x:.2f}, {y:.2f})")
                knee = max(z, last[2]) + ROUTE_KNEE_HEIGHT_M
                origin = Vector((last[0], last[1], knee))
                ahead = Vector((x, y, knee)) - origin
                if ahead.length > 0 and ray(origin, ahead.normalized(), ahead.length)[0]:
                    problems.append(f"blocked at ({x:.2f}, {y:.2f})")
            last = (x, y, z)
    return problems


def verify_routes(plan_path):
    """구워진 메시 위로 스폰 사이 길을 걸어 본다. 언리얼 충돌은 이 삼각형을 그대로 쓴다.

    도면 검사가 설계를 본다면 이 검사는 결과물을 본다. 도면에 없는 턱, 뚫린 바닥, 길을 막은 소품을 잡는다.
    """
    modules, plan = _load_plan(plan_path)
    map_plan = modules["map_plan"]
    # 물과 효과는 충돌이 없으므로 재는 동안 숨긴다.
    no_collision = [bpy.data.objects[name] for name in ("Water",) if name in bpy.data.objects]
    no_collision += list(_collection("MD_Effects").objects)
    for obj in no_collision:
        obj.hide_viewport = True
    bpy.context.view_layer.update()
    scene, depsgraph = bpy.context.scene, bpy.context.evaluated_depsgraph_get()

    def ray(origin, direction, distance):
        return scene.ray_cast(depsgraph, origin, direction, distance=distance)

    results = {}
    crossed = set()
    start = tuple(math.floor(v) for v in plan.spawns[0]["at"])
    for spawn in plan.spawns[1:]:
        goal = tuple(math.floor(v) for v in spawn["at"])
        route = _find_route(plan, map_plan.DIRECTIONS, start, goal)
        crossed |= {plan.cell(*cell).link.id for cell in route if plan.cell(*cell).link}
        results[f"{spawn['tag']}@{spawn['at']}"] = _walk(plan, map_plan.MAX_STEP_M, route, ray)
    for link in plan.links.values():
        if link.id in crossed or link.low_z < plan.data["ground"]["z"]:
            continue
        x, y, width, depth = link.rect
        cx, cy = x + width // 2, y + depth // 2
        dx, dy = link.axis
        reach = link.length // 2 + 1
        route = _find_route(plan, map_plan.DIRECTIONS, (cx - dx * reach, cy - dy * reach), (cx + dx * reach, cy + dy * reach))
        crossed.add(link.id)
        results[f"{link.kind} {link.id}"] = _walk(plan, map_plan.MAX_STEP_M, route, ray)
    for obj in no_collision:
        obj.hide_viewport = False
    return results, sorted(crossed)


def _export_fbx(obj, path):
    """물체를 원점 기준으로 내보낸다. 놓는 자리는 manifest가 기억한다."""
    location, rotation = obj.location.copy(), obj.rotation_euler.copy()
    hidden = obj.hide_viewport
    obj.hide_viewport = False
    obj.location = (0, 0, 0)
    obj.rotation_euler = (0, 0, 0)
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=str(path),
        use_selection=True,
        object_types={"MESH"},
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_ALL",
        axis_forward="-Y",
        axis_up="Z",
        mesh_smooth_type="FACE",
        add_leaf_bones=False,
        bake_anim=False,
        use_mesh_modifiers=False,
    )
    obj.location, obj.rotation_euler = location, rotation
    obj.hide_viewport = hidden


def export_for_unreal(plan_path):
    """장면을 FBX와 manifest.json으로 내보낸다. 언리얼 쪽은 이 목록만 보고 레벨을 만든다."""
    show_all(plan_path)
    _, plan = _load_plan(plan_path)
    out_dir = PROJECT_DIR / "Saved" / "MapDesign" / plan.name
    fbx_dir = out_dir / "fbx"
    fbx_dir.mkdir(parents=True, exist_ok=True)
    meshes, placements, exported = [], [], set()
    for collection_name, kind_of in (("MD_Site", "site"), ("MD_Buildings", "building"), ("MD_Props", "prop"), ("MD_Effects", "effect")):
        for obj in _collection(collection_name).objects:
            kind = {"Water": "water", "Bounds": "bounds"}.get(obj.data.name, kind_of)
            if obj.data.name not in exported:
                exported.add(obj.data.name)
                path = fbx_dir / f"{obj.data.name}.fbx"
                _export_fbx(obj, path)
                meshes.append({"name": obj.data.name, "fbx": str(path), "kind": kind})
            placements.append({"mesh": obj.data.name, "at": list(obj.location), "yaw": math.degrees(obj.rotation_euler.z)})
    lights = [{"at": list(obj.location), "color": list(obj.data.color), "candela": obj["candela"], "radius_m": obj["radius_m"],
               "source_radius_m": obj["source_radius_m"]} for obj in _collection("MD_Lights").objects]
    manifest = {"name": plan.name, "bounds": [plan.width, plan.depth], "meshes": meshes, "placements": placements, "lights": lights,
                "spawns": [dict(spawn, z=plan.ground_height(*spawn["at"])) for spawn in plan.spawns], "materials": _module("mesh_kit").MATERIALS, "texture_dir": str(TEXTURE_DIR)}
    (out_dir / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=1), encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(out_dir / f"{plan.name}.blend"))
    return len(meshes), len(placements), str(out_dir)
