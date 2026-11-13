"""Blender가 내보낸 manifest.json을 언리얼 에셋과 레벨로 만든다.

실행: UnrealEditor-Cmd <uproject> -run=pythonscript -script="Scripts/MapDesign/import_unreal.py CanalTown"
다시 돌리면 메시와 재질을 교체하고, 레벨은 이 스크립트가 놓은 액터만 새로 놓는다. 손으로 추가한 액터는 남는다.

좌표: Blender는 오른손, 언리얼은 왼손 좌표계이므로 y를 뒤집고 회전도 반대로 돈다. 미터를 센티미터로 바꾼다.
"""
import json
import math
import os
import sys

import unreal

GENERATED_TAG = "MapDesign"
# 게임 맵은 월드 설정에 Experience가 있어야 시작한다. 그 속성은 스크립트로 쓸 수 없어, 액터가 적은 맵을 복사해 월드 설정만 물려받고 액터는 비운다.
LEVEL_SETTINGS_TEMPLATE = "/Game/Map/LV_TrainingRoom"
CM_PER_M = 100.0

# 캡슐 반높이보다 조금 높게 놓아 스폰과 몬스터가 바닥에 끼지 않게 한다. 몬스터 생성기는 제 자리에 그대로 몬스터를 만든다.
SPAWN_HEIGHT_CM = 100.0
# 보상 상자는 다른 맵과 같은 블루프린트다. 보상은 블루프린트 기본값(DA_Reward)이다.
REWARD_CHEST_CLASS = "/Game/Env/BP_RewardChest.BP_RewardChest_C"
# 몬스터 생성기도 콜로세움·마을·바람 둥지와 같은 블루프린트다. 몬스터 종류, 되살아나는 시간, 추적 범위는 블루프린트 기본값이다.
MONSTER_SPAWNER_CLASS = "/Game/StackOBot/AI/BP_BugSpawner.BP_BugSpawner_C"
# 맵 경계를 벗어난 캐릭터를 Respawn 시작점에서 되살리는 구(AOutOfBoundsVolume). 다른 맵의 BP_RespawnTrigger는
# 정해 둔 한 좌표로 순간이동시키므로 새 맵에서는 쓰지 않는다.
OUT_OF_BOUNDS_CLASS = "/Game/Env/BP_OutOfBoundsVolume.BP_OutOfBoundsVolume_C"
# 구는 맵 가운데 바닥에 두고, 막는 벽 꼭대기의 모서리까지 감싼 뒤 이만큼 더 넓힌다.
OUT_OF_BOUNDS_MARGIN_CM = 500.0
NAV_BRUSH_SIZE_CM = 200.0
NAV_MARGIN_CM = 500.0
NAV_HEIGHT_CM = 3000.0

SUN_ROTATION = unreal.Rotator(0.0, -42.0, 125.0)
SUN_INTENSITY_LUX = 8.0
SUN_COLOR = unreal.LinearColor(1.0, 0.93, 0.82, 1.0)
SKY_INTENSITY = 1.2
FOG_DENSITY = 0.006
EXPOSURE_BIAS = 0.3
WATER_OPACITY = 0.82
WATER_PAN_SPEED = 0.012
# 흐르는 물의 무늬가 1초에 움직이는 UV. v 1은 재질의 tile_m(1.2m)이라 초속 2m 남짓으로 떨어진다.
# Blender의 v는 흐르는 쪽으로 커지고 FBX를 가져오면 v가 뒤집히므로, 양수로 흘리면 무늬가 흐르는 쪽으로 간다.
FLOW_PAN_SPEED = 1.8
# 흐르는 물은 아래 물면보다 위에 있다. 반투명끼리 순서가 뒤바뀌어 물면이 물줄기를 덮지 않게 나중에 그린다.
FLOW_SORT_PRIORITY = 1
DEFAULT_NORMAL_TEXTURE = "/Engine/EngineMaterials/DefaultNormal"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
material_library = unreal.MaterialEditingLibrary
asset_library = unreal.EditorAssetLibrary


def log(message):
    # 커맨드렛에서는 print와 일반 로그가 보이지 않아 경고로 남긴다.
    unreal.log_warning(f"MAPDESIGN {message}")


