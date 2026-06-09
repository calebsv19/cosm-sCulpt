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


@dataclass
class SubjectBounds:
    object_id: str
    x: float
    y: float
    z: float
    radius: float
    height: float


@dataclass
class LightingContext:
    subject: SubjectBounds
    front_x: float
    front_y: float
    front_angle: float
    radius: float
    front_limit_degrees: float
    allow_backlight: bool
    ambient_mode: str


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


def object_subject_bounds(obj: dict) -> Optional[SubjectBounds]:
    object_id = obj.get("id")
    if not object_id:
        return None
    kind = obj.get("kind")
    pos = obj.get("position", {})
    x = float(pos.get("x", 0.0))
    y = float(pos.get("y", 0.0))
    z = float(pos.get("z", 0.0))
    if kind == "rect_prism":
        axis = obj.get("axis", "xy")
        width = float(obj.get("width", 0.0))
        height = float(obj.get("height", 0.0))
        depth = float(obj.get("depth", 0.0))
        if axis == "xy":
            hz = depth * 0.5
            radius = max(width, height) * 0.5
        elif axis == "xz":
            hz = height * 0.5
            radius = max(width, depth) * 0.5
        elif axis == "yz":
            hz = height * 0.5
            radius = max(depth, width) * 0.5
        else:
            return None
        return SubjectBounds(object_id, x, y, z, max(0.1, radius), max(0.1, hz * 2.0))
    if kind == "mesh_asset_instance":
        scale = obj.get("scale", {})
        sx = abs(float(scale.get("x", 1.0)))
        sy = abs(float(scale.get("y", 1.0)))
        sz = abs(float(scale.get("z", 1.0)))
        radius = max(0.25, max(sx, sy) * 0.75)
        height = max(0.25, sz * 1.5)
        return SubjectBounds(object_id, x, y, z, radius, height)
    return None


def resolve_subject_bounds(request: dict, authoring: dict, policy: dict) -> SubjectBounds:
    subject_id = policy.get("subject_object_id") or policy.get("subject_id")
    selector = policy.get("subject")
    if isinstance(selector, dict):
        subject_id = selector.get("object_id") or subject_id
    candidates = [
        bounds
        for bounds in (object_subject_bounds(obj) for obj in request.get("objects", []))
        if bounds is not None
    ]
    if subject_id:
        for bounds in candidates:
            if bounds.object_id == subject_id:
                return bounds
    non_floor = [
        bounds
        for bounds in candidates
        if not bounds.object_id.lower().startswith("floor")
    ]
    if non_floor:
        cx = sum(item.x for item in non_floor) / len(non_floor)
        cy = sum(item.y for item in non_floor) / len(non_floor)
        cz = sum(item.z for item in non_floor) / len(non_floor)
        radius = max(
            max(math.hypot(item.x - cx, item.y - cy) + item.radius for item in non_floor),
            0.25,
        )
        height = max(item.height for item in non_floor)
        return SubjectBounds("subject_cluster", cx, cy, cz, radius, height)
    focus = authoring.get("camera_focus_target", {})
    return SubjectBounds(
        "camera_focus_target",
        float(focus.get("x", 0.0)),
        float(focus.get("y", 0.0)),
        float(focus.get("z", 1.0)),
        0.75,
        1.5,
    )


def normalize_xy(x: float, y: float, fallback: Tuple[float, float] = (0.0, -1.0)) -> Tuple[float, float]:
    length = math.hypot(x, y)
    if length < 1e-9:
        return fallback
    return x / length, y / length


def first_path_point(path: dict) -> Optional[Tuple[float, float]]:
    points = path.get("points", []) if isinstance(path, dict) else []
    if not points:
        return None
    point = points[0]
    return (float(point.get("x", 0.0)), float(point.get("y", 0.0)))


