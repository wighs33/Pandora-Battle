"""도면을 평면도 PNG로 그린다. Blender를 켜지 않고 몇 초 안에 배치를 확인하기 위한 도구다.

실행: python Scripts/MapDesign/preview_plan.py <도면.json> <결과.png>
"""
import math
import sys

from PIL import Image, ImageDraw

from map_plan import DIRECTIONS, load_plan, validate

PIXELS_PER_M = 10
LABEL_GRID_M = 10
SIDE_PANEL_PX = 420
LINE_HEIGHT_PX = 14

SURFACE_COLORS = {
    "grass": (120, 163, 104),
    "cobble": (150, 145, 136),
    "paving": (184, 176, 160),
    "bed": (72, 110, 138),
}
LINK_COLORS = {"stairs": (214, 170, 92), "ramp": (222, 196, 120), "bridge": (160, 112, 74)}
BUILDING_COLOR = (92, 72, 62)
FRONT_COLOR = (250, 210, 120)
SPAWN_COLOR = (230, 60, 60)
RESPAWN_COLOR = (240, 140, 60)
TEXT_COLOR = (235, 235, 235)
PANEL_COLOR = (28, 30, 34)
ISSUE_COLORS = {"error": (255, 120, 110), "warning": (255, 205, 110)}
# 높이 1m마다 이만큼 밝게 칠해 언덕을 색으로 읽게 한다.
HEIGHT_SHADE_PER_M = 9


def to_px(plan, x, y):
    return x * PIXELS_PER_M, (plan.depth - y) * PIXELS_PER_M


def shaded(color, z):
    amount = int(z * HEIGHT_SHADE_PER_M)
    return tuple(max(0, min(255, channel + amount)) for channel in color)


def draw_cells(plan, draw):
    for (ix, iy), cell in plan.cells.items():
        left, bottom = to_px(plan, ix, iy)
        right, top = to_px(plan, ix + 1, iy + 1)
        color = LINK_COLORS[cell.link.kind] if cell.link else SURFACE_COLORS[cell.surface]
        draw.rectangle([left, top, right - 1, bottom - 1], fill=shaded(color, cell.z))


def draw_links(plan, draw):
    for link in plan.links.values():
        x, y, width, depth = link.rect
        cx, cy = x + width / 2, y + depth / 2
        dx, dy = link.axis
        reach = link.length * 0.35
        start = to_px(plan, cx - dx * reach, cy - dy * reach)
        end = to_px(plan, cx + dx * reach, cy + dy * reach)
        draw.line([start, end], fill=(40, 30, 20), width=3)
        draw.ellipse([end[0] - 5, end[1] - 5, end[0] + 5, end[1] + 5], fill=(40, 30, 20))


def draw_buildings(plan, draw):
    for building in plan.buildings:
        x, y, width, depth = building["rect"]
        left, bottom = to_px(plan, x, y)
        right, top = to_px(plan, x + width, y + depth)
        draw.rectangle([left, top, right, bottom], fill=BUILDING_COLOR, outline=(30, 24, 20))
        dx, dy = DIRECTIONS[building["front"]]
        cx, cy = x + width / 2 + dx * width / 2, y + depth / 2 + dy * depth / 2
        half_x, half_y = (0, depth / 2) if dx else (width / 2, 0)
        draw.line([to_px(plan, cx - half_x, cy - half_y), to_px(plan, cx + half_x, cy + half_y)], fill=FRONT_COLOR, width=4)
        draw.text((left + 6, top + 6), building["id"], fill=TEXT_COLOR)


def draw_props(plan, draw):
    for prop in plan.props:
        px, py = to_px(plan, *prop["at"])
        radius = 16 if prop.get("kit") == "fountain" else 3
        color = (120, 190, 230) if prop.get("kit") == "fountain" else (40, 44, 40)
        draw.ellipse([px - radius, py - radius, px + radius, py + radius], fill=color)


def draw_spawns(plan, draw):
    for spawn in plan.spawns:
        px, py = to_px(plan, *spawn["at"])
        color = RESPAWN_COLOR if spawn["tag"] == "Respawn" else SPAWN_COLOR
        draw.ellipse([px - 7, py - 7, px + 7, py + 7], fill=color, outline=(255, 255, 255))
        yaw = math.radians(spawn["yaw"])
        draw.line([(px, py), (px + math.cos(yaw) * 16, py - math.sin(yaw) * 16)], fill=color, width=3)


def draw_grid(plan, draw):
    for x in range(0, plan.width + 1, LABEL_GRID_M):
        px, _ = to_px(plan, x, 0)
        draw.line([(px, 0), (px, plan.depth * PIXELS_PER_M)], fill=(0, 0, 0, 60))
        draw.text((px + 2, 2), str(x), fill=TEXT_COLOR)
    for y in range(0, plan.depth + 1, LABEL_GRID_M):
        _, py = to_px(plan, 0, y)
        draw.line([(0, py), (plan.width * PIXELS_PER_M, py)], fill=(0, 0, 0, 60))
        draw.text((2, py + 2), str(y), fill=TEXT_COLOR)


def draw_issues(plan, draw, issues):
    left = plan.width * PIXELS_PER_M + 12
    y = 12
    draw.text((left, y), f"{plan.name}  {plan.width} x {plan.depth} m", fill=TEXT_COLOR)
    y += LINE_HEIGHT_PX * 2
    if not issues:
        draw.text((left, y), "All rules pass", fill=(150, 230, 150))
    for issue in issues:
        text = str(issue)
        while text:
            draw.text((left, y), text[:62], fill=ISSUE_COLORS[issue.level])
            text = text[62:]
            y += LINE_HEIGHT_PX


def main():
    plan = load_plan(sys.argv[1])
    issues = validate(plan)
    image = Image.new("RGB", (plan.width * PIXELS_PER_M + SIDE_PANEL_PX, plan.depth * PIXELS_PER_M), PANEL_COLOR)
    draw = ImageDraw.Draw(image, "RGBA")
    draw_cells(plan, draw)
    draw_grid(plan, draw)
    draw_links(plan, draw)
    draw_buildings(plan, draw)
    draw_props(plan, draw)
    draw_spawns(plan, draw)
    draw_issues(plan, draw, issues)
    image.save(sys.argv[2])
    errors = sum(1 for issue in issues if issue.level == "error")
    print(f"{sys.argv[2]}: {errors} errors, {len(issues) - errors} warnings")
    for issue in issues:
        print(issue)


if __name__ == "__main__":
    main()
