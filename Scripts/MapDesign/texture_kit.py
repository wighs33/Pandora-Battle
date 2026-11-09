"""맵 재질 텍스처를 코드로 그린다. 외부 에셋 없이 이어 붙여도 경계가 보이지 않는 그림을 만든다.

모든 무늬는 주기적인 잡음과 감싸는 거리로 만들어 네 변이 맞물린다. 각 재질은 색(_B)과 노멀(_N) 두 장이다.
numpy만 쓰므로 Blender 안에서도 그대로 돈다.
"""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

import numpy as np

SIZE = 1024
# 스타일화된 그림처럼 보이도록 높낮이에서 얻은 음영을 색에 조금 칠한다.
PAINTED_SHADE = 0.32
LIGHT_DIRECTION = np.array([-0.45, 0.55, 0.7])


def write_png(path, rgb):
    """PIL 없는 Blender 파이썬에서도 저장되도록 PNG를 직접 쓴다."""
    data = (np.clip(rgb, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
    height, width, _ = data.shape
    raw = b"".join(b"\x00" + data[y].tobytes() for y in range(height))

    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def spectral_noise(seed, beta=2.0, stretch=(1.0, 1.0), lowest_cycles=0):
    """주기적인 잡음. 주파수가 높을수록 약하게 걸러 구름 같은 얼룩을 만든다. stretch로 결을 낸다.

    lowest_cycles를 주면 한 장에 그보다 적게 반복하는 큰 얼룩을 지워, 결이 고른 굵기로 고루 퍼진다.
    """
    rng = np.random.default_rng(seed)
    white = rng.normal(size=(SIZE, SIZE))
    cycles_x = np.fft.fftfreq(SIZE)[None, :] * SIZE
    cycles_y = np.fft.fftfreq(SIZE)[:, None] * SIZE
    fx = cycles_x / SIZE * stretch[0]
    fy = cycles_y / SIZE * stretch[1]
    radius = np.sqrt(fx * fx + fy * fy)
    radius[0, 0] = 1.0
    spectrum = np.fft.fft2(white) / radius ** (beta / 2.0)
    spectrum[0, 0] = 0.0
    spectrum[np.sqrt(cycles_x * cycles_x + cycles_y * cycles_y) < lowest_cycles] = 0.0
    field = np.real(np.fft.ifft2(spectrum))
    field -= field.min()
    return field / field.max()


def lerp(a, b, t):
    return a + (b - a) * t


def smoothstep(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def color(*rgb):
    return np.array(rgb, dtype=np.float64)


def height_to_normal(height, strength):
    """높이에서 노멀을 만든다. np.roll로 재서 변끼리 이어진다."""
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * strength
    normal = np.stack((-dx, dy, np.ones_like(height)), axis=-1)
    return normal / np.linalg.norm(normal, axis=-1, keepdims=True)


def painted(albedo, height, strength):
    """높낮이로 생기는 그늘을 그림에 칠해, 조명이 약한 곳에서도 돌 모양이 읽히게 한다."""
    normal = height_to_normal(height, strength)
    light = LIGHT_DIRECTION / np.linalg.norm(LIGHT_DIRECTION)
    shade = np.clip(np.sum(normal * light, axis=-1), 0.0, 1.0) / light[2]
    return albedo * lerp(1.0, shade, PAINTED_SHADE)[..., None]


def stone_cells(rows, per_row, jitter, seed, aspect=1.0, blocky=0.0):
    """줄을 어긋나게 깔린 돌의 배치. 감싸는 거리로 재서 변끼리 돌이 이어진다.

    첫째, 둘째로 가까운 씨앗까지의 거리 차가 돌의 가장자리다. blocky를 높이면 돌이 네모나진다.
    """
    rng = np.random.default_rng(seed)
    seeds = []
    for row in range(rows):
        for index in range(per_row):
            x = (index + (0.5 if row % 2 else 0.0) + rng.uniform(-jitter, jitter)) / per_row
            y = (row + 0.5 + rng.uniform(-jitter, jitter)) / rows
            seeds.append((x % 1.0, y % 1.0))
    seeds = np.array(seeds)
    grid = (np.arange(SIZE) + 0.5) / SIZE
    first = np.full((SIZE, SIZE), np.inf)
    second = np.full((SIZE, SIZE), np.inf)
    cell = np.zeros((SIZE, SIZE), dtype=np.int32)
    for index, (sx, sy) in enumerate(seeds):
        dx = np.abs(grid - sx)
        dx = np.minimum(dx, 1.0 - dx) * aspect
        dy = np.abs(grid - sy)
        dy = np.minimum(dy, 1.0 - dy)
        round_distance = np.sqrt(dx[None, :] ** 2 + dy[:, None] ** 2)
        square_distance = np.maximum(dx[None, :], dy[:, None])
        distance = lerp(round_distance, square_distance, blocky)
        closer = distance < first
        second = np.where(closer, first, np.minimum(second, distance))
        first = np.where(closer, distance, first)
        cell = np.where(closer, index, cell)
    return cell, (second - first) * rows, len(seeds)


def stones(rows, per_row, seed, palette, mortar, gap, aspect=1.0, blocky=0.0, jitter=0.18, bulge=0.6, normal_strength=9.0):
    """돌바닥과 돌벽의 공통 그림."""
    cell, edge, count = stone_cells(rows, per_row, jitter, seed, aspect, blocky)
    rng = np.random.default_rng(seed + 1)
    choice = rng.integers(0, len(palette), size=count)
    brightness = rng.uniform(0.86, 1.1, size=count)
    stone_color = np.array(palette)[choice] * brightness[:, None]

    mask = smoothstep(gap, gap + 0.09, edge)
    dome = smoothstep(gap, gap + 0.6, edge) ** bulge
    chips = spectral_noise(seed + 2, 1.6)
    blotch = spectral_noise(seed + 3, 2.4)
    height = mask * (0.55 + dome * 0.45) + (chips - 0.5) * 0.12 * mask

    albedo = stone_color[cell] * (0.8 + blotch[..., None] * 0.4) * (0.9 + chips[..., None] * 0.2)
    # 돌 가장자리는 밝게, 틈 쪽은 어둡게 칠해 손으로 그린 느낌을 낸다.
    rim = smoothstep(gap + 0.02, gap + 0.14, edge) * (1.0 - smoothstep(gap + 0.14, gap + 0.34, edge))
    albedo = albedo * (1.0 + rim[..., None] * 0.14)
    albedo = lerp(np.broadcast_to(mortar, albedo.shape), albedo, mask[..., None])
    return painted(albedo, height, normal_strength), height_to_normal(height, normal_strength)


def plaster():
    """벽 회칠. 색은 재질의 색 곱셈으로 입히므로 밝게 그린다."""
    soft = spectral_noise(101, 2.6)
    grain = spectral_noise(102, 1.2)
    albedo = (0.82 + soft * 0.14 + (grain - 0.5) * 0.05)[..., None] * color(1.0, 0.99, 0.97)
    height = soft * 0.5 + grain * 0.5
    return painted(albedo, height, 3.0), height_to_normal(height, 3.0)


def grass():
    patches = spectral_noise(201, 2.8)
    fine = spectral_noise(202, 1.0)
    blades = spectral_noise(203, 1.4, stretch=(1.0, 4.0))
    albedo = lerp(color(0.26, 0.42, 0.14), color(0.44, 0.58, 0.2), patches[..., None])
    albedo = lerp(albedo, color(0.56, 0.62, 0.26), smoothstep(0.62, 0.8, spectral_noise(204, 2.4))[..., None] * 0.6)
    albedo = albedo * (0.84 + fine[..., None] * 0.2 + blades[..., None] * 0.14)
    height = fine * 0.5 + blades * 0.5
    return painted(albedo, height, 3.0), height_to_normal(height, 3.0)


def dirt():
    patches = spectral_noise(301, 2.5)
    fine = spectral_noise(302, 1.1)
    albedo = lerp(color(0.3, 0.24, 0.17), color(0.46, 0.37, 0.25), patches[..., None]) * (0.86 + fine[..., None] * 0.26)
    height = patches * 0.6 + fine * 0.4
    return painted(albedo, height, 4.0), height_to_normal(height, 4.0)


def wood():
    """세로 판자. 결은 세로로 길게 늘인 잡음이다."""
    planks = 5
    grain = spectral_noise(401, 2.0, stretch=(0.08, 2.0))
    fine = spectral_noise(402, 1.2, stretch=(0.04, 2.0))
    x = (np.arange(SIZE) + 0.5) / SIZE * planks
    plank = np.floor(x).astype(np.int32) % planks
    edge = np.abs(x - np.round(x)) * 2.0
    gap = smoothstep(0.0, 0.05, edge)
    rng = np.random.default_rng(403)
    shade = rng.uniform(0.82, 1.14, size=planks)[plank]
    albedo = lerp(color(0.2, 0.12, 0.07), color(0.44, 0.28, 0.16), (grain * 0.7 + fine * 0.3)[..., None])
    albedo = albedo * shade[None, :, None]
    height = gap[None, :] * (0.7 + grain * 0.2 + fine * 0.1)
    albedo = albedo * lerp(0.35, 1.0, gap)[None, :, None]
    return painted(albedo, height, 7.0), height_to_normal(height, 7.0)


def bark():
    ridges = spectral_noise(451, 1.7, stretch=(0.18, 2.5))
    fine = spectral_noise(452, 1.1, stretch=(0.3, 2.0))
    height = smoothstep(0.3, 0.72, ridges) * 0.8 + fine * 0.2
    albedo = lerp(color(0.13, 0.09, 0.06), color(0.42, 0.31, 0.21), height[..., None])
    return painted(albedo, height, 10.0), height_to_normal(height, 10.0)


def leaves():
    clumps = spectral_noise(471, 2.0)
    fine = spectral_noise(472, 0.9)
    height = smoothstep(0.32, 0.7, clumps) * 0.6 + fine * 0.4
    albedo = lerp(color(0.12, 0.27, 0.11), color(0.42, 0.56, 0.2), height[..., None])
    return painted(albedo, height, 8.0), height_to_normal(height, 8.0)


def roof_tiles(seed, palette):
    """겹쳐 얹은 둥근 기와. 위 줄이 아래 줄의 머리를 덮으므로 아래 끝이 가장 높다."""
    rows, columns = 10, 8
    y = (np.arange(SIZE) + 0.5) / SIZE * rows
    x = (np.arange(SIZE) + 0.5) / SIZE * columns
    row = np.floor(y).astype(np.int32)
    v = (y - row)[:, None]
    offset = (row % 2 * 0.5)[:, None]
    shifted = x[None, :] + offset
    column = np.floor(shifted).astype(np.int32) % columns
    u = shifted - np.floor(shifted)
    # v=0이 기와의 아래 끝이다(텍스처 위쪽이 지붕 아래쪽).
    edge = np.abs(u - 0.5) * 2.0
    lip = 0.12 + 0.28 * edge ** 2.2
    tile = smoothstep(lip - 0.02, lip + 0.03, v)
    arch = np.sqrt(np.clip(1.0 - edge ** 2, 0.0, 1.0))
    height = tile * (0.35 + 0.5 * arch + 0.3 * (1.0 - v))

    rng = np.random.default_rng(seed)
    choice = rng.integers(0, len(palette), size=rows * columns)
    tone = rng.uniform(0.84, 1.12, size=rows * columns)
    index = (row[:, None] * columns + column) % (rows * columns)
    base = np.array(palette)[choice][index] * tone[index][..., None]
    blotch = spectral_noise(seed + 1, 2.2)
    albedo = base * (0.8 + blotch[..., None] * 0.4)
    gap_shadow = lerp(0.25, 1.0, tile)
    side_shadow = lerp(0.72, 1.0, arch)
    albedo = albedo * (gap_shadow * side_shadow)[..., None]
    # 그림의 아래쪽이 처마가 되도록 뒤집는다. 둥근 끝이 물 흐르는 쪽을 향해야 기와로 읽힌다.
    albedo, height = albedo[::-1], height[::-1]
    return painted(albedo, height, 7.0), height_to_normal(height, 7.0)


def flat():
    """색만 쓰는 재질(유리, 쇠, 등불)을 위한 흰 그림."""
    return np.ones((SIZE, SIZE, 3)), np.broadcast_to(color(0.0, 0.0, 1.0), (SIZE, SIZE, 3))


def water():
    ripples = spectral_noise(901, 1.9)
    return np.ones((SIZE, SIZE, 3)), height_to_normal(ripples, 26.0)


def water_flow():
    """떨어지는 물의 결. 흐르는 쪽(그림의 세로, v)으로 긴 물줄기 결에 가로 물결을 섞는다.

    색 그림은 흑백 가리개다. 재질이 이 값을 불투명도로 써서 결 사이가 비친다. 가로 물결이 있어야 흘릴 때 움직임이 보인다.
    """
    streaks = spectral_noise(911, 2.8, stretch=(1.0, 8.0), lowest_cycles=14)
    ripples = spectral_noise(912, 3.0, stretch=(6.0, 1.0), lowest_cycles=3)
    shape = streaks * 0.65 + ripples * 0.35
    mask = smoothstep(0.4, 0.62, shape)
    return np.repeat(mask[..., None], 3, axis=-1), height_to_normal(shape, 18.0)


STONE_WARM = [color(0.66, 0.6, 0.5), color(0.58, 0.54, 0.47), color(0.72, 0.67, 0.56), color(0.52, 0.5, 0.46)]
STONE_COOL = [color(0.55, 0.56, 0.56), color(0.47, 0.48, 0.5), color(0.62, 0.61, 0.58), color(0.42, 0.43, 0.45)]
PAVING_LIGHT = [color(0.78, 0.72, 0.6), color(0.7, 0.66, 0.56), color(0.84, 0.78, 0.66), color(0.66, 0.6, 0.5)]
TILE_RED = [color(0.62, 0.28, 0.16), color(0.54, 0.22, 0.13), color(0.7, 0.36, 0.2), color(0.48, 0.2, 0.13)]
TILE_SLATE = [color(0.27, 0.33, 0.4), color(0.22, 0.27, 0.34), color(0.34, 0.4, 0.46), color(0.3, 0.32, 0.37)]
MORTAR_DARK = color(0.2, 0.18, 0.15)
MORTAR_SOIL = color(0.24, 0.2, 0.14)

TEXTURES = {
    "Cobble": lambda: stones(9, 9, 11, STONE_COOL, MORTAR_SOIL, 0.06, bulge=0.45),
    "Paving": lambda: stones(6, 4, 12, PAVING_LIGHT, MORTAR_DARK, 0.03, aspect=0.8, blocky=0.85, jitter=0.12, bulge=1.3, normal_strength=5.0),
    "Ashlar": lambda: stones(8, 5, 13, STONE_WARM, MORTAR_DARK, 0.05, aspect=0.5, blocky=0.7, jitter=0.1, bulge=0.8),
    "Plaster": plaster,
    "Grass": grass,
    "Dirt": dirt,
    "Wood": wood,
    "Bark": bark,
    "Leaves": leaves,
    "RoofRed": lambda: roof_tiles(21, TILE_RED),
    "RoofSlate": lambda: roof_tiles(31, TILE_SLATE),
    "Flat": flat,
    "WaterRipple": water,
    "WaterFlow": water_flow,
}


def build_all(out_dir):
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    for name, make in TEXTURES.items():
        albedo, normal = make()
        write_png(out / f"T_MD_{name}_B.png", albedo)
        write_png(out / f"T_MD_{name}_N.png", normal * 0.5 + 0.5)
    return sorted(TEXTURES)
