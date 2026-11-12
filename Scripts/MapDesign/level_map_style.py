"""레벨 지도 칠하기: 지도 찍기 도구(LabProject.Tools.MapCapture)가 남긴 바닥색·법선·깊이를 모든 레벨이 같은 '판도라 지도' 필터로 칠한다.

실행: python Scripts/MapDesign/level_map_style.py <맵 키>...   (numpy, Pillow)
입력: Saved/MapStyle/raw/<맵 키>/  (page<n>_*.f32, area_*.f32, manifest.json, thumbnail.png)
출력: Saved/MapStyle/styled/<맵 키>/ page<n>.png, area.png, highlight<n>.png(지역 빛), thumbnail.png → level_map.py apply가 들여온다.

필터(Docs/UI_Design_Plan.md 14장 지도):
1. 바닥색: 잔무늬를 지우고(가운데값 거르기), 채도 0.72배, 따뜻하게 당기고, 밝기를 7단으로 나눈다(그린 지도처럼).
2. 빛: 화면 왼쪽 위에서 오는 빛으로 법선 음영 세 단(모든 지도 같은 방향).
3. 높이: 낮은 곳일수록 조금 어둡고 차게.
4. 선: 깊이가 끊기는 곳(벽, 절벽, 지붕 끝)은 먹선, 땅 바깥 윤곽은 금 상감선.
5. 바깥: 판도라 상자 먹칠(위 #1A1620 → 아래 #0B0A0E)과 3% 결, 땅이 드리운 그림자.
지역 빛: 전체 지도에서 지역 트리거 상자 안에 드는 땅을 밝은 금빛으로 덮고 그 둘레에 금 테와 번짐.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RAW = os.path.join(ROOT, "Saved", "MapStyle", "raw")
STYLED = os.path.join(ROOT, "Saved", "MapStyle", "styled")
FAR = 200000.0                                    # 이보다 먼 깊이는 빈 하늘(지도 바깥)
INK_TOP, INK_BOT = np.array([0x1A, 0x16, 0x20]) / 255, np.array([0x0B, 0x0A, 0x0E]) / 255
LINE_INK = np.array([0x1A, 0x12, 0x08]) / 255
GOLD_HI, GOLD, GOLD_LO = np.array([0xF2, 0xD9, 0x9A]) / 255, np.array([0xD6, 0xB0, 0x64]) / 255, np.array([0x8A, 0x6A, 0x2E]) / 255
LIGHT = np.array([-0.45, -0.55, 0.70]) / np.linalg.norm([-0.45, -0.55, 0.70])  # 화면 기준(오른쪽, 아래, 보는 쪽)
LINE_PER_PIXEL = 6.0                              # 먹선 문턱: 이웃 픽셀과의 높이 차가 픽셀 넓이의 이 배수를 넘으면 선
LINE_MIN = 50.0                                   # 먹선 문턱 아래 한계(cm)
THUMBNAIL_SIZE = 512


# 읽기 ---------------------------------------------------------------------------------------------------------------------
def load_f32(path):
    raw = open(path, "rb").read()
    head, _, body = raw.partition(b"\n")
    w, h, c = (int(v) for v in head.split())
    return np.frombuffer(body, dtype=np.float32).reshape(h, w, c).copy()


def to_srgb(linear):
    linear = np.clip(linear, 0, 1)
    return np.where(linear <= 0.0031308, linear * 12.92, 1.055 * np.power(linear, 1 / 2.4) - 0.055)


def camera_axes(yaw_deg, pitch_deg):
    """언리얼 카메라의 앞·오른쪽·위(월드)."""
    y, p = math.radians(yaw_deg), math.radians(pitch_deg)
    forward = np.array([math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), math.sin(p)])
    right = np.array([-math.sin(y), math.cos(y), 0.0])
    up = np.array([-math.sin(p) * math.cos(y), -math.sin(p) * math.sin(y), math.cos(p)])
    return forward, right, up


# 그림 도구 ------------------------------------------------------------------------------------------------------------------
def _box(array, radius, axis):
    if radius < 1:
        return array
    pad = [(0, 0)] * array.ndim
    pad[axis] = (radius + 1, radius)
    padded = np.pad(array, pad, mode="edge")
    total = np.cumsum(padded, axis=axis)
    upper = np.take(total, range(2 * radius + 1, padded.shape[axis]), axis=axis)
    lower = np.take(total, range(0, padded.shape[axis] - 2 * radius - 1), axis=axis)
    return (upper - lower) / (2 * radius + 1)


def blur(array, radius):
    """상자 평균 세 번(가우스 근사). radius는 대략 표준편차(px)."""
    out = np.asarray(array, dtype=np.float32)
    box = max(int(round(radius * 0.9)), 1 if radius > 0 else 0)
    for _ in range(3):
        out = _box(_box(out, box, 0), box, 1)
    return out


def median_rgb(rgb, size):
    image = Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8))
    return np.asarray(image.filter(ImageFilter.MedianFilter(size)), dtype=np.float32) / 255


def grow(mask, pixels):
    image = Image.fromarray((mask * 255).astype(np.uint8))
    return np.asarray(image.filter(ImageFilter.MaxFilter(pixels * 2 + 1)), dtype=np.float32) / 255


def shrink(mask, pixels):
    image = Image.fromarray((mask * 255).astype(np.uint8))
    return np.asarray(image.filter(ImageFilter.MinFilter(pixels * 2 + 1)), dtype=np.float32) / 255


def rgb_to_hsv(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mx, mn = rgb.max(-1), rgb.min(-1)
    diff = mx - mn + 1e-6
    h = np.where(mx == r, (g - b) / diff % 6, np.where(mx == g, (b - r) / diff + 2, (r - g) / diff + 4)) / 6
    s = np.where(mx > 0, (mx - mn) / (mx + 1e-6), 0)
    return np.stack([h % 1.0, s, mx], -1)


def hsv_to_rgb(hsv):
    h, s, v = hsv[..., 0] * 6, hsv[..., 1], hsv[..., 2]
    i = np.floor(h).astype(int) % 6
    f = h - np.floor(h)
    p, q, t = v * (1 - s), v * (1 - s * f), v * (1 - s * (1 - f))
    out = np.zeros(h.shape + (3,), dtype=np.float32)
    for k, (a, b, c) in enumerate([(v, t, p), (q, v, p), (p, v, t), (p, q, v), (t, p, v), (v, p, q)]):
        mask = i == k
        out[mask] = np.stack([a[mask], b[mask], c[mask]], -1)
    return out


def lacquer(h, w, seed=7):
    t = np.linspace(0, 1, h)[:, None, None]
    ground = INK_TOP * (1 - t) + INK_BOT * t
    grain = blur(np.random.default_rng(seed).normal(0, 1, (h, w)).astype(np.float32), 0.8)[..., None] * 0.03
    return np.clip(np.broadcast_to(ground, (h, w, 3)) * (1 + grain), 0, 1)


def save_rgb(path, rgb):
    Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).save(path)


# 지도 칠하기 -----------------------------------------------------------------------------------------------------------------
def paint_albedo(base):
    hsv = rgb_to_hsv(median_rgb(median_rgb(base, 5), 3))
    hsv[..., 1] = np.clip(hsv[..., 1] * 0.72, 0, 0.6)
    levels = 7
    hsv[..., 2] = np.round(np.clip(hsv[..., 2], 0, 1) * levels) / levels * 0.86 + 0.07
    return np.clip(hsv_to_rgb(hsv) * np.array([1.05, 1.0, 0.90]), 0, 1)


def shading(normal, yaw, pitch):
    forward, right, up = camera_axes(yaw, pitch)
    n = np.stack([normal @ right, normal @ -up, normal @ -forward], -1)
    lambert = np.clip(n @ LIGHT, 0, 1)
    steps = np.select([lambert > 0.82, lambert > 0.55, lambert > 0.25], [1.0, 0.88, 0.74], 0.62)
    return blur(steps.astype(np.float32), 0.8)


def ink_lines(height, mask, threshold):
    gy, gx = np.gradient(height)
    line = (np.hypot(gx, gy) > threshold) & mask
    return blur(grow(line.astype(np.float32), 1), 0.6)


def paint(prefix, camera):
    """camera: manifest의 yaw·pitch·world_width. 칠한 그림(RGB)과 땅 마스크를 돌려준다."""
    base = to_srgb(load_f32(prefix + "_base.f32")[..., :3])
    normal = load_f32(prefix + "_normal.f32")[..., :3]
    depth = load_f32(prefix + "_depth.f32")[..., 0]
    h, w = depth.shape
    mask = depth < FAR
    height = np.where(mask, -depth, -depth[mask].max() if mask.any() else 0.0)
    low, high = np.percentile(height[mask], 3), np.percentile(height[mask], 97)
    rel = np.clip((height - low) / max(high - low, 1.0), 0, 1)

    land = paint_albedo(base) * shading(normal, camera["yaw"], camera["pitch"])[..., None]
    land *= (0.80 + 0.20 * rel)[..., None] * np.array([0.97, 1.0, 1.05]) ** (1 - rel[..., None])
    threshold = max(LINE_MIN, LINE_PER_PIXEL * camera["world_width"] / w)
    ink = ink_lines(height, mask, threshold)[..., None] * 0.78
    land = np.clip(land * (1 - ink) + LINE_INK * ink, 0, 1)

    shadow = np.roll(np.roll(blur(mask.astype(np.float32), 10), 10, 0), 8, 1)[..., None]
    background = lacquer(h, w) * (1 - 0.55 * shadow)
    soft = blur(mask.astype(np.float32), 0.7)[..., None]
    image = background * (1 - soft) + land * soft

    rim = blur(np.clip(grow(mask.astype(np.float32), 2) - shrink(mask.astype(np.float32), 1), 0, 1), 0.7)[..., None]
    image = image * (1 - rim) + (GOLD_LO * 0.4 + GOLD * 0.6) * rim
    rim_hi = np.roll(rim, -1, 0) * (1 - rim) * 0.6
    return image * (1 - rim_hi) + GOLD_HI * rim_hi, mask


# 지역 빛 -------------------------------------------------------------------------------------------------------------------
def world_points(depth, camera, distance):
    """정사영 깊이에서 픽셀마다 월드 위치를 되살린다."""
    h, w = depth.shape
    forward, right, up = camera_axes(camera["yaw"], camera["pitch"])
    width = camera["world_width"]
    u = (np.arange(w) + 0.5) / w - 0.5
    v = (np.arange(h) + 0.5) / h - 0.5
    plane = (np.array(camera["look_at"])[None, None, :] + u[None, :, None] * width * right[None, None, :]
             - v[:, None, None] * (width * h / w) * up[None, None, :])
    return plane + (depth[..., None] - distance) * forward[None, None, :]


def inside_trigger(points, trigger):
    pitch, yaw, roll = (math.radians(a) for a in trigger["rotation"])
    # 언리얼 회전(피치·요·롤)으로 상자 축을 만든다.
    cp, sp, cy, sy, cr, sr = math.cos(pitch), math.sin(pitch), math.cos(yaw), math.sin(yaw), math.cos(roll), math.sin(roll)
    x_axis = np.array([cp * cy, cp * sy, sp])
    y_axis = np.array([sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp])
    z_axis = np.array([-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp])
    offset = points - np.array(trigger["location"])
    ext = trigger["extent"]
    return (np.abs(offset @ x_axis) <= ext[0]) & (np.abs(offset @ y_axis) <= ext[1]) & (np.abs(offset @ z_axis) <= ext[2])


def region_highlight(region_mask):
    """지역을 덮는 밝은 금빛(18%) + 둘레 금 테(3px)와 번짐. RGBA."""
    fill = blur(region_mask.astype(np.float32), 1.2)
    edge = np.clip(grow(region_mask.astype(np.float32), 2) - shrink(region_mask.astype(np.float32), 1), 0, 1)
    glow = blur(edge, 6.0)
    alpha = np.clip(0.18 * fill + 0.55 * glow / max(glow.max(), 1e-3) + 0.9 * blur(edge, 0.7), 0, 1)
    color = np.where((blur(edge, 0.7) > 0.3)[..., None], GOLD_HI, GOLD_HI * 0.9 + GOLD * 0.1)
    return np.dstack([color, alpha])


# 썸네일 --------------------------------------------------------------------------------------------------------------------
def grade_thumbnail(path):
    """가운데 정사각형, 밝기 1~99%를 늘이고, 회색 기준 절반 맞춤 + 따뜻하게, 채도 0.92, 가장자리 어둡게."""
    image = Image.open(path).convert("RGB")
    w, h = image.size
    side = min(w, h)
    image = image.crop(((w - side) // 2, (h - side) // 2, (w + side) // 2, (h + side) // 2))
    rgb = np.asarray(image.resize((THUMBNAIL_SIZE, THUMBNAIL_SIZE), Image.LANCZOS), dtype=np.float32) / 255
    lum = rgb @ np.array([0.2126, 0.7152, 0.0722])
    lo, hi = np.percentile(lum, 1), np.percentile(lum, 99)
    rgb = np.clip((rgb - lo) / max(hi - lo, 1e-3) * 0.93 + 0.03, 0, 1)
    mean = rgb.reshape(-1, 3).mean(0)
    rgb = np.clip(rgb * (1 + 0.5 * (mean.mean() / mean - 1)) * np.array([1.04, 1.0, 0.92]), 0, 1)
    gray = (rgb @ np.array([0.2126, 0.7152, 0.0722]))[..., None]
    rgb = gray + (rgb - gray) * 0.92
    yy, xx = np.mgrid[0:THUMBNAIL_SIZE, 0:THUMBNAIL_SIZE] / (THUMBNAIL_SIZE - 1) - 0.5
    return rgb * (1 - 0.35 * np.clip(np.hypot(xx, yy) * 1.6 - 0.35, 0, 1))[..., None]


def style_level(map_key):
    source, out = os.path.join(RAW, map_key), os.path.join(STYLED, map_key)
    os.makedirs(out, exist_ok=True)
    manifest = json.load(open(os.path.join(source, "manifest.json"), encoding="utf-8"))
    for index, page in enumerate(manifest["pages"]):
        image, _ = paint(os.path.join(source, f"page{index}"), page)
        save_rgb(os.path.join(out, f"page{index}.png"), image)
    area = manifest.get("area")
    if area:
        image, land = paint(os.path.join(source, "area"), area)
        save_rgb(os.path.join(out, "area.png"), image)
        points = world_points(load_f32(os.path.join(source, "area_depth.f32"))[..., 0], area, manifest["camera_distance"])
        for index, page in enumerate(manifest["pages"]):
            region = np.zeros(land.shape, dtype=bool)
            for trigger in manifest["triggers"]:
                if trigger["region"] == page["region"]:
                    region |= inside_trigger(points, trigger)
            # 트리거 상자 경계가 땅을 곧게 자른 모서리를 둥글게: 닫고(작은 틈 메우기) 연다(가는 조각 지우기).
            region = (shrink(grow(region.astype(np.float32), 6), 6) > 0.5) & land
            region = grow(shrink(region.astype(np.float32), 3), 3) > 0.5
            Image.fromarray((np.clip(region_highlight(region), 0, 1) * 255).astype(np.uint8), "RGBA").save(os.path.join(out, f"highlight{index}.png"))
            print(map_key, f"highlight{index}", "pixels", int(region.sum()))
    save_rgb(os.path.join(out, "thumbnail.png"), grade_thumbnail(os.path.join(source, "thumbnail.png")))
    print(map_key, "styled", len(manifest["pages"]), "page(s)", "+ area" if area else "")


if __name__ == "__main__":
    for key in sys.argv[1:]:
        style_level(key)
