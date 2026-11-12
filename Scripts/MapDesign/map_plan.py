"""도면(JSON)을 칸 지도로 펼치고 플레이 규칙을 검사한다.

Blender와 언리얼 없이 돌아가므로 도면을 고칠 때마다 바로 확인할 수 있다.
좌표는 미터이고 x는 동쪽, y는 북쪽, z는 위쪽이다. 사각형은 [x, y, 폭, 깊이]로 적고, 모두 1m 칸에 맞춘다.
"""
from __future__ import annotations

import json
import math
import sys
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path

# 캐릭터가 걸어서 넘는 턱 높이. CharacterMovement의 MaxStepHeight 기본값이다.
MAX_STEP_M = 0.45
# 경사로는 UE의 걸을 수 있는 각도(44.76도)보다 충분히 낮게 둔다. 달리면서 튀어 오르지 않게 하기 위해서다.
MAX_RAMP_DEG = 30.0
# 계단 한 칸의 높이. 캐릭터 다리가 계단 위에서 어색하지 않은 값이다.
STAIR_RISER_M = 0.18
MIN_STAIR_TREAD_M = 0.28
MAX_STAIR_TREAD_M = 0.60
# 길은 캡슐 두 개가 지나칠 수 있어야 교전 중 막히지 않는다.
MIN_ROAD_WIDTH_M = 3
MIN_SPAWN_SPACING_M = 12.0
# 건물 정면이 길이나 광장에 이만큼 닿아야 입구가 살아 있다.
MIN_FRONTAGE_RATIO = 0.6
# 다리나 계단 하나가 끊겨도 이보다 넓은 구역이 고립되면 경고한다.
MIN_ISOLATED_CELLS = 30
# 계단과 다리의 출입구 앞은 이 칸 수만큼 비워 둔다. 교전 중 길목이 소품에 걸리면 안 된다.
LINK_APPROACH_CELLS = 2
# 소품이 바닥에서 막는 반지름(m). 나무는 줄기만 막고 잎은 머리 위에 있다.
PROP_BLOCK_RADIUS_M = {
    "fountain": 3.9,
    "cart": 3.0,
    "bench": 1.3,
    "crate": 0.8,
    "crate_large": 1.0,
    "barrel": 0.6,
    "lantern": 0.3,
    "bush": 1.5,
    "tree_round": 0.6,
    "tree_wide": 0.7,
    "tree_cypress": 0.6,
}
# 보상 상자(언리얼 BP_RewardChest)가 바닥에서 차지하는 반지름(m). 소품과 이만큼 더 떨어져야 서로 파묻히지 않는다.
CHEST_RADIUS_M = 0.8
# 스폰 캡슐이 상자에 끼지 않고, 나오자마자 상자를 여는 일이 없게 스폰에서 떨어뜨린다.
MIN_CHEST_SPAWN_GAP_M = 4.0

DIRECTIONS = {"E": (1, 0), "W": (-1, 0), "N": (0, 1), "S": (0, -1)}
OPPOSITE = {"E": "W", "W": "E", "N": "S", "S": "N"}
PAVED_SURFACES = {"cobble", "paving"}


@dataclass
class Water:
    id: str
    rect: tuple
    bed_z: float
    level_z: float
    parapet: bool


@dataclass
class Link:
    """높이가 다른 곳을 잇는 구조물. kind는 stairs, ramp, bridge다."""

    id: str
    kind: str
    rect: tuple
    up: str
    low_z: float
    high_z: float

    @property
    def axis(self):
        return DIRECTIONS[self.up]

    @property
    def length(self):
        return self.rect[2] if self.axis[0] else self.rect[3]

    @property
    def width(self):
        return self.rect[3] if self.axis[0] else self.rect[2]

    def progress(self, x, y):
        """낮은 쪽 끝이 0, 높은 쪽 끝이 1이다."""
        rx, ry, rw, rd = self.rect
        dx, dy = self.axis
        if dx:
            t = (x - rx) / rw
            return t if dx > 0 else 1.0 - t
        t = (y - ry) / rd
        return t if dy > 0 else 1.0 - t

    def z_at(self, x, y):
        return self.low_z + (self.high_z - self.low_z) * self.progress(x, y)

    def runs_along(self, direction):
        return DIRECTIONS[direction][0] != 0 if self.axis[0] else DIRECTIONS[direction][1] != 0


