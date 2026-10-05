# ProMaster measurement checklist

Working vehicle: likely 2023 Ram ProMaster 3500, 159-inch wheelbase, extended body,
High Roof. Confirm model year and configuration from the vehicle before treating
this profile as confirmed. Record measurements in millimeters; the scene stores meters.

Use the same datum for every reading: choose and photograph the rear cargo-floor
threshold center. Record forward distance from it, passenger-side distance from
center, and height above the bare cargo floor. The current scene is centered on
its nominal floor outline, with +X passenger, +Y front and +Z up. Converting a
rear-datum measurement to that scene requires `Y = forward_mm / 1000 - 2.0485`;
that 2.0485 m offset is provisional until the actual floor length is established.
Do not mix rear-door outer skin, threshold and inner closed-door faces as origins.

For each row, record **value, exact endpoints, method, estimated uncertainty,
photo/reference, date and bare/finished state**. Mark missing readings unknown.

| Priority | Measurement | Where / why | Value mm |
|---|---|---|---|
| 1 | Clear cargo length at floor | Closed rear-door inner face to usable front limit; note seats/partition position | |
| 1 | Clear length at desk and bed heights | Same rear datum, record front obstruction at each height | |
| 1 | Width at floor | Front, middle and rear; distinguish ribs from sheet metal | |
| 1 | Width at desk height | Front/middle/rear of the proposed rear desk zone | |
| 1 | Width at lowered bed height | Both bed ends and midpoint, across steel/ribs; proposed deck center 950 mm | |
| 1 | Width at raised bed height | Both bed ends and midpoint; proposed deck center 1650 mm | |
| 1 | Minimum height over bed | Floor to lowest rib/roof obstruction at corners and midpoint | |
| 1 | Wheel-well gap | Narrowest inner-face distance, not nominal full floor width | |
| 1 | Wheel-well position | Rear datum to trailing/leading edges on each side | |
| 1 | Wheel-well size/shape | Length, height and inward projection; photograph contour | |
| 1 | Sliding-door location | Rear datum to both opening edges | |
| 1 | Sliding-door clear opening | Width, height, threshold height and header projection | |
| 1 | Rear-door clear opening | Width/height and closing hardware intrusions | |
| 2 | Upper chase clearance | Width/height at proposed chase level, both sides and rear crossover | |
| 2 | OEM ribs and mounting holes | XYZ positions, hole sizes and photographs; support suitability is separate | |
| 2 | Finished floor stack | Subfloor, insulation and finish thickness above bare floor | |
| 2 | Finished walls/ceiling | Furring, insulation and panels; local finished inner surface | |
| 2 | Bed platform/mattress | Actual footprint, total thickness and minimum sleeping headroom | |
| 2 | Lift hardware | Guide/drive footprint, mounting locations and actual permitted travel | |
| 2 | Battery/inverter/fuse enclosure | Actual product dimensions, terminal/access faces and service requirements | |
| 2 | Kitchen/tank/pump | Actual footprints, connectors and access requirements | |

The first useful measurement delivery is floor length, wheel-well geometry,
sliding-door location, and width/height at both bed positions. Those readings let
us replace the largest box approximations before refining furniture and routes.

Preserve entity IDs when updating geometry. Keep OEM nominal values alongside
vehicle measurements and their uncertainty; do not silently overwrite source
provenance. Add build layers separately, regenerate affected bed envelopes, and
rerun checks after dimensional corrections.

References: [published dimensional basis](promaster_2023_dimension_basis.md),
[corrected narrow scene](../config/examples/van_layout_promaster_2023_narrow_v2.layout.json).