def to_unreal(location):
    return unreal.Vector(location[0] * CM_PER_M, -location[1] * CM_PER_M, location[2] * CM_PER_M)


def to_unreal_yaw(yaw):
    return -yaw


def import_texture(path, folder, is_normal):
    name = os.path.splitext(os.path.basename(path))[0]
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    asset_tools.import_asset_tasks([task])
    texture = unreal.load_asset(f"{folder}/{name}")
    if is_normal:
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        # texture_kit의 노멀은 OpenGL 방식이고 언리얼은 초록 채널이 반대다.
        texture.set_editor_property("flip_green_channel", True)
    return texture


def new_expression(material, expression_class, x, y):
    return material_library.create_material_expression(material, expression_class, x, y)


def set_normal_sampler(expression):
    """노멀 샘플러의 기본 텍스처도 노멀이어야 한다. 비워 두면 색 텍스처가 들어가 재질 컴파일이 실패한다."""
    expression.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    expression.set_editor_property("texture", unreal.load_asset(DEFAULT_NORMAL_TEXTURE))


def create_surface_master(folder):
    """모든 표면 재질의 부모. 색, 노멀, 거칠기, 발광만 받아 Blender 미리보기와 입력을 같게 둔다."""
    path = f"{folder}/M_MD_Surface"
    if asset_library.does_asset_exist(path):
        return unreal.load_asset(path)
    material = asset_tools.create_asset("M_MD_Surface", folder, unreal.Material, unreal.MaterialFactoryNew())
    albedo = new_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, -200)
    albedo.set_editor_property("parameter_name", "Albedo")
    tint = new_expression(material, unreal.MaterialExpressionVectorParameter, -700, 50)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    multiply = new_expression(material, unreal.MaterialExpressionMultiply, -350, -100)
    material_library.connect_material_expressions(albedo, "RGB", multiply, "A")
    material_library.connect_material_expressions(tint, "", multiply, "B")
    material_library.connect_material_property(multiply, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal = new_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, 300)
    normal.set_editor_property("parameter_name", "Normal")
    set_normal_sampler(normal)
    material_library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)

    roughness = new_expression(material, unreal.MaterialExpressionScalarParameter, -350, 150)
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.85)
    material_library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    emissive = new_expression(material, unreal.MaterialExpressionScalarParameter, -700, 600)
    emissive.set_editor_property("parameter_name", "EmissiveIntensity")
    emissive.set_editor_property("default_value", 0.0)
    glow = new_expression(material, unreal.MaterialExpressionMultiply, -350, 500)
    material_library.connect_material_expressions(tint, "", glow, "A")
    material_library.connect_material_expressions(emissive, "", glow, "B")
    material_library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    material_library.recompile_material(material)
    return material


def create_water_master(folder):
    """반투명 물. 같은 노멀을 서로 다른 속도로 흘려 잔물결을 만든다."""
    path = f"{folder}/M_MD_Water"
    if asset_library.does_asset_exist(path):
        return unreal.load_asset(path)
    material = asset_tools.create_asset("M_MD_Water", folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)

    tint = new_expression(material, unreal.MaterialExpressionVectorParameter, -700, -200)
    tint.set_editor_property("parameter_name", "Tint")
    material_library.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)

    roughness = new_expression(material, unreal.MaterialExpressionScalarParameter, -700, 0)
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.08)
    material_library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    opacity = new_expression(material, unreal.MaterialExpressionScalarParameter, -700, 150)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", WATER_OPACITY)
    material_library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    samples = []
    for index, (speed_x, speed_y) in enumerate(((WATER_PAN_SPEED, WATER_PAN_SPEED * 0.6), (-WATER_PAN_SPEED * 0.7, WATER_PAN_SPEED))):
        panner = new_expression(material, unreal.MaterialExpressionPanner, -1000, 300 + index * 250)
        panner.set_editor_property("speed_x", speed_x)
        panner.set_editor_property("speed_y", speed_y)
        sample = new_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, 300 + index * 250)
        sample.set_editor_property("parameter_name", "Normal")
        set_normal_sampler(sample)
        material_library.connect_material_expressions(panner, "", sample, "UVs")
        samples.append(sample)
    blend = new_expression(material, unreal.MaterialExpressionLinearInterpolate, -350, 400)
    blend.set_editor_property("const_alpha", 0.5)
    material_library.connect_material_expressions(samples[0], "RGB", blend, "A")
    material_library.connect_material_expressions(samples[1], "RGB", blend, "B")
    material_library.connect_material_property(blend, "", unreal.MaterialProperty.MP_NORMAL)
    material_library.recompile_material(material)
    return material