def resolve_front_vector(authoring: dict, subject: SubjectBounds, policy: dict) -> Tuple[float, float, float]:
    front_reference = policy.get("front_reference", "camera")
    if front_reference == "explicit_vector":
        vector = policy.get("front_vector") or policy.get("explicit_vector") or {}
        vx, vy = normalize_xy(float(vector.get("x", 0.0)), float(vector.get("y", -1.0)))
    elif front_reference == "object_forward":
        vector = policy.get("object_forward") or {}
        vx, vy = normalize_xy(float(vector.get("x", 0.0)), float(vector.get("y", -1.0)))
    else:
        camera = first_path_point(authoring.get("camera_path", {}))
        if camera is None:
            camera = (subject.x, subject.y - 4.0)
        vx, vy = normalize_xy(camera[0] - subject.x, camera[1] - subject.y)
    return vx, vy, math.atan2(vy, vx)


def lighting_policy_bool(policy: dict, key: str, default: bool) -> bool:
    value = policy.get(key)
    if value is None:
        return default
    return bool(value)


def ambient_policy_defaults(mode: str) -> dict:
    if mode == "none":
        return {"mode": "none", "ambient_strength": 0.0, "top_fill_strength": 0.0}
    if mode == "transparent_fill":
        return {"mode": "transparent_fill", "ambient_strength": 0.42, "top_fill_strength": 2.2}
    if mode == "studio":
        return {"mode": "studio", "ambient_strength": 0.32, "top_fill_strength": 1.6}
    return {"mode": "review_fill", "ambient_strength": 0.28, "top_fill_strength": 1.2}


def apply_ambient_policy(authoring: dict, ambient_policy: Optional[dict], required_mode: Optional[str] = None) -> dict:
    policy = dict(ambient_policy or {})
    mode = policy.get("mode") or required_mode or "none"
    if required_mode and mode == "none":
        mode = required_mode
    normalized = ambient_policy_defaults(mode)
    normalized.update(policy)
    normalized["mode"] = mode
    ambient_strength = clamp(float(normalized.get("ambient_strength", 0.0)), 0.0, 1.0)
    top_fill_strength = clamp(float(normalized.get("top_fill_strength", 0.0)), 0.0, 20.0)
    environment_brightness = normalized.get("environment_brightness")
    environment = authoring.setdefault("environment", {})
    if mode == "none":
        environment["light_mode"] = 0
        environment["ambient_strength"] = 0.0
        environment["top_fill_strength"] = top_fill_strength
    else:
        environment["light_mode"] = 2 if ambient_strength > 0.0 else 1
        environment["ambient_strength"] = ambient_strength
        environment["top_fill_strength"] = top_fill_strength
    if environment_brightness is not None:
        environment["ambient_brightness"] = clamp(float(environment_brightness), 0.0, 255.0)
    normalized["ambient_strength"] = ambient_strength
    normalized["top_fill_strength"] = top_fill_strength
    authoring["ambient_policy"] = normalized
    return normalized


def make_path_point(x: float,
                    y: float,
                    rotation: float = 0.0,
                    vx1: float = 0.0,
                    vy1: float = 0.0,
                    vx2: float = 0.0,
                    vy2: float = 0.0) -> dict:
    point = {
        "x": round(x, 4),
        "y": round(y, 4),
        "rotation": round(rotation, 6),
        "handleLink": False,
    }
    if abs(vx1) > 1e-9 or abs(vy1) > 1e-9:
        point["velocity1"] = {"vx": round(vx1, 4), "vy": round(vy1, 4)}
    if abs(vx2) > 1e-9 or abs(vy2) > 1e-9:
        point["velocity2"] = {"vx": round(vx2, 4), "vy": round(vy2, 4)}
    return point


