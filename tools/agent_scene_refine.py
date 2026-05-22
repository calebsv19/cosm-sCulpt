#!/usr/bin/env python3
import argparse
import copy
import json
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def yaw_pitch_to_target(px: float,
                        py: float,
                        pz: float,
                        tx: float,
                        ty: float,
                        tz: float) -> Tuple[float, float]:
    dx = tx - px
    dy = ty - py
    dz = tz - pz
    horizontal = math.hypot(dx, dy)
    yaw = 0.0
    pitch = 0.0
    max_pitch = math.radians(70.0)
    if horizontal > 1e-9:
        yaw = math.atan2(dx, -dy)
    if horizontal > 1e-9 or abs(dz) > 1e-9:
        pitch = math.atan2(dz, horizontal)
    pitch = clamp(pitch, -max_pitch, max_pitch)
    return yaw, pitch


@dataclass
class Footprint:
    object_id: str
    x: float
    y: float
    hx: float
    hy: float
    z_min: float
    z_max: float
    depth: float


def load_json(path: Path) -> dict:
    return json.loads(path.read_text())


def save_json(path: Path, data: dict) -> None:
    path.write_text(json.dumps(data, indent=2) + "\n")


def build_material_map(request: dict) -> Dict[str, dict]:
    authoring = (
        request.get("extensions", {})
        .get("ray_tracing", {})
        .get("authoring", {})
    )
    entries = authoring.get("object_materials", [])
    return {
        entry.get("object_id"): entry
        for entry in entries
        if isinstance(entry, dict) and entry.get("object_id")
    }


def find_floor_bounds(request: dict) -> Optional[Tuple[float, float, float, float]]:
    for obj in request.get("objects", []):
        if obj.get("kind") == "plane" and obj.get("axis") == "xy":
            pos = obj.get("position", {})
            width = float(obj.get("width", 0.0))
            height = float(obj.get("height", 0.0))
            x = float(pos.get("x", 0.0))
            y = float(pos.get("y", 0.0))
            return (x - width / 2.0, x + width / 2.0, y - height / 2.0, y + height / 2.0)
    return None


def rect_prism_footprint(obj: dict) -> Optional[Footprint]:
    if obj.get("kind") != "rect_prism":
        return None
    pos = obj.get("position", {})
    axis = obj.get("axis", "xy")
    x = float(pos.get("x", 0.0))
    y = float(pos.get("y", 0.0))
    z = float(pos.get("z", 0.0))
    width = float(obj.get("width", 0.0))
    height = float(obj.get("height", 0.0))
    depth = float(obj.get("depth", 0.0))
    if axis == "xy":
        hx = width / 2.0
        hy = height / 2.0
        z_min = z - depth / 2.0
        z_max = z + depth / 2.0
    elif axis == "xz":
        hx = width / 2.0
        hy = depth / 2.0
        z_min = z - height / 2.0
        z_max = z + height / 2.0
    elif axis == "yz":
        hx = depth / 2.0
        hy = width / 2.0
        z_min = z - height / 2.0
        z_max = z + height / 2.0
    else:
        return None
    return Footprint(
        object_id=obj["id"],
        x=x,
        y=y,
        hx=hx,
        hy=hy,
        z_min=z_min,
        z_max=z_max,
        depth=depth,
    )


def is_transparent_tall_prism(obj: dict, material: Optional[dict]) -> bool:
    if obj.get("kind") != "rect_prism":
        return False
    if obj.get("axis") != "xy":
        return False
    depth = float(obj.get("depth", 0.0))
    if depth < 1.6:
        return False
    if not material:
        return False
    alpha = material.get("alpha")
    material_id = material.get("material_id")
    return (
        (alpha is not None and float(alpha) < 0.98)
        or material_id == 5
    )


def compute_cluster_center(footprints: List[Footprint]) -> Tuple[float, float, float]:
    if not footprints:
        return (0.0, 0.0, 1.2)
    cx = sum(fp.x for fp in footprints) / len(footprints)
    cy = sum(fp.y for fp in footprints) / len(footprints)
    cz = sum((fp.z_min + fp.z_max) * 0.5 for fp in footprints) / len(footprints)
    return (cx, cy, cz)