def create_flow_master(folder):
    """흐르는 물(분수 물줄기, 물막, 물보라). 결 그림을 v(흐르는 쪽)로 흘리고 그 흑백 값을 불투명도와 빛으로 써서 결 사이가 비친다.

    물막은 얇은 한 겹이라 양면으로 그린다.
    """
    path = f"{folder}/M_MD_WaterFlow"
    if asset_library.does_asset_exist(path):
        return unreal.load_asset(path)
    material = asset_tools.create_asset("M_MD_WaterFlow", folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    material.set_editor_property("two_sided", True)

    tint = new_expression(material, unreal.MaterialExpressionVectorParameter, -700, -300)
    tint.set_editor_property("parameter_name", "Tint")
    material_library.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)

    roughness = new_expression(material, unreal.MaterialExpressionScalarParameter, -700, -100)
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.05)
    material_library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    panner = new_expression(material, unreal.MaterialExpressionPanner, -1100, 200)
    panner.set_editor_property("speed_x", 0.0)
    panner.set_editor_property("speed_y", FLOW_PAN_SPEED)
    streaks = new_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -800, 100)
    streaks.set_editor_property("parameter_name", "Albedo")
    material_library.connect_material_expressions(panner, "", streaks, "UVs")
    normal = new_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -800, 400)
    normal.set_editor_property("parameter_name", "Normal")
    set_normal_sampler(normal)
    material_library.connect_material_expressions(panner, "", normal, "UVs")
    material_library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)

    opacity = new_expression(material, unreal.MaterialExpressionScalarParameter, -700, 650)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.7)
    masked = new_expression(material, unreal.MaterialExpressionMultiply, -400, 550)
    material_library.connect_material_expressions(streaks, "R", masked, "A")
    material_library.connect_material_expressions(opacity, "", masked, "B")
    material_library.connect_material_property(masked, "", unreal.MaterialProperty.MP_OPACITY)

    emissive = new_expression(material, unreal.MaterialExpressionScalarParameter, -700, 800)
    emissive.set_editor_property("parameter_name", "EmissiveIntensity")
    emissive.set_editor_property("default_value", 0.0)
    lit_streaks = new_expression(material, unreal.MaterialExpressionMultiply, -400, -200)
    material_library.connect_material_expressions(tint, "", lit_streaks, "A")
    material_library.connect_material_expressions(streaks, "R", lit_streaks, "B")
    glow = new_expression(material, unreal.MaterialExpressionMultiply, -200, -200)
    material_library.connect_material_expressions(lit_streaks, "", glow, "A")
    material_library.connect_material_expressions(emissive, "", glow, "B")
    material_library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    material_library.recompile_material(material)
    return material


