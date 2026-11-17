# Contact-registration flag names (2026-10-29)

The installed executable supplies explicit property names for both high bits in simulation mask2:

| Bit | Mask | Engine property |
| --- | --- | --- |
| 63 | 0x8000000000000000 | registerForContacts |
| 62 | 0x4000000000000000 | registerForPersistentContacts |

## Evidence chain

The property serializer at RVA 0x26ce0ac reads FilterData +0x48 (simulation mask2). Its bit-63 branch compares the property identifier stored at RVA 0x3f2abc8; its bit-62 branch compares RVA 0x3f2abd0. Those runtime identifiers are initialized as follows:

- Function 0x26cdad4 registers hash 0x76ee071abfff5ede with string RVA 0x302ef48, registerForContacts, and stores it at 0x3f2abc8.
- Function 0x26cda08 registers hash 0x4e86d4d5ec6924fb with string RVA 0x35a15e0, registerForPersistentContacts, and stores it at 0x3f2abd0.

The companion setter at 0x26ce318 matches the same identifiers, parses boolean values and calls 0xfee37c on FilterData +0x40 with selector 0 or 1 respectively. This corroborates the property-to-bit mapping. The earlier mask2 update helper preserves both constants.

Evidence files: filter-flag-name-refs.json, filter-flag-names.json, filter-shift-candidates.json under artifacts/native-research. extract-filter-flag-names.py reproduces string extraction; find-filter-flag-names.py traces initializer references. Slot values are runtime initialized, so absent on-disk data cannot be treated as the live identifier value.

## Consequences and limits

The flags have engine-provided contact-registration names, not collision-group names. Preserve them. Names alone do not prove the exact notification lifecycle or which PhysX pair flags they control. The simulation contact-pair callback and its mask combination remain unverified; the broad callback-ABI and mask instruction searches did not establish a valid binding. In particular, do not assume clearing Vehicle from one contact mask alone prevents vehicle contact, or clear the Vehicle category without verifying retained world contact.

The saved 0.8.0 scans are now annotated with these flag names. Every observed non-bottom shape has registerForContacts=true and registerForPersistentContacts=false; bottom-marked shapes have both false. This is correlation for the two tested vehicles, not a universal shape-role rule.

Restoration tests now cover all four combinations of the two flags and preserve all 128 filter bits. CTest passes. Installed 0.8.0 remains unchanged; no gameplay mutation or extra game test is required for this offline milestone.

Next investigation should anchor the callback through scene creation/registration or concrete PhysX dispatch rather than treating instruction-pattern matches as identity. Safe physics scheduling and contact refresh remain required before an engine write adapter is enabled.