def apply_spacing_pass(request: dict, min_gap: float = 0.35, iterations: int = 16) -> dict:
    materials = build_material_map(request)
    objects = request.get("objects", [])
    prism_objects = [
        obj for obj in objects if is_transparent_tall_prism(obj, materials.get(obj.get("id")))
    ]
    footprints = [rect_prism_footprint(obj) for obj in prism_objects]
    footprints = [fp for fp in footprints if fp is not None]
    floor = find_floor_bounds(request)
    if not floor or len(footprints) < 2:
        return {"adjustments": [], "transparent_prism_ids": [fp.object_id for fp in footprints]}

    min_x, max_x, min_y, max_y = floor
    adjustments = []
    obj_by_id = {obj["id"]: obj for obj in objects}

    for _ in range(iterations):
        moved = False
        for i in range(len(footprints)):
            for j in range(i + 1, len(footprints)):
                a = footprints[i]
                b = footprints[j]
                dx = b.x - a.x
                dy = b.y - a.y
                req_x = a.hx + b.hx + min_gap
                req_y = a.hy + b.hy + min_gap
                overlap_x = req_x - abs(dx)
                overlap_y = req_y - abs(dy)
                if overlap_x <= 0.0 or overlap_y <= 0.0:
                    continue
                moved = True
                if abs(dx) < 1e-6 and abs(dy) < 1e-6:
                    dx = 0.001
                    dy = 0.001
                if overlap_x < overlap_y:
                    push = overlap_x / 2.0
                    sign = 1.0 if dx >= 0.0 else -1.0
                    a.x -= sign * push
                    b.x += sign * push
                else:
                    push = overlap_y / 2.0
                    sign = 1.0 if dy >= 0.0 else -1.0
                    a.y -= sign * push
                    b.y += sign * push
        for fp in footprints:
            fp.x = clamp(fp.x, min_x + fp.hx + min_gap, max_x - fp.hx - min_gap)
            fp.y = clamp(fp.y, min_y + fp.hy + min_gap, max_y - fp.hy - min_gap)
        if not moved:
            break

    for fp in footprints:
        obj = obj_by_id.get(fp.object_id)
        if not obj:
            continue
        pos = obj.setdefault("position", {})
        old_x = float(pos.get("x", 0.0))
        old_y = float(pos.get("y", 0.0))
        if abs(old_x - fp.x) > 1e-5 or abs(old_y - fp.y) > 1e-5:
            adjustments.append(
                {
                    "object_id": fp.object_id,
                    "from": {"x": old_x, "y": old_y},
                    "to": {"x": round(fp.x, 4), "y": round(fp.y, 4)},
                }
            )
            pos["x"] = round(fp.x, 6)
            pos["y"] = round(fp.y, 6)

    return {
        "adjustments": adjustments,
        "transparent_prism_ids": [fp.object_id for fp in footprints],
    }


def detect_room_corner_signs(request: dict) -> Tuple[int, int]:
    wall_x_sign = -1
    wall_y_sign = 1
    for obj in request.get("objects", []):
        if obj.get("kind") != "rect_prism":
            continue
        pos = obj.get("position", {})
        axis = obj.get("axis")
        if axis == "yz":
            wall_x_sign = -1 if float(pos.get("x", 0.0)) <= 0.0 else 1
        elif axis == "xz":
            wall_y_sign = 1 if float(pos.get("y", 0.0)) >= 0.0 else -1
    return wall_x_sign, wall_y_sign


