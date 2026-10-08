"""Offline timing, orientation, preservation and create-only output contract."""
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

tool = str(Path(sys.argv[1]).resolve())
repo = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="camera-export-test-") as tmp:
    root = Path(tmp)
    doc = json.loads((repo / "config/examples/van_construction_f4.layout.json").read_text())
    camera = doc["sceneAuthoring"]["cameras"][0]
    camera["pathId"] = "test_path"
    camera["orientationMode"] = "per_point"
    path = {
        "pathId": "test_path", "label": "Timing test", "role": "camera",
        "curveType": "linear", "boundCameraId": camera["cameraId"],
        "boundLightId": "", "closed": False, "playbackMode": "once",
        "durationSeconds": 9, "normalizedDistance": 0, "playing": False,
        "controlPoints": [{"x": 0, "y": 0, "z": 1.6},
                          {"x": 2, "y": 0, "z": 1.6},
                          {"x": 2, "y": 0, "z": 1.6}],
        "tangentModes": [],
        "cameraPoses": {"version": 1, "next_id": 3,
                        "orientation": "quaternion_wxyz_x_forward_z_up",
                        "keys": [
                            {"id": "a", "rotation": [1, 0, 0, 0], "fov": 60, "seconds": 2},
                            {"id": "b", "rotation": [math.sqrt(.5), 0, 0, math.sqrt(.5)], "fov": 80, "seconds": 1},
                            {"id": "c", "rotation": [0, 0, 0, 1], "fov": 80, "seconds": 7}]}}
    doc["sceneAuthoring"]["cameras"] = [camera]
    doc["sceneAuthoring"]["paths"] = [path]
    source = root / "input.json"
    source.write_text(json.dumps(doc))
    digest = hashlib.sha256(source.read_bytes()).digest()

    def run(out, fps="2", path_id="test_path"):
        return subprocess.run([tool, str(source), path_id, str(out), "camera_test", fps, "960", "540"],
                              capture_output=True, text=True)

    a, b = root / "a", root / "b"
    first = run(a)
    assert first.returncode == 0, first.stderr
    assert run(b).returncode == 0
    for name in ["camera_samples.json", "scene_authoring.json", "scene_runtime.json"]:
        assert (a / name).read_bytes() == (b / name).read_bytes(), name
    samples = json.loads((a / "camera_samples.json").read_text())
    assert samples["duration_seconds"] == 3 and len(samples["frames"]) == 7
    assert [x["seconds"] for x in samples["anchors"]] == [0, 2, 3]
    midpoint = samples["frames"][2]
    assert abs(midpoint["position"]["x"] - 1) < 1e-5
    assert abs(midpoint["forward"]["x"] - math.sqrt(.5)) < 1e-5
    assert abs(midpoint["forward"]["y"] - math.sqrt(.5)) < 1e-5
    assert midpoint["vertical_fov_degrees"] == 70
    turning = samples["frames"][5]
    assert turning["position"]["x"] == 2 and turning["position"]["y"] == 0
    assert abs(turning["forward"]["x"] + math.sqrt(.5)) < 1e-5
    assert abs(samples["frames"][-1]["forward"]["x"] + 1) < 1e-5
    for frame in samples["frames"]:
        f, u = frame["forward"], frame["up"]
        assert abs(sum(f[k]*u[k] for k in f)) < 1e-5
        assert abs(sum(f[k]**2 for k in f)-1) < 1e-5
    before = {str(p.relative_to(a)): p.read_bytes() for p in a.rglob("*") if p.is_file()}
    assert run(a).returncode != 0
    assert before == {str(p.relative_to(a)): p.read_bytes() for p in a.rglob("*") if p.is_file()}
    assert run(root / "bad_fps", "nan").returncode != 0
    assert run(root / "missing", path_id="unknown").returncode != 0
    assert not (root / "bad_fps").exists() and not (root / "missing").exists()
    assert hashlib.sha256(source.read_bytes()).digest() == digest
print("Camera export determinism, timed pose, stationary turn, endpoint and refusal checks passed")
