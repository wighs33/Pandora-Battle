"""도면의 "kit" 소품을 코드로 만든다. 같은 소품은 메시 하나를 공유한다."""
from __future__ import annotations

import math

from mesh_kit import MATERIALS, MeshData

LATHE_SEGMENTS = 36
SMALL_LATHE_SEGMENTS = 12

# (반지름, 높이) 단면. 아래에서 위로 적는다.
FOUNTAIN_BASIN = [(3.5, 0.0), (3.5, 0.5), (3.68, 0.58), (3.68, 0.72), (3.2, 0.72), (3.2, 0.3), (1.0, 0.3)]
FOUNTAIN_COLUMN = [(1.0, 0.3), (0.72, 0.55), (0.46, 1.3), (0.56, 1.6), (0.34, 1.8)]
FOUNTAIN_UPPER_BOWL = [(0.34, 1.8), (1.25, 2.1), (1.72, 2.32), (1.72, 2.44), (1.45, 2.44), (1.3, 2.28), (0.34, 2.28)]
FOUNTAIN_TOP = [(0.34, 2.28), (0.24, 2.8), (0.34, 3.0), (0.14, 3.3), (0.2, 3.5), (0.0, 3.82)]
FOUNTAIN_BASIN_WATER_Z = 0.56
# 위 접시는 가장자리(2.44)까지 물이 차서 넘친다.
FOUNTAIN_BOWL_WATER_Z = 2.42
FOUNTAIN_BASIN_WATER = [(3.2, FOUNTAIN_BASIN_WATER_Z), (0.0, FOUNTAIN_BASIN_WATER_Z)]
FOUNTAIN_BOWL_WATER = [(1.45, FOUNTAIN_BOWL_WATER_Z), (0.0, FOUNTAIN_BOWL_WATER_Z)]

# 분수 물줄기. 꼭대기에서 솟아 위 접시로 떨어지는 줄기, 위 접시에서 넘쳐 아래 수반으로 떨어지는 물막, 떨어진 자리의 물보라.
# 모양은 던진 물체의 길(포물선)로 계산해, 줄기가 돌에 닿지 않고 물면에서 끝난다.
GRAVITY_M_S2 = 9.81
FOUNTAIN_JET_COUNT = 6
FOUNTAIN_JET_SEGMENTS = 8
FOUNTAIN_JET_SAMPLES = 18
# 꼭대기 원뿔 속에서 나와 0.33m 솟았다가 위 접시 물면의 반지름 1m 자리에 떨어진다.
FOUNTAIN_JET_START = (0.11, 3.62)
FOUNTAIN_JET_RISE_M = 0.33
FOUNTAIN_JET_LANDING_R = 1.0
# 나올 때 가늘고 떨어지며 퍼진다.
FOUNTAIN_JET_RADIUS_M = (0.022, 0.04)
# 물면을 따라 가장자리 돌 위로 넘어가는 단면. 돌 윗면(2.44)보다 1cm 남짓 뜬다.
FOUNTAIN_VEIL_OVER_RIM = [(1.36, 2.425), (1.48, 2.452), (1.72, 2.452)]
FOUNTAIN_VEIL_SPEED_M_S = 0.55
FOUNTAIN_VEIL_SAMPLES = 12
# 물보라 고리를 물면 위에 띄워 같은 면에 겹치지 않게 한다.
WATER_SURFACE_LIFT_M = 0.008
FOUNTAIN_BOWL_SPLASH_R = (0.82, 1.28)
FOUNTAIN_BASIN_SPLASH_R = (1.95, 2.65)

LANTERN_POST = [(0.17, 0.0), (0.15, 0.12), (0.07, 0.3), (0.05, 1.4), (0.07, 1.5), (0.045, 2.9), (0.09, 3.0)]
LANTERN_GLASS = [(0.09, 3.0), (0.2, 3.32), (0.17, 3.42)]
LANTERN_HOOD = [(0.17, 3.42), (0.27, 3.45), (0.05, 3.72), (0.0, 3.8)]
# 등불 빛. 유리 가운데에서 유리와 같은 색으로 빛난다. 밝기는 언리얼 단위(candela)다.
# 낮에 게임 화면으로 재어, 그늘진 가로등 밑이 따뜻하게 밝아지되(바닥 +16%, 벽 +35%) 벽 그림자는 남는 값으로 정했다.
# 5 cd는 거의 보이지 않고, 25 cd부터는 낮의 그림자가 지워진다.
LANTERN_LIGHT = {
    "offset": (0.0, 0.0, 3.21),
    "color": MATERIALS["LampGlow"]["tint"],
    "candela": 15.0,
    "radius_m": 9.0,
    "source_radius_m": 0.1,
}