@dataclass
class Cell:
    surface: str
    z: float
    zone: str
    water: Water | None = None
    link: Link | None = None
    building: str | None = None
    prop: str | None = None

    @property
    def walkable(self):
        if self.building or self.prop:
            return False
        return self.link is not None or self.water is None

    @property
    def ground_z(self):
        """이 칸의 맨 아래 지면. 수로는 바닥, 다리 밑은 그 아래 땅이다."""
        if self.water:
            return self.water.bed_z
        if self.link and self.link.kind != "bridge":
            return self.link.low_z
        return self.z


@dataclass
class Issue:
    level: str
    where: str
    message: str

    def __str__(self):
        return f"[{self.level}] {self.where}: {self.message}"


@dataclass
class MapPlan:
    data: dict
    width: int
    depth: int
    cells: dict = field(default_factory=dict)
    links: dict = field(default_factory=dict)
    waters: dict = field(default_factory=dict)

    @property
    def name(self):
        return self.data["name"]

    @property
    def buildings(self):
        return self.data.get("buildings", [])

    @property
    def spawns(self):
        return self.data.get("spawns", [])

    @property
    def props(self):
        return self.data.get("props", [])

    @property
    def chests(self):
        return self.data.get("chests", [])

    def cell(self, ix, iy):
        return self.cells.get((ix, iy))

    def ground_height(self, x, y):
        """위치의 걷는 높이. 건물과 소품을 땅에 놓을 때 쓴다."""
        cell = self.cell(math.floor(x), math.floor(y))
        if cell is None:
            raise ValueError(f"({x}, {y}) is outside the plan")
        return cell.link.z_at(x, y) if cell.link else cell.z

    def edge_z(self, ix, iy, direction):
        """칸의 한쪽 변에서 걷는 높이."""
        cell = self.cells[(ix, iy)]
        if cell.link is None:
            return cell.z
        dx, dy = DIRECTIONS[direction]
        return cell.link.z_at(ix + 0.5 + dx * 0.5, iy + 0.5 + dy * 0.5)

    def can_cross(self, ix, iy, direction):
        """이웃 칸으로 걸어서 넘어갈 수 있는지 본다."""
        dx, dy = DIRECTIONS[direction]
        here = self.cell(ix, iy)
        there = self.cell(ix + dx, iy + dy)
        if here is None or there is None or not here.walkable or not there.walkable:
            return False
        if here.link is not there.link:
            # 계단과 다리는 양 끝으로만 드나든다. 옆면은 벽이나 난간이다.
            if here.link and not here.link.runs_along(direction):
                return False
            if there.link and not there.link.runs_along(direction):
                return False
        return abs(self.edge_z(ix, iy, direction) - self.edge_z(ix + dx, iy + dy, OPPOSITE[direction])) <= MAX_STEP_M

    def reachable_from(self, start, skip_link=None):
        """시작 칸에서 걸어서 닿는 칸들을 찾는다. skip_link는 끊겼다고 가정한다."""
        seen = {start}
        queue = deque([start])
        while queue:
            ix, iy = queue.popleft()
            for direction, (dx, dy) in DIRECTIONS.items():
                neighbor = (ix + dx, iy + dy)
                if neighbor in seen or not self.can_cross(ix, iy, direction):
                    continue
                if skip_link and self.cells[neighbor].link is skip_link:
                    continue
                seen.add(neighbor)
                queue.append(neighbor)
        return seen


def _rect_cells(rect):
    x, y, width, depth = rect
    for ix in range(x, x + width):
        for iy in range(y, y + depth):
            yield ix, iy


