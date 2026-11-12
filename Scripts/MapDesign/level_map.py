"""레벨 지도(DA_Level의 MapLayout)를 정하고, 칠한 지도 그림을 들여와 붙인다. 언리얼 에디터 파이썬.

실행: UnrealEditor-Cmd <uproject> -run=pythonscript -script="Scripts/MapDesign/level_map.py <명령> <맵 키>..."
- fit <맵 키>...   지도가 한 장인 레벨: 놀이 공간(정적 메시 위치의 5~95% 범위)을 감싸는 정사각형을 지도 범위로 정한다.
                   이미 지도 범위가 있으면 건드리지 않는다(지역이 여럿인 레벨은 범위를 손으로 정한다).
- apply <맵 키>... level_map_style.py가 칠한 그림(Saved/MapStyle/styled/<맵 키>)을 들여와 지도·전체 지도·지역 빛·썸네일에 붙인다.
순서: fit → 게임에서 지도 찍기(LabProject.Tools.MapCapture) → level_map_style.py → apply.
"""
import os
import sys

import unreal

LEVEL_DEFINITION = "/Game/Data/DA_Level"
MAP_FOLDER = "/Game/UI/Asset/Images/Map"
STYLED = os.path.join(unreal.Paths.project_saved_dir(), "MapStyle", "styled")
PLAY_SPREAD = (0.05, 0.95)     # 놀이 공간: 정적 메시 위치의 이 범위(먼 배경 산에 끌려가지 않게)
PLAY_MARGIN = 1.15             # 놀이 공간 둘레 여유
OPTION_FIELDS = ("map_key", "display_name", "map", "max_player_count", "thumbnail", "map_layout", "initial_player_map_region")


def log(message):
    # 커맨드렛에서는 print와 일반 로그가 보이지 않아 경고로 남긴다.
    unreal.log_warning(f"LEVELMAP {message}")


def load_levels():
    definition = unreal.load_asset(LEVEL_DEFINITION)
    return definition, list(definition.get_editor_property("ingame_levels"))


def make_layout(pages, area_view, thumbnail_yaw):
    """찍을 때만 쓰는 값(에디터 전용 ThumbnailYaw)은 생성자 인수로 받지 않아 만든 뒤 쓴다."""
    layout = unreal.LevelMapLayout(pages=pages, area_view=area_view)
    layout.set_editor_property("thumbnail_yaw", thumbnail_yaw)
    return layout


def replace_option(levels, index, **changes):
    """맵 목록 항목은 EditDefaultsOnly라 사본에 하나씩 쓸 수 없어, 바뀐 값을 넣어 새로 만든다."""
    fields = {name: levels[index].get_editor_property(name) for name in OPTION_FIELDS}
    fields.update(changes)
    levels[index] = unreal.LobbyMatchMapOption(**fields)


def find_index(levels, map_key):
    for index, level in enumerate(levels):
        if str(level.get_editor_property("map_key")) == map_key:
            return index
    raise RuntimeError(f"{map_key} is not in {LEVEL_DEFINITION}")


def play_bounds(world_path):
    unreal.EditorLoadingAndSavingUtils.load_map(world_path)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    points = [actor.get_actor_location() for actor in actors if actor.get_component_by_class(unreal.StaticMeshComponent)]
    if len(points) < 4:
        raise RuntimeError(f"{world_path} has too few meshes to frame a map")

    def spread(values):
        values = sorted(values)
        pick = lambda fraction: values[round(fraction * (len(values) - 1))]
        return pick(PLAY_SPREAD[0]), pick(PLAY_SPREAD[1])

    (x0, x1), (y0, y1) = spread([p.x for p in points]), spread([p.y for p in points])
    return (x0 + x1) / 2, (y0 + y1) / 2, max(x1 - x0, y1 - y0) * PLAY_MARGIN


def fit(map_keys):
    definition, levels = load_levels()
    for map_key in map_keys:
        index = find_index(levels, map_key)
        layout = levels[index].get_editor_property("map_layout")
        if len(layout.get_editor_property("pages")) > 0:
            log(f"{map_key} already has a map layout, left as is")
            continue
        world_path = levels[index].get_editor_property("map").get_path_name().split(".")[0]
        cx, cy, side = play_bounds(world_path)
        page = unreal.LevelMapPage(world_center=unreal.Vector2D(cx, cy), world_yaw=0.0, world_size=unreal.Vector2D(side, side))
        replace_option(levels, index, map_layout=make_layout([page], layout.get_editor_property("area_view"),
                                                             layout.get_editor_property("thumbnail_yaw")))
        log(f"{map_key} map page center=({cx:.0f}, {cy:.0f}) side={side:.0f}")
    definition.set_editor_property("ingame_levels", levels)
    log(f"saved={unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False)}")


def import_texture(path, folder, name):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(f"{folder}/{name}")
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


def page_name(map_key, page, count):
    """지도가 한 장이면 T_Map_<맵 키>, 여럿이면 지역 이름을 붙인다(T_Map_WindNest_Dome)."""
    return f"T_Map_{map_key}" if count == 1 else f"T_Map_{map_key}_{page.get_editor_property('region').name.title()}"


def apply(map_keys):
    definition, levels = load_levels()
    for map_key in map_keys:
        index = find_index(levels, map_key)
        source = os.path.join(STYLED, map_key)
        folder = f"{MAP_FOLDER}/{map_key}"
        layout = levels[index].get_editor_property("map_layout")
        pages = list(layout.get_editor_property("pages"))
        for page_index, page in enumerate(pages):
            name = page_name(map_key, page, len(pages))
            page.set_editor_property("texture", import_texture(os.path.join(source, f"page{page_index}.png"), folder, name))
            highlight = os.path.join(source, f"highlight{page_index}.png")
            if os.path.exists(highlight):
                page.set_editor_property("area_highlight", import_texture(highlight, folder, f"{name}_Glow"))
        area_view = layout.get_editor_property("area_view")
        if len(pages) > 1:
            area_view.set_editor_property("texture", import_texture(os.path.join(source, "area.png"), folder, f"T_Map_{map_key}_Area"))
        thumbnail = levels[index].get_editor_property("thumbnail")
        thumbnail_name = thumbnail.get_name() if thumbnail else map_key
        thumbnail = import_texture(os.path.join(source, "thumbnail.png"), MAP_FOLDER, thumbnail_name)
        replace_option(levels, index, thumbnail=thumbnail,
                       map_layout=make_layout(pages, area_view, layout.get_editor_property("thumbnail_yaw")))
        log(f"{map_key} applied {len(pages)} page(s){' + area' if len(pages) > 1 else ''} + thumbnail {thumbnail_name}")
    definition.set_editor_property("ingame_levels", levels)
    log(f"saved={unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False)}")


if __name__ == "__main__":
    command, keys = sys.argv[1], sys.argv[2:]
    {"fit": fit, "apply": apply}[command](keys)
