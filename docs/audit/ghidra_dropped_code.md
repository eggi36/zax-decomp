# Audit: code Ghidra drops

Audit class 1 of the port audit playbook (2026-11-13): every function whose Ghidra decompile leaves code out — "Removing unreachable block", "Could not recover jumptable", `halt_baddata`, "Bad instruction", a decompile that fails — and for each one we reimplemented, whether our C++ has the effect of the left-out code. The point: a reimplementation written from a decompile inherits its gaps, and Ghidra's "unreachable" is often only "a value it believes never set" (a local passed by address, a field written through `this` in a callee).

## How it was made

- `tools/ghidra/ZaxDecompWarnings.java` (new): decompiles every non-thunk function of the persistent project (`tools/ghidra.sh`, Ghidra 12.1.4, a copy of `out/ghidra/` opened `-readOnly`) and writes one row per warning line (`decomp_warnings.tsv`) and the C of every function with one. Run:
  `analyzeHeadless <copy of out/ghidra> Zax -process Zax.exe -noanalysis -readOnly -scriptPath tools/ghidra -postScript ZaxDecompWarnings.java <out folder>` (about 2 minutes).
- 11,744 functions decompiled, 1,244 with a warning line; 1,090 of those only "Globals starting with '_' overlap smaller symbols" (naming, no code left out). 15 functions of `docs/reimpl.csv` are no function in the project; they were created and decompiled separately: all 15 without a warning [verified].
- Each dropped block was read in the exe's instructions (`tools/disasm.py`) with its predecessors (the branch that skips it) and compared with the C++ the hook entry names. Coverage: `zax-testtools/tools/coverage_set.py report` over the archive `coverage-runs/union/` (column "Covered": 1 = a recorded play entered the function in the hooked run, 0 = none did).
- Results of the run (outside the repository, they hold the exe's code): `runs/audit/ghidra-out/` of the workspace (`decomp_warnings.tsv`, `decomp_c/`), `runs/audit/dropped_code_list.csv`, `runs/audit/unreachable_disasm.txt`, `runs/audit/unreachable_preds.txt`.

## Result

**No lost code** [verified: every dropped block of the 49 reimplemented functions read in the exe's instructions]. Of 50 functions with "Removing unreachable block" (91 blocks):

- **8 functions where Ghidra dropped code that runs** (the playbook's case): six where a local is filled by a callee through its address (`File_ReadExact` 0x4a1300, `File_WriteAll` 0x4a13f0, `File_CanOpen` 0x4a1570, `CImageFieldDesc::ReadText` 0x4b59f0, `ScreenImage_Blank` 0x579030, `CDifficultyMenu_ctor` 0x5bf820) and two `cpuid` bodies inside SEH frames (`Cpu_Signature` 0x5154e0, `Cpu_DetectMmx` 0x515560). Our C++ has every store and call of those blocks in the exe's order [verified]. Three of them no recorded play enters (`File_WriteAll`, `Cpu_Signature`, `Cpu_DetectMmx`: the CPU functions run before the hooks); for those the comparison is by reading only.
- **40 functions whose blocks are dead in the exe too**: 35 inlined `IndexOf` loops that start at -1 (`cmp count, -1; jbe` — `count > 0xffffffff` unsigned is never true), and constant or impossible tests (a constant string pointer tested for null in `LoadPath`, `gcd_init`, `qr_init`; a ZString text pointer `buffer + 8` tested for null; the negative fix-up of a signed `% 256` over sums of unsigned bytes in the challenge RC4 0x5c91e0). Whether the C++ has them does not matter; most do not.
- `ov_pcm_seek_page` 0x5c4a40: libvorbis' 64-bit compares; the library is its unchanged release source, so nothing can be missing [confirmed].
- `Cms_Main` 0x71e6d0: the SecuROM loader, dead in build 1308 and not reimplemented.

"Could not recover jumptable" (68 functions, each with "Treating indirect jump as call"): **all are tail jumps, none a switch** [verified: the instruction at each address] — 62 `jmp [reg + slot]` through a vtable, three through import slots (`getSystemCP` 0x5dcd3b: `GetOEMCP`/`GetACP` at 0x5ea08c/0x5ea09c; `Inventory_CreateThroughFactory` 0x4d4ff0 through the pointer 0x6c052c), two `jmp [ebx]` dispatches of the C runtime's x87 library (`__trandisp1/2` 0x5d4f90/0x5d4ff7, reimplemented in assembler, checked against a relocated copy of the exe's library). A tail jump decompiled as call-and-return loses nothing.

`halt_baddata`, "Bad instruction", overlapping instructions and the one failed decompile (`Cms_EncryptedLayer_71cd40`: "Bad instruction count exceeded") are all in the SecuROM loader's encrypted layers 0x71a920–0x72942a: dead in build 1308, not reimplemented. "Subroutine does not return" (50 lines) always stands before `exit` 0x5d0e36, `_exit` or `ExitProcess`, which do not return [verified]; no code is lost there.

## Functions with code Ghidra drops

| Address | Function | Dropped blocks | Covered | What the blocks do | Verdict | Our code |
|---|---|---|---|---|---|---|
| `0x425a70` | CInventory::ChooseAfterAdd | `0x425a87` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/inventory_items.cpp |
| `0x426650` | CInventory_IsDiscovered | `0x42665d` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/inventory.cpp |
| `0x426690` | CInventory_Discover | `0x4266a1` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/inventory_items.cpp |
| `0x426740` | CInventory_Undiscover | `0x426751`, `0x426781` | 0 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/inventory.cpp |
| `0x46ef80` | ControlConfig::Ctor | `0x46f2a3` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/control_config.cpp |
| `0x46f3e0` | ControlConfig::ForbidKey | `0x46f3f1` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/control_config.cpp |
| `0x46f680` | ControlConfig::Capture | `0x46f7c8`, `0x46f74b`, `0x46f71a`, `0x46f790`, `0x46f881` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/control_config.cpp |
| `0x480800` | DPlayGame_ProcessSlots | `0x4809ba` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/dplay.cpp |
| `0x480a70` | DPlayGame_ProcessMessages | `0x480b5c` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/dplay.cpp |
| `0x4a1300` | File_ReadExact | `0x4a13ba`, `0x4a1388`, `0x4a1345`, `0x4a135f`, `0x4a1367`, `0x4a136b`, `0x4a13a2`, `0x4a13d4` | 1 | local CFileBuf's stream (+4) set by FileBuf_Open 0x4a1840 through `ecx = &local`; Ghidra keeps it 0 and drops the size check (slot 7), the read (slot 3, the small-read count +0x10 for n < 2000) and both closes | dropped wrongly; our C++ has every effect [verified: asm read against `File_ReadExact`] | reimpl/src/file_buffer.cpp File_ReadExact |
| `0x4a13f0` | File_WriteAll | `0x4a14b0`, `0x4a145e`, `0x4a1444`, `0x4a1476`, `0x4a1490`, `0x4a14ca`, `0x4a1431` | 0 | same pattern: the write (stream slot 2, `call [eax+8]`) and the closes (`g_FilesOpen` 0x6bdf60 decrement, slot 9, deleting destructor) dropped | dropped wrongly; C++ has the write and close [verified] | reimpl/src/file_buffer.cpp File_WriteAll |
| `0x4a1570` | File_CanOpen | `0x4a161c`, `0x4a15cd`, `0x4a15e5`, `0x4a15ff`, `0x4a1636`, `0x4a15b3` | 1 | same pattern: the close after a successful open dropped | dropped wrongly; C++ closes [verified] | reimpl/src/file_buffer.cpp File_CanOpen |
| `0x4b59f0` | CImageFieldDesc::ReadText | `0x4b5abf`, `0x4b5ad9`, `0x4b5adf`, `0x4b5ae3`, `0x4b5ae6`, `0x4b5aeb` | 1 | height [esp+0xc] zeroed, then filled by sscanf 0x5d1539 through its address; Ghidra keeps 0 and drops the whole row loop (FileBuf_ReadLine, blank skip, "}" end, Hex_Decode 0x587ff0) | dropped wrongly; C++ has the loop with the same argument order (w = 1st, h = 2nd, depth = 3rd %d) [verified] | reimpl/src/effect_lists.cpp ImageField_ReadText |
| `0x4dfc70` | ZString_SetDashPathSub | `0x4dfe72` | 0 | `lea edx,[ecx+8]; test edx,edx; je` (the text of a ZString buffer, null only for buffer 0xfffffff8): the "" fallback | dead in the exe (a buffer pointer is never -8) [confirmed: code] | reimpl/src/zstring.cpp ZString::SetDashPathSub |
| `0x4ef9e0` | CCharacterListHolder::IndexOf | `0x4ef9ed` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/objects_rest_d.cpp |
| `0x4f3490` | World::UnloadLayer | `0x4f34ac` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/world_class.cpp |
| `0x4f4650` | World::AddLayer | `0x4f4668`, `0x4f46a7` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/world_class.cpp |
| `0x4f4700` | World::SetPlayerLayer | `0x4f4780` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/world_class.cpp |
| `0x4f9b70` | CChildList_RemoveChild | `0x4f9b85` | 0 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/broadcast.cpp |
| `0x4fadc0` | CLayerPart_Update | `0x4faeaf` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/layer_parts.cpp |
| `0x4fbc90` | CLayerPart_RemoveAI | `0x4fbca1` | 0 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/layer_part_helpers.cpp |
| `0x4fbd90` | CLayerPart_RemoveDeleteAI | `0x4fbda1` | 0 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/layer_part_helpers.cpp |
| `0x4fc390` | CLayerPart_FindNearest | `0x4fc576` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/layer_part_helpers.cpp |
| `0x512d20` | CDefineMission::Execute | `0x512ef4` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/missions.cpp |
| `0x5154e0` | Cpu_Signature | `0x515512` | 0 | SEH-guarded `cpuid` (Ghidra does not follow the __try body of _except_handler3 frames) | dropped wrongly; C++ does cpuid 1 & 0x3fff [verified]; the handler path (return 0 when cpuid faults) is not reproduced and cannot be reached on a CPU with cpuid | reimpl/src/platform.cpp Cpu_Signature |
| `0x515560` | Cpu_DetectMmx | `0x515597` | 0 | same: SEH-guarded `cpuid`, the MMX bit test and `emms` | dropped wrongly; C++ returns (eax & ~0xff) | MMX as the exe (al from the flag byte, upper bytes from cpuid's eax) [verified] | reimpl/src/platform.cpp Cpu_DetectMmx |
| `0x51ff50` | NetPacket_WriteBlockType | `0x51ff77` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/net_packet.cpp |
| `0x55cad0` | ObjectTable_AddReference | `0x55caeb` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/object_table.cpp |
| `0x55cc90` | ObjectTable_Register | `0x55ccb5` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/object_table.cpp |
| `0x55ce50` | ObjectTable_WriteList | `0x55cee1` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/object_table.cpp |
| `0x569b70` | CSegmentedInventoryItem::MayTake | `0x569d2e`, `0x569d5a` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/ai_shield.cpp |
| `0x5762e0` | CSkeletonAI::Message | `0x57649a` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/ai_skeleton.cpp |
| `0x5765c0` | CSkeletonAI::OnMessage | `0x576609` | 0 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/ai_skeleton.cpp |
| `0x579030` | ScreenImage_Blank | `0x57907c`, `0x579081`, `0x579087`, `0x579090` | 1 | local ScreenImage's held pointer +0x2c (set by ScreenImage::Show 0x578a70 through `this`) taken as 0: its release (slot 1 with 1 when owned) and operator delete dropped | dropped wrongly; C++ releases and deletes [verified: offsets +0,+4,+0x28,+0x2c,+0x30 re-derived] | reimpl/src/screen_image.cpp ScreenImage_Blank |
| `0x579ab0` | CSortList2D_RemoveFromCells | `0x579b17` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/sort_list.cpp |
| `0x579c10` | CSortList2D_Update | `0x579ce5` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/sort_list.cpp |
| `0x579e70` | CSortList2D_IsListed | `0x579ec8` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/sort_list.cpp |
| `0x5892b0` | CSwappedLayerFilenameMappingTable::LoadPath | `0x589341` | 1 | `mov ecx,0x622fa4; test ecx,ecx; jne` (a constant text): the EmptyText fallback | dead in the exe (constant non-null) [verified: asm] | reimpl/src/models.cpp |
| `0x589570` | CSwappedLayerFilenameMappingTable::Expire | `0x5896d9`, `0x58973d` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/models.cpp |
| `0x589790` | CSwappedLayerFilenameMappingTable::Flush | `0x5897d3`, `0x589837` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/models.cpp |
| `0x589890` | CSwappedLayerFilenameMappingTable::FlushAll | `0x5898ed`, `0x589951` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/models.cpp |
| `0x589a50` | CSwappedLayerFilenameMappingTable::Touch | `0x589a77` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/models.cpp |
| `0x589af0` | CSwappedLayerFilenameMappingTable::Forget | `0x589b14` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/models.cpp |
| `0x589b80` | CSwappedLayerFilenameMappingTable::Unexpire | `0x589c43` | 1 | `cmp count, -1; jbe` (or `cmp count, ebp` with ebp = -1): an inlined IndexOf with start -1 tests `count > 0xffffffff` unsigned, never true; the block reads element -1 | dead in the exe too (unsigned compare with 0xffffffff) [verified: asm of every block] | reimpl/src/models.cpp |
| `0x5bf820` | CDifficultyMenu_ctor | `0x5bfa6b` | 1 | local index array's data pointer (grown by 0x5c11c0 through its address) taken as 0: the pool free of the array (pool_get 0x506470 + pool_free 0x5065e0) dropped | dropped wrongly; C++ frees it (pool_get_free with capacity << 2) [verified] | reimpl/src/ui_menus.cpp DifficultyMenu_Ctor |
| `0x5c4a40` | ov_pcm_seek_page | `0x5c4b41`, `0x5c4a8f`, `0x5c4b97` | 1 | 64-bit `ogg_int64_t` compares (`cmp hi; jg; jl; cmp lo`) whose high-word branches Ghidra folds | not lost: libvorbis 1.0beta4 vorbisfile.c compiled from its unchanged release source (static audit review: libraries against their releases) [confirmed] | reimpl/third_party/libvorbis-1.0beta4/lib/vorbisfile.c |
| `0x5c5280` | gcd_init | `0x5c535f` | 1 | `mov edx,0x627cf0 ("key.gamespy.com"); test edx,edx; jne`: the null host branch | dead in the exe (constant) [verified: asm] | reimpl/src/gamespy_gcd.cpp gcd_init |
| `0x5c82a0` | qr_init | `0x5c852a` | 1 | `mov ecx,0x62807c ("master.gamespy.com"); test ecx,ecx; jne`: the null host branch | dead in the exe (constant) [verified: asm] | reimpl/src/gamespy_qr.cpp qr_init |
| `0x5c91e0` | gs_encode | `0x5c92bd`, `0x5c9237`, `0x5c9297`, `0x5c92f9` | 0 | the negative fix-up of MSVC's signed `% 256` (`and r,0x800000ff; jns; dec; or 0xffffff00; inc`) after sums of bytes | dead in the exe (the sums are of unsigned bytes and masked indices, never negative) [verified: asm] | reimpl/src/gamespy_qr.cpp (RC4 of the challenge) |
| `0x71e6d0` | Cms_Main | `0x71e8d1` | - | SecuROM loader: `cmp [0x753874], 0x7180b0; jbe` after storing 0x7184c0 | not reimplemented: the CMS/SecuROM loader is dead in build 1308 (docs/exe_symbols.csv) [confirmed] | - |

## Tail jumps Ghidra calls jump tables

Verdict for all: a tail call, nothing dropped [verified: instruction at the address].

| Address | Function | Indirect jump | Covered | Our code |
|---|---|---|---|---|
| `0x401130` | CJWindow::TreeActive | `0x40113e` | 1 | reimpl/src/ui_c3_window.cpp |
| `0x401210` | ToolkitCallback::Call | `0x40121a` | 1 | reimpl/src/ui_c3_window.cpp |
| `0x4020c0` | Callback_4020c0 | `0x4020c2` | 0 | reimpl/src/crt_thunks.cpp |
| `0x404380` | CJListBox::Value | `0x404382` | 0 | reimpl/src/ui_listbox.cpp |
| `0x4080a0` | ComboBox::Slot44 | `0x4080a8` | 1 | reimpl/src/ui_edit.cpp |
| `0x408ba0` | ComboBox::SelectedId | `0x408ba8` | 1 | reimpl/src/ui_edit.cpp |
| `0x408bb0` | ComboBox::SelectedText | `0x408bb8` | 0 | reimpl/src/ui_edit.cpp |
| `0x40b2e0` | CProxyFrame::Clear | `0x40b2e5` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b340` | CProxyFrame::Pixels | `0x40b345` | 1 | reimpl/src/frame_proxy.cpp |
| `0x40b350` | CProxyFrame::Block1C | `0x40b355` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b360` | CProxyFrame::Block24 | `0x40b365` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b370` | CProxyFrame::Block2C | `0x40b375` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b380` | CProxyFrame::Block34 | `0x40b385` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b390` | CProxyFrame::BytesPerPixel | `0x40b395` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b3a0` | CProxyFrame::BitsPerPixel | `0x40b3a5` | 1 | reimpl/src/frame_proxy.cpp |
| `0x40b3d0` | CProxyFrame::Frame | `0x40b3d5` | 1 | reimpl/src/frame_proxy.cpp |
| `0x40b3e0` | CProxyFrame::Flags | `0x40b3e5` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b3f0` | CProxyFrame::Height | `0x40b3f5` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b410` | CProxyFrame::Pitch | `0x40b415` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b420` | CProxyFrame::Log2Width | `0x40b425` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b430` | CProxyFrame::Log2Height | `0x40b435` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b440` | CProxyFrame::OriginX | `0x40b445` | 1 | reimpl/src/frame_proxy.cpp |
| `0x40b450` | CProxyFrame::OriginY | `0x40b455` | 1 | reimpl/src/frame_proxy.cpp |
| `0x40b460` | CProxyFrame::Width | `0x40b465` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b470` | CProxyFrame::Rows | `0x40b475` | 1 | reimpl/src/frame_proxy.cpp |
| `0x40b480` | CProxyFrame::Table20 | `0x40b485` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b490` | CProxyFrame::Table28 | `0x40b495` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b4a0` | CProxyFrame::Table30 | `0x40b4a5` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40b4b0` | CProxyFrame::Table38 | `0x40b4b5` | 0 | reimpl/src/frame_proxy.cpp |
| `0x40c5e0` | CJWindow::IsVisible | `0x40c5f5` | 1 | reimpl/src/ui_c3_window.cpp |
| `0x40fc50` | Button::UpdateLook | `0x40fc58` | 1 | reimpl/src/ui_button.cpp |
| `0x4267e0` | LayerPart_GetInventory | `0x4267e2` | 1 | reimpl/src/inventory.cpp |
| `0x429e20` | CZaxInventoryScreen::ShowVoiceLog | `0x429e28` | 1 | reimpl/src/ui_inventory.cpp |
| `0x429e30` | CZaxInventoryScreen::ShowMission | `0x429e38` | 1 | reimpl/src/ui_inventory.cpp |
| `0x429e40` | CZaxInventoryScreen::ShowInventory | `0x429e48` | 1 | reimpl/src/ui_inventory.cpp |
| `0x46aca0` | DefinitionList::Slot17 | `0x46aca2` | 0 | reimpl/src/definitions.cpp |
| `0x48da40` | ResourceField_Callback48da40 | `0x48da42` | 1 | reimpl/src/c4_fields.cpp |
| `0x49ccb0` | Object_Slot1 | `0x49ccb2` | 1 | reimpl/src/entity_draw.cpp |
| `0x4a2170` | FileBuf_Detach | `0x4a2179` | 1 | reimpl/src/file_buffer.cpp |
| `0x4a2180` | FileBuf_Tell | `0x4a2189` | 1 | reimpl/src/file_buffer.cpp |
| `0x4a2190` | FileBuf_Size | `0x4a2199` | 1 | reimpl/src/file_buffer.cpp |
| `0x4cdec0` | App::WindowSlot76 | `0x4cdec9` | 1 | reimpl/src/c4_misc.cpp |
| `0x4cdf60` | App_WindowSlot65 | `0x4cdf65` | 1 | reimpl/src/ui_flow.cpp |
| `0x4ceb60` | CObject::AppSlot14 | `0x4ceb68` | 1 | reimpl/src/ai_tasks.cpp |
| `0x4d4ff0` | Inventory_CreateThroughFactory | `0x4d4ff0` | 1 | reimpl/src/inventory_defs.cpp |
| `0x4e9980` | COffsetClippedProxyFrame::Height | `0x4e9986` | 1 | reimpl/src/layer_draw.cpp |
| `0x4e9990` | COffsetClippedProxyFrame::Width | `0x4e9996` | 1 | reimpl/src/layer_draw.cpp |
| `0x4e99a0` | CLayer::Precache | `0x4e99a6` | 1 | reimpl/src/layer.cpp |
| `0x4f0a20` | World::Slot60 | `0x4f0a22` | 1 | reimpl/src/world_class.cpp |
| `0x4f3a30` | World::Slot66 | `0x4f3a32` | 1 | reimpl/src/world_class.cpp |
| `0x4faf30` | CLayerPart_ParentSlot36 | `0x4faf39` | 1 | reimpl/src/layer_parts.cpp |
| `0x500700` | CLightningLayerPart::ZFieldSetter | `0x500702` | 1 | reimpl/src/lightning.cpp |
| `0x500710` | CLightningLayerPart::XLengthFieldSetter | `0x500712` | 1 | reimpl/src/lightning.cpp |
| `0x500720` | CLightningLayerPart::YLengthFieldSetter | `0x500722` | 1 | reimpl/src/lightning.cpp |
| `0x500730` | CLightningLayerPart::HeightFieldSetter | `0x500732` | 1 | reimpl/src/lightning.cpp |
| `0x50db70` | WindowMissionOver_Close | `0x50db7c` | 1 | reimpl/src/ui_mission.cpp |
| `0x51f6c0` | JumpSlot24 | `0x51f6c2` | 1 | reimpl/src/net_packet.cpp |
| `0x51f6d0` | JumpSlot25 | `0x51f6d2` | 1 | reimpl/src/net_packet.cpp |
| `0x51f6e0` | JumpSlot26 | `0x51f6e2` | 1 | reimpl/src/net_packet.cpp |
| `0x51f6f0` | JumpSlot27 | `0x51f6f2` | 1 | reimpl/src/net_packet.cpp |
| `0x548740` | PolyPoints_Clear | `0x548746` | 1 | reimpl/src/raster_poly.cpp |
| `0x558d60` | CQuestTimeStat::Clock | `0x558d68` | 1 | reimpl/src/quest_stats.cpp |
| `0x571800` | CSimpleLayerPart_FieldSetX | `0x571802` | 1 | reimpl/src/layer_parts.cpp |
| `0x571810` | CSimpleLayerPart_FieldSetY | `0x571812` | 1 | reimpl/src/layer_parts.cpp |
| `0x5cf6f0` | inflate_codes_free | `0x5cf6f7` | 1 | reimpl/third_party/zlib-1.1.3/infcodes.c |
| `0x5d4f90` | __trandisp1 | `0x5d4ff5` | 0 | reimpl/src/crt_x87.cpp |
| `0x5d4ff7` | __trandisp2 | `0x5d5081` | 0 | reimpl/src/crt_x87.cpp |
| `0x5dcd3b` | getSystemCP | `0x5dcd55`, `0x5dcd6a` | 1 | reimpl/src/crt_startup.cpp |

## What stays open

- The SEH handler paths of `Cpu_Signature`/`Cpu_DetectMmx` (return 0 / no MMX when `cpuid` faults) are not reproduced; no CPU the port runs on lacks `cpuid` [assumption: reasoning, not a run]. Not a Ghidra loss; listed for completeness.
- This check sees only what Ghidra itself says it dropped. Code Ghidra drops silently (a function body cut short at a wrong "no return", a wrong function boundary) is covered by the static audit (`tools/static_audit.py`: calls, constants, globals per function) and the replays, not here.