def load_plan(path) -> MapPlan:
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    width, depth = data["bounds"]
    plan = MapPlan(data=data, width=width, depth=depth)

    ground = data["ground"]
    for ix in range(width):
        for iy in range(depth):
            plan.cells[(ix, iy)] = Cell(ground["surface"], ground["z"], "ground")

    # 나중에 적은 것이 앞의 것을 덮는다. 땅, 길, 물, 계단, 건물 순으로 쌓는다.
    for zone in data.get("zones", []):
        for key in _rect_cells(zone["rect"]):
            if key in plan.cells:
                cell = plan.cells[key]
                cell.surface, cell.z, cell.zone = zone["surface"], zone["z"], zone["id"]

    for road in data.get("roads", []):
        for key in _rect_cells(road["rect"]):
            if key in plan.cells:
                cell = plan.cells[key]
                cell.surface = road.get("surface", "cobble")
                cell.z = road.get("z", cell.z)

    for entry in data.get("water", []):
        water = Water(entry["id"], tuple(entry["rect"]), entry["bed_z"], entry["level_z"], entry.get("parapet", False))
        plan.waters[water.id] = water
        for key in _rect_cells(water.rect):
            if key in plan.cells:
                plan.cells[key].water = water
                plan.cells[key].surface = "bed"

    for kind in ("stairs", "ramps", "bridges"):
        for entry in data.get(kind, []):
            is_bridge = kind == "bridges"
            link = Link(
                entry["id"],
                {"stairs": "stairs", "ramps": "ramp", "bridges": "bridge"}[kind],
                tuple(entry["rect"]),
                entry.get("axis", "E") if is_bridge else entry["up"],
                entry["z"] if is_bridge else entry["low_z"],
                entry["z"] if is_bridge else entry["high_z"],
            )
            plan.links[link.id] = link
            for key in _rect_cells(link.rect):
                if key in plan.cells:
                    plan.cells[key].link = link

    for building in data.get("buildings", []):
        for key in _rect_cells(building["rect"]):
            if key in plan.cells:
                plan.cells[key].building = building["id"]

    for prop in data.get("props", []):
        _block_cells_under_prop(plan, prop)
    return plan


def _block_cells_under_prop(plan, prop):
    """소품이 놓인 칸을 막는다. 칸의 가운데가 소품 반지름 안에 들면 지나갈 수 없다고 본다."""
    x, y = prop["at"]
    radius = PROP_BLOCK_RADIUS_M[prop["kit"]]
    reach = math.ceil(radius)
    for ix in range(math.floor(x) - reach, math.floor(x) + reach + 1):
        for iy in range(math.floor(y) - reach, math.floor(y) + reach + 1):
            cell = plan.cell(ix, iy)
            if cell and math.dist((ix + 0.5, iy + 0.5), (x, y)) <= radius:
                cell.prop = prop["kit"]


def _check_inside(plan, label, rect, issues):
    x, y, width, depth = rect
    if x < 0 or y < 0 or x + width > plan.width or y + depth > plan.depth:
        issues.append(Issue("error", label, f"rect {list(rect)} leaves the {plan.width}x{plan.depth} bounds"))


def _front_cells(rect, front):
    """건물 정면 바로 바깥의 칸들."""
    x, y, width, depth = rect
    if front == "N":
        return [(ix, y + depth) for ix in range(x, x + width)]
    if front == "S":
        return [(ix, y - 1) for ix in range(x, x + width)]
    if front == "E":
        return [(x + width, iy) for iy in range(y, y + depth)]
    return [(x - 1, iy) for iy in range(y, y + depth)]


def _check_roads(plan, issues):
    for road in plan.data.get("roads", []):
        _check_inside(plan, f"road {road['id']}", road["rect"], issues)
        if min(road["rect"][2:]) < MIN_ROAD_WIDTH_M:
            issues.append(Issue("error", f"road {road['id']}", f"narrower than {MIN_ROAD_WIDTH_M} m"))


