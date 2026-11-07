# Bugs and questionable behaviour of the original Zax.exe

As of 2026-10-31 (begun 2026-10-28). A collection of every bug or questionable behaviour of the original Zax.exe (build 1308) that the analysis and the decompilation found. Nothing here is new research: every entry is taken from `docs/DECOMPILATION.md`, `zax-formats/docs/FORMATS.md`, `docs/EXE_FINDINGS.md`, `zax-formats/docs/OPEN_ISSUES.md` or the comments in `reimpl/src`, with the evidence level the source gives (never upgraded; since 2026-10-29 an evidence pass checked the entries without a tag of their own and the group 5 reports against the reimplementation's source and `docs/reimpl.csv` and upgraded, corrected or moved them, as each entry says). Where the source's wording is the only description, it is quoted. Details, the checks and the runs are in the named source section.

## How to read this file

- **Group 5** (since 2026-10-29) holds reports from outside this project that are **not verified**: neither our reimplementation's source nor a VM run backs them yet. Entries move into groups 1–4 only once our source (or a VM comparison) confirms them; until then they are leads, not findings.
- **Groups**: 1. visible in the game (reached with the game's own data or settings and with a described effect); 2. crash, hang or garbage on unusual data (relevant for modding; also the deliberate fatal checks a modder can run into); 3. dormant (the sources say it is not reached with the game's data or settings, or only in code the release does not run); 4. harmless oddities, typos, leaks, and oddities whose effect in the game the sources do not describe.
- **Evidence**: the tag of the source. "No tag of its own" means the source lists the quirk without a tag inside a section whose heading says the reimplementation was compared with the original in the VM (`[verified …, VM]`); the checks there compare both sides on the same inputs, so the behaviour is the original's, but the source gives the quirk no level of its own.
- **Fix**: whether it is a candidate for an optional fix later (phase 7 or later, behind an opt-in `Fixes=1`, original behaviour by default). "No" where the behaviour is part of a file format, unreachable, or harmless.
- **Editor**: whether the level editor should warn about it when a modder's data would trigger it.
- Return values that only differ in the upper bytes of `eax`, FPU status words and similar ABI details are not listed; the sources document them per function ("Return values").

## The reimplementation keeps them on purpose

The reimplementation (`reimpl/`, `Zax-Reimpl.dll`) reproduces every one of these behaviours, crashes and hangs included, deliberately:

- **The in-process checks** run original and reimplementation side by side inside the running game on the same inputs and compare the results byte for byte (`docs/DECOMPILATION.md` sections 2 and 5). A "fixed" function would differ from the original on exactly the inputs that matter and hide real errors among intended differences. Where the original crashes or hangs, the checks leave those cases out instead.
- **Faithful first**: phase 5a/5b ended with the milestone "faithful and complete" (section 12), and phase 6, the standalone exe, stays faithful: it is checked by comparison runs against the original, and phase 7 (SDL3 and the port) uses it as its reference (section 3). Fixes come after that, opt-in.

**Not reproduced** (the exceptions the sources name):
- `ColorField_FromText` `0x46df60` (entry 2.8): "The colour's `FromText` reads red and green into uninitialised locals: a text without them leaves stack garbage in the colour. The reimplementation starts them at 0; a missing blue keeps the text pointer as in the original. This is the only behaviour that is not reproduced." (section 6, "Field values as text").
- Later sections and source comments note a few more places where the reimplementation starts an uninitialised value at 0 or does not reproduce a crash; they only differ when a read fails or on states the game does not produce: `0x4e96c0` "passes an uninitialised stack slot as the 'part hit' that the sweep writes on every hit; the reimplementation starts it at 0" and `0x4eaab0`'s upper channel bytes are 0 (section 7, "World update and object placement"); the binary readers `0x419810` and `0x418050` start their count at 0 ("uninitialised in the original", `reimpl/src/field_objects.cpp`) and `ObjectTable::ReadLocationBinary` `0x55c9b0` its dwords ("the original's are uninitialised stack, here 0", `reimpl/src/object_table.cpp`); the multiplayer game type: "Without a match state the original reads near address 0; the reimplementation does not reproduce that crash" (section 6, "Multiplayer game type base class and match state"); filter 10 keeps its histogram on the heap instead of overflowing the stack (entry 2.27).

---

## 1. Visible in the game

**1.1 Ground blend at 16 bit rounds down: dark blotches** — `0x541541`
- What: the plasma ground mixes two textures per channel as `A + floor((B − A) · (alpha >> 3) / 32)` (red, blue) and `(alpha >> 2) / 64` (green). "The rounding down makes every mixed pixel half a colour step darker on average (R/B ≈ 4, G ≈ 2 of 255)".
- When: every 16-bit frame with a plasma ground. Impact: "dark blotches appear in the game wherever the plasma falls just below a band boundary".
- Evidence: [verified 2026-10-19] (game screenshot of level 1). Source: `zax-formats/docs/FORMATS.md` 2.2, "Mixing at 16 bit".
- Fix: yes (visual, opt-in). Editor: no (the editor's renderer reproduces it on purpose).

**1.2 The 16-bit blend never reaches the source colour** — the Copy/Black Bias edge blend (`zax-formats/docs/FORMATS.md` 6, render mode)
- What: per channel essentially `d + ⌊(s − d) · a5 / 32⌋` (red, blue) and `d + ⌊(s − d) · a6 / 64⌋` (green), "so `w = 255` gives 31/32 and 63/64 of the difference, never the pure source; rounding is always down (towards −∞ for negative differences, except the blue borrow)"; a negative blue difference "borrows from the green field".
- When: every soft sprite edge at 16 bit. Impact: edges slightly darker, a blue borrow into green.
- Evidence: [confirmed: code `0x44bf70`, `0x433df0`, `0x434280`] (`zax-formats/docs/OPEN_ISSUES.md`, renderer limits). Source: `zax-formats/docs/FORMATS.md` 6.
- Fix: yes (visual, opt-in). Editor: no.

**1.3 Amplify Light drops the lowest green bit** — `0x434280`, `0x434310`, tables of `0x433df0`
- What: per channel the top five bits (green "drops its low bit"), `out = min(31, D + ⌊D · S / 31⌋)`, "so the lowest green bit of the result is always 0".
- When: every light sprite at 16 bit. Impact: a slightly coarser green in lit areas.
- Evidence: [confirmed: code] (`zax-formats/docs/OPEN_ISSUES.md`, renderer limits); R3 lists it under "Quirks kept [confirmed: code; the checks cover them]". Source: `zax-formats/docs/FORMATS.md` 6; `docs/DECOMPILATION.md` section 9, "Batch R3".
- Fix: yes (visual, opt-in). Editor: no.

**1.4 Overlap ignores the sprite's colours** — the Overlap spans (`0x446a10`, `0x446a30`)
- What: "The Overlap spans replace every destination pixel by the word of the ramp map `0x6bc64c` at its low byte and ignore the source and the alpha (so the effect of 'Overlap Blit' depends only on what lies under the sprite's set pixels)."
- When: every sprite layer drawn with Overlap. Impact: as quoted. Whether this is intended is not stated.
- Evidence: [confirmed: code; the checks cover them]. Source: section 9, "Batch R3".
- Fix: only if the intent is clarified. Editor: no.

**1.5 A multiplayer start forces 640×480 and writes it into Zax.CFG** — `Game_StartFromMenu` `0x5a2940` → `Display_Set640x480` `0x590f00`
- What: unless the screen is already 640×480, every multiplayer start sets 640×480 "and **writes 640×480 into `Zax.CFG`**".
- When: host, join by address, GameSpy "Join", `-connect`. Impact: "after a multiplayer game the resolution stays 640×480, also at the next start, until it is changed in the video options".
- Evidence: [confirmed: code `0x5a2940`, `0x590f00`] (not tested in the VM). Source: `zax-formats/docs/OPEN_ISSUES.md` item 17.
- Fix: yes (opt-in; relevant for phase 8, higher resolutions). Editor: no.

**1.6 `Disable Mini-Map` has no effect** — `CLayerSaveData` +0x168, registered at `0x4e645f`
- What: "the game shows the mini map also with `1` [verified], and no code reads the field [confirmed]".
- Evidence: [verified] / [confirmed]. Source: `zax-formats/docs/FORMATS.md` 5, mini map.
- Fix: yes (opt-in, if the field should work). Editor: yes (say that the field does nothing in the game).

**1.7 The companion's spot ignores the computed height** — `CCompanionAI` (`CompanionAI_PickSpot` `0x46ead0`), constant `0x5efeec`
- What: "the companion's spot has the leader's y + 1.0 (the constant `0x5efeec`) instead of the computed sine part (the original computes it and overwrites it)".
- When: every companion update. Impact: the companion's target spot is not spread around the leader as computed.
- Evidence: [confirmed: code `0x46ead0`, `reimpl/src/ai_chase.cpp`; VM]. Source: section 8, "The game's own behaviours and CCameraTrakerAI …".
- Fix: yes (opt-in, gameplay). Editor: no.

**1.8 "Smooth Turning" always on in `CEntityBehaviorMoveToPos`** — `CEntityBehaviorMoveToPos::Update` `0x430f20`
- What: "Update tests 'Smooth Turning' on its flags with 8 already set (the original ORs 8 into the copy it tests), so the turn always goes through the steering".
- When: every move-to-position behaviour. Impact: the field's off setting has no effect there.
- Evidence: [confirmed: code `0x430f20`, `reimpl/src/ai_rest_a.cpp` (bit 8 is "Smooth Turning", `MoveToPos_Make`); VM]. Source: section 8, "Phase 3 remains, band A".
- Fix: yes (opt-in, gameplay). Editor: no (could mention that "Smooth Turning" off is ignored here).

**1.9 The pursue AI restores its target's passable flag correctly (not a bug)** — `CChasePursueAI::TryMove` `0x46cb10`
- What: "the pursue AI restores its target's passable flag only when it was not set (and then calls slot 58 with 0)". It makes the target passable (`0x2000000`, slot 58) for the move and afterwards, when the flag was not set before, clears it again (slot 59) and calls slot 58 with 0; when it was set before, it stays set. That is the state before the move in both cases, the same result as `CChaseInvestigateAI`'s restore (`0x46c0a0`: slot 59, then slot 58 with the old bit). (Until 2026-10-29 the entry said "the wrong way round" — wrong, the code restores the flag; only the call of slot 58 with 0 is odd, and it sets no bit.)
- Impact: none described.
- Evidence: [confirmed: code `0x46cb10`, `0x46c0a0`, `reimpl/src/ai_chase.cpp`; VM]. Source: section 8, "The game's own behaviours …".
- Fix: no. Editor: no.

**1.10 A shield absorbs more than its charge** — `CShieldAI`'s damage slot (`CShieldAI::Damage` `0x56da70`)
- What: "`CShieldAI`'s damage slot absorbs the whole damage even when it exceeds the charge."
- When: any hit larger than the shield's charge. Impact: the overflow damage never reaches the character.
- Evidence: [confirmed: code `0x56da70`, `reimpl/src/ai_shield.cpp`; VM]. Source: section 8, "The skeleton AI, the shields, …".
- Fix: yes (opt-in, gameplay balance). Editor: no.

**1.11 A pickup disappears when nobody took the item** — `CPickupAI`
- What: the pickup "removes the pickup even when nobody took the item".
- Evidence: [confirmed: code `0x53e110` (`CPickupAI::Collect`: also when the alternate item refuses too, the part hides or is marked for deletion), `reimpl/src/ai_pickup.cpp`; VM]. Source: section 8, "The patrol, pickup and player control AIs".
- Fix: yes (opt-in). Editor: no.

**1.12 Using non-count items finds the same item again** — `Part_UseItems` `0x426930`
- What: "marks a non-count item for deletion and searches again from the start, finding the same item until n are used up".
- When: an action uses n > 1 items of a kind that has no count. Impact: as quoted (fewer distinct items used than asked for).
- Evidence: [confirmed: code `0x426930`, `reimpl/src/inventory_items.cpp`; VM] (the function's name is an [assumption] in `docs/exe_symbols.csv`). Source: section 8, "Inventory items and definitions".
- Fix: yes (opt-in). Editor: no.

**1.13 `CChangeSoundEnvironmentTypeAction` does nothing** — empty function `0x4186e0`
- What: "`CChangeSoundEnvironmentTypeAction` hands the type to an empty function (`0x4186e0`, 'ret 4')."
- Impact: the action has no effect in the release.
- Evidence: [confirmed: code `0x4c5730` (the action's slot 23), `0x4186e0`, `reimpl/src/action_slots2.cpp`, `reimpl/src/short_hooks.cpp`; VM]. Source: section 8, "Behaviour of the action classes 0x4c0000–0x4cafff …".
- Fix: no (a feature, not a fix: the code is not in the release). Editor: yes (say that the action has no effect).

**1.14 `COldBadPatrolAI` reseeds the random generator** — constructor, `0x55c390`, generator `0x6d06c0`
- What: "`COldBadPatrolAI`'s constructor reseeds the random generator `0x6d06c0` with 2342342 …, so every such object created changes the game's random sequence."
- When: whenever such an object is created; whether the game's data creates them during play is not stated.
- Evidence: [confirmed: code `0x53a990`/`0x53aa00` (the constructor inlined in Create and CreateArray), `0x55c390`, `reimpl/src/ai_classes.cpp`; VM]. Source: section 8, "AI classes: Create, CreateArray, constructors …".
- Fix: yes (opt-in). Editor: yes (an obsolete class; warn when a level uses it).

**1.15 Fidget order 5 never advances; orders 3 and 4 are not implemented** — `CFidgetData::PickFidget` `0x4a0680`
- What: order 5 "the next" "never advances: it reads the index after the last one taken and only wraps it"; "3 and 4 'Not Implemented Yet!!!'".
- When: fidget settings with order 3, 4 or 5 (whether any model uses them is not stated).
- Evidence: [confirmed: code `0x4a0680`, `reimpl/src/behavior_rest.cpp`; VM]. Source: section 8, "The rest of the entity behaviours …".
- Fix: yes (opt-in, order 5). Editor: yes, if it ever edits fidget settings.

**1.16 DirectPlay's slot 100 is one past the player array** — the message handler of the DirectPlay layer (`dplay.h`), factory object 0x900 bytes
- What: "a new player gets the highest free slot from **100** down to 2 – slot 100 is the 'slots changed' byte +0x8fc and the dword +0x900, one past the array"; "the factory's 0x900-byte object is too short for the slot array's slot 100 (`+0x900`/`+0x904`), so a create-player message that takes slot 100 writes past it (the original's bug, kept; dplay.h)".
- When: a create-player message while slot 100 is free, i.e. in a real network session. Impact: a write past the object; no symptom is documented.
- Found in a LAN game (batch 6-14, 2026-10-30) [verified: VM, run `20261030-214158-f9a0`]: slot 100 counts as free when the dword at +0x904 (past the object) is zero — then the joining player gets slot 100, whose "added" byte is the "slots changed" byte, and `ProcessSlots` (slots 0–99) never makes it a session: the host keeps receiving the client's packets for a player it never added and the client never gets the map. With the replay's fixed heap (a block's padding zeroed) the first LAN recording did exactly that; with Windows' heap that dword is the second dword of the next block's (encoded) heap entry header and the player gets slot 99 (all LAN games of batch 6-11 joined) [verified 2026-10-31, batch 6-16a: runs `20261031-023956-eeb0` (`/Original`) and `-024248-c46c` (exe) with `FixedHeap=0` logged +0x904 = `080e3896` / `080e9263`, the value changing with each process's heap encoding]. The replay now marks the bytes after a block (`replay.cpp`, `mark_tail`, version 7 only).
- Evidence: [confirmed: imports, slot numbers, message ids; …] for the layer; checked with fakes only. Source: section 6, "Net game, net session, block readers, DirectPlay game side"; section 10, "Batch P5a-2".
- Fix: yes (memory safety, opt-in). Editor: no.

**1.17 The level's "Listener Settings" have no effect** — setters `0x4e6c20`, `0x4e6c50`, `0x4e6c80` of `CLayerSaveData`
- What: "Listener Settings/Environment Type" (setter `0x4e6c20`) passes the type to `0x4186e0`, an empty function (`ret 4`), like `CChangeSoundEnvironmentTypeAction` (entry 1.13); "Listener Settings/Environment Size" (setter `0x4e6c80` → `0x57c230`) stores `0x7136f0`, which no instruction reads (its only reference is the store at `0x57c235`); "Listener Settings/Rolloff" (setter `0x4e6c50` → `0x57c210`) stores `0x713698`, read only by `0x57c220`, whose only caller is `0x42de33` in `CAmbientSoundAI`'s debug drawing (never reached in the release, `docs/EXE_FINDINGS.md`, "Debug drawing of the AIs and parts").
- When: every level; in the 112 original levels 91 set another Environment Type and 6 a Rolloff of 10 (the defaults: Environment Type "Generic", Rolloff 5.0, Environment Size 7.5, field registration in `reimpl/src/layer.cpp`).
- Evidence: [confirmed: code, static]. Source: the parent session's analysis 2026-10-28 (addresses above).
- Fix: no (a feature, not a fix: the sound environment code is not in the release). Editor: yes (the level check says that a non-default value does nothing, `zax-formats/zaxtools/levelcheck.py`).
- Since 2026-10-29: the demo of 2001-07-01 (build 890) still has the code: EAX 2.0 through `EAX.DLL`, the 26 types are EAX's presets one to one, Environment Size is EAX property 12, Rolloff the DirectSound3D rolloff factor (`docs/EXE_FINDINGS.md`, "Demo 2001-07-01") [confirmed: code of that build]. A candidate for an opt-in feature of the port (e.g. reverb through OpenAL EFX) [assumption].

**1.18 The aim cursor is recentred on (320, 240), also at 800×600** — `ControlConfig::AnalogueVector` `0x4708c0` (the recentring at `0x470a6a`), constants `0x5eff98`/`0x5eff9c` (320, 240) and `0x5effa0` (200)
- What: the analogue aiming vector (+0x5c/+0x60, whose angle is the aim, `0x470b50`) is "the mouse's offset from (320, 240) times 0.005" (with +0x4c), or the joystick's axes outside the dead zone of 0.15 (with +0x4d); normalised beyond length 1 and clamped to [−1, 1], "a changed vector moves the mouse back to (x · 200 + 320, y · 200 + 240)". The centre is fixed, not the screen's: at 800×600 it lies 80 pixels left of and 60 above the screen's centre (400, 300).
- When: the joystick (or the mouse vector of +0x4c) at 800×600, and at every resolution above 640×480. Impact: the aim circle sits off the centre of the screen [assumption: the effect is read from the code, not seen; needs a game test with a joystick at 800×600].
- Evidence: [confirmed: code `0x4708c0`, `reimpl/src/input_rest_a.cpp`; VM] (the formula; the cursor is in screen pixels: `0x518570` centres it elsewhere at the screen's half size, `reimpl/src/c4_rest.cpp`). Source: reported by the ZaxHiRes mod (`NOTES.md`: "aim cursor recentred at (320,240) +- 200"; `README.md`: with a joystick the cursor "still centres on the old 640x480 middle"), until 2026-10-29 entry 5.4 [unverified]; `reimpl/src/input_rest_a.cpp`.
- Fix: yes (opt-in: the screen's centre; relevant for phase 8, widescreen). Editor: no.

**1.19 The sound dialog's channel choice is lost at the next start** — `SoundDialog::Notify` `0x57f870`, `Sound_LoadOptions` `0x57ee90`
- What: the two channel buttons of the sound dialog write `Sounds Maxing Chanels` = 32 or 5 into `Zax.CFG` (`0x57f960`, `0x57f991`; string `0x62242c`) and set the limit at once (`0x57b1b0`); at start the game reads `Sounds Mixing Chanels` (default 32, `0x57ef3c`; string `0x6223e0`). Nothing reads the key the dialog writes.
- When: after choosing 5 channels and restarting the game, 32 apply again (unless `Sounds Mixing Chanels` is set by hand).
- Evidence: [confirmed 2026-10-29: code, reimpl `c4_windows.cpp`, `sound_resource.cpp`; VM; the scan of every INI call]; [verified 2026-10-29, VM, `/Original`, 800×600: runs `20261029-214006-4f64` and `20261029-214053-c8c1`]. The channel pair is the dialog's last row, "Sound Mixing": "Best" = 32, "Faster" = 5; the pressed button is drawn sunken. Reached by keys (the row's accelerator X, then Right and Space; mouse clicks of the helper did not reach the dialog's buttons, run `20261029-213846-4c62`). After choosing "Faster" the VM's `Zax.CFG` holds `Sounds Maxing Chanels=5` (before: `=32`) and no `Sounds Mixing Chanels` key at all. A new start with exactly that `Zax.CFG` opens the dialog with "Best" pressed (the dialog shows the cache's current limit, `+8 == 5`, so 32 applies), and the key stays `Sounds Maxing Chanels=5` in the file, read by nothing.
- Fix: yes (read the key the dialog writes, or both). Editor: no.

**1.20 A sequence played once, saved while it plays, resumes at a speed from stale pool memory** — `CEntityBehaviorPlaySequenceOnce`: `Create` `0x4966d0`, `RegisterFields` `0x496920`, `Begin` `0x496ee0`, `Update` `0x497010`
- What: the behaviour keeps the speed it plays at in +0x18 (`Begin` sets it from "Speed To Play" +0x14; `Update` advances the entity's "Current Frame" by the player's frames per second × +0x18 × the seconds; 0 means done). +0x18 is not a registered field, so a save does not hold it, and the class's `Create` (`operator new` from the pool, the base constructors, `Object_InitFields`) never writes it. A save made while the behaviour plays stores it with its stack's "InProcess=1"; after loading, the stack calls `Update` without `Begin` again, so the sequence plays at whatever dword the reused pool block held at +0x18.
- When: after loading a save that holds an entity in the middle of a `CPlayAnimationAction` (in the save games of batch saves: the lava bubbles of the 15–18 levels, `Environments/Volcanic Pits/Lava Detail/Lava Bubble Two`, driven by a `CRepeatTimerTriggerAI`). In the loads traced the stale dword was the vtable `0x5ef324` of a pointer array (the denormal float 8.7e-39), so the bubble's frame grows by about 1e-39 per frame and its animation stays on frame 0 (no end, so no "Done Sequence") [the picture: assumption, not looked at; the frame values are logged]. Another history of the pool block gives another speed, any float.
- Evidence: [confirmed: code `0x4966d0` (disassembly), `reimpl/src/ai_behaviors.cpp`; verified 2026-11-01, VM (ARM), the frame log's per-part dump `[Reimpl] WorldLogFrame`: the save games `M15BohrerReparatur_4`, `M15EndedesKreises_2`, `M16JispursAusbruch_3`, `M17BohrerReparatur`, `M18JispursAusbruch`, runs `20261101-142659-66a7` / `-e77e`, `20261101-142848-e2bd` / `-efb2`, `-0b9a` / `-e98c`, `-18dc` / `-075d`, `-4653` / `-9319`]: the behaviour's +0x18 after the load is `0x5ef324` with `/Original` and the hooked DLL, the "Current Frame" bits `0x9e3fe5`, `0xa8cd16`, … from the frame after the load. The standalone exe reads the same stale dword, which there is its own copy of that vtable (`0xc67004`), so its bubble's frame grows about 2.09 times as fast: the only difference of the standalone exe in the 123 save games (six of them; also `M14AzahPankehrtzurck_4`, whose written save differs only in that "Current Frame"; DECOMPILATION.md 13, "Batch saves").
- Fix: yes (save +0x18, or set it in `Create` and run `Begin` again after a load). Editor: no.

## 2. Crash, hang or garbage on unusual data

### Text files and object parsing

**2.1 `Local.txt`: overrun, endless loop, null read** — `0x500ac0` (loading)
- What: "the three line buffers are 0x400 bytes and the parts are joined without a bound (a part longer than the buffer overruns the stack); a part without `"[END]"` up to the end of the file loops for ever; a second line shorter than 11 characters makes the text start behind its terminator (in stale buffer bytes); a text whose `"[END]"` sits within its first 11 characters dereferences null." An LF-only file is one line (entry 2.2). "A line of 0x3ff characters or more ends the file there (the reader refuses it without moving on)."
- When: a hand-made or translated `Local.txt` (`ENGLISH: …[END]` / `LOCALIZED: …[END]` pairs; the retail game has none in the VM's data).
- Evidence: [confirmed: code `0x500ac0`, `0x500900`, `0x500f50`, `reimpl/src/local_table.cpp`; VM] (the quirks as well as the table). Source: section 6, "The Local.txt table, registered strings, three DirectPlay helpers".
- Fix: yes (opt-in: bound the buffers, stop at the end of the file). Editor: yes, if it ever writes or checks `Local.txt`: every part ends with `[END]`, parts under 0x3ff characters and joined below 0x400 bytes, the `LOCALIZED: ` line at least 11 characters, CRLF.

**2.2 Line reading splits only at CR** — `MemoryStream_ReadLine` `0x506140`
- What: "the line reader only ends lines at '\r' (a following '\n' is skipped as well), so text with bare '\n' line ends comes back as one line if it fits; a line that does not fit into n - 1 bytes returns false without moving."
- When: text read through the memory stream with LF-only line ends. Impact: the whole file is one line (see 2.1 for `Local.txt`).
- Evidence: [confirmed: code `0x506140`, `reimpl/src/memory_stream.cpp`; VM]. Source: `reimpl/src/memory_stream.h`; section 6, "The Local.txt table …".
- Fix: yes (opt-in). Editor: yes (always write CRLF; the editor's `textformat` already does).
- Level files [verified 2026-10-28, `/Original`, `Test/CheckLine`]: the disk stream's `Stream_ReadLine` `0x4a2ad0` reads into `Object_ReadText`'s 0x400-byte buffer, so a level line (with its indentation) of 0x3ff characters or more ends the reading there: a way point polygon line of about 1,450 characters left the level without its ground (black) and without the objects after it; the game ran on, no message. The longest line of the 112 original levels has 707 characters. The level check reports such lines (`zax-formats/zaxtools/levelcheck.py`, `line-too-long`).

**2.3 Long class names overrun a stack buffer** — `0x418cb0`, `0x418650`
- What: they "read class names into 0x64-byte stack buffers without a length check (`ReadCountedText` only checks the rest of the stream)".
- When: a binary object stream with a class name of 0x64 characters or more.
- Evidence: [confirmed: disassembly; VM where noted]. Source: section 6, "Object files, CObject base methods, the slots that create objects".
- Fix: yes. Editor: yes (class names must be the registered ones, which are short).

**2.4 A count that does not match the elements** — the pointer array and `new[]` array readers; `0x419810`, `0x418050`, `0x416310`
- What: "A count line that does not match the elements leaves states the original cannot handle either: the pointer array creates default objects (element slot 10) and the game ends deleting them …, the `new[]` array reads past its data". The three readers "read their count into an uninitialised slot (`0x416310`: the caller's ecx, the stream)".
- When: a level or object file whose `… Count` line disagrees with its items.
- Evidence: [confirmed: disassembly; VM where noted] (VM runs named in the source). Source: section 6, "Object files, CObject base methods, …".
- Fix: yes. Editor: yes (the editor's `Node.update_array_count()` keeps counts right; warn on files that do not).

**2.5 An unknown or abstract class ends the game** — `Create` `0x415c00`
- What: "A missing or abstract class ends the game: message box …, then `exit(-1)`." A deliberate fatal check.
- Evidence: [confirmed: code `0x415c00`, `reimpl/src/class_registry.cpp`; VM]. Source: section 6, "Extra lists, class registry, directory lists".
- Fix: no (deliberate; a fix could report and skip the object). Editor: yes (class names against the exe's registration, `zax-formats/zaxtools/exeschema.py`).

**2.6 An unexpected `{` ends the game** — `Object_ReadText`
- What: "`{` ends the game (`"Found unexpected '{' while parsing class ..."`, `ShowErrorBox`, `exit(-1)`)".
- Evidence: [confirmed]. Source: section 6, "Object table, object stream, objects read as text and binary".
- Fix: no (deliberate). Editor: yes (brace structure).

**2.7 An unknown name in a list field ends the game** — `NamedList_FindOrFail` `0x591fc0`; `FlagsField_FromText` `0x414fa0`; `ResourceField_SetByName` `0x54e490`
- What: flags: "an unknown name or an empty name between commas ends the game through `FindOrFail` (lists without a `$all` entry; `$all` is only understood by the default `0x414de0`)"; resource fields: "otherwise `FindOrFail` (ends the game for a missing name)". Also: the flags' `FromText` "writes into the text (a `0` at each comma, the comma put back afterwards), so the text must be writable"; "bits from index 32 on wrap (`shl` by `i & 31`) and bits 16-31 of the mask are dropped without a setter".
- When: a field value naming a sound, font, sequence, category, flag … that the list does not have (also a typo, or `a,,b`).
- Evidence: [confirmed: code `0x591fc0`, `0x414fa0`, `0x54e490`, `reimpl/src/named_list.cpp`, `reimpl/src/field_text2.cpp`, `reimpl/src/resource_fields.cpp`; VM]. Source: section 6, "Field values as text, second part" and "Cached resources, …".
- Fix: no for the fatal check (deliberate); yes for the 32-bit wrap. Editor: yes (names of list fields against the game's lists; no empty names between commas).

**2.8 Missing parts of a value leave garbage** — `ByteField_FromText` `0x54df30`, `PointField_FromText` `0x4a8140`, `IntPointField_FromText` `0x5455d0`, `ColorField_FromText` `0x46df60`
- What: the byte's setter "carries the upper three bytes of the text pointer"; "The points' `FromText` let `sscanf` write into stack slots: a missing x keeps the bits of the text pointer, a missing y those of the descriptor pointer. Both ignore the setter the constructors store."; the colour: a text without red and green "leaves stack garbage in the colour" (the reimplementation starts them at 0, see above), "a missing blue keeps the text pointer as in the original".
- When: `Field=1` for a point, `Color=255,0` and similar.
- Evidence: [confirmed: code `0x54df30`, `0x4a8140`, `0x5455d0`, `0x46df60`, `reimpl/src/field_text.cpp`; VM] (the colour's red and green as the original only where the text has them, see above). Source: section 6, "Field values as text".
- Fix: yes (opt-in: missing parts 0). Editor: yes (points need two numbers, colours three).

**2.9 Resource and temp file fields on odd data** — the resource kinds' slots, `TempFileField_FromText` `0x58b8f0`
- What: "the reader of both forms reads the 0x3e9 marker with a 4-byte `Read` whose result is not checked, so for fewer than 4 bytes the marker is uninitialised stack (not tested); `ReadBinary` with −1 sets the value to null without deleting the old file or freeing its buffer; `FromText` writes to null when no line has a `=` before the end."
- Evidence: [confirmed: code `0x58b8f0` and the resource kinds' slots, `reimpl/src/resource_fields.cpp`; VM] (the short marker read: not tested, as the source says). Source: section 6, "Cached resources, resource and temp file slots, …".
- Fix: yes. Editor: yes for text (a temp file field needs its `Length=` line).

**2.10 A record with an empty name** — the definition classes' slots 30 and 39
- What: "a record with an empty name makes slots 30 and 39 read a null name (slot 1 gives null for it); the game's folders never give one."
- When: a definition file with an empty name in a mod's folders.
- Evidence: [confirmed: code `0x4777b0` (slot 30), `0x46ae50` (slot 39, the name by slot 1), `reimpl/src/definitions.cpp`; VM]. Source: section 6, "Definition classes, the category and damage type lists, …".
- Fix: yes. Editor: yes (no empty names of definitions).

**2.11 Lists with a count but no records** — `0x56a6b0`, the sequence list's slots, the mission over window, the end screen and the difficulty comparator
- What: "`0x56a6b0` reads address 0x1c when the catalogue has no records"; "Both slots read the records before any test: a list with no records and a count crashes the original too"; "a list without records but a count crashes the original"; "the end screen and the difficulty comparator read through null when a record is missing or loading".
- Evidence: [confirmed: code `0x56a6b0`, `reimpl/src/ai_shield.cpp`; VM]; section 9 band B: [confirmed: code; VM] on the sequence lists' part; the mission over window, the end screen and the difficulty comparator: no tag of their own (reimplemented in VM-checked batches, not re-read one by one in the evidence pass). Source: section 8, "The skeleton AI, …"; section 9, "Phase 4 remains, band B"; section 10, "Batch C3", "Phase 5 leftovers".
- Fix: yes. Editor: no (game folders, not levels).

**2.12 An empty map name hangs the save name pattern** — `0x5bc910`
- What: making the pattern from a typed name: "an empty map name never ends, as in the original".
- Evidence: [confirmed: code `0x5bc910`, `reimpl/src/game_types.cpp`; VM]. Source: section 8, "The game types Capture the Flag and Salvage King, …".
- Fix: yes. Editor: maybe (whether a level can give an empty map name is not stated).

### Models, sequences, images and fonts

**2.13 A missing model or sequence ends the game** — `0x48b120`, `PlaySequence`, `CEntityAnimated` slots 92 and 93
- What: "several functions end the game on a missing model or sequence, in the original as here (`0x48b120` reads slot 25 of a null sequence, `PlaySequence` and slot 93 call `_stricmp` with a null name for a sequence index beyond the model's or a model without sequences, slot 92 reads `0xb4` when the top parent is not animated)". Also `CEntityMovable` "slot 94 reads the model before its null test"; `0x490060` "uses its new activity without a null test".
- When: an object whose model or sequence is missing, a sequence index beyond the model's.
- Evidence: [confirmed: code]. Source: section 8, "The entity base classes …" and "The entity classes above CEntityAnimated".
- Fix: yes (report instead of crash). Editor: yes (models and sequence names must exist; `Cur Sequence` and indices within the model).

**2.14 Frames that do not divide by the rotations end the game** — `CRotationSequence` slot 27
- What: "ends the game with 'There are not an equal number of frames per rotation…' when the frames do not divide by the rotations and the sequence is being loaded by name (`0x713130`)". A deliberate check.
- Evidence: [confirmed: code]. Source: section 9, "Sprites and entities".
- Fix: no (deliberate). Editor: yes (frames of a rotation sequence must divide by its rotations).

**2.15 A bounce factor outside 0..1 ends the game** — `CEntityMovable` slot 94
- What: "ends the game for a bounce factor above 1, below 0 or NaN ('Model %s has a Bounce Factor of %f. …', 'FATAL ERROR!', exit(-1))". A deliberate check.
- Evidence: [confirmed: code]. Source: section 8, "The entity classes above CEntityAnimated".
- Fix: no (deliberate). Editor: yes (model "Bounce Factor" in [0, 1]).

**2.16 The frame setter never ends on NaN or huge frames** — `EntityAnimated_SetFrame` `0x48feb0`
- What: it "wraps the frame into the sequence's frame count by repeated adding/subtracting (it does not end for NaN, infinity or huge values, in the original as here)".
- Evidence: [confirmed: code `0x48feb0`, `reimpl/src/entities.cpp`; VM]. Source: section 8, "The entity base classes …".
- Fix: yes. Editor: yes (`Current Frame` finite and within a sane range).

**2.17 HUD and detail drawing of items without their data** — the HUD item renderers and detail renderers
- What: "The drawing code reads definitions, type definitions, HUD and detail sequences without null tests and divides by a sequence's frame count: an item the HUD never shows (no type definition, no HUD icon, a detail image without frames, a deployable template without its part) crashes the original as it does the reimplementation."
- Evidence: no tag of its own (the HUD's renderers are reimplemented and VM-checked, the source names no addresses; not re-read one by one in the evidence pass). Source: section 9, "The HUD: …".
- Fix: yes. Editor: yes, for item definitions of mods.

**2.18 A meter value above its maximum writes past the frame** — the meter's bars (with R1's line, `0x4abae0`)
- What: "the meter's bars take their width from value / maximum unclipped (a value above the maximum writes past the frame). The game's HUD frames have origin 0 and its meters lie inside the screen."
- Evidence: [confirmed: code, crashes in the check runs]. Source: section 9, "The HUD: …".
- Fix: yes (clip). Editor: maybe (where a level sets a meter's value above its maximum; which fields feed the meters is not listed).

**2.19 8-bit RLE sprites with zero-length codes** — the dynamic span tables of Amplify Light, Black Bias and Transparent Mask; the remap helpers; `CSimpleWarpEffect`'s loops
- What: "the dynamic tables of Amplify Light, Black Bias and Transparent Mask hold the loop in all 64 entries, so a zero-length literal or run code (`0x40`, `0x00`) in an 8-bit RLE frame crashes the 8 → 16 and the Black Bias / Test Light / Amplify Light drawers"; the remap helpers "loop while the pixels left in a row are not exactly 0 (a row whose codes overshoot its width runs on) and their literal loops are do-while (a zero-length literal runs 2^32 times)"; the simple warp: "a run of 0 pixels runs 2^32 times; a row whose runs do not add up to the width never ends".
- When: RLE data the game's own encoder does not write ("the encoder never writes one").
- Evidence: [confirmed: a check case did, run `20261027-065659-33e5`; …] (R2); [confirmed: code `0x43cd00`, `0x43cde0`, `reimpl/src/rle16.cpp`; VM] (R5's remap helpers); [confirmed: code `0x571ff0`–`0x573440` (the row helpers), `reimpl/src/simple_warp.cpp`; VM] (simple warp, until 2026-10-29 [observed in the code]). Source: section 9, "Batch R2", "Batch R5", "The last effects: …"; `reimpl/src/rle16.cpp`.
- Fix: yes. Editor: yes, for model or sequence import (no zero-length codes, rows adding up to the width).

**2.20 Amplify Light with an edge block calls address 0** — Amplify Light's RLE drawers
- What: "Amplify registers no edge spans (its tables `0x6b70f0`/`0x6b76f0` are 0) and its drawers call address 0 for an edge block". "The game's light sprites have no edge block."
- When: a light sprite with soft edges.
- Evidence: [confirmed: VM crash; the blitter map's assumption]. Source: section 9, "Batch R3"; section 12.
- Fix: yes. Editor: yes (light layers without an edge block).

**2.21 A short sequence cache file goes on creating frames** — `CSequence`/`CRotationSequence` slot 15
- What: "slot 15 reads the frame's byte into the stack slot of its file argument; when the file ends there, the byte left is the file pointer's low byte, so a short file goes on creating frames".
- Evidence: [confirmed: code; VM]. Source: section 9, "Effects and text: …".
- Fix: yes. Editor: yes, if it writes `.seq16` caches (complete files).

**2.22 Fonts: text wrapping hangs** — the wrap layout `0x4a7010`/`0x4a7110`; the word-wrapped text `0x4a6790`, `0x4a68b0`, `0x4a6ae0`
- What: the wrap layout "hangs when the wrap width is narrower than a word (the original as well)"; the paragraph functions: "When nothing fits on the first line and the following lines are wider, … an empty rest never ends (in the original too …)".
- When: a text overlay or label with a wrap width below the widest word (e.g. "Text Wrapping Width" of `CFrameOverlayTextOverlay`, default 0x80).
- Evidence: [confirmed: code `0x4a7010`, `0x4a7110`, `reimpl/src/text_rest4a.cpp`; VM] (wrap layout); [confirmed: code `0x4a6790`, `0x4a68b0`, `0x4a6ae0`, `reimpl/src/font_layout.cpp`; VM] (word wrap, until 2026-10-29 [observed in the code]). Source: section 9, "The window toolkit", "Effects and text: …".
- Fix: yes. Editor: yes (wrap widths at least the widest word).

**2.23 The glyph lookup accepts one glyph past the last** — the fonts' measuring code
- What: "The glyph lookup of the measuring code compares the index with last − first + 1 (one past the last glyph), the drawing with the frame count."
- When: a text with the character one past a font's last glyph.
- Evidence: [confirmed: code]. Source: section 9, "Sprites and entities".
- Fix: yes. Editor: maybe (texts against the font's glyph range).

**2.24 A font lower than 5 pixels divides by zero in the combo box** — the combo box's init
- What: "the combo box's init divides by the font height / 5 (a font lower than 5 pixels …)".
- Evidence: [confirmed: VM runs of the first versions of the checks …] (the paragraph). Source: section 9, "List boxes, combo boxes, …".
- Fix: yes. Editor: no (fonts, not levels).

**2.25 The mini map of a level in an archive** — `Map_LoadMiniMap` `0x4ece20`
- What: "If neither the image loads nor the `.tga` test passes (a level only in an archive without the `.tga`), the game ends with 'Fatal Error loading Mini-Map image'." A deliberate check; a level on the disk always qualifies for the rebuild.
- Evidence: [confirmed 2026-10-28: code …; verified 2026-10-28, `/Original`, `Test/MMNoTga`]. Source: `zax-formats/docs/FORMATS.md` 5, mini map; section 10, "Batch C5".
- Fix: no (deliberate; a fix could rebuild into the cache only). Editor: yes (a level packed into an archive needs its mini map image).

**2.26 TGA files** — `CFrame::LoadTga` `0x4a9d70`, `ImageFile_Read` `0x58c6f0`
- What: `CFrame::LoadTga` "ends the game on a recursive load or an uncompressed TGA that is not on disk"; `ImageFile_Read` "returns at a frame depth other than 8, 16, 24 and 32 without freeing its RLE row buffer"; overflowing RLE rows reach the "corrupt TGA" message box (the checks keep them out: "RLE rows never overflow (that ends the game)").
- Evidence: [confirmed: code `0x4a9d70`, `0x58c6f0`, `reimpl/src/image_lo4.cpp`, `reimpl/src/image_file.cpp`; VM] (until 2026-10-29 partly [observed in the code]). Source: section 9, "Phase 4 remains, band C" and "Phase 4 leftovers".
- Fix: yes. Editor: yes, for TGA images of mods (compressed TGAs only inside archives).

**2.27 Filter 10 overflows the stack** — `0x4b2b50` (filter 10's Apply: an 8-bit frame's palette from a colour histogram)
- What: it "keeps a 1 MB colour histogram (2^18 dwords, 6 bits per channel) on the stack (`_chkstk` 0x100000), as much as the exe's whole stack reserve …: it overflows the main thread's stack whenever it runs". The reimplementation keeps the histogram on the heap.
- When: whenever this frame load filter runs; which game paths run it is not stated.
- Evidence: [confirmed: VM, run `20261027-190931-c6ca` crashed the game in this check …]. Source: section 9, "Batch R6".
- Fix: yes. Editor: no.

### Level objects: actions, AIs, items

**2.28 Actions and AIs that read their target without a test** — many slots
- What (as listed in the sources): `CActivateAction` "reads the model's sound without a test, `CDestroyAction` the target without a test, `CSetHomeAction` calls slot 129 on the target taken as a character without a test"; the door functions read the model without a test; `CMultipleDelayedActionsAction` "runs the first action now (no null test)"; `CIsEnemyNearbyAction` queries the first character's layer "(no null test)"; `CLimitedTimeAI`'s Begin calls "What To Do" "without a null test"; "every slot of `CTemporaryTaskAI` (its temporary AI), `CTenaciousInvestigateAI`'s Update …, the generator's warp behaviour block and the definition lists' slots 8 and 9 have no null test"; `CScanAreaAI`'s Begin "calls the entity's slot 46 also for a null entity, its Update reads the entity's layer for the action without a test"; `CSayQuip`'s slot 23 "reads the category without a test (with the list empty or the entry loading the game would end)"; a failed `Entity_CreateFromModel` or `Object_Clone` in the particle AIs "leaves a null pointer that the exe uses without a test"; the companion AI's Begin and Update read the entity without a test.
- When: an action or AI whose target name does not resolve, an empty required field, a wrong class of target.
- Evidence: [confirmed: code; checked] for most of the sections. Source: section 8 ("Behaviour of the action classes 0x4c0000–0x4cafff …", "The game's own actions …", "The task, generator, go-to and turret AIs …", "The particle AIs …", "The non-interactive sequences …", "Phase 3 remains, band D").
- Fix: yes (skip and report). Editor: yes (reference fields must resolve, `zax-formats/zaxtools/links.py`; required fields not empty).

**2.29 A shield AI on a non-character ends the game** — `CShieldAI::FindItem` `0x56d7d0` and the item slots
- What: they "read the character's inventory without a test (a shield AI on a non-character would end the game)"; `0x56da70` reads "address 8 when the damage type has no loaded record".
- Evidence: [confirmed: code `0x56d7d0`, `0x56da70`, `reimpl/src/ai_shield.cpp`; VM]. Source: section 8, "The skeleton AI, the shields, …".
- Fix: yes. Editor: yes (`CShieldAI` only on characters).

**2.30 A pickup whose item refuses and has no alternate** — `CPickupAI`
- What: "`CPickupAI`'s pickup reads the alternate item without a test when the item refuses (a crash in the exe when there is none)".
- Evidence: [confirmed: code `0x53e110`, `reimpl/src/ai_pickup.cpp`; VM]. Source: section 8, "The patrol, pickup and player control AIs".
- Fix: yes. Editor: maybe (when an item can refuse is not stated).

**2.31 A random patrol path with one point to choose hangs** — `CPatrolPathAI`
- What: "With 'Random' and only one point to choose from (the polygon's count - 1, less one with 'Ignore last point', equal to 1) while at point 0, the loop that wants another point never ends (in the exe too; no level of the checks has one)."
- Evidence: [confirmed: code `0x53c8d0` (`CPatrolPathAI::NextPoint`), `reimpl/src/ai_patrol.cpp`; VM]. Source: section 8, "The patrol, pickup and player control AIs".
- Fix: yes. Editor: yes (a "Random" path needs more than one point to choose).

**2.32 A NaN time hangs the generator and the repeat timer** — `CGeneratorAI`, `CRepeatTimerTriggerAI`
- What: the generator's "count-down never ends for NaN seconds or a NaN time (in the exe too)"; the repeat timer adds its delay "until it is above 0 again (for ever with NaN times, as in the exe)".
- Evidence: [confirmed; checked]. Source: section 8, "The task, generator, …"; `reimpl/src/ai_triggers.cpp`.
- Fix: yes. Editor: yes (time fields finite).

**2.33 A light beam touching a part that is no `CEntityBase`** — `CLightBeamAI::Collide` `0x4fe5f0`
- What: the collision "reads the part's model and owner without a test when the part is not a CEntityBase, and uses the player's damage when the other has any flags and either flag word `0x6c29bc`/`0x6c29a0` is not 0"; "`CLightBeamAI`'s collision with a part that is not a CEntityBase ends the game in the original too".
- Evidence: [confirmed: code `0x4fe5f0`, `reimpl/src/ai_chase.cpp`; VM]. Source: section 8, "The game's own behaviours …".
- Fix: yes. Editor: no (depends on what touches the beam at run time).

**2.34 Items and definitions without their data** — `CCharacterInventoryItem` slot 51, `ProjectileFloat24/30` `0x4d6300`/`0x4d6410`, `GiveItemsToPlayers` `0x5a26f0`, the weapon action, the inventory's neighbours
- What: slot 51 "reads the definition, the level array and the type definition without null tests"; `0x4d6300`/`0x4d6410` "read their float from address 0 + offset when the definition has no loaded projectile (the game ends there, as in the original)"; `GiveItemsToPlayers` "reads the definition and the new item without a null test" (a name missing from the catalogue: "it reads through a null pointer (`[ebx+0x1c]` at `0x5a2792`), so the game crashes", `docs/EXE_FINDINGS.md`, cheats); "the weapon action uses the raw 'definition' argument for the ammunition test and reads the definition's 'Energy' without a null test"; the inventory's neighbour functions "resolve the definition of every item without a null check (a null item ends the game, with the original as well)".
- Evidence: [confirmed: code `0x4d6300`, `0x4d6410`, `reimpl/src/inventory_defs.cpp`; `0x5a26f0`, `reimpl/src/game_rest_d.cpp`; VM]; [confirmed: Zax.exe] (cheats); slot 51, the weapon action and the neighbours: no tag of their own (reimplemented in VM-checked batches, not re-read one by one in the evidence pass). Source: section 6, "Inventory bookkeeping, …"; section 8, "Inventory items and definitions", "The rest of the entity behaviours …", "Phase 3 remains, band D"; `docs/EXE_FINDINGS.md`, "Cheat commands".
- Fix: yes. Editor: yes (item names in inventories and actions against `Inventory/Item Definitions`).

**2.35 "Red Flag" and "Blue Flag" categories end the game** — the world's game data, slot 2 `0x4f13a0`
- What: "item categories (a 'Red Flag' or 'Blue Flag' category ends the game with 'Fatal error')". A deliberate check.
- Evidence: [confirmed: code; verified in the game] (section paragraph). Source: section 7, "The rest of the world class: …".
- Fix: no (deliberate). Editor: yes, for mods that add categories.

### Data packaging and settings

**2.36 Archives and file names** — `ZZipList` `Load` `0x5c2240`, `Zip_MountAll` `0x4a0c40`, the disk search `0x4a4840`, `Files_ListNames` `0x4a1150`
- What: `Load` reads the end record 0x16 bytes before the end, "so an archive comment makes it fail"; "A compressed entry ends the game ('Compressed Zip File Found')"; "with one entry the index stays uninitialised"; "mounting twice ends the game"; "A name containing `~` ends the game (message box, `exit(-1)`)"; "a name longer than stride − 1 ends the game with 'Illegal file name'".
- When: mod archives (`*.zip` below the game folder, `*.RED`) and file names in the data folders.
- Evidence: [confirmed: code `0x5c2240`, `0x4a0c40`, `reimpl/src/zip_archive.cpp`; `0x4a4840`, `0x4a1150`, `reimpl/src/file_search.cpp`; VM]. Source: section 6, "File search, file spec and the archive folders", "Zip archives".
- Fix: yes for the uninitialised index and the comment; no for the deliberate fatal checks. Editor: yes (archives stored uncompressed without a comment; no `~` in file names).

**2.37 Unsupported resolutions crash the game** — `/ScreenWidth`, `/ScreenHeight`, `/ScreenBitDepth`; the video dialog; `ScreenFrame_SetMode`
- What: "only 640×480 or 800×600 and 8 or 16 bit, other values crash the game [verified: VM]"; "a refused display mode in the video dialog's destructor ends the game ('Couldn't set resolution')"; the screen frame: 24 bits are tried as 32 when the mode fails, "a second failure ends the game".
- Evidence: [verified: VM] (command line), with [confirmed: code `0x567d60`, `reimpl/src/display_ddraw.cpp`; VM] (the mode list takes only 640×480 and 800×600); [confirmed: code `0x591750`, `reimpl/src/ui_settings.cpp`; `0x590ce0`, `reimpl/src/screen_frame.cpp`; VM] (dialogs, the 24 → 32 retry). Source: `docs/EXE_FINDINGS.md`, "Command line"; section 9, "The rest of the user interface", "Phase 4 remains, band C".
- 24 and 32 bits (2026-11-01, `docs/DECOMPILATION.md` 13, "Batch hang-32bit"): no crash and no hang but a fatal message box (`ShowErrorBox` `0x4d24c0`, `MessageBoxA` owned by the full-screen window, so hidden behind the DirectDraw surface; `exit(-1)` after its OK). The image, model and sequence lists take their file extension from the display depth (`Sequence_Extension` `0x56b2b0`, `CFrame_FileExtension` `0x4a8980`, `CModel_FileExtension` `0x516a20`, `Sequence_CacheFileName` `0x56b780`, `CFrame::FileName` `0x4a9900`, `CModel_FileName` `0x516da0`), and the shipped data has only the 8- and 16-bit caches and no sources, so at 24/32 bits every list is empty and `Rules.txt`'s font "Gold Editor" ends the game in "Load Game Data" ("Fatal Not Found Error", `NamedList_FindOrFail` `0x591fc0`, 2.7). On the XP VM the 32-bit mode set fails first ("Couldn't set resolution", `Display_Init` `0x590ce0`). Also: when 24 bits fail and 32 are set, `Display_Init` still tells the application 24 (`App::Set24` `0x4ce500` with the asked depth) while the screen frame is 32 bits, so the frames are made in the wrong format; and the ground's band mapper `CPlasmaTileMap_MapBands` `0x541250` has no 32-bit branch (32-bit ground tiles are never written).
- Evidence (24/32 bits): [verified 2026-11-01: VM, `/Original`, main thread samples (`[Reimpl] SampleAfterMs`) and screenshots, x86 `20261101-214345-8c2f` (24), `-3463` (32), ARM `20261101-214719-d817` (32), XP `20261101-214720-f685` (32); the archives' listings of all six editions]; [confirmed: code] (the extensions, the retry's depth, the band mapper); the sheared picture at 24 bits with the switch below [verified: VM, `20261101-215217-b27e`], its cause [assumption: the x86 driver has no 24-bit mode, so the retry took 32].
- Fix: yes (phase 7/8: other resolutions). For 24/32 bits: the port keeps the 16-bit frame pipeline and converts the finished frame at presentation; Zax-Reimpl's opt-in `[Reimpl] TrueColorData16=1` (load the 16-bit caches at 24/32 bits) reaches the level but the picture stays unusable (black at 32 bits, garbage at 24), so it is a diagnosis switch, not a fix. Editor: no (the launcher offers only the working values).

**2.38 A network packet ending in 0xff loops for ever** — `NetPacket_ReadBlock` `0x5200d0`
- What: "a packet ending in 0xff makes ReadBlock loop for ever (as in the original)".
- When: malformed network data.
- Evidence: [confirmed] (section). Source: `reimpl/src/net_packet.h`; section 6, "Network packet, block readers, …".
- Fix: yes. Editor: no.

**2.39 Build 1308 lacks a null test that 1309 has** — `CZaxWorld::DrawHud` `0x59e730`, the direction pointer
- What: the direction pointer (part 1 of `DrawHud`, `reimpl/src/hud_world.cpp` `draw_pointer`) reads the current layer's view (`+0x80`/`+0x84`) from `World::CurrentLayer` `0x4f1050` without a test; that function gives 0 when player 0's layer index is -1, out of range or negative. Build 1309 (Spanish, Polish, 2001-10-24) tests the result and skips the pointer (`call 0x4f1050; test eax, eax; je` at `0x59e856`), 16 bytes more code.
- When: a HUD frame with the pointer due (the direct drawing flag `0x6c1d08`, or a "Primary" item in the local character's inventory) while player 0 has no current layer [assumption: e.g. around a level switch; needs a game test]; then 1308 reads address 0x80 and ends.
- Evidence: [confirmed 2026-10-29: code of both builds, the clean 1308 of the 2001-08-06 demo against the Polish 1309; the effect is read from the code, not reproduced in the VM]. Source: `docs/EXE_FINDINGS.md`, "Build 1308 → 1309".
- Fix: yes (adopt 1309's test). Editor: no.

**2.40 A "Warped" surface ends the game when its level is drawn into the mini map at game time 0** — `CWaterWarpEffectFrame::Update` `0x598420`, `CRenderablePolygon` slot 23 `0x55dd90` (render type 7), `PolyFill16_Texture` `0x549aa0`, reached from `Map_SetupMiniMap` `0x4ed150`
- What: the warp effect's frame update compares the game time (the application's step total, the double at `*0x6c02cc` +0x40) with the time of its last update (+0x87c, a float, 0 after the constructor) and, when they are equal (`fcom`, NaN counts as equal), returns its bent frame +0x86c as it is, without making it. The bent frame is made only on the first update with a different time. So the first update at game time 0.0 returns a null frame. The surface's queueing (slot 23) stores that null as the draw item's texture (+0x40, rasterizer kind 5). The 16-bit tiled-texture fill then calls the texture's vtable: `PolyFill16_Texture` loads the texture parameter into `ebx` (`[esp+0xa0]`) and reads `mov eax, [ebx]` at `0x549bdc`. This is an access violation, and the process ends without a message (the job logs say "Zax.exe ended before 'Preload things on Map'"; earlier notes called it a hang).
- When: the game time is still 0.0 only during the application's first step. `App_RunSteps` `0x4cdd80` calls the step (slot 3) first and adds its seconds to +0x40 afterwards (`0x4cde29`/`0x4cde55`). `Game_Startup` zeroes +0x40 (`0x4cd78c`), and these are the only writers of that qword (a linear scan of the code for qword stores to `[reg+0x40]`). The main menu is made at start-up (`App_Start` `0x5a12d0` → `CZaxWorld::LoadGameData`), before the first step. The launcher's direct start (`/Map`, also the editor's Test in game) starts the map inside that first step (`zax-launcher/zax_launcher_dll.c` `first_step`). So the whole level load, and with it the mini map, runs at game time 0.0. A level then needs a "Warped" surface whose bounds meet the level rectangle (always true for the mini map, which draws the whole layer) and a mini map the game draws itself (no matching `.tga`/frame: the editor's 1 × 1 placeholders). With a mini map file the first draw of the surface happens in a later step at a game time above 0, and the frame is made. Started from the main menu (Start Game, a difficulty), the same level loads, the game draws its own mini map with the surface in it, and the water warps. No retail surface is "Warped" (all 187 textured ones are "Textured"), and no retail start path loads a level in the first step: the bug needs own data and the launcher's direct start.
- Evidence: [confirmed 2026-11-01: code `0x598420` (`reimpl/src/water_warp.cpp`), `0x55dd90` (`reimpl/src/render_poly.cpp`), `0x549aa0` (the texture dereference at `0x549bdc`), `0x4cdd80`, `0x4cd78c`, `0x5a12d0`, `World_RenderLayer` `0x4f4ae0` (`reimpl/src/world_frame.cpp`), `Map_SetupMiniMap` (`reimpl/src/world_c5.cpp`); the qword scan]. [verified 2026-11-01: VM, `/Original`. The direct start ends the game in "Layer Loading" (runs `20261101-102309-5d41`, `-102640-8420`, `-102733-af0c`, also with `Planet 01 Water`; hooked alike), with the editor's mini map it loads (`-102813-78cb`). The same level with the 1 × 1 placeholders started from the main menu through a `Rules.txt` (`First Map Name=Test/GapsMission`, keys S and Enter) loads, writes its own `-GapsMission.tga` and shows the water in the HUD mini map (`20261101-141031-475a`, ARM)]. The 8-bit fill (`0x54b230`) was not followed [assumption: it reads the null texture the same way]. Source: `docs/DECOMPILATION.md` 13, "Batch analyses 2026-11-01", 1 (found in "Batch gaps-sp", third round).
- Fix: candidate (opt-in): make the bent frame before the time test in `0x598420` (or skip a type-7 surface whose frame is null in `0x55dd90`). The launcher could start the map in the second step instead of the first (the game time is then above 0, as after a click in the main menu), which is closer to a real start; that is a tool change, not a fix of the game, and it is not done. Editor: warn about a "Warped" surface when the game is to draw the mini map (File → let the game draw the mini map) and the level is started directly (Test in game, the launcher's `/Map`); the editor's own mini map avoids it.

## 3. Dormant: not reached with the game's data or settings

**3.1 Do-while pixel loops with a count of 0 run 2^32 times** — many spans, rows and blitters
- What: T2–T9's blend loops, the mode table spans and the 8 → 16 literal test (R2); "The generic loops of T12, T14 and T16 and both Overlap spans are do-while loops: n = 0 runs 2^32 times" (R3); "The generic loops of T30, T32 and T34 are do-whiles" (R6); "the do-while width loops of the blending, converting and Overlap loops (width 0 runs 2^32 times)" (R5); `Remap8_Row` "with n = 0 loops 2^32 times"; "A literal of length 0 loops 2^32 times (the encoder never writes one)" (R7, the frame load filters); the Bink wrapper: "A pitch of 0 in the 8-bit path loops 2^32 times".
- When: "The callers never pass 0 (the generic entries are only reached for 17–63)" (R3). For RLE data see entry 2.19.
- Evidence: [confirmed: code; the checks cover them]. Source: section 9 (R2, R3, R5, R6, R7, "Phase 4 remains, band A"); `reimpl/src/bink_rest5a.cpp`.
- Fix: yes (cheap: test the count first). Editor: no.

**3.2 The 16-bit blend without MMX drifts** — `Frame_FillRect` (16 bits, alpha below 0xff)
- What: "without MMX the 16-bit blend still splits every row into groups of four and a rest but draws only the rest (width mod 4 pixels), starting where the previous row's rest ended plus the row skip, so the groups stay unchanged and the rows drift towards the buffer's start (every CPU the game runs on today has MMX)".
- Evidence: [confirmed: code `0x4abe90`, `reimpl/src/frame_basics.cpp`; VM]. Source: section 9, "Frame classes and the frame check harness".
- Fix: yes (relevant once the MMX code gets a portable form, phase 7). Editor: no.

**3.3 24- and 32-bit drawing paths** — Transparent Mask's 24R → 32, the 24-bit polygon fills, `CScreenFrame::FillRect`, 8 → 24/32 blits
- What: "Transparent Mask's 24R → 32 draws with the **24-bit** spans T28–T35 on a 32-bit destination" (pixels land between the destination's pixels); "The 24-bit polygon fill of type 1 … halves the bytes at q + 6, q + 3 and q while stepping q by 3 … every byte from the fourth on is halved three times and the five bytes after the span too", types 2–4 add the tint "with an 8-bit wrap" ("blue gets + 80 and green and red + 0"); `CScreenFrame::FillRect` "builds a 24-bit fill colour with the colour pointer's top byte as its fourth byte"; "8 → 24/32-bit blits read the source frame's own palette (slot 58): a frame without one crashes".
- When: only for 24- or 32-bit frames; "the game runs in 8 or 16 bit only … and the data has no 24/32-bit frames"; Transparent Mask's path is "unreachable in the game: it has no 24-bit RLE frames".
- Evidence: [confirmed: code; the checks cover them] (R6); [confirmed: callers, VM counters] (whether the game uses them); [assumption: unused …] (the blitter map; stays: about the data and the paths the game takes, which the code alone does not settle). Source: section 9, "Batch R1", "Batch R6", "Phase 4 remains, band C"; `reimpl/src/blit24_tm.cpp`.
- Fix: yes, if a port ever draws in 24/32 bits. Editor: no.

**3.4 CFrame's line clips wrongly with an origin** — CFrame slot 51 `0x4abae0`; the line rasterizers
- What: DrawLine "subtracts the frame's origin before it clips to the rectangle that includes the origin, so a frame with an origin ≠ 0 gets lines outside its buffer; a horizontal line exactly on the frame's height is drawn one row past the buffer". The rasterizers "draw unclipped when both end points (+2) lie inside the frame (the x-major paths clamp a negative row to 0, the steep ones do not)".
- When: "The game's HUD frames have origin 0".
- Evidence: [confirmed: code, crashes in the check runs]. Source: section 9, "The HUD: …", "Batch R1", "Batch R7".
- Fix: yes. Editor: no.

**3.5 The offset frame's GetColor moves the colour pointer** — `0x525230`
- What: it "passes `(x - dx, y, colour - dy)`: the colour pointer moved by dy FieldColors instead of y (dormant: COffsetClippedProxyFrame's own slot 53 does not use it)".
- Evidence: [confirmed: code; the checks cover them]. Source: section 9, "Batch R1"; `reimpl/src/frame_proxy.cpp`.
- Fix: yes. Editor: no.

**3.6 GetColor on a 16-bit RLE frame reads 64 MB past a table** — CFrame slot 53, `FieldColor_SetRGBA` `0x53f080`
- What: GetColor "on a drawn pixel of a 16-bit RLE frame passes r = 0xfffff8 to `FieldColor_SetRGBA` `0x53f080`, whose `Pixel16_FromRGB` indexes its red table with it unclipped: a read about 64 MB past the table, which crashes when that address is not mapped (it did on `Assimilate Cave1` in a full run, not alone)". Also "GetColor on a 16-bit RLE frame gives a fixed colour … when the pixel is drawn and black when not".
- When: which game code calls GetColor on such frames is not stated.
- Evidence: [confirmed: code; the checks cover them]. Source: section 9, "Batch R1".
- Fix: yes. Editor: no.

**3.7 Inverted rectangles fill a negative width** — `Rect_Intersect`, `Blit_ClipRects`, FillRect, ClearRect
- What: they "pass an inverted rectangle that needs no cut; FillRect and ClearRect with such a rectangle fill a negative width (the original crashes; the checks leave it out)".
- Evidence: [confirmed: code; the checks cover them]. Source: section 9, "Batch R1".
- Fix: yes. Editor: no.

**3.8 Unsigned bounds wrap through memory** — `World_DrawDirtyTiles`, the polygon outline
- What: "`World_DrawDirtyTiles` compares its tile bounds unsigned: a rectangle whose right or bottom edge lies before the frame makes the scan wrap through all memory (a check case ended the game on both sides; the game passes views inside the layer)"; "The polygon outline's line count is unsigned: a skip of count or more runs over the point array." The game does pass a view that starts above the layer: at 800 × 600 and zoom 1.0 in a level lower than the view (`Expired Error` after loading a save, entry 3.26) the first tile row is −1, the unsigned row test (`0x4e83b4`) skips the whole loop and no part is drawn [verified 2026-10-31, trace, entry 3.26].
- Evidence: [confirmed: code]. Source: section 9, "The world's frame: …"; `reimpl/src/layer_draw.cpp`.
- Fix: yes (relevant for phase 8, other view sizes). Editor: no.

**3.9 The menu background is rescaled only when both dimensions change** — `0x4f2d90`
- What: the scaled image is refilled "only when **both** its width and its height differ from +0x2a8's (a change of one dimension keeps the old scaled image)".
- When: 640×480 ↔ 800×600 change both.
- Evidence: [confirmed: code]. Source: section 9, "The world's frame: …".
- Fix: yes (relevant for phase 8, widescreen). Editor: no.

**3.10 Fixed-size locals that more teams or pages would overrun** — Capture the Flag's team loop, the save/load layout
- What: "Capture the Flag's team loop reads one name per team from two stack slots: a third team would read past them"; "the save/load layout keeps the old client widths of its pages in a 4-entry local that a fifth page would overrun".
- Evidence: [confirmed: code] (CTF); no tag of its own (save/load). Source: section 9, "The world's frame: …"; section 10, "Batch C3".
- Fix: yes. Editor: no.

**3.11 Player 0's layer outside its index** — the world's frame
- What: "Player 0's layer is looked up inline each time, with a null when its index is outside; every path that uses it dereferences it."
- Evidence: [confirmed: code]. Source: section 9, "The world's frame: …".
- Fix: yes. Editor: no.

**3.12 The sound list's `.ogg` → `.wav` fallback can never run** — sound file list slot 2 (`0x4e2030`)
- What: "If the name is not found and its extension is `.wav`, it retries with `.ogg`. A second branch, apparently meant for `.ogg` → `.wav`, compares with `.wav` again and so can never be reached (a bug in the original)."
- Impact: a missing `.ogg` is never replaced by a `.wav` of the same name.
- Evidence: [confirmed: code `0x4e2030`, `reimpl/src/music.cpp`; VM]. Source: section 6, "Pointer arrays".
- Fix: yes (opt-in). Editor: yes (a sound referenced as `.ogg` must exist as `.ogg`; `.wav` names may fall back to `.ogg`).

**3.13 Dead branches and dead tests** — `0x4e8080` (`0x6c1cf7`), the mission manager, `CCharacterListHolder`, the skeleton update
- What: "The second branch of `0x4e8080` depends on `0x6c1cf7`; that byte lies in the uninitialised data area and is written nowhere, so it is always 0 and the branch is dead (like `g_NoCutting`)"; the mission manager "first tests the count above 0xffffffff, never true"; "`CCharacterListHolder`'s index function has a dead test of the entry before the data (the count is compared with −1 unsigned)"; "The skeleton update's loop counter has a dead comparison with 49"; "`MatWindow::Draw` tests the frame's width against 640 and draws the same either way".
- Evidence: [confirmed: code …] (`0x4e8080`); [confirmed: code `0x505990`, `reimpl/src/ui_materials.cpp`; VM] (`MatWindow::Draw`, until 2026-10-29 [observed in the code]); no tag of their own (the mission manager, `CCharacterListHolder`, the skeleton update: reimplemented in VM-checked batches, the source names no addresses; not re-read one by one in the evidence pass). Source: `zax-formats/docs/FORMATS.md` 2.1 (surfaces, order); section 8 ("Mission objectives …", "Phase 3 remains, band D", "The skeleton AI, …"); section 9, "Phase 4 leftovers".
- Fix: no. Editor: no.

**3.14 `MessageLines_RemoveFirst` on an empty list** — the message lines
- What: it "wraps the count and moves lines past the end (never called so)".
- Evidence: [confirmed: code `0x506fb0`, `reimpl/src/rest_c.cpp`; VM]. Source: section 8, "Phase 3 remains, band C".
- Fix: yes (cheap). Editor: no.

**3.15 No test of failed allocations** — `CSphereOfZinOptions::RegisterFields`, `PointWeapon`'s RegisterFields, `Make`, `RowTable_Alloc`, the converting wrappers, the generator
- What: "`CSphereOfZinOptions::RegisterFields` writes the converter, units and help text through the first descriptor without checking its allocation"; "`PointWeapon`'s RegisterFields writes through a null class when the pool has no block"; "Make writes through null when the pool gives no block"; the row table: "without a null test (the exe writes through a failed allocation)"; the converting wrappers: "no null test of the new block"; the generator: "the pool block is used without a null test".
- When: only when memory runs out.
- Evidence: no tag of its own (the functions are reimplemented in VM-checked batches, the source names no addresses; a failed allocation is not reached by the checks; not re-read one by one in the evidence pass). Source: section 6, "Small object classes, …"; section 8 (several); `reimpl/src/core_rest_b.cpp`.
- Fix: no (out of memory). Editor: no.

**3.16 The cheat routines' null read** — `0x5a26f0` from the cheats
- What: a cheat item name missing from the catalogue "reads through a null pointer …, so the game crashes"; "all 38 names from `loadup`, `cash`, `GunIt`, `chargeit`, `mine` exist in the original data [verified]". The cheats are dead in the release (the launcher's `/Cheats` reaches them).
- Evidence: [confirmed: Zax.exe] / [verified]. Source: `docs/EXE_FINDINGS.md`, "Cheat commands". See also 2.34.
- Fix: no. Editor: no.

**3.17 Debug drawing: the second cross of `CGoToAI` is yellow** — `CGoToAI::DebugDraw` `0x4d18b0`
- What: "the second cross is meant to be red (a red `FieldColor` is made) but takes the yellow one".
- When: the AI debug drawing; when the release draws it is not stated.
- Evidence: [confirmed: code; VM]. Source: section 9, "Phase 4 remains, band B"; `reimpl/src/draw_rest4b.cpp`.
- Fix: yes (cosmetic). Editor: no.

**3.18 Network edge cases** — the DirectPlay layer, GameSpy
- What: "a null session or provider name makes the original use its stack buffer as it is (stale bytes); a session in two slots would be searched as index −1 (`data[-1]`)"; "`gcd_think` crashes on a message from the key server that starts with `\` and has no second `\` (`strchr` gives null, the length becomes negative and `strncpy` writes 7 million bytes)"; "`gs_encrypt` divides by the key length (an empty secret key would be a division by zero; the game's is always 'J3An3s')". In this exe the CD key check "goes nowhere" (the crack's port `htons(0xffff)`).
- Evidence: [confirmed: imports, slot numbers, message ids; …] (DirectPlay); [confirmed: code, the checks] (GameSpy). Source: section 6, "Net game, net session, …"; section 11, "GameSpy SDK (2001), reimplemented".
- Fix: yes (when the network layer is replaced). Editor: no.

**3.19 Map caching on odd states** — `CSwappedLayerFilenameMappingTable`, `FlushAll` `0x589890`
- What: "saving into an expired mapping or loading/saving one without files ends the game"; "FlushAll loops for ever on a name without a loaded layer, as in the original."
- Evidence: [confirmed: code, strings; checked with a fake world]. Source: section 7, "CModel, the model list …, the map caching table".
- Fix: yes. Editor: no.

**3.20 `FormatLength`'s unknown specifiers** — `FormatLength` `0x585490`
- What: "Characters it does not know (x, o, l, h, n, X and even the terminator) count as flags, so the specification goes on behind them; %C consumes an argument without moving on and never ends."
- When: only the game's own format strings reach it.
- Evidence: [confirmed: code `0x585490`, `reimpl/src/zstring.cpp`; VM]. Source: `reimpl/src/zstring.cpp`.
- Fix: yes. Editor: no.

**3.21 Toolkit setups the game does not create** — the window toolkit, list boxes, dialogs
- What: "Ctrl+Tab in a tab control without tabs, a radio ring without a parent, a resize grip or a frame drag without a parent, a slider drag on a slider exactly as wide as its thumb + 1, Escape with a capture and no screen window"; `SelectId` `0x404390` "reads before the row array", `ShowId` `0x406920` "calls itself for ever", `UpdateBar` `0x4074f0` "writes through a null bar", the sort's compare `0x406c60` "faults on a text with fewer tabs" and cuts the texts in place, `SetSource` `0x406fc0` "with no source selects through it"; the first character, the "Inventory Window" sequence, the dialogue window's frame sequence and the end game credits' font "read without a test"; "the popup menu's Run reads the chosen item without a test"; "`Dialog_AddLine` reads the font's height without a test"; "`ScreenImage::Show` and `SequenceWindow_Ctor` dereference a missing frame or sequence"; the save/load screen's header resize "reads the title bar without a test (it always has one)".
- Evidence: [confirmed: VM runs of the first versions of the checks, the crash on the original's side] (list boxes); [confirmed: code `0x404390`, `0x406920`, `0x4074f0`, `0x406c60`, `0x406fc0`, `reimpl/src/ui_listbox.cpp`; VM]; no tag of their own (the other toolkit and dialog cases: reimplemented in VM-checked batches, not re-read one by one in the evidence pass). Source: section 9 ("The window toolkit", "List boxes, …", "The rest of the user interface", "Phase 4 remains, band C"); section 10, "Batch C3".
- Fix: yes. Editor: no.

**3.22 Scene lists copied shallowly** — `CBeginNonInteractiveSequenceAction`'s scene list
- What: "the scene list's append and the conversion copy the scenes shallowly (an owned action of an old scene would be deleted with the old array while the new one points to it; the game's scenes never have one there [assumption])".
- Evidence: [assumption] (that the game's scenes have none; stays: about the data). Source: section 8, "The non-interactive sequences, …".
- Fix: yes. Editor: maybe (scenes with owned actions in old-format files).

**3.23 Binary writing of the save game statistics ends the game** — `CZaxSaveGameStats`
- What: "writing a `CZaxSaveGameStats` of the running game as binary ends the game with the original code as well … [assumption: its image field 'Screen Shot' without a screenshot]".
- Evidence: [confirmed: VM] (the crash); [assumption] (the cause; stays: needs a VM test with a screenshot in the statistics). Source: section 6, "Object table, object stream, …".
- Fix: maybe. Editor: no.

**3.24 A zoom clamped by a small map sticks in every later level** — `World::SetZoom` `0x4f6770`, its calls with the current zoom at `0x4f2551` (`World::ScreenSizeChanged` `0x4f2430`) and `0x4f4810` (`World::SetPlayerLayer` `0x4f4700`), `World::ZoomKeys` `0x4f3200`
- What: `SetZoom` limits the zoom so the zoomed view fits player 0's tile map (width and height − 1, rounded down to 128 pixels) and stores the limited value as the player's zoom (+0xd8); there is no separate "chosen" zoom. `ScreenSizeChanged` and `SetPlayerLayer` (a player's level set) call it again with that stored zoom, so in a larger level the limit is not lifted. `ZoomKeys` lets the zoom drift back to 1 only above (1 − lower limit) · 0.75 + lower limit (`0x5f14e8` = 0.75; the lower limit `0x618034` is 320 / screen width); a zoom below that stays.
- When: entering a level narrower than the view's width or lower than its height. The tile map has the level's size (`0x53f680` from the header's Width/Height, `0x4e69f0`). Of the 112 levels only `Levels/Expired Error.zax` (641×513, the level the game loads for an expired map mapping, `docs/DECOMPILATION.md` "Map caching") is narrower than an 800-pixel view [verified 2026-10-29: the Width/Height of all 112 levels]; at 800×600 its limit is 640 / 800 = 0.8, below the drift band's 0.85, so 0.8 stays in every later level until the zoom keys change it [verified 2026-10-29, VM, `/Original`, 800×600: run `20261029-215158-cdd5`, a test copy of `Expired Error` (unchanged but for a proximity trigger with a `CRelocateAction` to `ACT 1/M3/03 SI Cave`, start point `Spawn Point Cave Exit`), started with `/Spawn "Start Here"`; reference run `20261029-215007-2f32`, a direct start at that start point: 15 s and 30 s after the switch the view is the direct start's magnified by exactly 1.25 = 1 / 0.8 about the screen's centre (best match of scaled copies 1.0–1.4 in steps of 0.025), no drift back. A start into `Expired Error` without `/Spawn` puts Zax at the level's top-left corner: the team start point "Game Start " does not exist there (run `20261029-215004-b9c6`)]. At 640×480 no level is affected. Loading a save sets the zoom back to 1.0 without resizing the zoom frame (entry 3.26). The mod reports 0.46 at 1920×1080 after entering the mothership (`Inside Ship`, 1024×760) from the overworld.
- Evidence: [confirmed: code `0x4f6770`, `0x4f3200`, `reimpl/src/world_rest.cpp`; `0x4f2430`, `reimpl/src/c4_rest.cpp`; `0x4f4700`, `reimpl/src/world_class.cpp`; VM]; the call sites `0x4f2551` and `0x4f4810` read in the disassembly. Source: reported by the ZaxHiRes mod (`NOTES.md`, runtime probe), until 2026-10-29 entry 5.2 [unverified].
- Fix: candidate (opt-in: keep the player's chosen zoom apart from the limited one; relevant for phase 8, widescreen). Editor: no.

**3.25 The ground tile cache is a fixed 4.5 MB** — the tile cache `0x7110e0` made by `CPlasmaTileMap`'s constructor `0x53f490` (`push 0x480000` at `0x53f4f0`, `HandleHeap::Ctor` `0x4d27c0`), `CPlasmaTileMap_PrepareFrame` `0x540790`
- What: the cache is one heap of 0x480000 bytes shared by all tile maps (made when the first tile map is constructed, freed with the last). `0x540790` puts a tile's frame of (cell size + 1)² pixels in the screen's depth on a handle of it, allocated when the handle is empty (`0x4d2680`); every cell has up to three handles (the ground and two transition layers, +0x2d24). At 16 bits a tile takes 65 × 65 × 2 = 8,450 bytes, so the cache holds at most 558 tiles (until 2026-10-29, from the mod: "576 tiles of 64x64x16 bit" — not quite, the frames are 65 pixels wide and high). The mod reports that 1920×1080 needs about 527 tiles against about 154 at 800×600, that the cache then thrashes and about 150 tiles per frame are built again (about 40 fps; 80–90 fps with a cache sized for the resolution) [unverified: the mod's profile and counters].
- When: only at resolutions the game does not offer; at 640×480 and 800×600 no effect is reported.
- Evidence: [confirmed: code `0x53f490`, `reimpl/src/plasma_tile_map.cpp`; `0x540790`, `reimpl/src/tile_map_draw.cpp`; VM] (size, sharing, tile size); the thrashing [unverified]. Source: reported by the ZaxHiRes mod (`NOTES.md`, perf profile and runtime counters), until 2026-10-29 entry 5.5.
- Fix: yes for widescreen (size the cache by the resolution). Editor: no.

**3.26 Loading a save leaves the zoom frame at a map-limited size while the zoom is 1.0** — `World::ReadSaveGame` `0x4f3e70` (its slot-78 calls `0x4f3f38` (binary) / `0x4f3f76` (text) and the write of 1.0 at `0x4f4109`), `World::SetPlayerLayer` `0x4f4700` (its `SetZoom` call `0x4f4810`), `World::SetZoom` `0x4f6770`, `World::DrawFrame` `0x4f2d90`, the camera `World::SetViewTarget` `0x4f5dd0`; reached from `CZaxWorld::LoadGame` `0x5a0e70` → `CZaxWorld::ReadSaveGame` `0x59add0`; why Zax is not drawn: `CLayer_ClampView` `0x4ed7f0`, `Layer_DrawPass` `0x4e8900`, `World_DrawDirtyTiles` `0x4e8080` (its unsigned row test at `0x4e83b4`, entry 3.8); the repairs: `CPlayerWalkingControlAI::Keys` `0x544260`, `World::ZoomKeys` `0x4f3200`, `VideoDialog::Dtor` `0x591750` → `World::ScreenSizeChanged` `0x4f2430`
- What: `World::ReadSaveGame` deletes every layer, reads the layer holder and then hands player 0's level to every player through slot 78 (`SetPlayerLayer`), which calls `SetZoom` with the player's stored zoom (+0xd8). `SetZoom` limits that zoom to the loaded level's tile map (entry 3.24) and, when the limited value differs from the stored one, resizes the zoom frame +0x2a4 to the view times the limited zoom and stores the limited zoom. After both reads `0x4f3e70` puts every player's views on the level's view position and writes **+0xd8 = 1.0 directly** (`mov dword ptr [edi + 0xd8], 0x3f800000` at `0x4f4109`), without `SetZoom` and without resizing the zoom frame. Afterwards the zoom is 1.0 while the zoom frame keeps the size of the zoom that was in effect during the read (the map-limited one, or a 3.24 zoom carried over from before). Two consequences follow, both from code paths that are correct on their own:
  - Drawing: `World::DrawFrame` uses the zoom frame only when the zoom is not exactly 1.0; at 1.0 the level is drawn 1 : 1 straight into the 800 × 544 view (`0x4f2800`, the letterbox proxy of the frame +0x2a8). In a level smaller than the view only the level's own area (640 × 512 for `Expired Error`) is drawn; nothing clears the rest of the view, so the pixels x 640–799 (and y 512–543) keep whatever was on the screen before (the last frame of the previous level, or the load dialog). This is the state the zoom limit of 3.24 exists to prevent.
  - Camera: `World::SetViewTarget` `0x4f5dd0` centres the view by the zoom frame's size, not the view's. With a 640 × 435 zoom frame at zoom 1.0 Zax stands about 80 pixels left of and 60 pixels above the view's centre (view origin = target − (320, 205) instead of − (400, 264), measured in the traces below). This is the mod's "player at (463,246)" at 1920 × 1080 (its 896 × 477 zoom frame).
- When: at 800 × 600 whenever a save of `Expired Error` (the only level narrower than 800 pixels, 3.24) is loaded, whatever came before; and whenever any save is loaded while a 3.24 zoom is in effect (the frame then keeps the old limited size). At 640 × 480 no level is affected (`Expired Error`'s 640 pixels fit, the zoom stays 1.0 with a 640 × 424 frame). The mod reports it at 1920 × 1080 in the mothership (`Inside Ship`, 1024 × 760). The bad state lasts until something calls `SetZoom` with a value whose limited result differs from 1.0:
  - a zoom-in or zoom-out key (keypad + / −): `CPlayerWalkingControlAI::Keys` `0x544260` calls `SetZoom` with the zoom ± one step; in `Expired Error` it is limited to 0.8 and the frame is rebuilt (the 3.24 state: the level magnified 1.25 and drawn whole), in a level of normal size the frame becomes 99 % of the view and the camera is centred again [verified 2026-10-31: keypad − after the bad load in `Expired Error`, trace `SetZoom` → limited 0.8, frame 640 × 435, Zax drawn magnified (run `20261031-215452-634e`); in `SI Cave` see the runs below];
  - entering a level smaller than the view (its `SetPlayerLayer` limits 1.0 and resizes);
  - a display mode change in the game's Configure → Video dialog: `VideoDialog::Dtor` `0x591750` sets the chosen resolution and calls the reporter's slot 8, `World::ScreenSizeChanged` `0x4f2430`, which sizes the zoom frame to the new view times the stored zoom and calls `SetZoom` with that zoom [verified 2026-10-31, run `20261031-221704-b8ca` (trace) and `/Original` run `20261031-223110-6770`, the same pictures: after the bad load Esc, C, V, R (the resolution slider; the helper's mouse clicks do not reach the game's cursor, the slider did not move in run `20261031-220708-77e4`), Left, Esc, Esc, R (Resume). At 640 × 480 the frame becomes 640 × 424 at zoom 1.0 (`SetZoom(1.0)` is not limited there: 640 fits, 424 < 512), the camera at the level's origin, Zax drawn normally. Switched back to 800 × 600 the same way (Right instead of Left): the frame 800 × 544, then `SetZoom(1.0)` limited to 0.8 resizes it to 640 × 435 and centres the view: the 3.24 state, the level magnified 1.25 with Zax drawn. In a level of normal size the first change already gives a frame of the view's size and a centred camera (code; not run)].
  It does **not** end with a level change to a level of normal size: `SetPlayerLayer` calls `SetZoom(1.0)`, nothing is limited, the difference to the stored 1.0 is 0 and `SetZoom` returns early, so the camera stays off centre in every later level. Nor with the **zoom reset** key (keypad *, action 0xe of the default bindings `0x59c550`): `CPlayerWalkingControlAI::Keys` calls `SetZoom(1.0)` only when the zoom differs from 1.0, and after the load it is exactly 1.0, so nothing is called (trace: both presses, held 1 s and 2.5 s, logged "zoom 1.0, no call"; zoom, frame 640 × 435 and the picture unchanged, Zax still not drawn; run `20261031-215452-634e`, `/Original` `20261031-224513-18f9`) [verified]. The other reset, `World::ZoomKeys` `0x4f3200` (bit value 4 of the player record's +0xe0, which calls `SetZoom(1.0)` unconditionally), has no key in single player: +0xe0 is written by the block reader `0x4f7820` (vtable `0x5f9184`) from a network packet [confirmed: code, `reimpl/src/c4_net.cpp`; no other writer in the reimplementation's source, the exe not scanned for one] and stayed 0 in every traced frame of both runs.
  Not seen: the "camera in the top-left corner" as the mod words it at 800 × 600 — in `Expired Error` the view stays at the level's origin (the clamp `CLayer_ClampView` `0x4ed7f0` sets x to 0 for a view wider than the map) and the drawn 640 × 512 area stays black **without Zax** in every run (also 25 s after loading and after walking); at 640 × 480 the same save shows Zax at zoom 1.0.
  **Why Zax is not drawn** [verified 2026-10-31, trace, runs `20261031-215452-634e` and `20261031-221704-b8ca`]: at zoom 1.0 the view is 800 × 544 but the level's clamp area is (513 − 1) & ~0x7f = 512 pixels high, so `CLayer_ClampView`'s upper limit for y is 512 − 544 = −32: it turns every y ≥ 0 into −32 (a negative y becomes 0 and is not tested against the limit). The layer's visible rectangle (slot 36: the clip rectangle `0x4f2800` sets, plus the frame offset) therefore starts at y = −32. `Layer_DrawPass` `0x4e8900` aligns it to the dirty tile frame's 64-pixel tiles and hands `World_DrawDirtyTiles` `0x4e8080` the view (0, −64, 832, 576); its first tile row is −64 >> 6 = −1, and the row loop compares it with the last row unsigned (`cmp esi, eax` / `jae` at `0x4e83b4`, entry 3.8), so 0xffffffff is never below the last row and the loop runs zero times. Every part of the queues is drawn only inside that loop (per run of dirty tiles), so nothing is drawn, though Zax's two parts are found and queued; the ground is drawn separately by the tile map with the widened rectangle whose top is clamped to 0 (`Expired Error`'s ground is black). The trace per frame: before the load (zoom 0.8, frame 640 × 435) the draw pass's rectangle top 0, tile rows 0..8, 2 runs and 6 draws; the first frame after the load still top 0 with Zax drawn, then every frame top −32, rows −1..9, 0 runs, 0 draws, 2 parts queued, until a repair above; after it top 0 and draws again. At 640 × 480 the limit is 512 − 424 = 88 and y stays at 0 or above (after the switch to 640 × 480 one frame still had top −32, then rows 0..8 and Zax drawn). (A signed test would not help: the loop would start at row −1, before the dirty frame's rows.)
- Evidence: [verified 2026-10-31, VM, ARM, 800 × 600 × 16 unless stated, test copy `Test/ExpiredZoom` of `Expired Error` (unchanged but for a proximity trigger at (480, 181) with a `CRelocateAction` to `ACT 1/M3/03 SI Cave`, start point `Spawn Point Cave Exit`, built with the editor's `catalog.make_node` + `transition.make_exit`; not in the repository), start point "Start Here"]. A temporary trace build of the reimplementation (logging around `0x4f6770`, `0x4f2430`, `0x4f4700`, `0x4f3e70`, `0x5a0e70`, `0x4f5dd0` and the drawing state of `0x4f2d90`: player 0's zoom +0xd8, the zoom frame's and the view's size, the camera; not committed) ran hooked (`/Xriginal`); its screenshots equal `/Original`'s on the same steps (runs `20261031-210039-36d5` / `-210039-ffbd`: at most 0.79 % of the pixels differ, the loaded view 0.01 %; `20261031-211347-eda0` / `20261031-211957-bff5`: at most 0.28 %), so the trace does not change the behaviour. The runs:
  - F11 in `Expired Error` (zoom 0.8, frame 640 × 435), through the trigger to `SI Cave` (`SetPlayerLayer` → `SetZoom(0.8)` returns early: frame and zoom kept, 3.24), F12: `SetPlayerLayer` → `SetZoom(0.8)` in `Expired Error` returns early, then `0x4f4109` writes 1.0 with the frame still 640 × 435; the view x 640–799 keeps `SI Cave`, the rest black without Zax, also after walking (`20261031-210039-36d5`, the 2026-10-29 case `20261029-221803-7e23` reproduced). A zoom key then rebuilds the frame at 0.8 (Zax visible, magnified).
  - The same, then back through the trigger into `SI Cave`: full width drawn at zoom 1.0, the frame still 640 × 435, the camera off centre (view origin = Zax's position − (320, 205)) (`20261031-211347-eda0`).
  - A level of normal size before: a direct start in `SI Cave` (zoom 1.0, frame 800 × 544), F12 with the `Expired Error` quick save of the run above: `SetZoom(1.0)` is limited to 0.8 and resizes the frame to 640 × 435, then 1.0 is written: the same picture (`20261031-211621-629e`).
  - From the main menu, a fresh process (no game before): Load Game, the quick save selected, the Load button (`20261031-211815-a319`), or F12 with the load dialog open (`20261031-211621-d4a3`); and in a game of `SI Cave`, Esc, End Game, Load Game, Load (`20261031-211815-507c`): the same picture each time, the strip x 640–799 keeps the load dialog.
  - A save of `SI Cave` made while the 3.24 zoom 0.8 was in effect, loaded with F12: the zoom becomes 1.0 (3.24's magnification ends) but the frame stays 640 × 435, so the camera is off centre (`20261031-211957-bfcd`); a zoom key there resizes the frame to 792 × 538 and centres the camera again (`20261031-212238-9fc9`; `/Original` `20261031-212238-6fab` shows the same, the zoom reached depends on how long the key is held).
  - 640 × 480 (`--mode 640x480x16`), the first run's steps: the zoom stays 1.0 with a 640 × 424 frame throughout, the loaded `Expired Error` shows Zax normally (`20261031-211347-3ec1`).
  - 2026-10-31, the second trace (the same kind of temporary build, also logging the draw pass `0x4e8900`, the dirty tile scan `0x4e8080`, `0x4f2430` and the zoom keys; started at "Start Here" of `Test/ExpiredZoom`, then F12 with the `Expired Error` quick save of the runs above): the repairs and the missing Zax as described above (`20261031-215452-634e`: keypad * twice, then keypad −; `20261031-221704-b8ca`: the display mode to 640 × 480 and back). `/Original` runs of the same steps show the same pictures (`20261031-224513-18f9`, `20261031-223110-6770`: at most 0.33 % of the pixels differ per screenshot).
  [confirmed: code `0x4f3e70`, `reimpl/src/world_rest.cpp`; `0x4f4700`, `reimpl/src/world_class.cpp`; `0x4f6770`, `0x4f5dd0`, `reimpl/src/world_rest.cpp`; `0x4f2d90`, `0x4f2800`, `reimpl/src/world_frame.cpp`; `0x4ed7f0`, `reimpl/src/layer_view.cpp`; `0x4e8900`, `0x4e8080`, `reimpl/src/layer_draw.cpp`; `0x544260`, `reimpl/src/ai_player.cpp`; `0x4f2430`, `reimpl/src/c4_rest.cpp`; `0x591750`, `reimpl/src/ui_settings.cpp`; VM]; the addresses `0x4f3f38`, `0x4f3f76`, `0x4f4109`, `0x4e83b4` read in the disassembly. Source: reported by the ZaxHiRes mod (`NOTES.md`, runtime probe; its explanation "SetZoom returns early when the zoom is unchanged, so the surface is never fixed" names the early return correctly but not the cause, the direct write at `0x4f4109`), until 2026-10-31 entry 5.1.
- Fix: candidate (opt-in, relevant for phase 8, widescreen, where many levels are smaller than the view; together with 3.24): at the end of `0x4f3e70` set the zoom through `SetZoom` instead of the direct write (for example store a value that cannot match, then `SetZoom(1.0)`, which limits it to the loaded level and rebuilds the frame and the camera), or make `SetZoom` also compare the zoom frame's size with the view times the zoom before returning early. Editor: no.

## 4. Harmless oddities, typos, leaks

### Typos the file formats depend on

**4.1 Misspelt class, field and key names**
- What: "CInventoryItemDefiniton" (sic, the alternative class name of `CInventoryItemDefinition`, `0x4d5620`); "Recharge Uints" (sic, `CRechargeShieldAction`); "Force Invisable" (sic, `CSameAsParentAI`); `SyfaceTypeSoundItem`; the alias "CBeginNonInteraciveSequenceAction"; the class names `CCameraTrakerAI` and `CPointOfIntrestAI`; Zax.CFG "Master Voume", "SFx Voume" (sic) and "Sounds Mixing Chanels"; the vtable comment of `ai_classes.h` names one more for `CTouchingPolygonTriggerAI`.
- Evidence: [verified 2026-10-29: every name, and the alias "CTouchingPolyongTriggerAI" that the vtable comment names, is a string in `gamedata/Zax - German/Zax.exe`] (taken from the registrations). Source: section 8 ("Inventory items and definitions", "The skeleton AI, …", "The game types Capture the Flag …", "The non-interactive sequences …"); section 9, "The game side of the sound"; `docs/EXE_FINDINGS.md`, "Configuration".
- Fix: no (part of the file formats and of Zax.CFG). Editor: no warning; it must write these names exactly as the game does.

### Leaks and double frees

**4.2 Leaks** — the mission manager, DirectPlay, `ImageFile_Read`, `ReadBinary`, the stream
- What: the mission manager keeps a copy of the old objective that "is not deleted, a leak of the original"; "a failing `SetSP` of the global address leaks the temporary address (as does every error after it before `Connect`)"; "`SetProviderByName` … leaks the list when the name is missing"; `ImageFile_Read` at an odd depth (2.26); `ReadBinary` with −1 (2.9); a multiplayer game type's slot 23 makes "a new match state – the old one is not deleted"; `CSetMusicAction`'s `0x55e090` "loads a record without a range test or a test for an object already there"; the `new[]` arrays: "shrinking in place copies the tail down and leaves the objects behind the count alive".
- Evidence: no tag of its own (section headings [verified …, VM]); [confirmed: code `0x55e090`, `reimpl/src/random_misc.cpp`; `0x58c6f0`, `reimpl/src/image_file.cpp`; VM] for the record load and `ImageFile_Read`; the others are reimplemented in VM-checked batches without addresses in the source, not re-read one by one in the evidence pass. Source: section 6 ("Value slots without file I/O", "Multiplayer game type base class …"); section 8, "Mission objectives, …", "The non-interactive sequences, …"; section 10, "Batch P5a-2".
- Fix: yes (no behaviour change). Editor: no.

**4.3 The WAVE reader frees with the wrong allocator and twice** — the sound readers
- What: "the WAVE reader frees a pool-allocated format with operator delete when it converts (and the converted format, a heap block, with the pool's delete), its destructor gives its stream buffer back twice."
- When: converting compressed (ADPCM) WAVE data for the first part; the destructor always.
- Evidence: [confirmed: code; checked with fakes and the level's files]. Source: section 9, "The game side of the sound".
- Fix: yes. Editor: no.

**4.4 Destructors that do not free** — `CSpawnEffectAction`, `CPlaySoundBySurfaceTypeAction`'s `SyfaceTypeSoundItem`s
- What: `CSpawnEffectAction` queues "Action to execute After Effect" "and frees neither it nor `CActivateAction`'s, `CAddAIAction`'s, `CAddEffectAction`'s owned objects in its destructor"; the inline `SyfaceTypeSoundItem`s are "not freed by the destructor".
- Evidence: [confirmed: code `0x4c3180`, `0x4c0290`, `reimpl/src/action_slots2.cpp`; VM] (the deleting destructors of `CSpawnEffectAction` and `SyfaceTypeSoundItem`). Source: section 8, "Behaviour of the action classes 0x4c0000–0x4cafff …".
- Fix: yes. Editor: no.

### Uninitialised bytes and stack contents

**4.5 Uninitialised bytes in written data** — the image kind's slot 26 `0x4a9740`, "MiniMap Color" (`0x53f010`), `CreateArray`, `CFrameOverlayTextOverlay`
- What: slot 26 "writes a 0x24-byte frame header from the stack whose bytes 0xa–0xb are never set" (they "depend on the callers' stack"); `0x53f010` "leaves byte 0xa of the colour unwritten, so the default colour of 'MiniMap Color' carries one byte of uninitialised stack (copied into every new category)"; `CreateArray` "takes heap memory and `InitFields` sets no values"; "Text HotSpot Coordinates" have "no default: the constructor leaves them as the memory was"; `CRenderablePolygon`'s constructor leaves "the base fields … as the heap had them"; the descriptor's flag +0x24 is left uninitialised by its constructor; "Colour copies skip bytes 7 and 0xb".
- Impact: files written twice can differ in these bytes.
- Evidence: [confirmed: VM] (slot 26, `0x4a9740`); [confirmed: code `0x53f010`, `reimpl/src/ai_pickup.cpp`; VM] ("MiniMap Color"); no tag of their own (the others: reimplemented in VM-checked batches, not re-read one by one in the evidence pass). Source: section 6 ("Field descriptors, base class", "Field kinds", "Object table, …", "Definition classes, …"); section 9, "Effects and text: …", "Phase 4 remains, band B".
- Fix: yes (write zeros). Editor: no.

**4.6 Stack contents passed on** — `ScreenFrame_SetPalette` `0x568870`, `0x4f2620`, the control configuration, `CSayQuip`, the network frame
- What: "`ScreenFrame_SetPalette` without the 'all' flag reads the system palette's first and last ten entries into the same ten slots, so the DirectDraw palette's entries 245–255 are stack contents (only their flag byte is set); the game passes the flag 1 everywhere except the constructor's path"; "`0x4f2620` pushes the slot 61 byte as a whole stack dword"; "the control configuration's INI read keeps for fields `sscanf` does not find the values of the previous binding (the first: the stack)"; CSayQuip's "decision's dword carries three bytes of the stack"; "the frame passes the network block, as its 'changed' argument, the dword of its second argument's stack slot with the low byte replaced"; "`Zax_DropSecondary` passes the aim as x, y, x (the third dword is the first again) and calls the projectile's slot 41 before testing it, and tests no weapon item"; the File buffer writer's "signature buffer … is the slot of the name argument"; "the bit kind's read leaves the stream pointer's low byte as the value when the read writes nothing"; `Hex_Decode` "sign-extends the digits (characters ≥ 0x80 decode as letters)".
- Impact: effects in the game not described.
- Evidence: [confirmed: code] (band C of phase 4); [confirmed: code `0x568870`, `reimpl/src/screen_frame.cpp`; `0x4f2620`, `reimpl/src/world_frame.cpp`; `0x5ab9a0`, `reimpl/src/zax_behaviors.cpp`; `0x587ff0`, `reimpl/src/misc_rest.cpp`; VM]; no tag of their own (the control configuration, `CSayQuip`, the network frame, the File buffer writer, the bit kind: reimplemented in VM-checked batches, not re-read one by one in the evidence pass). Source: section 6 ("File buffer and archive streams"); section 8 ("The game's own behaviours …", "The non-interactive sequences …", "Phase 3 remains, band A", "Phase 3 remains, band C", "The rest of the entity behaviours …"); section 9, "The world's frame: …", "Phase 4 remains, band C".
- Fix: yes. Editor: no.

### Other oddities

**4.7 Self loops in every `.way` file** — `0x538ae0`, `cmp esi, [esp+0x40]` at `0x538d34`
- What: "after inserting a node the game looks for all nodes within `MaxDistToConnect` (including the new node itself) and connects them to it. `cmp esi, [esp+0x40]` (`0x538d34`) is supposed to skip the new node, but it compares the position in the hit list instead of the node number. So almost every node gets an edge to itself". In all 112 files at 245,919 of 246,102 nodes.
- Impact: "The loops are meaningless for path finding"; the game's rebuilt `.way` keeps them ("self loops included").
- Evidence: [confirmed: code `0x538ae0`, numbers verified]. Source: `zax-formats/docs/FORMATS.md` 4; `zax-formats/docs/OPEN_ISSUES.md` (format gaps); section 7, "Way point map, path, way points, way point polygons".
- Fix: no (harmless; a fix would change the `.way` bytes). Editor: no (the editor leaves the `.way` to the game).

**4.8 `File_Exists` only counts files on disk** — `File_Exists` `0x4a1650`
- What: "if a mounted archive knows the path, the answer is the byte `ZZipList::Exists` returns, i.e. with check_disk whether the archive file exists, otherwise 0. A file that is only in Data.dat therefore does not 'exist'; only the disk counts for it." `Files_Count` `0x4a12d0` "counts a file on disk and in an archive twice".
- Evidence: [confirmed: code `0x4a1650`, `reimpl/src/file_buffer.cpp`; `0x4a12d0`, `reimpl/src/file_search.cpp`; VM]. Source: section 6, "File search, …", "Disk and memory streams, File_Open, path helpers".
- Fix: maybe (callers such as `Object_WriteFile` rely on it). Editor: no.

**4.9 Clones that are not full copies** — the field kinds' slot 1
- What: "the clone of the object kind does not copy +0x28 (`0x418740`), the image clone copies only the base part (`0x4b5970`), the polygon clone clears +0x28 (`0x548010`), the elevation kind clones through the enum int copy constructor and so gets its vtable (`0x53f240`), `0x5fd8b0` has no clone (`_purecall`), `0x601c30` returns none (`0x4011e0`)"; "Equal limits mean 'no limits'"; the dynamic property `Add` "copies +4 of the elements too (CObject's own dword), so a source with dynamic properties would share them"; `DynamicAssign` copies "into entry i even if an existing entry with the descriptor was found at another index"; growing a `new[]` array in place "calls slot 19 twice on every new element".
- Evidence: [confirmed: code `0x418740`, `0x4b5970`, `0x548010`, `0x53f240`, `reimpl/src/field_kinds.cpp`; VM]; no tag of their own (the limits, the dynamic properties, the `new[]` array: in `reimpl/src/field_slots.cpp`, VM-checked, not re-read one by one in the evidence pass). Source: section 6, "Field descriptors, base class", "Field kinds", "Value slots without file I/O"; section 9, "Effects and text: …"; `reimpl/src/field_slots.cpp`.
- Fix: maybe. Editor: no.

**4.10 Drawing details without a described effect** — the RLE pixel tests, the hit test, the sprite and frame helpers
- What: the RLE pixel tests "count a segment `[start, start + n)` as hit when `start + n >= x`, so also the pixel just past it"; "IsPixelSet on a plain frame … the origin is subtracted twice"; the hit test "subtracts the item's position before calling slot 4, which subtracts it again"; "TintRect (slot 57) does not subtract the origin; FillColor (slot 26) on 8 bits ignores the pitch (like Fill) and computes the palette index four times"; "`Fill` sets bytes per pixel × width × height bytes from the buffer's start and ignores the pitch"; the MMX screen rows read the rest "from where the groups started (the destination is advanced, the source is not)" (whether the game's widths leave a rest is not stated); "Pixel16_BuildTables and BuildInvColorTable take a black palette entry as blue 8"; "a circle's points add the unrounded scaled y to the centre, the line's and the cylinder's second points round it first"; the quantiser's "first box's variance is 1, not computed", "an empty histogram leaves the quantiser's bounds undefined"; "`Nearest` of an empty range returns first · 2; the lightness remap ignores its third and fourth arguments"; "slot 78's x and y subtract a value from itself (0)"; `ImageFile_Write`'s "run search compares the row's last pixel with the bytes behind the row".
- Evidence: [confirmed: code; the checks cover them] (R1, R5); [confirmed: code] (the others). Source: section 9 (R1, R5, "Sprites and entities", "The last effects", "Phase 4 remains, band B", "Phase 4 remains, band C").
- Fix: maybe, one by one. Editor: no.

**4.11 Toolkit and UI details** — the window toolkit, the menus, the message lines
- What: "the check box disabled reads the opacity of `g_BlendTransparentMask` twice and restores `g_BlendBlackBias` with it" (the title drawing likewise); "a horizontal own scroll bar of a window sets its height to the parent's client width (`0x40f5d0` where the width was meant) and places its top with its width"; "the scroll bar's rect cut `0x412fb0` tests a horizontal bar's edges for equality only"; "joining a radio group takes the partner's tab stop"; "the text label's constructor sets its title five times"; "the slider's default step is (count + 10) / 20"; "the 'Window Title Tile' texture loads under the name 'Window Texture'"; "the INI position passes the size flag's upper bytes to `SetTransparent`; placement 19 puts the window at the parent's width; the grid's wheel slot takes a second argument it ignores"; `MatGrid::Make` "compares the dynamic property count of the inventory's slot 26 answer with the catalogue index", `MatGrid::Fill` "marks the icon record's `+0x7d`/`+0x7e` by the unclamped index while it loads the clamped one"; the message lines' `Update` "skips the line that moves into a removed line's place", their space skipping "stops at the previous line's length after the first line" (the fonts' word wrap likewise), "every line's heap copy holds the whole message"; "The draw item pool grows by 300 items and then by one more each time `used` reaches the count below 300"; the blend modes' one-entry `FindBlitter` cache is state between draws.
- Evidence: [confirmed: code `0x413940` (the bar's rect, calling `0x40f5d0`), `0x412fb0`, `reimpl/src/ui_scrollbar.cpp`; VM] (the scroll bar); [confirmed: code `0x4a6790`–`0x4a6ae0`, `reimpl/src/font_layout.cpp`; VM] (the word wrap, until 2026-10-29 [observed in the code]); [observed in the code] (the materials generator); [confirmed: code; VM] (the message lines); no tag of their own (the rest: reimplemented in VM-checked batches, not re-read one by one in the evidence pass). Source: section 8, "Phase 3 remains, band C"; section 9 ("The window toolkit", "Effects and text: …", "Sprites and entities", "Phase 4 remains, band B", "Phase 4 leftovers"); section 10, "Batch C3".
- Fix: maybe, one by one. Editor: no.

**4.12 Game logic details without a described effect** — entities, AIs, items
- What: "Slot 65/66 combine whole bytes (AND/OR), not booleans"; "slot 97 of CEntityAnimated asks for the creator (slot 68) and does not use it"; "slot 88/89 search from the object after/before `after` and give 0 when it is not attached; slot 81 replaces the first attached object of the same class"; "`CSkeletonAI`'s slot 29 also checks for a 'CFailedTargetMessage' after a message the patrol branch handled"; "the chase investigate AI's Begin reads the entity's position and drops it"; "the backward trigger play tests the forward bit when the frame index is locked"; `CTurretAI`'s slot 31 "says 'Please change all CTurretAIs to Sentinon AIs' every time"; `CPatrolPathAI`'s "Order" 0/1 "rewrites the field itself at each turn (back and forth and forth and back alternate)"; `CPatrolAreaAI` registers "TimeOfLastChangeDirectionDueToCollide" "which no reimplemented function reads or writes [confirmed for this range]"; `CTurretAI`/`CSentinonAI`/`CAutoGunAI` "copy the category mask `0x6c29a0` into 'Valid Targets'"; "`CEntityBehaviorMoveToPos` creates its `CPathCollideChecker` … only when +0xb4 is 0 after InitFields" (`CPlayerWalkingControlAI`'s path checker likewise); `CInventory::Update` "bounds its category loop by `*0x6c0814` …, the constructor sizes the category data by `*0x6c0834`"; "The item slot 33 … passes the inventory as the third argument of the definition's slot 26 only when ammunition came with the weapon, otherwise the item itself"; the deleting destructors of `CInventoryBase` and `CInventoryItemDefinition` "have no array path"; the binding text "cuts two characters even from a one-character text (writing before the buffer)"; "the difficulty's slot 7 runs (on null in the original when that record is being loaded)"; the inventory's `Repair` `0x4262e0`: "after a removal at index 0 the index check wraps, so no index moves".
- Evidence: [confirmed: code] (entities); [confirmed: code `0x4262e0`, `reimpl/src/inventory.cpp`; VM] (`Repair`); no tag of their own (the others: reimplemented in VM-checked batches, not re-read one by one in the evidence pass). Source: section 6, "Inventory bookkeeping, three window helpers"; section 8 (several sections, as named above).
- Fix: maybe, one by one (the binding text: yes). Editor: no.

**4.13 A mini map of the right size is never checked against the level** — `Map_LoadMiniMap` `0x4ece20`
- What: "A mini map of the right size is never checked against the level"; the game "uses the outdated mini map without errors" (`CLAUDE.md`, practical test 2026-10-18).
- Evidence: [confirmed 2026-10-28: code …]. Source: `zax-formats/docs/FORMATS.md` 5, mini map.
- Fix: no. Editor: yes (an outdated mini map after edits; the editor writes the mini map itself, `zax-formats/zaxtools/minimap.py`).

**4.14 The 3D sound listener sits at a fixed (320, 240) from the view's corner** — `World::Frame` `0x4f2620`, `fadd` at `0x4f2712` / `0x4f2763`, constants `0x5eff98` / `0x5eff9c`
- What: every frame the listener is set (`Listener_SetPosition` `0x57c170`) to (view x + 320, −10, −(view y + 240)) (the mod's notes give the last two in the other order), the view being player 0's layer's "View Position" (+0x80/+0x84), the top-left corner of the view (`0x4ed7f0` clamps it to the tile map minus the visible size). The constants are shared with the aim cursor (entry 1.18) and the zoom limits (`0x4f2430`). So the listener is the view's centre only for a 640×480 view: the game's view is the screen without the 56-pixel bar (`0x4f6980`), so at zoom 1 its centre lies at (320, 212) at 640×480 (the listener 28 pixels below it) and at (400, 272) at 800×600 (the listener 80 pixels left of and 32 above it).
- Impact: the stereo position of 3D sounds is slightly off; whether it can be heard is not described [assumption: needs a game test].
- Evidence: [confirmed: code `0x4f2620`, `reimpl/src/world_frame.cpp`; VM]; the view position: [confirmed: code, `reimpl/src/layer_view.h`]. Source: reported by the ZaxHiRes mod (`NOTES.md`, static), until 2026-10-29 entry 5.3 [unverified].
- Fix: candidate (opt-in: the view's centre; relevant for phase 8, widescreen). Editor: no.

## 5. Reported by others, not verified

Source of every entry: the ZaxHiRes mod (`zar9roc/ZaxHiRes` on GitHub, MIT licence, state 2026-10-28; a DxWrapper plugin for higher resolutions that patches Zax.exe in memory), its `NOTES.md` and `README.md`. The mod was made for exactly our exe (its tested SHA-256 `7d9efcae…` is `gamedata/Zax - German/Zax.exe`), so its addresses are ours. Its author found these with disassembly and runtime probes under Wine at 1280×800 and 1920×1080, resolutions the game itself does not offer. Names in brackets are ours from `docs/exe_symbols.csv`. Copies of the mod: `gamedata/Mods_Patches/Mods/ZaxHiRes-main.zip` (source and notes), `ZaxHiRes-2026-10-28.zip` (release); never pushed.

Since 2026-10-29 every entry has been checked against our reimplementation's source: the code confirms 5.2–5.5, which moved to 3.24 (the zoom that sticks), 3.25 (the tile cache; its tile count corrected), 4.14 (the sound listener) and 1.18 (the aim cursor, which the code shows at 800×600 too); 5.1 (the camera after loading a save) was confirmed in the game and traced on 2026-10-31 and moved to 3.26. No entry of the mod is left unverified; its notes on the resolution limits follow.

The mod's notes also map the places where the exe assumes 640×480 or 800×600: the display mode list `0x567d60` (`ScreenFrame_EnumModesCallback`) accepting only those two sizes and an aspect ratio strictly between 1.2333 and 1.4333 [confirmed: code `0x567d60`, `reimpl/src/display_ddraw.cpp`; VM], the menu canvas (800×600, or 640×480 at (80, 60) on a 640-wide screen: `0x5beae0`, `reimpl/src/ui_c3_menus.cpp`; `0x5be4a0`, `reimpl/src/ui_menus.cpp`), the HUD bar `0x59e730` (the filler between its parts only at a frame width of 800, `reimpl/src/hud_world.cpp`), the inventory screen `0x428c00` (640×200) and its slide `0x4295be` in `CZaxInventoryScreen::Open` `0x4294f0` (80 pixels to the left only on a 640-wide screen, `reimpl/src/ui_inventory.cpp`), the version label (moved to the bottom right of 800×600, 720×540 at 640 width, `0x5bf290`, `reimpl/src/ui_c3_menus.cpp`), the zoom limits `0x4f24fd`/`0x4f2520` in `0x4f2430` (320 / width and 960 / width, at most 1.0, `reimpl/src/c4_rest.cpp`) [confirmed: code; VM, all of them, 2026-10-29]. They are design limits rather than bugs and a lead for widescreen support in phase 7/8.
