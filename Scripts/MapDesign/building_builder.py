"""건물 한 채를 공통 부품으로 짓는다.

벽과 지붕을 먼저 잡고, 창틀·처마·목재는 같은 함수로 얹어 건물끼리 규칙이 같다.
규칙은 제작 중 실패에서 나왔다.
- 창은 벽에 실제로 구멍을 내고 넣는다. 창 앞을 막는 면을 만들지 않는다.
- 장식은 벽보다 앞으로 내고 서로 같은 면에 겹치지 않게 끊는다. 겹치면 화면에서 깜빡인다.
- 원점은 정면 벽의 바닥 중앙이다. 그래야 정면을 보도 경계에 정확히 맞출 수 있다.
- 지붕 밑면은 벽 위 목재보다 높게 얹는다. 목재가 벽 앞으로 나와 있어 그대로 얹으면 지붕을 뚫는다.

좌표: 정면 벽이 y=0, 건물은 -y로 뻗고, x는 정면 폭 방향이다.
"""
from __future__ import annotations

import math
import random
from dataclasses import dataclass

from mesh_kit import MATERIALS, Frame, MeshData, scale

FLOOR_HEIGHT_M = 3.0
PLINTH_HEIGHT_M = 0.8
BAY_WIDTH_M = 2.6
CORNER_MARGIN_M = 0.55

WINDOW_WIDTH_M = 1.05
WINDOW_HEIGHT_M = 1.45
WINDOW_SILL_M = 0.95
WINDOW_REVEAL_M = 0.18
GLASS_INSET_M = 0.14
FRAME_BAR_M = 0.08
MULLION_M = 0.05
MULLION_DEPTH_M = 0.04
SILL_DROP_M = 0.1
SILL_OVERHANG_M = 0.08
SHUTTER_WIDTH_M = 0.46
SHUTTER_GAP_M = 0.03
SHUTTER_DEPTH_M = 0.05

DOOR_WIDTH_M = 1.3
DOOR_HEIGHT_M = 2.35
DOOR_INSET_M = 0.14
DOOR_TRIM_M = 0.16
DOOR_TRIM_DEPTH_M = 0.07
DOOR_STEP_DEPTH_M = 0.45
DOOR_STEP_HEIGHT_M = 0.12

TIMBER_DEPTH_M = 0.05
TIMBER_POST_M = 0.2
TIMBER_BAND_M = 0.18
TIMBER_STUD_M = 0.13

ROOF_PITCH_DEG = 46.0
ROOF_EAVE_M = 0.5
ROOF_RAKE_M = 0.35
ROOF_THICKNESS_M = 0.16
# 벽 앞으로 나온 목재가 처마 밑면에 닿지 않게 벽을 이만큼 더 올린다.
ROOF_SEAT_CLEARANCE_M = 0.08
RIDGE_HALF_M = 0.12

CHIMNEY_WIDTH_M = 0.9
CHIMNEY_ABOVE_RIDGE_M = 0.9
CHIMNEY_CAP_M = 0.12
CHIMNEY_CAP_OVERHANG_M = 0.1


@dataclass
class Opening:
    a0: float
    b0: float
    a1: float
    b1: float
    kind: str


def footprint(spec):
    """(정면 폭, 깊이). 동서를 보는 건물은 사각형의 깊이가 정면 폭이 된다."""
    _, _, width, depth = spec["rect"]
    return (width, depth) if spec["front"] in ("N", "S") else (depth, width)


def placement(spec, z):
    """건물 원점을 놓을 위치와 z축 회전(도). 정면 벽이 사각형의 정면 변과 정확히 겹친다."""
    x, y, width, depth = spec["rect"]
    front = spec["front"]
    if front == "N":
        return (x + width / 2, y + depth, z), 0.0
    if front == "S":
        return (x + width / 2, y, z), 180.0
    if front == "E":
        return (x + width, y + depth / 2, z), -90.0
    return (x, y + depth / 2, z), 90.0