BARREL_BODY = [(0.33, 0.0), (0.42, 0.28), (0.46, 0.55), (0.42, 0.82), (0.33, 1.1), (0.3, 1.1), (0.3, 1.0), (0.0, 1.0)]
BARREL_HOOPS = [(0.22, 0.408), (0.88, 0.408)]
BARREL_HOOP_HALF_M = 0.035

CRATE_FRAME_M = 0.1


def build_fountain():
    mesh = MeshData("Prop_fountain")
    origin = (0.0, 0.0, 0.0)
    for profile in (FOUNTAIN_BASIN, FOUNTAIN_COLUMN, FOUNTAIN_UPPER_BOWL, FOUNTAIN_TOP):
        mesh.add_lathe(origin, profile, LATHE_SEGMENTS, "Stone")
    for profile in (FOUNTAIN_BASIN_WATER, FOUNTAIN_BOWL_WATER):
        mesh.add_lathe(origin, profile, LATHE_SEGMENTS, "Water")
    return mesh


def _ballistic(start, velocity, end_z, samples):
    """(반지름, 높이)에서 (수평, 수직) 속도로 나온 물이 end_z에 닿을 때까지의 길. 같은 시간 간격으로 점을 찍어 꺾이는 꼭대기 쪽이 촘촘하다."""
    r0, z0 = start
    vr, vz = velocity
    # z0 + vz t - g t^2 / 2 = end_z 의 양의 근
    duration = (vz + math.sqrt(vz * vz + 2 * GRAVITY_M_S2 * (z0 - end_z))) / GRAVITY_M_S2
    times = [duration * i / (samples - 1) for i in range(samples)]
    return [(r0 + vr * t, z0 + vz * t - GRAVITY_M_S2 * t * t / 2) for t in times]


def build_fountain_flow():
    """분수에서 흐르는 물. 충돌 없이 분수 자리에 따로 놓는다. 재질이 무늬를 v(흐르는 쪽)로 흘린다."""
    mesh = MeshData("Prop_fountain_flow")
    origin = (0.0, 0.0, 0.0)

    rise_speed = math.sqrt(2 * GRAVITY_M_S2 * FOUNTAIN_JET_RISE_M)
    fall_m = FOUNTAIN_JET_START[1] + FOUNTAIN_JET_RISE_M - FOUNTAIN_BOWL_WATER_Z
    flight_s = rise_speed / GRAVITY_M_S2 + math.sqrt(2 * fall_m / GRAVITY_M_S2)
    out_speed = (FOUNTAIN_JET_LANDING_R - FOUNTAIN_JET_START[0]) / flight_s
    arc = _ballistic(FOUNTAIN_JET_START, (out_speed, rise_speed), FOUNTAIN_BOWL_WATER_Z, FOUNTAIN_JET_SAMPLES)
    thin, wide = FOUNTAIN_JET_RADIUS_M
    radii = [thin + (wide - thin) * i / (len(arc) - 1) for i in range(len(arc))]
    for jet in range(FOUNTAIN_JET_COUNT):
        angle = 2 * math.pi * (jet + 0.5) / FOUNTAIN_JET_COUNT
        path = [(r * math.cos(angle), r * math.sin(angle), z) for r, z in arc]
        mesh.add_tube(path, radii, FOUNTAIN_JET_SEGMENTS, "WaterFlow")

    lip = FOUNTAIN_VEIL_OVER_RIM[-1]
    fall = _ballistic(lip, (FOUNTAIN_VEIL_SPEED_M_S, 0.0), FOUNTAIN_BASIN_WATER_Z + WATER_SURFACE_LIFT_M, FOUNTAIN_VEIL_SAMPLES)
    # 물막은 위에서 아래로 적어 v가 흐르는 쪽으로 커진다. 그 순서로 돌리면 면이 안쪽을 봐 뒤집는다.
    # 반지름이 칸마다 커지므로 u를 가장자리 반지름 하나로 재야 결이 칸 경계에서 끊기지 않는다.
    mesh.add_lathe(origin, FOUNTAIN_VEIL_OVER_RIM + fall[1:], LATHE_SEGMENTS, "WaterFlow", flip=True, u_radius=lip[0])

    # 물보라는 떨어진 자리에서 바깥으로 퍼진다. 안에서 밖으로 적으면 면이 아래를 봐 뒤집는다.
    for (inner, outer), water_z in ((FOUNTAIN_BOWL_SPLASH_R, FOUNTAIN_BOWL_WATER_Z), (FOUNTAIN_BASIN_SPLASH_R, FOUNTAIN_BASIN_WATER_Z)):
        z = water_z + WATER_SURFACE_LIFT_M
        mesh.add_lathe(origin, [(inner, z), (outer, z)], LATHE_SEGMENTS, "WaterFlow", smooth=False, flip=True)
    return mesh