def create_instances(manifest, masters, textures, folder):
    """materials.json의 재질마다 인스턴스를 만든다. 이름은 Blender 재질 슬롯과 짝이 맞는다. shader가 부모 재질을 고른다."""
    instances = {}
    for name, spec in manifest["materials"].items():
        path = f"{folder}/MI_MD_{name}"
        if asset_library.does_asset_exist(path):
            instance = unreal.load_asset(path)
        else:
            instance = asset_tools.create_asset(f"MI_MD_{name}", folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        shader = spec.get("shader", "surface")
        material_library.set_material_instance_parent(instance, masters[shader])
        tint = unreal.LinearColor(*spec["tint"], 1.0)
        material_library.set_material_instance_vector_parameter_value(instance, "Tint", tint)
        material_library.set_material_instance_scalar_parameter_value(instance, "Roughness", spec["roughness"])
        material_library.set_material_instance_texture_parameter_value(instance, "Normal", textures[f"T_MD_{spec['texture']}_N"])
        if shader != "water":
            material_library.set_material_instance_texture_parameter_value(instance, "Albedo", textures[f"T_MD_{spec['texture']}_B"])
            material_library.set_material_instance_scalar_parameter_value(instance, "EmissiveIntensity", spec.get("emissive", 0.0))
        if "opacity" in spec:
            material_library.set_material_instance_scalar_parameter_value(instance, "Opacity", spec["opacity"])
        instances[name] = instance
    return instances


def import_mesh(entry, folder):
    name = f"SM_MD_{entry['name']}"
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    data.combine_meshes = True
    data.auto_generate_collision = False
    data.generate_lightmap_u_vs = False
    data.remove_degenerates = True
    # FBX의 숫자는 미터인데 언리얼은 센티미터로 읽는다. 교체 가져오기는 처음 설정을 계속 쓰므로 이 값을 바꾸면 Meshes 폴더를 지우고 다시 가져온다.
    data.import_uniform_scale = CM_PER_M

    task = unreal.AssetImportTask()
    task.filename = entry["fbx"]
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = options
    asset_tools.import_asset_tasks([task])
    mesh = unreal.load_asset(f"{folder}/{name}")
    if mesh is None:
        raise RuntimeError(f"Failed to import {entry['fbx']}")
    return mesh


def setup_mesh(mesh, kind, instances):
    """슬롯 이름으로 재질을 잇고, 충돌은 보이는 면 그대로 쓴다. 계단과 벽을 캡슐이 정확히 밟아야 한다."""
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.material_slot_name).replace("MD_", "", 1)
        if slot_name in instances:
            mesh.set_material(index, instances[slot_name])
        else:
            log(f"no material for slot {slot.material_slot_name} on {mesh.get_name()}")
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)


def clear_generated_actors(actors):
    for actor in actors.get_all_level_actors():
        if GENERATED_TAG in [str(tag) for tag in actor.tags]:
            actors.destroy_actor(actor)


def clear_template_actors(actors):
    """템플릿에서 온 액터를 모두 지운다. 월드 설정은 지워지지 않고 남는다."""
    for actor in actors.get_all_level_actors():
        if not isinstance(actor, unreal.WorldSettings):
            actors.destroy_actor(actor)


def tag(actor, label):
    actor.set_actor_label(label)
    actor.tags = [GENERATED_TAG]
    return actor


def place_meshes(actors, manifest, meshes):
    counts = {}
    kinds = {entry["name"]: entry["kind"] for entry in manifest["meshes"]}
    for placement in manifest["placements"]:
        name = placement["mesh"]
        rotation = unreal.Rotator(0.0, 0.0, to_unreal_yaw(placement["yaw"]))
        # 커맨드렛에는 뷰포트가 없어 에셋으로 스폰하는 경로가 실패한다. 빈 액터에 메시를 넣는다.
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, to_unreal(placement["at"]), rotation)
        component = actor.static_mesh_component
        component.set_static_mesh(meshes[name])
        counts[name] = counts.get(name, 0) + 1
        tag(actor, f"{name}_{counts[name]}")
        kind = kinds[name]
        if kind in ("water", "effect"):
            component.set_collision_profile_name("NoCollision")
            component.set_editor_property("cast_shadow", False)
            if kind == "effect":
                component.set_editor_property("translucency_sort_priority", FLOW_SORT_PRIORITY)
        elif kind == "bounds":
            actor.set_actor_hidden_in_game(True)
            component.set_editor_property("cast_shadow", False)