def _check_links(plan, issues):
    for link in plan.links.values():
        label = f"{link.kind} {link.id}"
        _check_inside(plan, label, link.rect, issues)
        rise = link.high_z - link.low_z
        if link.kind == "stairs":
            steps = max(1, round(rise / STAIR_RISER_M))
            tread = link.length / steps
            if not MIN_STAIR_TREAD_M <= tread <= MAX_STAIR_TREAD_M:
                issues.append(Issue("error", label, f"tread {tread:.2f} m; use a length near {steps * 0.36:.0f} m for {rise} m of rise"))
        elif link.kind == "ramp":
            angle = math.degrees(math.atan2(rise, link.length))
            if angle > MAX_RAMP_DEG:
                issues.append(Issue("error", label, f"slope {angle:.1f} deg is above {MAX_RAMP_DEG}"))
        # 양 끝이 실제로 이웃 땅과 만나는지 본다. 어긋나면 계단이 공중에서 끝난다.
        x, y, width, depth = link.rect
        for ix, iy in _rect_cells(link.rect):
            for direction in DIRECTIONS:
                if not link.runs_along(direction):
                    continue
                dx, dy = DIRECTIONS[direction]
                neighbor = plan.cell(ix + dx, iy + dy)
                if neighbor is None or neighbor.link is link:
                    continue
                if neighbor.walkable and not plan.can_cross(ix, iy, direction):
                    issues.append(Issue("error", label, f"end at cell ({ix}, {iy}) does not meet the ground {direction}"))
                    break


def _link_approach_cells(plan):
    """계단, 경사로, 다리 양 끝 바로 앞(LINK_APPROACH_CELLS 칸)의 칸과 그 출입구의 링크. 수로로 내려가는 계단은 뺀다."""
    approaches = {}
    for link in plan.links.values():
        if link.low_z < plan.data["ground"]["z"]:
            continue
        for ix, iy in _rect_cells(link.rect):
            for direction, (dx, dy) in DIRECTIONS.items():
                if not link.runs_along(direction):
                    continue
                for distance in range(1, LINK_APPROACH_CELLS + 1):
                    key = (ix + dx * distance, iy + dy * distance)
                    cell = plan.cell(*key)
                    if cell and cell.link is not link:
                        approaches.setdefault(key, link)
    return approaches


def _check_link_approaches(plan, issues):
    """계단, 경사로, 다리 양 끝의 바로 앞에 소품이 없는지 본다."""
    for key, link in _link_approach_cells(plan).items():
        cell = plan.cell(*key)
        if cell.prop:
            issues.append(Issue("error", f"{link.kind} {link.id}", f"{cell.prop} blocks the way in at {key}"))


def _check_buildings(plan, issues):
    for building in plan.buildings:
        label = f"building {building['id']}"
        rect = building["rect"]
        _check_inside(plan, label, rect, issues)
        cells = [plan.cell(ix, iy) for ix, iy in _rect_cells(rect) if plan.cell(ix, iy)]
        if len({cell.z for cell in cells}) > 1:
            issues.append(Issue("error", label, "footprint is not flat"))
        if any(cell.water or cell.link or cell.surface in PAVED_SURFACES for cell in cells):
            issues.append(Issue("error", label, "footprint overlaps a road, water or link"))
        front = _front_cells(rect, building["front"])
        paved = [plan.cell(*key) for key in front if plan.cell(*key)]
        touching = sum(1 for cell in paved if cell.surface in PAVED_SURFACES)
        if touching < len(front) * MIN_FRONTAGE_RATIO:
            issues.append(Issue("error", label, f"front {building['front']} touches paving on only {touching}/{len(front)} cells"))


def _check_spawns_and_routes(plan, issues):
    spawns = plan.spawns
    if not spawns:
        issues.append(Issue("error", "spawns", "the plan has no spawn points"))
        return
    start = None
    for spawn in spawns:
        ix, iy = math.floor(spawn["at"][0]), math.floor(spawn["at"][1])
        cell = plan.cell(ix, iy)
        if cell is None or not cell.walkable:
            issues.append(Issue("error", f"spawn {spawn['tag']}", "is not on walkable ground"))
        elif start is None:
            start = (ix, iy)
    for index, first in enumerate(spawns):
        for second in spawns[index + 1:]:
            distance = math.dist(first["at"], second["at"])
            if distance < MIN_SPAWN_SPACING_M:
                issues.append(Issue("warning", "spawns", f"{first['tag']} and {second['tag']} are only {distance:.1f} m apart"))
    if start is None:
        return

    reached = plan.reachable_from(start)
    for spawn in spawns:
        key = (math.floor(spawn["at"][0]), math.floor(spawn["at"][1]))
        if key not in reached:
            issues.append(Issue("error", f"spawn {spawn['tag']}", "cannot be reached on foot from the first spawn"))
    walkable = {key for key, cell in plan.cells.items() if cell.walkable}
    stranded = walkable - reached
    if stranded:
        sample = sorted(stranded)[0]
        issues.append(Issue("error", "routes", f"{len(stranded)} walkable cells cannot be reached, for example {sample}"))

    # 교전 맵은 길목 하나만 막아도 맵이 둘로 갈라지면 안 된다.
    for link in plan.links.values():
        if link.kind == "stairs" and link.low_z < plan.data["ground"]["z"]:
            continue
        without = plan.reachable_from(start, skip_link=link)
        link_cells = sum(1 for cell in plan.cells.values() if cell.link is link)
        lost = len(reached) - len(without) - link_cells
        if lost >= MIN_ISOLATED_CELLS:
            issues.append(Issue("warning", f"{link.kind} {link.id}", f"is the only way to {lost} cells; add a second route"))


