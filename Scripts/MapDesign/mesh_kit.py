"""메시를 파이썬 데이터로 쌓는 공통 도구. bpy에 기대지 않으므로 형상 규칙을 Blender 밖에서도 검사할 수 있다.

단위는 미터다. 재질마다 tile_m(텍스처 한 장이 덮는 길이)이 있어, UV를 실제 길이로 맞추면 벽돌 크기가 어디서든 같다.
"""
from __future__ import annotations

import json
import math
import random
from dataclasses import dataclass
from pathlib import Path

MATERIALS = json.loads((Path(__file__).parent / "materials.json").read_text(encoding="utf-8"))

# 면이 이보다 위를 보면 바닥으로 보고 UV를 평면으로 편다.
FLAT_NORMAL_Z = 0.7


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def scale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def length(a):
    return math.sqrt(dot(a, a))


def unit(a):
    size = length(a)
    return scale(a, 1.0 / size) if size else a


def polygon_normal(points):
    """뉴웰 방식 법선. 볼록하지 않은 면에서도 방향이 맞는다."""
    normal = (0.0, 0.0, 0.0)
    for i, p in enumerate(points):
        q = points[(i + 1) % len(points)]
        normal = add(normal, ((p[1] - q[1]) * (p[2] + q[2]), (p[2] - q[2]) * (p[0] + q[0]), (p[0] - q[0]) * (p[1] + q[1])))
    return unit(normal)


@dataclass
class Poly:
    start: int
    count: int
    material: str
    uvs: list
    smooth: bool


