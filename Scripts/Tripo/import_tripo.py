"""Tripo로 만든 모델(GLB·FBX)을 언리얼로 들여온다. 언리얼 에디터 파이썬.

실행: UnrealEditor-Cmd <uproject> -run=pythonscript -script="Scripts/Tripo/import_tripo.py <모델 파일> <애셋 이름>"
(-script 안의 경로는 슬래시로 적는다. 역슬래시는 `\r`처럼 이스케이프로 읽힌다.)
- 모델 파일은 `tripo make ... -o Saved/Tripo/<이름>`이 내려받은 model.glb 또는 model.fbx다.
- 들여온 메시·재질·텍스처는 /Game/Tripo/<애셋 이름>/ 아래에 놓인다. 같은 이름이면 덮어쓴다.
- 그래픽 애셋이라 커밋하지 않는다(커밋 범위 규칙). 맵에 쓸지는 사용자가 정한다.
"""
import os
import sys

import unreal

DESTINATION_ROOT = "/Game/Tripo"


def log(message):
    # 커맨드렛에서는 print와 일반 로그가 보이지 않아 경고로 남긴다.
    unreal.log_warning(f"TRIPOIMPORT {message}")


def main():
    model_path, asset_name = os.path.abspath(sys.argv[-2]), sys.argv[-1]
    if not os.path.exists(model_path):
        raise RuntimeError(f"{model_path} does not exist")
    destination = f"{DESTINATION_ROOT}/{asset_name}"
    task = unreal.AssetImportTask()
    task.filename = model_path
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = list(task.get_editor_property("imported_object_paths"))
    if not imported:
        raise RuntimeError(f"nothing was imported from {model_path}")
    for path in imported:
        log(f"imported {path}")


if __name__ == "__main__":
    main()