def build_lantern():
    """가로등 기둥과 갓. 유리는 build_lantern_glass로 따로 만든다."""
    mesh = MeshData("Prop_lantern")
    origin = (0.0, 0.0, 0.0)
    mesh.add_lathe(origin, LANTERN_POST, SMALL_LATHE_SEGMENTS, "Iron")
    mesh.add_lathe(origin, LANTERN_HOOD, SMALL_LATHE_SEGMENTS, "Iron")
    return mesh


def build_lantern_glass():
    """등불 유리. 불빛이 이 안에 있으므로 그림자를 드리우면 빛이 밖으로 나가지 못한다."""
    mesh = MeshData("Prop_lantern_glass")
    mesh.add_lathe((0.0, 0.0, 0.0), LANTERN_GLASS, SMALL_LATHE_SEGMENTS, "LampGlow")
    return mesh


def _crate(name, size):
    """판자 상자. 몸통 앞으로 모서리 각재를 내어 겹치는 면을 만들지 않는다."""
    mesh = MeshData(name)
    half = size / 2
    frame = CRATE_FRAME_M
    bar = frame / 2
    mesh.add_box((-half + bar, -half + bar, 0.0), (half - bar, half - bar, size - bar), "Wood", skip=("bottom",))
    corners = [(-half + bar, -half + bar), (half - bar, -half + bar), (half - bar, half - bar), (-half + bar, half - bar)]
    for x, y in corners:
        mesh.add_beam((x, y, 0.0), (x, y, size), (1, 0, 0), "Wood", bar, bar)
    for index, (x, y) in enumerate(corners):
        next_x, next_y = corners[(index + 1) % 4]
        dx, dy = next_x - x, next_y - y
        run = max(abs(dx), abs(dy))
        ux, uy = dx / run, dy / run
        start = (x + ux * frame, y + uy * frame)
        end = (next_x - ux * frame, next_y - uy * frame)
        for z in (bar, size - bar):
            mesh.add_beam((start[0], start[1], z), (end[0], end[1], z), (-uy, ux, 0.0), "Wood", bar * 0.8, bar * 0.8)
    return mesh


def build_crate():
    return _crate("Prop_crate", 0.9)


def build_crate_large():
    return _crate("Prop_crate_large", 1.3)


def build_barrel():
    mesh = MeshData("Prop_barrel")
    origin = (0.0, 0.0, 0.0)
    mesh.add_lathe(origin, BARREL_BODY, LATHE_SEGMENTS, "Wood")
    for z, radius in BARREL_HOOPS:
        mesh.add_lathe(origin, [(radius, z - BARREL_HOOP_HALF_M), (radius, z + BARREL_HOOP_HALF_M)], LATHE_SEGMENTS, "Iron")
    return mesh


def build_bench():
    """돌다리에 나무 판을 얹은 벤치."""
    mesh = MeshData("Prop_bench")
    half = 1.1
    for x in (-half + 0.2, half - 0.2):
        mesh.add_box((x - 0.14, -0.2, 0.0), (x + 0.14, 0.2, 0.42), "Stone", skip=("bottom", "top"))
    mesh.add_box((-half, -0.26, 0.42), (half, 0.26, 0.5), "Wood")
    return mesh


def build_cart():
    """손수레. 수레 몸통과 바퀴, 손잡이를 따로 만들어 겹치는 면을 피한다."""
    mesh = MeshData("Prop_cart")
    bed_z = 0.75
    side_top = bed_z + 0.62
    half_length, half_width = 1.3, 0.7
    wall = 0.06
    mesh.add_box((-half_length, -half_width, bed_z), (half_length, half_width, bed_z + 0.1), "Wood")
    sides = [
        ((-half_length, -half_width, bed_z + 0.1), (half_length, -half_width + wall, side_top)),
        ((-half_length, half_width - wall, bed_z + 0.1), (half_length, half_width, side_top)),
        ((-half_length, -half_width + wall, bed_z + 0.1), (-half_length + wall, half_width - wall, side_top)),
        ((half_length - wall, -half_width + wall, bed_z + 0.1), (half_length, half_width - wall, side_top)),
    ]
    for low, high in sides:
        mesh.add_box(low, high, "Wood", skip=("bottom",))
    axle_z = 0.5
    mesh.add_cylinder((0.0, -half_width - 0.2, axle_z), (0.0, half_width + 0.2, axle_z), 0.05, 8, "Wood")
    for y in (-half_width - 0.14, half_width + 0.14):
        mesh.add_cylinder((0.0, y - 0.05, axle_z), (0.0, y + 0.05, axle_z), 0.5, 18, "Wood", smooth=False)
        mesh.add_cylinder((0.0, y - 0.07, axle_z), (0.0, y + 0.07, axle_z), 0.11, 10, "Iron")
    for y in (-half_width + 0.1, half_width - 0.1):
        mesh.add_beam((half_length, y, bed_z + 0.05), (half_length + 1.5, y, 0.12), (0, 1, 0), "Wood", 0.05, 0.05)
    return mesh