class MeshData:
    def __init__(self, name):
        self.name = name
        self.verts = []
        self.polys = []

    @property
    def is_empty(self):
        return not self.polys

    def add_face(self, points, material, facing=None, uvs=None, smooth=False):
        """면 하나를 더한다. facing을 주면 그쪽을 보도록 꼭짓점 순서를 맞춘다."""
        points = list(points)
        if facing is not None and dot(polygon_normal(points), facing) < 0:
            points.reverse()
            if uvs:
                uvs = list(reversed(uvs))
        if uvs is None:
            uvs = planar_uvs(points, material)
        self.polys.append(Poly(len(self.verts), len(points), material, uvs, smooth))
        self.verts.extend(points)

    def add_box(self, low, high, material, top_material=None, skip=()):
        """축에 맞춘 상자. skip에 "bottom" 등을 주면 보이지 않는 면을 뺀다."""
        x0, y0, z0 = low
        x1, y1, z1 = high
        faces = {
            "top": ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1), top_material or material),
            "bottom": ([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], (0, 0, -1), material),
            "east": ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], (1, 0, 0), material),
            "west": ([(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)], (-1, 0, 0), material),
            "north": ([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], (0, 1, 0), material),
            "south": ([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0), material),
        }
        for name, (points, facing, face_material) in faces.items():
            if name not in skip:
                self.add_face(points, face_material, facing=facing)

    def add_beam(self, start, end, side, material, side_half, up_half):
        """start에서 end까지 뻗는 각재. side와 그에 수직인 방향으로 단면을 만든다."""
        axis = unit(sub(end, start))
        side = unit(side)
        up = unit(cross(axis, side))
        corners = [(-1, -1), (1, -1), (1, 1), (-1, 1)]

        def ring(center):
            return [add(add(center, scale(side, a * side_half)), scale(up, b * up_half)) for a, b in corners]

        near, far = ring(start), ring(end)
        for i in range(4):
            j = (i + 1) % 4
            middle = scale(add(add(near[i], near[j]), add(far[i], far[j])), 0.25)
            center = scale(add(start, end), 0.5)
            self.add_face([near[i], near[j], far[j], far[i]], material, facing=sub(middle, center))
        self.add_face(near, material, facing=scale(axis, -1))
        self.add_face(far, material, facing=axis)

    def add_lathe(self, center, profile, segments, material, facing_out=True, smooth=True, flip=False, u_radius=None):
        """(반지름, 높이) 단면을 z축으로 돌려 깎는다. 분수와 가로등 기둥에 쓴다.

        v는 단면을 따라 잰 길이다. 흐르는 물은 단면을 흐르는 순서로 적고, 그 순서로는 면이 반대쪽을 볼 때 flip으로 뒤집는다.
        u는 둘레 길이를 칸마다 그 칸의 큰 반지름으로 잰다. 벽돌 크기는 어디서나 같지만 반지름이 바뀌는 칸 경계에서 u가 어긋난다.
        흐르는 물처럼 결이 단면을 따라 끊기지 않아야 하면 u_radius로 모든 칸을 한 반지름으로 잰다. 결은 각도를 따라 곧게 이어지고
        반지름이 커지는 만큼 넓어진다. 꼭짓점마다 제 반지름으로 재면 같은 각도의 u가 반지름마다 달라 결이 비스듬히 꼬인다.
        """
        tile = MATERIALS[material]["tile_m"]
        distance = 0.0
        for (r0, z0), (r1, z1) in zip(profile, profile[1:]):
            step = math.hypot(r1 - r0, z1 - z0)
            arc = 2 * math.pi * (u_radius or max(r0, r1, 0.2)) / tile
            for s in range(segments):
                a0 = 2 * math.pi * s / segments
                a1 = 2 * math.pi * (s + 1) / segments
                ring = [(a0, r0, z0), (a1, r0, z0), (a1, r1, z1), (a0, r1, z1)]
                points = [(center[0] + r * math.cos(a), center[1] + r * math.sin(a), center[2] + z) for a, r, z in ring]
                uvs = [
                    (arc * s / segments, distance / tile),
                    (arc * (s + 1) / segments, distance / tile),
                    (arc * (s + 1) / segments, (distance + step) / tile),
                    (arc * s / segments, (distance + step) / tile),
                ]
                if r0 == 0 or r1 == 0:
                    keep = [i for i in range(4) if not (ring[i][1] == 0 and i in (1, 2))]
                    points = [points[i] for i in keep]
                    uvs = [uvs[i] for i in keep]
                if flip:
                    points.reverse()
                    uvs.reverse()
                self.polys.append(Poly(len(self.verts), len(points), material, uvs, smooth))
                self.verts.extend(points)
            distance += step

    def add_tube(self, path, radii, segments, material):
        """점을 따라 휘는 관. 분수 물줄기에 쓴다. 끝은 막지 않는다(돌과 물속에 묻힌다).

        v는 관을 따라 잰 길이라 재질이 v로 흘리는 무늬가 관을 따라 흐른다. 단면은 앞 단면을 이어받아 돌려 관이 꼬이지 않는다.
        """
        tile = MATERIALS[material]["tile_m"]
        arc = 2 * math.pi * max(radii) / tile
        angles = [2 * math.pi * s / segments for s in range(segments + 1)]
        rings, distances = [], []
        normal, distance = None, 0.0
        for index, point in enumerate(path):
            tangent = unit(sub(path[min(index + 1, len(path) - 1)], path[max(index - 1, 0)]))
            if normal is None:
                helper = (0.0, 0.0, 1.0) if abs(tangent[2]) < 0.9 else (1.0, 0.0, 0.0)
                normal = unit(cross(tangent, helper))
            else:
                normal = unit(sub(normal, scale(tangent, dot(normal, tangent))))
            binormal = cross(tangent, normal)
            radius = radii[index]
            rings.append([add(point, add(scale(normal, radius * math.cos(a)), scale(binormal, radius * math.sin(a)))) for a in angles])
            if index:
                distance += length(sub(point, path[index - 1]))
            distances.append(distance / tile)
        for index in range(len(path) - 1):
            middle_axis = scale(add(path[index], path[index + 1]), 0.5)
            for s in range(segments):
                quad = [rings[index][s], rings[index][s + 1], rings[index + 1][s + 1], rings[index + 1][s]]
                u0, u1 = arc * s / segments, arc * (s + 1) / segments
                v0, v1 = distances[index], distances[index + 1]
                middle = scale(add(add(quad[0], quad[1]), add(quad[2], quad[3])), 0.25)
                self.add_face(quad, material, facing=sub(middle, middle_axis), uvs=[(u0, v0), (u1, v0), (u1, v1), (u0, v1)], smooth=True)


    def add_cylinder(self, start, end, radius, segments, material, caps=True, smooth=True):
        """두 점 사이의 원기둥. 바퀴와 축처럼 눕는 부품에 쓴다."""
        axis = unit(sub(end, start))
        helper = (0.0, 0.0, 1.0) if abs(axis[2]) < 0.9 else (1.0, 0.0, 0.0)
        side = unit(cross(axis, helper))
        up = cross(side, axis)
        tile = MATERIALS[material]["tile_m"]
        run = length(sub(end, start))

        def ring(center):
            points = []
            for index in range(segments):
                angle = 2 * math.pi * index / segments
                offset = add(scale(side, radius * math.cos(angle)), scale(up, radius * math.sin(angle)))
                points.append(add(center, offset))
            return points

        near, far = ring(start), ring(end)
        arc = 2 * math.pi * radius / tile
        for index in range(segments):
            other = (index + 1) % segments
            uvs = [(arc * index / segments, 0.0), (arc * (index + 1) / segments, 0.0), (arc * (index + 1) / segments, run / tile), (arc * index / segments, run / tile)]
            outward = sub(scale(add(near[index], near[other]), 0.5), start)
            self.add_face([near[index], near[other], far[other], far[index]], material, facing=outward, uvs=uvs, smooth=smooth)
        if caps:
            self.add_face(near, material, facing=scale(axis, -1))
            self.add_face(far, material, facing=axis)

    def add_blob(self, center, radii, seed, material, roughness=0.18, subdivisions=2):
        """울퉁불퉁한 둥근 덩어리. 나무 잎사귀를 스타일화된 덩어리로 만든다."""
        rng = random.Random(seed)
        vertices, triangles = _icosphere(subdivisions)
        phase = [rng.uniform(0, 6.28) for _ in range(3)]
        tile = MATERIALS[material]["tile_m"]
        placed = []
        for v in vertices:
            bump = 1.0 + roughness * (
                math.sin(v[0] * 3.1 + phase[0]) * math.sin(v[1] * 2.7 + phase[1]) + 0.6 * math.sin(v[2] * 4.3 + phase[2])
            )
            placed.append((center[0] + v[0] * radii[0] * bump, center[1] + v[1] * radii[1] * bump, center[2] + v[2] * radii[2] * bump))
        for a, b, c in triangles:
            points = [placed[a], placed[b], placed[c]]
            uvs = [((p[0] + p[2]) / tile, (p[1] + p[2]) / tile) for p in points]
            self.polys.append(Poly(len(self.verts), 3, material, uvs, True))
            self.verts.extend(points)


