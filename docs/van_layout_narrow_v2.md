# Narrow ProMaster layout correction v2

The user's 2026-10-04 correction supersedes the near-square bed/furniture layout
in the concept and nominal v1 review scenes. Those old files remain historical
snapshots, not the intended furniture plan.

The bed is interpreted as transverse: sleeping length along X (driver to passenger),
width along Y (front to rear), with vertical Z travel. The initial deck is
**1800 × 820 × 60 mm** and the mattress placeholder is **1760 × 780 × 120 mm**.
The bed width limit is **914.4 mm (3 feet)**. Transverse sleeping length is still an
unmeasured assumption: measure finished wall/rib clearances at both bed heights.

The section plan reserves approximately four feet at the entry followed by three
sections of about three feet. For this draft the usable planning span is **3880 mm**:
4 feet entry plus three **886.933 mm** sections. This is a provisional fit assumption,
not a measured vehicle dimension. It differs from the published maximum floor
length (4097 mm) and sits just below the older published beltline length (3889 mm).
Those source dimensions and OEM box proxies remain unchanged.

| Front to rear | Length | World Y boundaries (front → rear) | Contents |
| --- | --- | --- | --- |
| Entry | 1219.2 mm / 4 ft | +1831.5 → +612.3 mm | Passenger entry, driver electrical/battery bench |
| Kitchen / lounge | 886.933 mm / about 2 ft 11 in | +612.3 → −274.633 mm | Passenger kitchen, continuous driver lounge |
| Tall storage | 886.933 mm | −274.633 → −1161.567 mm | Driver wardrobe, passenger pantry |
| Work / narrow bed | 886.933 mm | −1161.567 → −2048.5 mm | Paired desks beneath one narrow transverse lift bed |

Furniture, guides, service reservations and the bed datum were repositioned together.
The driver battery bench stays opposite the passenger sliding door. Passenger upper
storage stops behind the entry; shallow storage above the header remains separate.
The bed assembly still travels through deck-center Z=950–1650 mm. Its envelope was
regenerated with 33 samples; all twelve saved obstruction checks pass on this proxy
scene. That is geometric consistency, not strength, verified furniture access or
confirmation that lift hardware fits the real vehicle.

Files:

- [Corrected dimensional scene input](../config/concepts/van_layout_promaster_2023_narrow_v2.json)
- [Corrected native scene](../config/examples/van_layout_promaster_2023_narrow_v2.layout.json)
- [Corrected scene with manual route exercise](../config/examples/van_routing_s5a.layout.json)
- [Top view](assets/van-narrow-v2.svg)
- [Measurement checklist](van_measurement_checklist.md)

The native scene uses existing stable IDs and motion/relationship/check structures.
It is delivered as a separate project to preserve prior files and manual edits.
