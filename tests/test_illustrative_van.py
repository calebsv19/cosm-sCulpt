#!/usr/bin/env python3
"""Exercise the native teaching document, including safe regeneration/refusals."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
tool = Path(sys.argv[1]).resolve()


def run(*args, success=True):
    result = subprocess.run([str(tool), *map(str, args)], capture_output=True, text=True)
    assert (result.returncode == 0) == success, result.stderr
    return result


with tempfile.TemporaryDirectory(prefix="ld-van-smoke-") as directory:
    first, second = (Path(directory) / name for name in ("first.json", "second.json"))
    audit = json.loads(run("--example-van", first).stdout)
    run("--example-van", second)
    baseline = first.read_bytes()
    assert baseline == second.read_bytes(), "Native example regeneration is not deterministic"
    assert baseline == (root / "config/examples/illustrative_van.layout.json").read_bytes()
    document = json.loads(baseline)
    engineering = document["engineering"]
    assert document["physicalContext"] == {
        "coordinateSystem": "right_handed_z_up_meters", "metersPerWorldUnit": 1
    }
    assert len(document["objects3d"]) == 38
    assert len(engineering["assemblies"]) == 8
    assert len(engineering["relationships"]) == 7
    assert len(document["geometricConstraints"]) == 4
    assert len(engineering["spatialChecks"]) == 3
    assert len(engineering["motionEnvelopes"]) == 2
    assert audit["dimensionsStatus"] == "illustrative_not_measured"
    assert [item["position"] for item in audit["exampleRangeChecks"]] == [1.6, 45]
    assert all(item["status"] == "contact_at_tested_pose" for item in audit["exampleRangeChecks"])
    assert all(item["status"] == "separated_range" for item in audit["resolvedExerciseChecks"])
    assert len(audit["resolvedSpatialReport"]["results"]) == 3
    assert all(item["severity"] == "pass" for item in audit["resolvedSpatialReport"]["results"])
    assert [(item["source"], item["target"]) for item in audit["results"] if item["severity"] == "error"] == [
        ("walkway", "door_obstruction")
    ]
    run("--check-layout", first)
    assert first.read_bytes() == baseline, "Read-only audit modified the layout"
    first.write_text("user edits must survive\n")
    run("--example-van", first, success=False)
    assert first.read_text() == "user edits must survive\n"
    invalid = Path(directory) / "invalid.json"
    run("--example-van", invalid, "--check-layout", second, success=False)
    assert not invalid.exists(), "Conflicting operation wrote a document"
print("illustrative-van-smoke passed: deterministic native document, motion exercise solutions, read-only audit, overwrite refusal")