def apply_camera_pass(request: dict, transparent_footprints: List[Footprint]) -> dict:
    floor = find_floor_bounds(request)
    authoring = request.setdefault("extensions", {}).setdefault("ray_tracing", {}).setdefault("authoring", {})
    if not floor:
        return {"applied": False}
    min_x, max_x, min_y, max_y = floor
    cx, cy, cz = compute_cluster_center(transparent_footprints)
    wall_x_sign, wall_y_sign = detect_room_corner_signs(request)
    span_x = max_x - min_x
    span_y = max_y - min_y
    center_x = (min_x + max_x) * 0.5
    center_y = (min_y + max_y) * 0.5
    outside_x_sign = 1.0 if wall_x_sign < 0 else -1.0
    outside_y_sign = -1.0 if wall_y_sign > 0 else 1.0
    far_pad_x = max(0.95, span_x * 0.09)
    far_pad_y = max(0.85, span_y * 0.09)
    near_pad_x = max(0.45, span_x * 0.04)
    near_pad_y = max(0.40, span_y * 0.04)

    start_x = center_x + outside_x_sign * (span_x * 0.5 + far_pad_x)
    start_y = center_y + outside_y_sign * (span_y * 0.5 + far_pad_y)
    second_x = center_x + outside_x_sign * (span_x * 0.5 + near_pad_x)
    second_y = center_y + outside_y_sign * (span_y * 0.5 + near_pad_y)

    bounds = request.setdefault("bounds", {})
    bounds["clamp_on_edit"] = False

    room_height = request.get("bounds", {}).get("max", {}).get("z", 4.0)
    z0 = clamp(room_height * 0.60, 2.4, 3.1)
    z1 = clamp(z0 - 0.10, 2.3, 3.0)
    focus_x = round(cx, 4)
    focus_y = round(cy, 4)
    focus_z = round(max(1.1, cz), 4)
    yaw0, pitch0 = yaw_pitch_to_target(start_x, start_y, z0, focus_x, focus_y, focus_z)
    yaw1, pitch1 = yaw_pitch_to_target(second_x, second_y, z1, focus_x, focus_y, focus_z)

    dx = second_x - start_x
    dy = second_y - start_y

    authoring["camera_path"] = {
        "mode": "BEZIER_CUBIC",
        "points": [
            {
                "x": round(start_x, 4),
                "y": round(start_y, 4),
                "rotation": round(yaw0, 6),
                "handleLink": False,
                "velocity1": {
                    "vx": round(dx * 0.38, 4),
                    "vy": round(dy * 0.38, 4),
                },
            },
            {
                "x": round(second_x, 4),
                "y": round(second_y, 4),
                "rotation": round(yaw1, 6),
                "handleLink": False,
                "velocity2": {
                    "vx": round(-dx * 0.24, 4),
                    "vy": round(-dy * 0.24, 4),
                },
            },
        ],
    }
    authoring["camera_path_depth"] = {
        "points": [
            {"z": round(z0, 4), "lookPitch": round(pitch0, 6), "velocity1": {"vz": 0.12}},
            {"z": round(z1, 4), "lookPitch": round(pitch1, 6), "velocity2": {"vz": -0.08}},
        ]
    }
    authoring["camera_focus_target"] = {
        "x": focus_x,
        "y": focus_y,
        "z": focus_z,
    }
    return {
        "applied": True,
        "camera_corner": {"x": round(start_x, 4), "y": round(start_y, 4)},
        "camera_outside_floor": {
            "start": (start_x < min_x or start_x > max_x) and (start_y < min_y or start_y > max_y),
            "second": (second_x < min_x or second_x > max_x) and (second_y < min_y or second_y > max_y),
        },
        "camera_orientation": {
            "start_yaw": round(yaw0, 6),
            "start_pitch": round(pitch0, 6),
            "second_yaw": round(yaw1, 6),
            "second_pitch": round(pitch1, 6),
        },
        "focus_target": authoring["camera_focus_target"],
    }


def bezier_point(p0, c1, c2, p1, t: float) -> Tuple[float, float]:
    u = 1.0 - t
    x = (u ** 3) * p0[0] + 3 * (u ** 2) * t * c1[0] + 3 * u * (t ** 2) * c2[0] + (t ** 3) * p1[0]
    y = (u ** 3) * p0[1] + 3 * (u ** 2) * t * c1[1] + 3 * u * (t ** 2) * c2[1] + (t ** 3) * p1[1]
    return x, y


def light_path_samples(authoring: dict, samples_per_segment: int = 24) -> List[Tuple[float, float]]:
    path = authoring.get("light_path", {})
    points = path.get("points", [])
    if len(points) < 2:
        return []
    sampled: List[Tuple[float, float]] = []
    for idx in range(len(points) - 1):
        p0 = points[idx]
        p1 = points[idx + 1]
        v1 = p0.get("velocity1") or {"vx": 0.0, "vy": 0.0}
        v2 = p1.get("velocity2") or {"vx": 0.0, "vy": 0.0}
        a = (float(p0.get("x", 0.0)), float(p0.get("y", 0.0)))
        d = (float(p1.get("x", 0.0)), float(p1.get("y", 0.0)))
        b = (a[0] + float(v1.get("vx", 0.0)), a[1] + float(v1.get("vy", 0.0)))
        c = (d[0] + float(v2.get("vx", 0.0)), d[1] + float(v2.get("vy", 0.0)))
        for s in range(samples_per_segment + 1):
            t = s / float(samples_per_segment)
            sampled.append(bezier_point(a, b, c, d, t))
    return sampled


def count_path_collisions(samples: List[Tuple[float, float]], footprints: List[Footprint], margin: float = 0.18) -> int:
    collisions = 0
    for sx, sy in samples:
        for fp in footprints:
            if (
                abs(sx - fp.x) <= fp.hx + margin
                and abs(sy - fp.y) <= fp.hy + margin
            ):
                collisions += 1
                break
    return collisions