def _crown(mesh, clusters, seed):
    for index, (center, radii) in enumerate(clusters):
        mesh.add_blob(center, radii, seed + index, "Leaves")


def build_tree_round():
    """둥근 수관. 키는 건물 2층 높이라 시야를 다 막지 않는다."""
    mesh = MeshData("Prop_tree_round")
    mesh.add_lathe((0, 0, 0), [(0.42, 0.0), (0.26, 0.5), (0.2, 2.3), (0.13, 3.6)], SMALL_LATHE_SEGMENTS, "Bark")
    mesh.add_beam((0.0, 0.0, 2.2), (0.9, 0.3, 3.3), (0.3, -0.9, 0.0), "Bark", 0.08, 0.08)
    mesh.add_beam((0.0, 0.0, 2.6), (-0.8, -0.4, 3.6), (0.4, -0.8, 0.0), "Bark", 0.07, 0.07)
    _crown(mesh, [
        ((0.0, 0.0, 4.6), (2.0, 2.0, 1.6)),
        ((1.3, 0.5, 3.9), (1.3, 1.3, 1.05)),
        ((-1.2, -0.7, 4.0), (1.25, 1.25, 1.0)),
        ((0.4, -1.3, 3.9), (1.1, 1.1, 0.9)),
        ((-0.5, 1.2, 4.1), (1.15, 1.15, 0.95)),
        ((0.2, 0.2, 5.6), (1.2, 1.2, 0.85)),
    ], 40)
    return mesh


def build_tree_wide():
    mesh = MeshData("Prop_tree_wide")
    mesh.add_lathe((0, 0, 0), [(0.5, 0.0), (0.32, 0.45), (0.24, 1.8), (0.16, 2.8)], SMALL_LATHE_SEGMENTS, "Bark")
    mesh.add_beam((0.0, 0.0, 1.9), (1.5, 0.2, 3.0), (0.2, -1.0, 0.0), "Bark", 0.1, 0.1)
    mesh.add_beam((0.0, 0.0, 2.1), (-1.4, 0.6, 3.1), (0.4, 0.9, 0.0), "Bark", 0.09, 0.09)
    _crown(mesh, [
        ((1.6, 0.2, 3.7), (1.7, 1.6, 1.05)),
        ((-1.5, 0.6, 3.8), (1.6, 1.5, 1.0)),
        ((0.0, -1.2, 3.7), (1.6, 1.3, 0.95)),
        ((0.1, 1.2, 4.1), (1.6, 1.4, 1.05)),
        ((0.0, 0.0, 4.5), (1.9, 1.8, 1.0)),
    ], 60)
    return mesh


def build_tree_cypress():
    """좁고 높은 측백나무. 길가에 세워도 길을 막지 않는다."""
    mesh = MeshData("Prop_tree_cypress")
    mesh.add_lathe((0, 0, 0), [(0.2, 0.0), (0.14, 0.6), (0.1, 1.2)], SMALL_LATHE_SEGMENTS, "Bark")
    mesh.add_blob((0.0, 0.0, 3.2), (0.95, 0.95, 2.6), 80, "Leaves", roughness=0.1, subdivisions=3)
    mesh.add_blob((0.0, 0.0, 5.3), (0.55, 0.55, 1.5), 81, "Leaves", roughness=0.1)
    return mesh


def build_bush():
    mesh = MeshData("Prop_bush")
    _crown(mesh, [
        ((0.0, 0.0, 0.5), (0.85, 0.85, 0.62)),
        ((0.65, 0.25, 0.42), (0.6, 0.6, 0.46)),
        ((-0.55, -0.35, 0.4), (0.58, 0.58, 0.44)),
    ], 90)
    return mesh


KITS = {
    "fountain": build_fountain,
    "lantern": build_lantern,
    "crate": build_crate,
    "crate_large": build_crate_large,
    "barrel": build_barrel,
    "bench": build_bench,
    "cart": build_cart,
    "tree_round": build_tree_round,
    "tree_wide": build_tree_wide,
    "tree_cypress": build_tree_cypress,
    "bush": build_bush,
}

# 소품에 딸린 부분 중 충돌과 그림자가 없어야 하는 것. 몸체와 같은 자리에 따로 놓는다.
# 분수 물줄기는 길을 막거나 캐릭터가 그 위에 서면 안 되고, 등불 유리는 안에 든 불빛을 가리면 안 된다.
EFFECTS = {
    "fountain": build_fountain_flow,
    "lantern": build_lantern_glass,
}

# 소품에 딸린 불빛. offset은 소품 원점에서 잰 자리다.
LIGHTS = {
    "lantern": [LANTERN_LIGHT],
}