def emit_polyline_path(points: List[Tuple[float, float]]) -> dict:
    out = []
    for idx, (x, y) in enumerate(points):
        if idx == 0 and len(points) > 1:
            nx, ny = points[idx + 1]
            out.append(make_path_point(x, y, vx1=(nx - x) * 0.35, vy1=(ny - y) * 0.35))
        elif idx == len(points) - 1 and idx > 0:
            px, py = points[idx - 1]
            out.append(make_path_point(x, y, vx2=-(x - px) * 0.35, vy2=-(y - py) * 0.35))
        elif idx > 0 and idx < len(points) - 1:
            px, py = points[idx - 1]
            nx, ny = points[idx + 1]
            out.append(
                make_path_point(
                    x,
                    y,
                    vx1=(nx - x) * 0.25,
                    vy1=(ny - y) * 0.25,
                    vx2=-(x - px) * 0.25,
                    vy2=-(y - py) * 0.25,
                )
            )
        else:
            out.append(make_path_point(x, y))
    return {"mode": "BEZIER_CUBIC", "points": out}


def angle_points(subject: SubjectBounds,
                 front_angle: float,
                 radius: float,
                 offsets: List[float]) -> List[Tuple[float, float]]:
    points = []
    for offset in offsets:
        angle = front_angle + offset
        points.append((subject.x + math.cos(angle) * radius, subject.y + math.sin(angle) * radius))
    return points


def z_values_for_policy(subject: SubjectBounds,
                        policy: dict,
                        mode: str,
                        point_count: int) -> List[float]:
    height_mode = policy.get("height_mode")
    if mode == "front_corkscrew":
        height_mode = "corkscrew"
    elif mode in ("front_vertical_sweep",):
        height_mode = "vertical_sweep"
    elif mode == "high_shadow_orbit" and height_mode is None:
        height_mode = "above_object"
    elif height_mode is None:
        height_mode = "object_height"
    base = subject.z + subject.height * 0.75
    if height_mode == "above_object":
        z = subject.z + max(subject.height * 1.8, 2.0)
        return [z for _ in range(point_count)]
    if height_mode == "vertical_sweep":
        vertical_range = float(policy.get("vertical_range", max(0.8, subject.height * 0.8)))
        low = subject.z + subject.height * 0.35
        high = low + vertical_range
        if point_count <= 1:
            return [high]
        return [low + (high - low) * (idx / float(point_count - 1)) for idx in range(point_count)]
    if height_mode == "corkscrew":
        vertical_range = float(policy.get("vertical_range", max(0.6, subject.height * 0.65)))
        mid = subject.z + subject.height * 0.8
        return [
            mid + math.sin(idx * math.pi * 0.85) * vertical_range * 0.5
            for idx in range(point_count)
        ]
    return [base for _ in range(point_count)]


def emit_depth_path(z_values: List[float]) -> dict:
    points = []
    for idx, z in enumerate(z_values):
        point = {"z": round(z, 4)}
        if idx == 0 and len(z_values) > 1:
            point["velocity1"] = {"vz": round((z_values[idx + 1] - z) * 0.35, 4)}
        elif idx == len(z_values) - 1 and idx > 0:
            point["velocity2"] = {"vz": round(-(z - z_values[idx - 1]) * 0.35, 4)}
        elif idx > 0 and idx < len(z_values) - 1:
            point["velocity1"] = {"vz": round((z_values[idx + 1] - z) * 0.25, 4)}
            point["velocity2"] = {"vz": round(-(z - z_values[idx - 1]) * 0.25, 4)}
        points.append(point)
    return {"points": points}


def build_lighting_context(request: dict, authoring: dict, policy: dict, ambient_mode: str) -> LightingContext:
    subject = resolve_subject_bounds(request, authoring, policy)
    front_x, front_y, front_angle = resolve_front_vector(authoring, subject, policy)
    radius_scale = float(policy.get("radius_scale", 2.3))
    radius = max(0.7, subject.radius * radius_scale)
    front_limit = clamp(float(policy.get("front_hemisphere_degrees", 150.0)), 20.0, 180.0)
    allow_backlight = lighting_policy_bool(policy, "allow_backlight", False)
    return LightingContext(subject, front_x, front_y, front_angle, radius, front_limit, allow_backlight, ambient_mode)


def signed_front_angle_degrees(point: Tuple[float, float], context: LightingContext) -> float:
    dx = point[0] - context.subject.x
    dy = point[1] - context.subject.y
    vx, vy = normalize_xy(dx, dy)
    dot = clamp(vx * context.front_x + vy * context.front_y, -1.0, 1.0)
    return math.degrees(math.acos(dot))