def _check_water_exits(plan, issues):
    """수로에 빠진 캐릭터가 나올 계단이 구간마다 있는지 본다. 다리 밑은 이어진 물이다."""
    remaining = {key for key, cell in plan.cells.items() if cell.water}
    while remaining:
        group = set()
        queue = deque([remaining.pop()])
        while queue:
            ix, iy = queue.popleft()
            group.add((ix, iy))
            for dx, dy in DIRECTIONS.values():
                neighbor = (ix + dx, iy + dy)
                if neighbor in remaining:
                    remaining.discard(neighbor)
                    queue.append(neighbor)
        if not any(plan.cells[key].link and plan.cells[key].link.kind == "stairs" for key in group):
            issues.append(Issue("error", "water", f"section near {sorted(group)[0]} has no exit stairs"))


def _check_chests(plan, issues):
    """보상 상자가 걸어서 닿는 평지에 있고, 소품·스폰·계단과 다리의 출입구를 막지 않는지 본다."""
    spawn_keys = [(math.floor(spawn["at"][0]), math.floor(spawn["at"][1])) for spawn in plan.spawns]
    start = next((key for key in spawn_keys if plan.cell(*key) and plan.cell(*key).walkable), None)
    reached = plan.reachable_from(start) if start else set()
    approaches = _link_approach_cells(plan)
    for index, chest in enumerate(plan.chests):
        label = f"chest {index} at {chest['at']}"
        key = (math.floor(chest["at"][0]), math.floor(chest["at"][1]))
        cell = plan.cell(*key)
        if cell is None or not cell.walkable or cell.link:
            issues.append(Issue("error", label, "is not on open walkable ground"))
            continue
        if key not in reached:
            issues.append(Issue("error", label, "cannot be reached on foot from the first spawn"))
        if key in approaches:
            link = approaches[key]
            issues.append(Issue("error", label, f"blocks the way in to {link.kind} {link.id}"))
        for prop in plan.props:
            gap = math.dist(chest["at"], prop["at"]) - PROP_BLOCK_RADIUS_M[prop["kit"]]
            if gap < CHEST_RADIUS_M:
                issues.append(Issue("error", label, f"overlaps the {prop['kit']} at {prop['at']}"))
        for spawn in plan.spawns:
            gap = math.dist(chest["at"], spawn["at"])
            if gap < MIN_CHEST_SPAWN_GAP_M:
                issues.append(Issue("error", label, f"is only {gap:.1f} m from {spawn['tag']}"))


def validate(plan) -> list:
    issues = []
    _check_roads(plan, issues)
    _check_links(plan, issues)
    _check_link_approaches(plan, issues)
    _check_buildings(plan, issues)
    _check_spawns_and_routes(plan, issues)
    _check_water_exits(plan, issues)
    _check_chests(plan, issues)
    return issues


def main():
    plan = load_plan(sys.argv[1])
    issues = validate(plan)
    for issue in issues:
        print(issue)
    errors = sum(1 for issue in issues if issue.level == "error")
    print(f"{plan.name}: {plan.width}x{plan.depth} m, {len(plan.buildings)} buildings, {errors} errors, {len(issues) - errors} warnings")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