def _bay_centers(length):
    """벽 길이에 맞춰 창 자리를 고르게 나눈다."""
    usable = length - 2 * CORNER_MARGIN_M
    count = max(1, int(usable // BAY_WIDTH_M))
    bay = usable / count
    return [CORNER_MARGIN_M + bay * (index + 0.5) for index in range(count)], bay


def _window(center, floor):
    b0 = floor * FLOOR_HEIGHT_M + WINDOW_SILL_M
    return Opening(center - WINDOW_WIDTH_M / 2, b0, center + WINDOW_WIDTH_M / 2, b0 + WINDOW_HEIGHT_M, "window")


def _wall_openings(length, floors, has_door, rng):
    """벽 하나의 창과 문 배치."""
    centers, bay = _bay_centers(length)
    openings = []
    door_bay = len(centers) // 2 if has_door else -1
    for floor in range(floors):
        for index, center in enumerate(centers):
            if floor == 0 and index == door_bay:
                openings.append(Opening(center - DOOR_WIDTH_M / 2, 0.0, center + DOOR_WIDTH_M / 2, DOOR_HEIGHT_M, "door"))
            elif has_door or rng.random() > 0.3:
                openings.append(_window(center, floor))
    return openings, centers, bay


def _slice_panels(length, height, openings):
    """구멍을 피해 벽을 사각형 조각으로 나눈다. 창 뒤에 면이 남지 않는다."""
    us = sorted({0.0, length, *(o.a0 for o in openings), *(o.a1 for o in openings)})
    vs = sorted({0.0, PLINTH_HEIGHT_M, height, *(o.b0 for o in openings), *(o.b1 for o in openings)})
    for a0, a1 in zip(us, us[1:]):
        for b0, b1 in zip(vs, vs[1:]):
            middle_a, middle_b = (a0 + a1) / 2, (b0 + b1) / 2
            if not any(o.a0 < middle_a < o.a1 and o.b0 < middle_b < o.b1 for o in openings):
                yield a0, b0, a1, b1


def _build_reveal(mesh, frame, opening, depth, material):
    """구멍 안쪽의 벽 두께."""
    a0, b0, a1, b1 = opening.a0, opening.b0, opening.a1, opening.b1
    sides = [
        ([(a0, b0), (a0, b1)], frame.u),
        ([(a1, b0), (a1, b1)], scale(frame.u, -1)),
        ([(a0, b1), (a1, b1)], scale(frame.v, -1)),
        ([(a0, b0), (a1, b0)], frame.v),
    ]
    for (p, q), facing in sides:
        points = [frame.at(p[0], p[1], 0), frame.at(q[0], q[1], 0), frame.at(q[0], q[1], -depth), frame.at(p[0], p[1], -depth)]
        mesh.add_face(points, material, facing=facing)


def _build_window(mesh, frame, opening, wall_material, shutters):
    a0, b0, a1, b1 = opening.a0, opening.b0, opening.a1, opening.b1
    _build_reveal(mesh, frame, opening, WINDOW_REVEAL_M, wall_material)
    frame.quad(mesh, a0, b0, a1, b1, "Glass", -GLASS_INSET_M)

    # 틀은 네 변이 서로 겹치지 않게 위아래 막대 사이에 세로 막대를 끼운다.
    bar = FRAME_BAR_M
    glass = -GLASS_INSET_M
    front = -0.02
    frame.box(mesh, a0, b0, a1, b0 + bar, glass, front, "Wood")
    frame.box(mesh, a0, b1 - bar, a1, b1, glass, front, "Wood")
    frame.box(mesh, a0, b0 + bar, a0 + bar, b1 - bar, glass, front, "Wood")
    frame.box(mesh, a1 - bar, b0 + bar, a1, b1 - bar, glass, front, "Wood")

    # 살은 가운데 세로살을 기준으로 가로살을 좌우로 나눈다.
    middle_a = (a0 + a1) / 2
    middle_b = (b0 + b1) / 2
    half = MULLION_M / 2
    bar_front = glass + MULLION_DEPTH_M
    frame.box(mesh, middle_a - half, b0 + bar, middle_a + half, b1 - bar, glass, bar_front, "Wood")
    frame.box(mesh, a0 + bar, middle_b - half, middle_a - half, middle_b + half, glass, bar_front, "Wood")
    frame.box(mesh, middle_a + half, middle_b - half, a1 - bar, middle_b + half, glass, bar_front, "Wood")

    frame.box(mesh, a0 - SILL_OVERHANG_M, b0 - SILL_DROP_M, a1 + SILL_OVERHANG_M, b0, 0.0, SILL_OVERHANG_M, "Stone")
    if shutters:
        for s0, s1 in ((a0 - SHUTTER_GAP_M - SHUTTER_WIDTH_M, a0 - SHUTTER_GAP_M), (a1 + SHUTTER_GAP_M, a1 + SHUTTER_GAP_M + SHUTTER_WIDTH_M)):
            frame.box(mesh, s0, b0, s1, b1, 0.0, SHUTTER_DEPTH_M, "Wood")


def _build_door(mesh, frame, opening, wall_material):
    a0, a1, b1 = opening.a0, opening.a1, opening.b1
    _build_reveal(mesh, frame, Opening(a0, 0.0, a1, b1, "door"), DOOR_INSET_M, "Stone")
    frame.quad(mesh, a0, 0.0, a1, b1, "Wood", -DOOR_INSET_M)
    trim = DOOR_TRIM_M
    depth = DOOR_TRIM_DEPTH_M
    frame.box(mesh, a0 - trim, 0.0, a0, b1, 0.0, depth, "Stone")
    frame.box(mesh, a1, 0.0, a1 + trim, b1, 0.0, depth, "Stone")
    frame.box(mesh, a0 - trim, b1, a1 + trim, b1 + trim, 0.0, depth, "Stone")
    frame.box(mesh, a0 - trim, 0.0, a1 + trim, DOOR_STEP_HEIGHT_M, 0.0, DOOR_STEP_DEPTH_M, "Curb")


def _build_timber(mesh, frame, length, height, floors, openings, bay, centers):
    """윗층 벽의 목재 뼈대. 모서리 기둥 사이에 띠를 끼우고, 띠 사이에 샛기둥을 끼운다."""
    depth = TIMBER_DEPTH_M
    frame.box(mesh, 0.0, PLINTH_HEIGHT_M, TIMBER_POST_M, height, 0.0, depth, "Wood")
    frame.box(mesh, length - TIMBER_POST_M, PLINTH_HEIGHT_M, length, height, 0.0, depth, "Wood")
    inner0, inner1 = TIMBER_POST_M, length - TIMBER_POST_M
    bands = [floor * FLOOR_HEIGHT_M for floor in range(1, floors)]
    bands.append(height - TIMBER_BAND_M)
    for z in bands:
        frame.box(mesh, inner0, z, inner1, z + TIMBER_BAND_M, 0.0, depth, "Wood")
    for floor in range(1, floors):
        bottom = floor * FLOOR_HEIGHT_M + TIMBER_BAND_M
        top = bands[floor] if floor < len(bands) else height - TIMBER_BAND_M
        for index in range(len(centers) - 1):
            a = (centers[index] + centers[index + 1]) / 2
            half = TIMBER_STUD_M / 2
            blocked = any(o.a0 - half < a < o.a1 + half and o.b0 < top and o.b1 > bottom for o in openings)
            if not blocked:
                frame.box(mesh, a - half, bottom, a + half, top, 0.0, depth, "Wood")


def _build_wall(mesh, frame, length, height, panel_top, floors, wall_material, has_door, shutters, rng):
    openings, centers, bay = _wall_openings(length, floors, has_door, rng)
    for a0, b0, a1, b1 in _slice_panels(length, panel_top, openings):
        material = "Stone" if b1 <= PLINTH_HEIGHT_M else wall_material
        frame.quad(mesh, a0, b0, a1, b1, material)
    for opening in openings:
        if opening.kind == "door":
            _build_door(mesh, frame, opening, wall_material)
        else:
            _build_window(mesh, frame, opening, wall_material, shutters)
    _build_timber(mesh, frame, length, height, floors, openings, bay, centers)


def _build_gable(mesh, frame, length, wall_top, rise, wall_material):
    """삼각 벽과 그 가운데 기둥. 지붕 밑을 막아 뒤가 뚫려 보이지 않게 한다."""
    apex = frame.at(length / 2, wall_top + rise)
    mesh.add_face([frame.at(0, wall_top), frame.at(length, wall_top), apex], wall_material, facing=frame.n)
    half = TIMBER_STUD_M / 2
    frame.box(mesh, length / 2 - half, wall_top, length / 2 + half, wall_top + rise - half, 0.0, TIMBER_DEPTH_M, "Wood")


def _build_roof(mesh, spec, width, depth, wall_top, roof_material):
    """맞배지붕. 용마루가 정면과 평행(width)이거나 수직(depth)이다."""
    ridge_along_x = spec["ridge"] == "width"
    pitch = math.radians(ROOF_PITCH_DEG)
    span = depth if ridge_along_x else width
    run = span / 2
    rise = run * math.tan(pitch)
    tile = MATERIALS[roof_material]["tile_m"]
    slope_len = (run + ROOF_EAVE_M) / math.cos(pitch)

    # 지붕 좌표: r은 용마루 방향, s는 가운데에서 처마 방향이다.
    def point(r, s, z):
        if ridge_along_x:
            return (r, -depth / 2 + s, z)
        return (s, -depth / 2 + r, z)

    r_half = (width if ridge_along_x else depth) / 2 + ROOF_RAKE_M
    eave_z = wall_top - ROOF_EAVE_M * math.tan(pitch)
    ridge_z = wall_top + rise
    drop = ROOF_THICKNESS_M / math.cos(pitch)

    for sign in (1, -1):
        eave = sign * (run + ROOF_EAVE_M)
        uvs = [(-r_half / tile, 0), (r_half / tile, 0), (r_half / tile, slope_len / tile), (-r_half / tile, slope_len / tile)]
        outward = (0, sign * math.sin(pitch), math.cos(pitch)) if ridge_along_x else (sign * math.sin(pitch), 0, math.cos(pitch))
        top = [point(-r_half, eave, eave_z), point(r_half, eave, eave_z), point(r_half, 0, ridge_z), point(-r_half, 0, ridge_z)]
        mesh.add_face(top, roof_material, facing=outward, uvs=uvs)
        under = [point(-r_half, eave, eave_z - drop), point(r_half, eave, eave_z - drop), point(r_half, 0, ridge_z - drop), point(-r_half, 0, ridge_z - drop)]
        mesh.add_face(under, "Wood", facing=scale(outward, -1))
        fascia = [point(-r_half, eave, eave_z - drop), point(r_half, eave, eave_z - drop), point(r_half, eave, eave_z), point(-r_half, eave, eave_z)]
        mesh.add_face(fascia, "Wood", facing=(0, sign, 0) if ridge_along_x else (sign, 0, 0))
        for r, r_sign in ((-r_half, -1), (r_half, 1)):
            edge = [point(r, eave, eave_z - drop), point(r, 0, ridge_z - drop), point(r, 0, ridge_z), point(r, eave, eave_z)]
            mesh.add_face(edge, "Wood", facing=(r_sign, 0, 0) if ridge_along_x else (0, r_sign, 0))

    across = (0, 1, 0) if ridge_along_x else (1, 0, 0)
    mesh.add_beam(point(-r_half, 0, ridge_z), point(r_half, 0, ridge_z), across, "Coping", RIDGE_HALF_M, RIDGE_HALF_M)
    return rise, ridge_z, point, r_half


def _build_chimney(mesh, point, width, depth, wall_top, ridge_z, ridge_along_x, rng):
    """지붕 위 굴뚝. 몸통을 지붕에 묻어 틈이 보이지 않게 한다."""
    half = CHIMNEY_WIDTH_M / 2
    along = (width if ridge_along_x else depth) / 2
    r = rng.choice((-1, 1)) * along * rng.uniform(0.35, 0.6)
    s = rng.choice((-1, 1)) * (depth if ridge_along_x else width) * 0.16
    center = point(r, s, wall_top)
    top = ridge_z + CHIMNEY_ABOVE_RIDGE_M
    low = (center[0] - half, center[1] - half, wall_top)
    high = (center[0] + half, center[1] + half, top)
    mesh.add_box(low, high, "Stone", skip=("bottom", "top"))
    cap = CHIMNEY_CAP_OVERHANG_M
    mesh.add_box((low[0] - cap, low[1] - cap, top), (high[0] + cap, high[1] + cap, top + CHIMNEY_CAP_M), "Coping")


def build_building(spec):
    """건물 스펙으로 메시 하나를 만든다. 같은 seed는 항상 같은 건물이 된다."""
    rng = random.Random(spec["seed"])
    width, depth = footprint(spec)
    floors = spec["floors"]
    wall_material = spec["wall"]
    roof_material = spec["roof"]
    wall_top = floors * FLOOR_HEIGHT_M + TIMBER_BAND_M
    panel_top = wall_top + ROOF_SEAT_CLEARANCE_M
    shutters = rng.random() > 0.4
    mesh = MeshData(f"Building_{spec['id']}")

    walls = {
        "front": (Frame.facing((0, 1), (width / 2, 0.0, 0.0)), width),
        "right": (Frame.facing((1, 0), (width / 2, -depth, 0.0)), depth),
        "back": (Frame.facing((0, -1), (-width / 2, -depth, 0.0)), width),
        "left": (Frame.facing((-1, 0), (-width / 2, 0.0, 0.0)), depth),
    }
    for name, (frame, length) in walls.items():
        _build_wall(mesh, frame, length, wall_top, panel_top, floors, wall_material, name == "front", shutters, rng)

    roof_base = panel_top + ROOF_THICKNESS_M / math.cos(math.radians(ROOF_PITCH_DEG))
    rise, ridge_z, point, _ = _build_roof(mesh, spec, width, depth, roof_base, roof_material)
    gables = ("right", "left") if spec["ridge"] == "width" else ("front", "back")
    for name in gables:
        frame, length = walls[name]
        _build_gable(mesh, frame, length, panel_top, rise, wall_material)
    _build_chimney(mesh, point, width, depth, wall_top, ridge_z, spec["ridge"] == "width", rng)
    return mesh
