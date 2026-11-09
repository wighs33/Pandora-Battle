"""도면을 토목 메시로 만든다: 바닥, 경계석, 축대, 계단, 경사로, 다리, 물, 외벽.

높이 차가 있는 칸 사이에 벽을 세우는 규칙만 있으면 되므로, 도면을 바꿔도 돌 하나 고칠 일이 없다.
덩어리(chunk)로 나누어 내보내 언리얼이 보이지 않는 구역을 그리지 않게 한다.
"""
from __future__ import annotations

import math
from collections import defaultdict

from map_plan import DIRECTIONS, STAIR_RISER_M
from mesh_kit import Frame, MeshData

CHUNK_M = 24

CURB_WIDTH_M = 0.22
CURB_HEIGHT_M = 0.10
# 턱은 MAX_STEP_M보다 낮아야 걷다가 걸리지 않는다.
COPING_WIDTH_M = 0.45
COPING_HEIGHT_M = 0.14
COPING_OVERHANG_M = 0.05
# 이 높이부터는 축대로 보고 위에 갓돌을 얹는다.
RETAINING_MIN_M = 0.5
SEAM_SINK_M = 0.1

PARAPET_WIDTH_M = 0.4
PARAPET_HEIGHT_M = 0.85
PARAPET_CAP_M = 0.08
PARAPET_CAP_OVERHANG_M = 0.04

CHEEK_WIDTH_M = 0.35
CHEEK_HEIGHT_M = 0.7

DECK_THICKNESS_M = 0.45
ARCH_CROWN_GAP_M = 0.2
ARCH_SEGMENTS = 14

OUTER_WALL_THICKNESS_M = 1.0
BLOCK_THICKNESS_M = 0.5

SURFACE_MATERIALS = {"grass": "Grass", "cobble": "Cobble", "paving": "Paving", "bed": "CanalBed"}


