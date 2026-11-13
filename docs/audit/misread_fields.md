# Audit: misread fields

Audit class 3 of the port audit playbook (2026-11-13): members whose offset lies inside another member of the same class, or of one of its base classes. Such an overlap in a layout table is the usual trace of a field read at the wrong offset or with the wrong width.

## How it was made

`tools/field_overlap.py` (new) over `docs/fields.csv` (the 1,639 fields the game registers, `tools/exe_classes.py`: offset, size, encoding, bit mask) and `docs/classes.csv` (parent and size of the 495 classes). It reports two fields of a class whose byte ranges overlap (`inside`), a field overlapping a field of a base class that is not the same field registered again (`base`), a field inside the nearest base's size that overlaps no registered base field (`base-part`: the derived class registering bytes the base owns) and a field past its class's size (`beyond`). Bits of one byte or dword with disjoint masks are no overlap. 76 fields have no offset in the table (embedded objects, `emb`, and encodings named by a function) and take no part [limit of the table].

First run: 10 `inside`, 1 `base`, 15 `base-part`. After the flags kind was taught to the tool (below): 15 `base-part`, nothing else. `python3 -I tools/field_overlap.py --csv OUT` lists them.

## Hits

| Original address | What | Verdict | Location / fix |
|---|---|---|---|
| CLayerPart RegisterFields `0x4f9c50`; flags kind `0x5ec684` (`CFlagsFieldDesc::Mask` `0x414dd0`, `FlagsField_FromText` `0x414fa0`) | `CLayerPart.Category` (+0x1c, 4 bytes, "str" in the table) against the ten flag bits at +0x1c (Visible 0x20000 … Full Height 0x8000000) and `CEntityCharacter.Weapons Use Amo` (+0x1c, 0x10000000): 11 hits | **no misread** [verified]: Category is a field of the flags kind, whose mask slot returns `0xffff` and whose text setter stores `old ^ ((old ^ new) & 0xffff)` (setter 0 in the registration); the bits sit at 0x10000 and up. The exe's other readers mask too (`dw(part, 0x1c) & 0xffff` in `reimpl/src/random_misc.cpp`, `world_rest.cpp`, as their exe functions). `Weapons Use Amo` 0x10000000 is a bit no CLayerPart field uses | `tools/field_overlap.py` `FLAG_MASKS` (Category and Used In as 16-bit masks). Nothing to fix |
| CParticleSystemAI constructor `0x52dd20`, CGenericParticleSystemAI RegisterFields `0x52f220` | 15 fields of `CGenericParticleSystemAI` (+0xa0 … +0x12c) inside `CParticleSystemAI` (0x134 bytes) | **no misread** [confirmed: `docs/DECOMPILATION.md` "Layouts" of the particle classes, constructors]: `CParticleSystemAI` is abstract and registers no fields; the derived class registers the base's members. Every offset matches the base layout: six embedded `CEnvelope`s of 0x24 bytes at +0x10/+0x34/+0x58/+0x7c/+0xa8/+0xd0 leave exactly +0xa0/+0xa4 (burst frequency, variance), +0xcc (speed variance) and +0xf4–+0x10b for the scalar fields; the `arr` fields' offsets +0x11c/+0x12c are the data pointers (+8) of the pointer arrays at +0x114/+0x124 | Nothing to fix |

## What stays open

- The 76 fields without an offset (embedded objects) are outside this check; their places are in the "Layouts" paragraphs of `docs/DECOMPILATION.md`, not in `docs/fields.csv`. For `CGenericParticleSystemAI` they were checked by hand (above).
- This check covers the registered fields only. Members no field names (the bulk of every class) are known from the code; the static audit (`tools/static_audit.py`) compares each function's field offsets with the exe's, and its review (2026-11-11) found no misread offset among its five real deviations [confirmed: `docs/DECOMPILATION.md`, "The static audit"].