def place_lamps(actors, manifest):
    """소품에 딸린 불빛(가로등). 정적 조명을 쓰지 않는 프로젝트라 움직이는 점광원으로 놓는다.

    색은 sRGB로 바꿔 넣는다. 유리 안에 있지만 유리는 그림자를 드리우지 않으므로 빛이 밖으로 나온다.
    """
    for index, light in enumerate(manifest.get("lights", [])):
        actor = actors.spawn_actor_from_class(unreal.PointLight, to_unreal(light["at"]), unreal.Rotator(0, 0, 0))
        tag(actor, f"Lamp_{index}")
        component = actor.get_component_by_class(unreal.PointLightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        component.set_editor_property("intensity", light["candela"])
        component.set_light_color(unreal.LinearColor(*light["color"], 1.0), True)
        component.set_editor_property("attenuation_radius", light["radius_m"] * CM_PER_M)
        component.set_editor_property("source_radius", light["source_radius_m"] * CM_PER_M)


def place_spawns(actors, manifest, level_height_of):
    for index, spawn in enumerate(manifest["spawns"]):
        x, y = spawn["at"]
        location = to_unreal((x, y, level_height_of(index)))
        location.z += SPAWN_HEIGHT_CM
        actor = actors.spawn_actor_from_class(unreal.PlayerStart, location, unreal.Rotator(0.0, 0.0, to_unreal_yaw(spawn["yaw"])))
        actor.set_editor_property("player_start_tag", spawn["tag"])
        tag(actor, f"Start_{spawn['tag']}_{index}")


def place_chests(actors, manifest):
    """보상 상자. 도면 검사가 자리(평지, 스폰·출입구·소품과의 거리)를 확인했다. 높이는 도면의 땅 높이다."""
    chest_class = unreal.load_class(None, REWARD_CHEST_CLASS)
    for index, chest in enumerate(manifest.get("chests", [])):
        x, y = chest["at"]
        rotation = unreal.Rotator(0.0, 0.0, to_unreal_yaw(chest["yaw"]))
        tag(actors.spawn_actor_from_class(chest_class, to_unreal((x, y, chest["z"])), rotation), f"Chest_{index}")


def place_monsters(actors, manifest):
    """몬스터 생성기. 도면 검사가 자리(평지, 스폰과 다른 생성기와의 거리)를 확인했다. 높이는 도면의 땅 높이다."""
    spawner_class = unreal.load_class(None, MONSTER_SPAWNER_CLASS)
    for index, monster in enumerate(manifest.get("monsters", [])):
        x, y = monster["at"]
        location = to_unreal((x, y, monster["z"]))
        location.z += SPAWN_HEIGHT_CM
        rotation = unreal.Rotator(0.0, 0.0, to_unreal_yaw(monster["yaw"]))
        tag(actors.spawn_actor_from_class(spawner_class, location, rotation), f"Monster_{index}")


def place_out_of_bounds(actors, manifest):
    """경계 구. 막는 벽을 넘거나 바닥을 뚫고 떨어져 구 밖으로 나가면 Respawn 시작점에서 되살아난다."""
    width, depth = manifest["bounds"]
    center = unreal.Vector(width * CM_PER_M / 2, -depth * CM_PER_M / 2, 0.0)
    reach_m = math.sqrt((width / 2) ** 2 + (depth / 2) ** 2 + manifest["ceiling_z"] ** 2)
    volume = actors.spawn_actor_from_class(unreal.load_class(None, OUT_OF_BOUNDS_CLASS), center, unreal.Rotator(0, 0, 0))
    tag(volume, "OutOfBounds")
    volume.get_component_by_class(unreal.SphereComponent).set_sphere_radius(reach_m * CM_PER_M + OUT_OF_BOUNDS_MARGIN_CM)


def place_lighting(actors):
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), SUN_ROTATION)
    tag(sun, "Sun")
    component = sun.get_component_by_class(unreal.DirectionalLightComponent)
    component.set_editor_property("intensity", SUN_INTENSITY_LUX)
    # 빛 색은 sRGB FColor로 들어간다. to_rgbe()를 넣으면 RGBE 값이 그대로 색이 되어 해가 어둡고 잿빛이 된다.
    component.set_light_color(SUN_COLOR, True)
    component.set_editor_property("atmosphere_sun_light", True)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)

    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3200), unreal.Rotator(0, 0, 0))
    tag(sky, "Sky")
    component = sky.get_component_by_class(unreal.SkyLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("real_time_capture", True)
    component.set_editor_property("intensity", SKY_INTENSITY)

    tag(actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 3400), unreal.Rotator(0, 0, 0)), "Atmosphere")
    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 0))
    tag(fog, "Fog")
    fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property("fog_density", FOG_DENSITY)

    post = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 300), unreal.Rotator(0, 0, 0))
    tag(post, "PostProcess")
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", EXPOSURE_BIAS)
    post.set_editor_property("settings", settings)