class SiteMeshes:
    """좌표에 맞는 덩어리 메시를 돌려준다."""

    def __init__(self, plan):
        self.plan = plan
        self.chunks = {}
        self.water = MeshData("Water")
        self.bounds = MeshData("Bounds")

    def at(self, x, y):
        key = (max(0, min(int(x // CHUNK_M), self.plan.width // CHUNK_M)), max(0, min(int(y // CHUNK_M), self.plan.depth // CHUNK_M)))
        if key not in self.chunks:
            self.chunks[key] = MeshData(f"Site_{key[0]}_{key[1]}")
        return self.chunks[key]

    def all_meshes(self):
        return [*self.chunks.values(), self.water, self.bounds]


def _merge_rects(cells):
    """같은 종류의 칸들을 큰 사각형으로 합친다. 면 수를 줄이고 UV가 끊기지 않게 한다."""
    remaining = set(cells)
    rects = []
    while remaining:
        x, y = min(remaining, key=lambda c: (c[1], c[0]))
        width = 1
        while (x + width, y) in remaining:
            width += 1
        depth = 1
        while all((ix, y + depth) in remaining for ix in range(x, x + width)):
            depth += 1
        for ix in range(x, x + width):
            for iy in range(y, y + depth):
                remaining.discard((ix, iy))
        rects.append((x, y, width, depth))
    return rects


def build_surfaces(plan, site):
    groups = defaultdict(list)
    for (ix, iy), cell in plan.cells.items():
        if cell.water:
            key = ("bed", cell.water.bed_z)
        elif cell.link:
            continue
        else:
            key = (cell.surface, cell.z)
        groups[(ix // CHUNK_M, iy // CHUNK_M, *key)].append((ix, iy))
    for (_, _, surface, z), cells in groups.items():
        for x, y, width, depth in _merge_rects(cells):
            points = [(x, y, z), (x + width, y, z), (x + width, y + depth, z), (x, y + depth, z)]
            site.at(x, y).add_face(points, SURFACE_MATERIALS[surface], facing=(0, 0, 1))


def _wall_top(cell):
    return cell.ground_z


def _collect_edges(plan):
    """높이가 다른 이웃 칸의 경계를 모아, 같은 줄에 이어진 것끼리 묶는다."""
    runs = defaultdict(list)
    for (ix, iy), cell in plan.cells.items():
        for direction in ("E", "N"):
            dx, dy = DIRECTIONS[direction]
            other = plan.cell(ix + dx, iy + dy)
            if other is None:
                continue
            if cell.link and cell.link.kind != "bridge" and cell.link is other.link:
                continue
            if cell.link and cell.link.kind != "bridge" and cell.link.runs_along(direction):
                continue
            if other.link and other.link.kind != "bridge" and other.link.runs_along(direction):
                continue
            low, high = sorted((_wall_top(cell), _wall_top(other)))
            if high - low < 0.01:
                continue
            # 벽은 낮은 쪽을 향한다.
            faces_other = _wall_top(other) < _wall_top(cell)
            normal = (dx, dy) if faces_other else (-dx, -dy)
            water_side = other if faces_other else cell
            land_side = cell if faces_other else other
            parapet = bool(water_side.water and water_side.water.parapet and not water_side.link and not land_side.link)
            line = ix + dx if dx else iy + dy
            start = iy if dx else ix
            runs[(direction, line, normal, low, high, parapet)].append(start)
    return runs


def _contiguous(values):
    values = sorted(values)
    first = previous = values[0]
    for value in values[1:]:
        if value != previous + 1:
            yield first, previous + 1
            first = value
        previous = value
    yield first, previous + 1


def build_walls(plan, site):
    for (direction, line, normal, low, high, parapet), starts in _collect_edges(plan).items():
        for start, end in _contiguous(starts):
            _build_wall_run(site, direction, line, normal, start, end, low, high, parapet)


def _build_wall_run(site, direction, line, normal, start, end, low, high, parapet):
    """줄 하나의 벽을 만든다. 긴 벽을 덩어리 경계에서 잘라 각 덩어리에 담는다."""
    for first, last in _split_by_chunk(start, end):
        p0 = (line, first) if direction == "E" else (first, line)
        p1 = (line, last) if direction == "E" else (last, line)
        left = p0 if (p1[0] - p0[0]) * -normal[1] + (p1[1] - p0[1]) * normal[0] > 0 else p1
        mesh = site.at((p0[0] + p1[0]) / 2 - normal[0] * 0.5, (p0[1] + p1[1]) / 2 - normal[1] * 0.5)
        frame = Frame.facing((normal[0], normal[1]), (left[0], left[1], 0.0))
        span = last - first
        frame.quad(mesh, 0, low - SEAM_SINK_M, span, high, "Retaining" if high - low >= RETAINING_MIN_M else "Curb")
        # 난간과 갓돌은 높은 쪽 땅 위에 따로 서므로, 길에서 보이는 뒷면(back)도 만든다.
        if parapet:
            frame.box(mesh, 0, high, span, high + PARAPET_HEIGHT_M, -PARAPET_WIDTH_M, 0, "Stone", back=True)
            frame.box(
                mesh, 0, high + PARAPET_HEIGHT_M, span, high + PARAPET_HEIGHT_M + PARAPET_CAP_M,
                -PARAPET_WIDTH_M - PARAPET_CAP_OVERHANG_M, PARAPET_CAP_OVERHANG_M, "Coping", back=True,
            )
        elif high - low >= RETAINING_MIN_M:
            frame.box(mesh, 0, high, span, high + COPING_HEIGHT_M, -COPING_WIDTH_M, COPING_OVERHANG_M, "Coping", back=True)


def _split_by_chunk(start, end):
    first = start
    while first < end:
        last = min(end, (first // CHUNK_M + 1) * CHUNK_M)
        yield first, last
        first = last


def build_curbs(plan, site):
    """돌길이 풀밭을 만나는 곳에 경계석을 둔다. 칸마다 놓지 않고 이어서 깐다."""
    runs = defaultdict(list)
    for (ix, iy), cell in plan.cells.items():
        if cell.surface != "cobble" or cell.link or cell.water:
            continue
        for direction, (dx, dy) in DIRECTIONS.items():
            other = plan.cell(ix + dx, iy + dy)
            if other is None or other.surface != "grass" or other.link or other.building or other.z != cell.z:
                continue
            line = ix + (1 if dx > 0 else 0) if dx else iy + (1 if dy > 0 else 0)
            runs[(direction, line, cell.z)].append(iy if dx else ix)
    for (direction, line, z), starts in runs.items():
        dx, dy = DIRECTIONS[direction]
        for start, end in _contiguous(starts):
            for first, last in _split_by_chunk(start, end):
                if dx:
                    x0, x1 = sorted((line, line - dx * CURB_WIDTH_M))
                    low, high = (x0, first, z), (x1, last, z + CURB_HEIGHT_M)
                else:
                    y0, y1 = sorted((line, line - dy * CURB_WIDTH_M))
                    low, high = (first, y0, z), (last, y1, z + CURB_HEIGHT_M)
                site.at(low[0], low[1]).add_box(low, high, "Curb", skip=("bottom",))


def _link_frames(link):
    """계단과 경사로의 진행 방향을 재는 도구. t는 낮은 끝에서의 거리, s는 폭 방향 거리다."""
    x, y, width, depth = link.rect
    dx, dy = link.axis

    def point(t, s, z):
        if dx:
            return (x + t if dx > 0 else x + width - t, y + s, z)
        return (x + s, y + t if dy > 0 else y + depth - t, z)

    return point


def _build_cheeks(mesh, link, point):
    """계단 옆의 경사진 벽. 옆으로 떨어지지 않게 막고, 낮은 끝이 땅에 닿게 한다."""
    run = link.length
    low_top = link.low_z + CHEEK_HEIGHT_M
    high_top = link.high_z + CHEEK_HEIGHT_M
    across = (0, 1, 0) if link.axis[0] else (1, 0, 0)
    down = (-link.axis[0], -link.axis[1], 0)
    for s0, s1 in ((0.0, CHEEK_WIDTH_M), (link.width - CHEEK_WIDTH_M, link.width)):
        for s, sign in ((s0, -1), (s1, 1)):
            points = [point(0, s, link.low_z), point(run, s, link.low_z), point(run, s, high_top), point(0, s, low_top)]
            mesh.add_face(points, "Retaining", facing=(across[0] * sign, across[1] * sign, 0))
        mesh.add_face([point(0, s0, low_top), point(run, s0, high_top), point(run, s1, high_top), point(0, s1, low_top)], "Coping", facing=(0, 0, 1))
        mesh.add_face([point(0, s0, link.low_z), point(0, s1, link.low_z), point(0, s1, low_top), point(0, s0, low_top)], "Retaining", facing=down)


def build_stairs(plan, site):
    for link in plan.links.values():
        if link.kind != "stairs":
            continue
        x, y, width, depth = link.rect
        mesh = site.at(x + width / 2, y + depth / 2)
        point = _link_frames(link)
        rise = link.high_z - link.low_z
        steps = max(1, round(rise / STAIR_RISER_M))
        tread = link.length / steps
        down = (-link.axis[0], -link.axis[1], 0)
        s0, s1 = CHEEK_WIDTH_M, link.width - CHEEK_WIDTH_M
        previous = link.low_z
        for index in range(steps):
            z = link.low_z + rise * (index + 1) / steps
            t0, t1 = index * tread, (index + 1) * tread
            mesh.add_face([point(t0, s0, z), point(t1, s0, z), point(t1, s1, z), point(t0, s1, z)], "Stone", facing=(0, 0, 1))
            mesh.add_face([point(t0, s0, previous), point(t0, s1, previous), point(t0, s1, z), point(t0, s0, z)], "Curb", facing=down)
            previous = z
        _build_cheeks(mesh, link, point)


def build_ramps(plan, site):
    for link in plan.links.values():
        if link.kind != "ramp":
            continue
        x, y, width, depth = link.rect
        mesh = site.at(x + width / 2, y + depth / 2)
        point = _link_frames(link)
        s0, s1 = CHEEK_WIDTH_M, link.width - CHEEK_WIDTH_M
        run = link.length
        mesh.add_face(
            [point(0, s0, link.low_z), point(run, s0, link.high_z), point(run, s1, link.high_z), point(0, s1, link.low_z)],
            "Cobble", facing=(0, 0, 1),
        )
        _build_cheeks(mesh, link, point)


def build_bridges(plan, site):
    for link in plan.links.values():
        if link.kind == "bridge":
            _build_bridge(plan, site, link)


def _build_bridge(plan, site, link):
    x, y, width, depth = link.rect
    z = link.low_z
    along_x = link.axis[0] != 0
    mesh = site.at(x + width / 2, y + depth / 2)

    def point(t, s, height):
        return (x + t, y + s, height) if along_x else (x + s, y + t, height)

    run, wide = (width, depth) if along_x else (depth, width)
    bottom = z - DECK_THICKNESS_M
    mesh.add_face([point(0, 0, z), point(run, 0, z), point(run, wide, z), point(0, wide, z)], "Cobble", facing=(0, 0, 1))

    # 난간은 다리 양옆에 서고, 가운데 폭은 넓게 비워 둔다.
    for s0, s1 in ((0.0, PARAPET_WIDTH_M), (wide - PARAPET_WIDTH_M, wide)):
        low, high = point(0, s0, z), point(run, s1, z + PARAPET_HEIGHT_M)
        mesh.add_box((min(low[0], high[0]), min(low[1], high[1]), z), (max(low[0], high[0]), max(low[1], high[1]), high[2]), "Stone", skip=("bottom",))
        cap_low = point(-PARAPET_CAP_OVERHANG_M, s0 - PARAPET_CAP_OVERHANG_M, high[2])
        cap_high = point(run + PARAPET_CAP_OVERHANG_M, s1 + PARAPET_CAP_OVERHANG_M, high[2] + PARAPET_CAP_M)
        mesh.add_box(
            (min(cap_low[0], cap_high[0]), min(cap_low[1], cap_high[1]), cap_low[2]),
            (max(cap_low[0], cap_high[0]), max(cap_low[1], cap_high[1]), cap_high[2]),
            "Coping",
        )

    span = _water_span(plan, link)
    if span is None:
        return
    t0, t1, bed = span
    _build_arch(mesh, point, wide, t0, t1, bed, bottom, z, along_x)
    # 물 밖으로 나온 다리 끝은 축대 위에 얹힌다.
    for s, facing_sign in ((0.0, -1), (wide, 1)):
        facing = (0, facing_sign, 0) if along_x else (facing_sign, 0, 0)
        for a, b in ((0.0, t0), (t1, run)):
            mesh.add_face([point(a, s, bottom), point(b, s, bottom), point(b, s, z), point(a, s, z)], "Stone", facing=facing)


def _water_span(plan, link):
    x, y, width, depth = link.rect
    along_x = link.axis[0] != 0
    offsets = []
    bed = None
    for t in range(width if along_x else depth):
        cell = plan.cell(x + t, y) if along_x else plan.cell(x, y + t)
        if cell and cell.water:
            offsets.append(t)
            bed = cell.water.bed_z
    if not offsets:
        return None
    return offsets[0], offsets[-1] + 1, bed


def _build_arch(mesh, point, wide, t0, t1, bed, bottom, top, along_x):
    """물 위의 반타원 아치. 아치 안쪽 돌과 양옆 벽을 함께 만든다."""
    crown = bottom - ARCH_CROWN_GAP_M
    middle = (t0 + t1) / 2
    half = (t1 - t0) / 2
    arc = []
    for index in range(ARCH_SEGMENTS + 1):
        angle = math.pi * index / ARCH_SEGMENTS
        arc.append((middle - half * math.cos(angle), bed + (crown - bed) * math.sin(angle)))
    for (a0, h0), (a1, h1) in zip(arc, arc[1:]):
        inward = middle - (a0 + a1) / 2
        down = bed - (h0 + h1) / 2
        facing = (inward, 0, down) if along_x else (0, inward, down)
        mesh.add_face([point(a0, 0, h0), point(a1, 0, h1), point(a1, wide, h1), point(a0, wide, h0)], "Stone", facing=facing)
        for s, sign in ((0.0, -1), (wide, 1)):
            side = (0, sign, 0) if along_x else (sign, 0, 0)
            mesh.add_face([point(a0, s, h0), point(a1, s, h1), point(a1, s, top), point(a0, s, top)], "Stone", facing=side)


def build_water(plan, site):
    for water in plan.waters.values():
        x, y, width, depth = water.rect
        z = water.level_z
        site.water.add_face([(x, y, z), (x + width, y, z), (x + width, y + depth, z), (x, y + depth, z)], "Water", facing=(0, 0, 1))


def build_boundary(plan, site):
    """맵 둘레의 성벽과 넘어갈 수 없게 막는 보이지 않는 벽."""
    boundary = plan.data["boundary"]
    wall_height = boundary["wall_height"]
    t = OUTER_WALL_THICKNESS_M
    edges = [
        ("south", [(ix, 0) for ix in range(plan.width)]),
        ("north", [(ix, plan.depth - 1) for ix in range(plan.width)]),
        ("west", [(0, iy) for iy in range(plan.depth)]),
        ("east", [(plan.width - 1, iy) for iy in range(plan.depth)]),
    ]
    for side, cells in edges:
        groups = defaultdict(list)
        for key in cells:
            cell = plan.cells[key]
            groups[cell.z if not cell.water else plan.data["ground"]["z"]].append(key[0] if side in ("south", "north") else key[1])
        for z, starts in groups.items():
            for start, end in _contiguous(starts):
                for first, last in _split_by_chunk(start, end):
                    top = z + wall_height
                    base = min(z, plan.data["ground"]["z"]) - 3.0
                    if side == "south":
                        low, high = (first, -t, base), (last, 0, top)
                    elif side == "north":
                        low, high = (first, plan.depth, base), (last, plan.depth + t, top)
                    elif side == "west":
                        low, high = (-t, first, base), (0, last, top)
                    else:
                        low, high = (plan.width, first, base), (plan.width + t, last, top)
                    site.at(max(low[0], 0), max(low[1], 0)).add_box(low, high, "Retaining", top_material="Coping", skip=("bottom",))
    # 충돌만 쓰는 벽. 성벽을 뛰어넘거나 스킬로 밀려 나가지 못하게 한다.
    block = boundary["block_height"]
    w, d, b = plan.width, plan.depth, BLOCK_THICKNESS_M
    for low, high in (
        ((-t, -t - b, -5.0), (w + t, -t, block)),
        ((-t, d + t, -5.0), (w + t, d + t + b, block)),
        ((-t - b, -t, -5.0), (-t, d + t, block)),
        ((w + t, -t, -5.0), (w + t + b, d + t, block)),
    ):
        site.bounds.add_box(low, high, "Coping")


def build_site(plan):
    site = SiteMeshes(plan)
    build_surfaces(plan, site)
    build_walls(plan, site)
    build_curbs(plan, site)
    build_stairs(plan, site)
    build_ramps(plan, site)
    build_bridges(plan, site)
    build_water(plan, site)
    build_boundary(plan, site)
    return [mesh for mesh in site.all_meshes() if not mesh.is_empty]
