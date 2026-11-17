# Complete chassis-proxy body inventory: local 0.18.0

Build and all eleven Release CTest suites pass. Not installed; game retains 0.17.0. Native enumeration is pending runtime validation.

ReadVerifiedProxyBounds resolves and holds a temporary generation-checked shared proxy reference for the read. Resolution and release use exact guarded game functions 0x27d430 and 0x27d374, with manager slot 0x3bfcdd0 corroborated by the guarded owner getter. An RAII cleanup releases the native reference on every exit.

Only reviewed reader vtables are accepted. Reader 0x614568 checks body index against proxy+0x5c and reads the actor array at +0x50. Reader 0x614618 uses a list header at proxy+0x40, with actor array at header+0 and count at header+0xc. Unknown layouts, zero counts, counts above 64 and null body entries fail closed. The code reads state 35 for every explicit index, validates all bounds, and unions them. No first-failed-read heuristic determines completeness.

The list header, pointer, count and actor pointer inventory are compared before/after reading. An observable rebuild invalidates the result. Holding a proxy reference prevents proxy destruction, but does not itself lock the body's simulation state. Sequential reads and before/after comparison are not an atomic snapshot or proof against transient ABA changes. The result remains diagnostic and does not authorize ConfirmClear.

BodyBoundsInventory tests cover unordered complete enumeration, duplicate/missing/out-of-range entries, malformed bounds, zero/over-cap counts, the 64-body boundary, and a counterexample where body 0 is clear while a second body overlaps.

The tracking diagnostic now logs bodies=A:B and inventory=proxy-all when it compares complete supported-proxy inventories. Its read failure remains unknown. This is full coverage of the declared bodies in that proxy, not every physical object associated with the entity. Separate chassis/attachment/trailer proxies still need mapping before a whole-vehicle claim. The runtime registry and motion/synchronization safeguards remain outstanding.

No standalone game test requested yet; combine this binding check with the next integration test.

Runtime follow-up: 0.19.0 validated complete inventories with one body per proxy in the sampled pair. See reusable-runtime.md; multi-body coverage is still untested.
