#!/usr/bin/env python3
import copy
import importlib.util
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REFINE_PATH = ROOT / "tools" / "agent_scene_refine.py"
FIXTURE_PATH = ROOT / "tests" / "fixtures" / "agent_tiny_skull_lighting_policy_request.json"


def load_refine_module():
    spec = importlib.util.spec_from_file_location("agent_scene_refine", REFINE_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


refine = load_refine_module()


def authoring_of(request):
    return request["extensions"]["ray_tracing"]["authoring"]


def subject_and_front(authoring):
    subject = authoring["lighting_policy"].get("subject_object_id", "tiny_skull_proxy")
    assert subject == "tiny_skull_proxy"
    sx = 0.0
    sy = 0.0
    camera = authoring["camera_path"]["points"][0]
    fx = camera["x"] - sx
    fy = camera["y"] - sy
    length = math.hypot(fx, fy)
    return sx, sy, fx / length, fy / length


def max_front_angle_degrees(points, sx, sy, fx, fy):
    max_angle = 0.0
    for point in points:
        dx = point["x"] - sx
        dy = point["y"] - sy
        length = math.hypot(dx, dy)
        assert length > 0.0
        dot = max(-1.0, min(1.0, (dx / length) * fx + (dy / length) * fy))
        max_angle = max(max_angle, math.degrees(math.acos(dot)))
    return max_angle


def refine_with_policy(mode, **overrides):
    request = json.loads(FIXTURE_PATH.read_text())
    authoring = authoring_of(request)
    authoring["lighting_policy"]["mode"] = mode
    authoring["lighting_policy"].update(overrides)
    refined, report = refine.refine_request(request)
    return refined, report


def test_front_key_orbit_starts_camera_side_and_stays_front():
    refined, report = refine_with_policy("front_key_orbit")
    authoring = authoring_of(refined)
    points = authoring["light_path"]["points"]
    sx, sy, fx, fy = subject_and_front(authoring)
    first = points[0]
    assert (first["x"] - sx) * fx + (first["y"] - sy) * fy > 0.0
    assert max_front_angle_degrees(points, sx, sy, fx, fy) <= 75.01
    assert report["light_path"]["front_hemisphere_violations"] == []
    assert authoring["environment"]["light_mode"] == 0


def test_full_orbit_completes_360_degrees():
    refined, report = refine_with_policy("full_orbit", allow_backlight=True)
    points = authoring_of(refined)["light_path"]["points"]
    assert report["light_path"]["mode"] == "full_orbit"
    assert len(points) >= 9
    assert abs(points[0]["x"] - points[-1]["x"]) < 1e-4
    assert abs(points[0]["y"] - points[-1]["y"]) < 1e-4


def test_corkscrew_moves_vertically_while_staying_front():
    refined, _ = refine_with_policy("front_corkscrew", revolutions=1.5, vertical_range=1.0)
    authoring = authoring_of(refined)
    z_values = [point["z"] for point in authoring["light_path_depth"]["points"]]
    assert max(z_values) - min(z_values) > 0.4
    sx, sy, fx, fy = subject_and_front(authoring)
    assert max_front_angle_degrees(authoring["light_path"]["points"], sx, sy, fx, fy) <= 75.01


def test_rim_light_requires_review_fill_by_default():
    refined, _ = refine_with_policy("rim_light")
    authoring = authoring_of(refined)
    points = authoring["light_path"]["points"]
    sx, sy, fx, fy = subject_and_front(authoring)
    first = points[0]
    assert (first["x"] - sx) * fx + (first["y"] - sy) * fy < 0.0
    assert authoring["ambient_policy"]["mode"] == "review_fill"
    assert authoring["environment"]["light_mode"] == 2
    assert authoring["environment"]["ambient_strength"] > 0.0


def test_transparent_review_gets_transparent_fill():
    refined, _ = refine_with_policy("transparent_review")
    authoring = authoring_of(refined)
    assert authoring["ambient_policy"]["mode"] == "transparent_fill"
    assert authoring["environment"]["ambient_strength"] >= 0.4
    assert authoring["environment"]["top_fill_strength"] >= 2.0


def main():
    tests = [
        test_front_key_orbit_starts_camera_side_and_stays_front,
        test_full_orbit_completes_360_degrees,
        test_corkscrew_moves_vertically_while_staying_front,
        test_rim_light_requires_review_fill_by_default,
        test_transparent_review_gets_transparent_fill,
    ]
    for test in tests:
        test()
    print("[agent-scene-refine-lighting-policy] PASS")


if __name__ == "__main__":
    main()