def _icosphere(subdivisions):
    """정이십면체를 잘게 나눈 구. 어느 쪽에서 봐도 면이 고르다."""
    t = (1.0 + math.sqrt(5.0)) / 2.0
    vertices = [unit(v) for v in (
        (-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1),
    )]
    triangles = [
        (0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
        (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1),
    ]
    for _ in range(subdivisions):
        cache = {}

        def midpoint(a, b):
            key = (min(a, b), max(a, b))
            if key not in cache:
                vertices.append(unit(scale(add(vertices[a], vertices[b]), 0.5)))
                cache[key] = len(vertices) - 1
            return cache[key]

        refined = []
        for a, b, c in triangles:
            ab, bc, ca = midpoint(a, b), midpoint(b, c), midpoint(c, a)
            refined += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        triangles = refined
    return vertices, triangles


def planar_uvs(points, material):
    """바닥은 x·y로, 벽은 벽을 따라가는 길이와 높이로 편다. 이어진 벽의 무늬가 끊기지 않는다."""
    tile = MATERIALS[material]["tile_m"]
    normal = polygon_normal(points)
    if abs(normal[2]) > FLAT_NORMAL_Z:
        return [(p[0] / tile, p[1] / tile) for p in points]
    tangent = unit((-normal[1], normal[0], 0.0)) if (normal[0] or normal[1]) else (1.0, 0.0, 0.0)
    return [(dot(p, tangent) / tile, p[2] / tile) for p in points]


class Frame:
    """벽면 좌표계. a는 바깥에서 볼 때 오른쪽, b는 위, c는 벽 바깥쪽이다."""

    def __init__(self, origin, u, v, n):
        self.origin = origin
        self.u = u
        self.v = v
        self.n = n

    @classmethod
    def facing(cls, normal, origin):
        """바깥 방향이 normal인 수직 벽. origin은 바깥에서 볼 때 왼쪽 아래 모서리다."""
        u = (-normal[1], normal[0], 0.0)
        return cls(origin, u, (0.0, 0.0, 1.0), (normal[0], normal[1], 0.0))

    def at(self, a, b, c=0.0):
        return add(add(add(self.origin, scale(self.u, a)), scale(self.v, b)), scale(self.n, c))

    def quad(self, mesh, a0, b0, a1, b1, material, c=0.0):
        points = [self.at(a0, b0, c), self.at(a1, b0, c), self.at(a1, b1, c), self.at(a0, b1, c)]
        mesh.add_face(points, material, facing=self.n)

    def box(self, mesh, a0, b0, a1, b1, c0, c1, material, sides=True, back=False):
        """벽에서 c0부터 c1까지 나온 상자. 벽에 붙는 장식은 뒷면이 벽에 묻히므로 만들지 않는다.

        땅 위에 따로 서는 상자(물가 난간, 축대 갓돌)는 back=True로 c0 쪽 뒷면도 만든다. 언리얼은 뒷면을 그리지 않아,
        뒷면이 없으면 그쪽에서 볼 때 상자가 투명하게 사라진다.
        """
        self.quad(mesh, a0, b0, a1, b1, material, c1)
        if back:
            mesh.add_face([self.at(a0, b0, c0), self.at(a1, b0, c0), self.at(a1, b1, c0), self.at(a0, b1, c0)], material,
                          facing=scale(self.n, -1))
        if not sides:
            return
        mesh.add_face([self.at(a0, b0, c0), self.at(a0, b1, c0), self.at(a0, b1, c1), self.at(a0, b0, c1)], material, facing=scale(self.u, -1))
        mesh.add_face([self.at(a1, b0, c0), self.at(a1, b1, c0), self.at(a1, b1, c1), self.at(a1, b0, c1)], material, facing=self.u)
        mesh.add_face([self.at(a0, b1, c0), self.at(a1, b1, c0), self.at(a1, b1, c1), self.at(a0, b1, c1)], material, facing=self.v)
        mesh.add_face([self.at(a0, b0, c0), self.at(a1, b0, c0), self.at(a1, b0, c1), self.at(a0, b0, c1)], material, facing=scale(self.v, -1))