def synthesize_lighting_policy(request: dict) -> dict:
    authoring = request.setdefault("extensions", {}).setdefault("ray_tracing", {}).setdefault("authoring", {})
    policy = authoring.get("lighting_policy")
    if not isinstance(policy, dict):
        return {"applied": False, "reason": "no_lighting_policy"}

    mode = policy.get("mode", "front_key_orbit")
    ambient_required = None
    if mode == "transparent_review":
        ambient_required = "transparent_fill"
    elif mode == "rim_light" and lighting_policy_bool(policy, "ambient_required_for_backlight", True):
        ambient_required = "review_fill"
    ambient = apply_ambient_policy(authoring, authoring.get("ambient_policy"), ambient_required)
    context = build_lighting_context(request, authoring, policy, ambient.get("mode", "none"))

    revolutions = max(0.1, float(policy.get("revolutions", 1.0)))
    front_half = math.radians(context.front_limit_degrees * 0.5)
    if mode == "front_corkscrew":
        count = max(5, int(math.ceil(revolutions * 6.0)))
        offsets = [
            math.sin(idx * math.pi * 2.0 / max(1.0, count - 1.0) * revolutions) * front_half * 0.75
            for idx in range(count)
        ]
    elif mode == "front_vertical_sweep":
        offsets = [0.0, 0.0, 0.0]
    elif mode in ("full_orbit", "fixed_height_orbit", "high_shadow_orbit"):
        count = max(5, int(math.ceil(revolutions * 8.0)) + 1)
        offsets = [math.pi * 2.0 * revolutions * idx / float(count - 1) for idx in range(count)]
    elif mode == "rim_light":
        back_angle = context.front_angle + math.pi
        offsets = [
            back_angle - context.front_angle - math.radians(28.0),
            back_angle - context.front_angle,
            back_angle - context.front_angle + math.radians(28.0),
        ]
    else:
        offsets = [-front_half * 0.85, -front_half * 0.35, 0.0, front_half * 0.35, front_half * 0.85]

    if mode == "high_shadow_orbit":
        radius = context.radius * 1.25
    elif mode == "front_corkscrew":
        radius = context.radius * 0.78
    else:
        radius = context.radius
    points = angle_points(context.subject, context.front_angle, radius, offsets)
    z_values = z_values_for_policy(context.subject, policy, mode, len(points))

    authoring["light_path"] = emit_polyline_path(points)
    authoring["light_path_depth"] = emit_depth_path(z_values)

    first_angle = signed_front_angle_degrees(points[0], context)
    hemisphere_violations = [
        idx
        for idx, point in enumerate(points)
        if signed_front_angle_degrees(point, context) > context.front_limit_degrees * 0.5 + 1e-5
    ]
    front_biased = mode in ("front_key_orbit", "front_corkscrew", "front_vertical_sweep", "transparent_review")
    if front_biased and not context.allow_backlight:
        assert first_angle <= 90.0 + 1e-5
        assert not hemisphere_violations

    return {
        "applied": True,
        "mode": mode,
        "subject": {
            "object_id": context.subject.object_id,
            "x": round(context.subject.x, 4),
            "y": round(context.subject.y, 4),
            "z": round(context.subject.z, 4),
            "radius": round(context.subject.radius, 4),
            "height": round(context.subject.height, 4),
        },
        "front_vector": {"x": round(context.front_x, 4), "y": round(context.front_y, 4)},
        "radius": round(radius, 4),
        "first_camera_side_degrees": round(first_angle, 4),
        "front_hemisphere_violations": hemisphere_violations,
        "ambient_policy": ambient,
        "path_point_count": len(points),
    }


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
    policy_result = synthesize_lighting_policy(request)
    if policy_result.get("applied"):
        return policy_result
    floor = find_floor_bounds(request)
    if not floor or not transparent_footprints:
        return policy_result
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
