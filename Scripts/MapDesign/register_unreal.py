"""만든 맵을 게임의 맵 목록(DA_Level의 IngameLevels)에 등록하고 썸네일을 가져온다.

실행: UnrealEditor-Cmd <uproject> -run=pythonscript -script="Scripts/MapDesign/register_unreal.py <맵이름> <맵 키> <최대 인원> <썸네일 png>"
(-script 안의 경로는 슬래시로 적는다. 역슬래시는 `\r`처럼 이스케이프로 읽힌다.)
- 맵 키는 방 목록·전적·서버 세션에 남는 이름이다.
- 화면 이름은 MenuText.csv의 `Map.<맵 키>` 줄에 먼저 적는다. 로비와 방 목록이 그 줄을 쓰고,
  맵 목록의 DisplayName도 그 줄을 가리키게 해 번역을 한 곳(MenuText,Map.<맵 키>)에서만 한다.
- 같은 맵 키가 이미 있으면 그 항목을 고쳐 쓴다. 다시 돌려도 항목이 늘지 않는다.
- 쿡 목록(DefaultGame.ini의 MapsToCook)과 번역(Game.po)은 이 스크립트 밖에서 맞춘다.
"""
import os
import sys

import unreal

LEVEL_DEFINITION = "/Game/Data/DA_Level"
THUMBNAIL_FOLDER = "/Game/UI/Asset/Images/Map"
MENU_TEXT_TABLE = "MenuText"


def log(message):
    # 커맨드렛에서는 print와 일반 로그가 보이지 않아 경고로 남긴다.
    unreal.log_warning(f"MAPREGISTER {message}")


def import_thumbnail(path, name):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = THUMBNAIL_FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(f"{THUMBNAIL_FOLDER}/{name}")


def main():
    map_name, map_key, max_players, thumbnail_path = sys.argv[-4:]
    name_key = f"Map.{map_key}"
    if not unreal.StringTableLibrary.is_registered_table_entry(MENU_TEXT_TABLE, name_key):
        raise RuntimeError(f"add {name_key} to MenuText.csv first")
    world = unreal.load_asset(f"/Game/Map/LV_{map_name}")
    if world is None:
        raise RuntimeError(f"/Game/Map/LV_{map_name} does not exist")
    thumbnail = import_thumbnail(os.path.abspath(thumbnail_path), map_key)

    definition = unreal.load_asset(LEVEL_DEFINITION)
    levels = list(definition.get_editor_property("ingame_levels"))
    index = next((i for i, level in enumerate(levels) if str(level.get_editor_property("map_key")) == map_key), None)
    fields = {"map_key": map_key, "display_name": unreal.TextLibrary.text_from_string_table(MENU_TEXT_TABLE, name_key),
              "map": world, "max_player_count": int(max_players), "thumbnail": thumbnail}
    if index is not None:
        for name in ("map_layout", "initial_player_map_region"):
            fields[name] = levels[index].get_editor_property(name)
    # 구조체 속성은 EditDefaultsOnly라 사본에 하나씩 쓸 수 없고, 생성자 인수로는 넣을 수 있다.
    option = unreal.LobbyMatchMapOption(**fields)
    if index is None:
        levels.append(option)
    else:
        levels[index] = option
    definition.set_editor_property("ingame_levels", levels)
    unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False)
    for level in definition.get_editor_property("ingame_levels"):
        log(level.export_text())


main()