def apply_light_path_pass(request: dict, transparent_footprints: List[Footprint]) -> dict:
    authoring = request.setdefault("extensions", {}).setdefault("ray_tracing", {}).setdefault("authoring", {})
    floor = find_floor_bounds(request)
    if not floor or not transparent_footprints:
        return {"applied": False}
    before = count_path_collisions(light_path_samples(authoring), transparent_footprints)

    min_x, max_x, min_y, max_y = floor
    cx, cy, cz = compute_cluster_center(transparent_footprints)
    bbox_min_x = min(fp.x - fp.hx for fp in transparent_footprints)
    bbox_max_x = max(fp.x + fp.hx for fp in transparent_footprints)
    bbox_min_y = min(fp.y - fp.hy for fp in transparent_footprints)
    bbox_max_y = max(fp.y + fp.hy for fp in transparent_footprints)
    corridor = 0.8

    p0 = (bbox_min_x - corridor * 1.05, cy - 1.15)
    p1 = (bbox_min_x - corridor * 0.55, bbox_max_y + corridor * 0.65)
    p2 = (bbox_max_x + corridor * 0.30, bbox_max_y + corridor * 0.78)
    p3 = (bbox_max_x + corridor * 0.95, cy + 0.25)

    points = [p0, p1, p2, p3]
    clamped_points = []
    for px, py in points:
        clamped_points.append(
            (
                clamp(px, min_x + 0.9, max_x - 0.9),
                clamp(py, min_y + 0.9, max_y - 0.9),
            )
        )

    authoring["light_path"] = {
        "mode": "BEZIER_CUBIC",
        "points": [
            {
                "x": round(clamped_points[0][0], 4),
                "y": round(clamped_points[0][1], 4),
                "rotation": 0.08,
                "handleLink": False,
                "velocity1": {
                    "vx": round((clamped_points[1][0] - clamped_points[0][0]) * 0.55, 4),
                    "vy": round((clamped_points[1][1] - clamped_points[0][1]) * 0.55, 4),
                },
            },
            {
                "x": round(clamped_points[1][0], 4),
                "y": round(clamped_points[1][1], 4),
                "rotation": 0.18,
                "handleLink": False,
                "velocity2": {
                    "vx": round(-(clamped_points[2][0] - clamped_points[0][0]) * 0.16, 4),
                    "vy": round(-(clamped_points[2][1] - clamped_points[0][1]) * 0.16, 4),
                },
            },
            {
                "x": round(clamped_points[2][0], 4),
                "y": round(clamped_points[2][1], 4),
                "rotation": 0.30,
                "handleLink": False,
                "velocity2": {
                    "vx": round(-(clamped_points[3][0] - clamped_points[1][0]) * 0.16, 4),
                    "vy": round(-(clamped_points[3][1] - clamped_points[1][1]) * 0.16, 4),
                },
            },
            {
                "x": round(clamped_points[3][0], 4),
                "y": round(clamped_points[3][1], 4),
                "rotation": 0.40,
                "handleLink": False,
                "velocity2": {
                    "vx": round(-(clamped_points[3][0] - clamped_points[2][0]) * 0.36, 4),
                    "vy": round(-(clamped_points[3][1] - clamped_points[2][1]) * 0.36, 4),
                },
            },
        ],
    }
    light_z = clamp(cz + 0.15, 1.6, 2.4)
    authoring["light_path_depth"] = {
        "points": [
            {"z": round(light_z, 4), "velocity1": {"vz": 0.05}},
            {"z": round(light_z + 0.12, 4), "velocity2": {"vz": 0.02}},
            {"z": round(light_z + 0.04, 4), "velocity2": {"vz": -0.04}},
            {"z": round(light_z + 0.10, 4), "velocity2": {"vz": -0.04}},
        ]
    }

    after = count_path_collisions(light_path_samples(authoring), transparent_footprints)
    return {
        "applied": True,
        "collisions_before": before,
        "collisions_after": after,
    }


def refine_request(request: dict) -> Tuple[dict, dict]:
    refined = copy.deepcopy(request)
    spacing = apply_spacing_pass(refined)
    materials = build_material_map(refined)
    footprints = [
        rect_prism_footprint(obj)
        for obj in refined.get("objects", [])
        if is_transparent_tall_prism(obj, materials.get(obj.get("id")))
    ]
    footprints = [fp for fp in footprints if fp is not None]
    camera = apply_camera_pass(refined, footprints)
    light = apply_light_path_pass(refined, footprints)
    report = {
        "scene_id": refined.get("scene_id"),
        "transparent_prism_count": len(footprints),
        "spacing": spacing,
        "camera": camera,
        "light_path": light,
    }
    return refined, report


def main() -> int:
    parser = argparse.ArgumentParser(description="Refine agent-authored corner-room scenes.")
    parser.add_argument("--request", required=True, help="Input LineDrawing request JSON")
    parser.add_argument("--output", required=True, help="Output refined request JSON")
    parser.add_argument("--report", required=True, help="Output refinement report JSON")
    args = parser.parse_args()

    request_path = Path(args.request)
    output_path = Path(args.output)
    report_path = Path(args.report)
    request = load_json(request_path)
    refined, report = refine_request(request)
    save_json(output_path, refined)
    save_json(report_path, report)
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