def place_navigation(actors, bounds):
    """길찾기 범위. 훈련 봇과 몬스터가 쓰는 내비게이션을 맵 전체에 깐다."""
    width, depth = bounds
    center = unreal.Vector(width * CM_PER_M / 2, -depth * CM_PER_M / 2, 0.0)
    volume = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, center, unreal.Rotator(0, 0, 0))
    tag(volume, "NavBounds")
    volume.set_actor_scale3d(unreal.Vector(
        (width * CM_PER_M + NAV_MARGIN_CM) / NAV_BRUSH_SIZE_CM,
        (depth * CM_PER_M + NAV_MARGIN_CM) / NAV_BRUSH_SIZE_CM,
        NAV_HEIGHT_CM / NAV_BRUSH_SIZE_CM,
    ))


def ground_heights(manifest):
    """스폰의 높이는 도면에서 온다."""
    return [spawn.get("z", 0.0) for spawn in manifest["spawns"]]


def main():
    map_name = sys.argv[-1]
    project_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
    manifest_path = os.path.join(project_dir, "MapDesign", map_name, "manifest.json")
    with open(manifest_path, encoding="utf-8") as file:
        manifest = json.load(file)

    # Interchange 대신 옛 FBX 경로를 써야 FbxImportUI 옵션이 먹는다.
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")

    root = f"/Game/Map/{map_name}"
    texture_folder, material_folder, mesh_folder = f"{root}/Textures", f"{root}/Materials", f"{root}/Meshes"

    textures = {}
    for name in {spec["texture"] for spec in manifest["materials"].values()}:
        for suffix, is_normal in (("B", False), ("N", True)):
            path = os.path.join(manifest["texture_dir"], f"T_MD_{name}_{suffix}.png")
            textures[f"T_MD_{name}_{suffix}"] = import_texture(path, texture_folder, is_normal)
    log(f"imported {len(textures)} textures")

    masters = {
        "surface": create_surface_master(material_folder),
        "water": create_water_master(material_folder),
        "flow": create_flow_master(material_folder),
    }
    instances = create_instances(manifest, masters, textures, material_folder)
    log(f"created {len(instances)} material instances")

    meshes = {}
    for entry in manifest["meshes"]:
        mesh = import_mesh(entry, mesh_folder)
        setup_mesh(mesh, entry["kind"], instances)
        meshes[entry["name"]] = mesh
    log(f"imported {len(meshes)} meshes")
    # 레벨을 바꿀 때 메모리 정리가 돌면 저장하지 않은 에셋이 사라지므로 먼저 저장하고, 뒤에서 다시 읽는다.
    asset_library.save_directory(root, only_if_is_dirty=True)

    level_path = f"/Game/Map/LV_{map_name}"
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if asset_library.does_asset_exist(level_path):
        levels.load_level(level_path)
        clear_generated_actors(actors)
    else:
        levels.new_level_from_template(level_path, LEVEL_SETTINGS_TEMPLATE)
        clear_template_actors(actors)

    meshes = {entry["name"]: unreal.load_asset(f"{mesh_folder}/SM_MD_{entry['name']}") for entry in manifest["meshes"]}
    place_meshes(actors, manifest, meshes)
    place_lamps(actors, manifest)
    heights = ground_heights(manifest)
    place_spawns(actors, manifest, lambda index: heights[index])
    place_chests(actors, manifest)
    place_monsters(actors, manifest)
    place_out_of_bounds(actors, manifest)
    place_lighting(actors)
    place_navigation(actors, manifest["bounds"])
    levels.save_current_level()
    asset_library.save_directory(root, only_if_is_dirty=True)
    log(f"level saved {level_path} with {len(manifest['placements'])} meshes")

    first = next(iter(meshes.values()))
    bounds = first.get_bounding_box()
    log(f"check bounds of {first.get_name()}: {bounds.min} {bounds.max}")


main()
