"""Function table of Zax.exe: every function with class, subsystem and status.

Based on the export of the Ghidra project (``tools/ghidra.sh`` -> ``out/ghidra/functions_raw.tsv``).

Subsystem:
- functions of a class (namespace from ``tools/ghidra_names.py``) by the class name or the topmost
  parent class (``SUBSYSTEMS``),
- libraries and copy protection by address range (``RANGES``; bounds from library strings and the
  import thunks, estimated),
- game code whose neighbours, class or name gave a wrong subsystem by an explicit range
  (``CORRECTIONS``; evidence in docs/DECOMPILATION.md section 6, "Subsystem assignment corrections").
  A correction wins over the class and the name, except for the container template instances
  (``PtrArray_*``, ``Array<n>_*``, ...: Core) and the class info getters (their class's subsystem),
  and except for a symbol whose source names its subsystem ("Rendering, not reimplemented: ...");
  since 2026-11-01, before it applied only to functions without a class or name match. A correction
  with an empty subsystem keeps its functions unassigned, so the neighbours cannot fill them in,
- everything else from its neighbours: VC++ places the functions of one source file one after
  another. A function between two functions of the same subsystem inherits it
  (``assignment=neighbours``); corrected functions are not overwritten by it.

Status: ``open`` (address only), ``assigned`` (class known, method unnamed), ``named`` (name from
exe_symbols.csv or a role such as constructor), ``reimplemented``, ``verified``. The last two are
maintained in ``docs/reimpl.csv`` (address, status), which is read here.

    .venv/bin/python -I tools/function_table.py

writes ``docs/functions.csv`` and prints an overview per subsystem.
"""
from __future__ import annotations

import csv
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Address ranges outside the game code [confirmed 2026-10-25, docs/DECOMPILATION.md "Libraries and copy
# protection": every function in them named against the library sources or by its call graph]
RANGES = [
    # vorbisfile 1.0 beta 4 (ov_clear ... ov_read, its jump table up to 0x5c5280)
    (0x5c3bf0, 0x5c5280, 'Library: Ogg Vorbis'),
    # GameSpy SDK: CD key SDK, server browsing, query and reporting, MD5, darray/hashtable; the
    # wsock32/kernel32 import thunks 0x5ca9e0-0x5caa8e at the end are not in docs/functions.csv
    (0x5c5280, 0x5caa90, 'Library: GameSpy SDK'),
    # zlib 1.1.3 (compress2 ... inflate_fast, which ends at 0x5d0312)
    (0x5caa90, 0x5d0320, 'Library: zlib'),
    # MSVC 6 C runtime from operator delete to _fileno 0x5de2e0
    (0x5d0320, 0x5de2f0, 'Library: C runtime'),
    # libogg framing.c and libvorbis 1.0 beta 4 (ogg_page_version ... Newton_Raphson)
    (0x5de2f0, 0x5e96f0, 'Library: Ogg Vorbis'),
    # RtlUnwind thunk, _allshr, memchr, _CIacos/acos
    (0x5e96f0, 0x5e98b0, 'Library: C runtime'),
    # libogg bitwise.c (oggpack_*), linked after the runtime objects
    (0x5e98b0, 0x5ea000, 'Library: Ogg Vorbis'),
    # SecuROM loader sections .cms_t/.cms_d; never executed in this exe (entry point in .text)
    (0x718000, 0x759000, 'Copy protection'),
]

# Sizes Ghidra gets wrong: functions with a __try whose __except filter and handler the compiler put behind the
# body; Ghidra's function ends before them (tools/audit.py finds them through the scope tables; phase 6 batch 6-0,
# docs/DECOMPILATION.md 13). Address: size up to the last ret.
SIZE_FIXES = {
    0x5154e0: 122,   # Cpu_Signature: filter 0x515537, handler 0x51553d, ret 0x515559 (Ghidra 87)
    0x515560: 189,   # Cpu_DetectMmx: handler 0x5155e8 and the non-MMX exit 0x51560a, ret 0x51561c (Ghidra 132)
}

# Game code that the neighbours, its class or its name assigned wrongly (a later entry wins over an earlier
# one). It wins over the class and the name except for CORRECTION_KEEPS and explicit symbol subsystems.
# Evidence: docs/DECOMPILATION.md section 6, "Subsystem assignment corrections" (and the batch sections
# cited there; section 13, "Batch section-12 leftovers", for the precedence).
# Names a correction does not override: the container template instances and the generic Core helpers that lie
# inside another subsystem's range (the definition list's object lookup and loaders of world_rest.cpp, the image
# field kind, the resource list's shared slot 35: Core whatever range they lie in), and the class info getters
# (registered with their class, which gives the subsystem)
CORRECTION_KEEPS = r'^(PtrArray|Array[0-9]+_|StringArray_|FieldDesc_|DefinitionList_|ImageField_|ResourceList::)|ClassInfo'

CORRECTIONS = [
    # 0x400000-0x4fffff, 2026-10-24 (DECOMPILATION.md section 6, "0x400000-0x4fffff, 2026-10-24")
    # window toolkit: "Grid Control", "~CJHeader", "~CJTabControl", "~CJSlider", "~CJScrollBar",
    # "UI/Button Switch.wav", "Window Texture", file dialog "No Files Found"; CJListBox, frame windows,
    # CScreenWindow ("Inventory bookkeeping, ...")
    (0x401000, 0x402090, 'User interface'),
    # MSVC vector constructor iterator (stdcall array, size, count, ctor), compiler helper
    (0x402090, 0x4020d0, 'Library: C runtime'),
    (0x4020d0, 0x40b280, 'User interface'),
    # CProxyFrame slots and 0x40b290 between them
    (0x40b280, 0x40b5c0, 'Rendering'),
    (0x40b5c0, 0x414cf0, 'User interface'),
    # CAction class info (CAction_ClassInfo), its ctor/dtors, "$trigger"/"$instigator"/"$target(" names
    (0x41a650, 0x41b120, 'Actions'),
    # CActionQueue ctor (vtable), "Items" of CActionQueueItem
    (0x41c6d0, 0x41c830, 'Actions'),
    # CollideTriggerAI class info and dtors (AIBase_Dtor)
    (0x41c900, 0x41c9d0, 'AI'),
    (0x41d3e0, 0x41d5e0, 'AI'),
    (0x41d5e0, 0x41d690, 'Actions'),
    # "OtherMapList" lookups (callers World_StartNewGame, CExpireMapAction)
    (0x41f660, 0x41fb80, 'World'),
    (0x420350, 0x4203a0, 'AI'),
    (0x423110, 0x423160, 'Rendering'),
    # deleting dtor of vtable 0x5edf7c (right after CCompassRenderer's), size 8 = CInventoryHUDRenderer
    (0x4236e0, 0x423710, 'Inventory and weapons'),
    # CInventory up to the array reserve 0x426b90 (strings "Primary", "No Inventory Cycle.wav", ...);
    # from 0x424c40 the array of CInventoryBase objects and CInventory_RemoveAt callers
    (0x424c40, 0x426bf0, 'Inventory and weapons'),
    # the player's inventory (Player_Inventory 0x428b10) and the static objects after it (initialisers and atexit
    # destructors); Inventory and weapons through their neighbours until the inventory screen after them became
    # User interface (2026-11-01)
    (0x428b10, 0x428c00, 'Inventory and weapons'),
    # the inventory screen ("~CZaxInventoryScreen", "Inventory Tab", "Mission Tab", "Voice Log Tab") and the
    # inventory window ("Inventory Window", its frame, "Inventory Up.wav"), the HUD renderers' shared helper
    # 0x4298d0; was Inventory and weapons through its neighbours (phase 3 remains, band A, 2026-10-26)
    (0x428c00, 0x429e50, 'User interface'),
    # span rasterizer called by CFrame::vf52, 16-bit pixel tables shared with the blend tables
    (0x42b160, 0x42d3d0, 'Rendering'),
    # window subclass ctor/dtor (window toolkit base 0x40a8a0/0x40b190)
    (0x42d3d0, 0x42d660, 'User interface'),
    # the frame class of vtable 0x5ee9c0 (a CProxyFrame that records draw calls); was AI through its
    # neighbours (band A, 2026-10-26)
    (0x42e950, 0x42f310, 'Rendering'),
    (0x42f6d0, 0x42f730, 'AI'),
    # CEntityBehaviorMoveToPos: dtor, path collide checks, "Crouch"/"Run"/"Walk"
    (0x430350, 0x431950, 'AI'),
    (0x431ac0, 0x431af0, 'Rendering'),
    # blend table helpers (InitBlendTables, Pixel16_FromRGB)
    (0x4337e0, 0x433dd0, 'Rendering'),
    # RLE span drawing, blend registration helpers (callers RegisterBlend_*, CSequence)
    (0x466080, 0x467140, 'Rendering'),
    (0x467140, 0x467180, 'AI'),
    # the CD key dialog (window toolkit 0x4035f0/0x403d70, "Player Error.wav"); was Multiplayer (band A,
    # 2026-10-26)
    (0x46b050, 0x46b4e0, 'User interface'),
    # CD key query ("CD Key", key.gamespy.com check, multiplayer expansion)
    (0x46b4e0, 0x46b760, 'Multiplayer'),
    (0x46b7b0, 0x46b810, 'AI'),
    # "CFailedTargetMessage" of CChaseInvestigateAI
    (0x46cd00, 0x46cd50, 'AI'),
    # "Player Name", "Net Game", "Crater Multiplayer" player options
    (0x46ce20, 0x46d010, 'Multiplayer'),
    # the network options dialog (window toolkit, its OK saves the player options and sends them,
    # ClientOptionsReader); was Multiplayer (band A, 2026-10-26)
    (0x46d010, 0x46d590, 'User interface'),
    # clipboard (caller: text field 0x4119e0)
    (0x46d590, 0x46d790, 'User interface'),
    # COffsetClippedProxyFrame ctor/dtor and proxy frame helpers
    (0x46d790, 0x46dcc0, 'Rendering'),
    # controls: "Control Configuration", "Controler ID Number", KeyBinding_SetDefault, Ini getters
    (0x46ec00, 0x471080, 'Game flow'),
    # controls options dialog ("Weapon AutoSwitch", window toolkit)
    (0x471080, 0x472440, 'User interface'),
    (0x472e80, 0x472f20, 'Inventory and weapons'),
    # credits control ("~CJCreditsControl", "ZaxCredits.txt" callers)
    (0x475eb0, 0x476450, 'User interface'),
    # damage in a radius through the layer's grid, world/AI ("Definition classes, ...")
    (0x477640, 0x477650, ''),
    # WinMain ("ZaxCurrentlyRunningMutex", "WinGameClass") and the window size
    (0x477940, 0x477d30, 'Game flow'),
    # "Death Behaviors" definition list (CDeathBehaviorListDefinition)
    (0x478260, 0x478c60, 'AI'),
    (0x479900, 0x479c40, 'Game flow'),
    (0x479c40, 0x479cf0, 'Inventory and weapons'),
    (0x47b2c0, 0x47b2f0, 'Actions'),
    # the dialogue display window ("Dialog Display Window", portraits, fonts, Window_AddChild; vtable
    # 0x5f13a0); was Quests and dialogue (band A, 2026-10-26)
    (0x47bbd0, 0x47d230, 'User interface'),
    # DirectPlay 8 layer: DPNERR_* texts, GameSpy, vtables 0x5f15a8/0x5f1600 ("The Local.txt table, ...")
    (0x47ee70, 0x480e20, 'Multiplayer'),
    # CFrame-derived image class, vtable 0x5f162c (same section); up to 0x481710 its drawing helpers
    # (callers World_DrawView and the CFrame code 0x4ace00-0x4afc00)
    (0x480e20, 0x481710, 'Rendering'),
    # CDoorAI class info, "Opening"/"Closing"
    (0x481710, 0x4828b0, 'AI'),
    # CEnergyMeter dtor and meter drawing (callers the inventory detail renderers)
    (0x486cf0, 0x487080, 'Inventory and weapons'),
    (0x487080, 0x487210, 'Objects'),
    (0x48d7f0, 0x48d9f0, 'Objects'),
    # CEntityAnimated dtor
    (0x48ea40, 0x48eb50, 'Objects'),
    # CEntityAnimatedLine helpers, CEntityAnimatedUpperBody class info
    (0x4936b0, 0x494410, 'Objects'),
    (0x494410, 0x494490, 'AI'),
    # CEntityBehaviorStack ctor/dtor and stack operations
    (0x49a3a0, 0x49a8c0, 'AI'),
    (0x49c280, 0x49c720, 'Actions'),
    # draw queue (after DrawQueue_Sort; callers World_DrawView)
    (0x49ca60, 0x49ccc0, 'Rendering'),
    # "COverlayWalkingBlockReaderObject" (NetPacket_RegisterReader)
    (0x49d390, 0x49d3a0, 'Multiplayer'),
    # CFidgetSettings class info, "Fidget"/"No Fidget Sequences Found"
    (0x49ff50, 0x4a09d0, 'AI'),
    (0x4a5910, 0x4a5980, 'Rendering'),
    # frame timer (QueryPerformanceCounter wrappers; callers the main loop 0x4cdfd0/0x4ce160)
    (0x4a81f0, 0x4a8440, 'Game flow'),
    # CFrame drawing (callers CFrame::vf51-vf56), resource image holders, CFrameInterface class info
    (0x4aca70, 0x4b40f0, 'Rendering'),
    # CPlasmaTileMap helpers (caller its ctor)
    (0x4b40f0, 0x4b4240, 'World'),
    # frame overlay options, CFrameOverlayTextOverlay ctor, "CFrame(%d,%d,%d)"
    (0x4b52e0, 0x4b5c60, 'Rendering'),
    (0x4b5c60, 0x4b5cb0, 'World'),
    (0x4b8460, 0x4b84b0, 'Actions'),
    (0x4c04d0, 0x4c0500, 'Actions'),
    # weapon discharge helpers (caller Weapon_DischargeThisFrame)
    (0x4c4dd0, 0x4c4ee0, 'Inventory and weapons'),
    # intro movies ("/SkipMovies", "Movies/%s.bik"; caller Game_Startup)
    (0x4cd5a0, 0x4cd780, 'Game flow'),
    # CGameType ctor/dtors (vtable 0x5f75b4)
    (0x4ceaa0, 0x4ceb50, 'Game flow'),
    (0x4d1d50, 0x4d1da0, 'Rendering'),
    # Win32 platform helpers: command line 0x6c047c, temp/module path, message boxes, CreateProcess,
    # MulDiv, GetTickCount (callers Core and Game_Startup)
    (0x4d2190, 0x4d2660, 'Core'),
    # "Multi-player Server Name", "Levels/Multiplayer/", "Could not create server"
    (0x4d2c50, 0x4d42a0, 'Multiplayer'),
    # CInventoryBase class info, ctor/dtors
    (0x4d4ec0, 0x4d5010, 'Inventory and weapons'),
    (0x4d55c0, 0x4d5620, 'Inventory and weapons'),
    # inventory display pages ("~CInventoryDisplayPage", "~CItemListWindow", "Info_Use_Deploy_Button")
    (0x4da240, 0x4dc1f0, 'Inventory and weapons'),
    (0x4dc1f0, 0x4dc2a0, 'Actions'),
    (0x4dd2d0, 0x4dd380, 'Inventory and weapons'),
    (0x4dd580, 0x4dd6d0, 'Inventory and weapons'),
    # CInvestigateAI/CInvestigateAreaAI class info and dtors
    (0x4dd6d0, 0x4dd930, 'AI'),
    # joystick (WINMM joy*, "No Joystick", "Hat Up", registry joystick names)
    (0x4dd930, 0x4de290, 'Game flow'),
    # window subclass (calls the toolkit's 0x402d80/0x402f00)
    (0x4de290, 0x4de540, 'User interface'),
    # the window toolkit's button look (vtable 0x5f88b4: its drawing 0x4e2240, constructor 0x4e2220) and the
    # static look factory 0x6c1040 (vtable 0x5f88c0, its make slot 0x4e26a0); caller the toolkit 0x40a9b0. Was
    # '' and 'Sound' ("SoundObject", a misnomer) until the band D batch 2026-10-26
    (0x4e2240, 0x4e26a0, 'User interface'),
    (0x4e26a0, 0x4e26d0, 'User interface'),
    # keyboard state 0x6c1050 (0x1ff key flags, key ring buffer, "No Key Mapping", GetKeyNameTextA)
    (0x4e2720, 0x4e2b80, 'Game flow'),
    # the text look (vtable 0x5f88d4: constructor 0x4e2b80, destructor 0x4e2bb0, text drawing 0x4e2be0, width
    # 0x4e2df0) of the window toolkit (band D batch 2026-10-26)
    (0x4e2b80, 0x4e2e50, 'User interface'),
    # warp effect drawing (CSimpleWarpEffect slots, blend table 0x6347b0)
    (0x4e2f10, 0x4e3680, 'Rendering'),
    # main menu display (caller CMainMenu_ctor, GetMainMenuDisplayOptions)
    (0x4e3680, 0x4e38e0, 'User interface'),
    # the text look factory's make slot (vtable 0x5f88fc, band D batch 2026-10-26)
    (0x4e38e0, 0x4e3910, 'User interface'),
    (0x4e4380, 0x4e4460, 'Multiplayer'),
    (0x4e5160, 0x4e5210, 'World'),
    # CLayerSaveData dtor and accessors (way point map, fog of war, update monitors)
    (0x4e6e80, 0x4e7450, 'World'),
    (0x4e7b70, 0x4e7c00, 'World'),
    # world drawing and collision (CSortList2D queries, "Bounce", "Fixing entity types")
    (0x4e8900, 0x4e9980, 'World'),
    # CLayer ctors and layer methods (g_World 0x6c2080, positional sounds and explosions)
    (0x4e9d80, 0x4eb6f0, 'World'),
    # mini map loading ("Loading Mini-Map"), grid queries
    (0x4ece20, 0x4edac0, 'World'),
    # CAmbientSound methods (vtable, callers CAmbientSoundAI)
    (0x4ee4a0, 0x4ee750, 'Sound'),
    # "CLayerBlockReader" registration and its write
    (0x4ee750, 0x4ee7f0, 'Multiplayer'),
    (0x4ee7f0, 0x4ee8e0, 'World'),
    (0x4eea50, 0x4ef000, 'World'),
    # CCharacterListHolder listeners
    (0x4ef8e0, 0x4efa20, 'Objects'),
    # world base: "Startup.wav", resources ("Fonts", "Interface/Cursors"), "Read Save Game",
    # "LoadNewLayerFP", spawn points, World_StartNewGame_Base
    (0x4f0230, 0x4f0a70, 'World'),
    (0x4f0ac0, 0x4f77a0, 'World'),
    # client/server block readers and the net session (NetPacket_*, BlockReader_Ctor, reader names)
    (0x4f78a0, 0x4f7fc0, 'Multiplayer'),
    # table lookup with lazily loaded entries, no network code; subsystem unknown
    (0x4f7fc0, 0x4f8770, ''),
    (0x4f8770, 0x4f9590, 'Multiplayer'),
    # StringField clone (StringField_CopyCtor)
    (0x4fa710, 0x4fa740, 'Core'),
    # CLayerPart ctor/dtor
    (0x4fa770, 0x4fa810, 'World'),
    (0x4fcb00, 0x4fcbb0, 'AI'),
    # CLayerPartAI/CFlyweightLayerPartAI class info
    (0x4fd220, 0x4fd5a0, 'AI'),
    (0x4fd5a0, 0x4fd5c0, 'Multiplayer'),
    (0x4fd710, 0x4fd760, 'World'),
    # CLightningLayerPart
    (0x4feb30, 0x4fecf0, 'Rendering'),
    (0x4fff60, 0x500000, 'Rendering'),
    # CLightningLayerPart's segment geometry, only caller its slot 23 0x4ff500
    (0x500080, 0x5001d0, 'Rendering'),
    # Vec3_Assign, reimplemented with the small helpers; generic math, see 0x508c50
    (0x5003e0, 0x500400, 'Core'),
    # CMaterialsGeneratorWindow ("~CMaterialsGeneratorWindow", "Materials Generator:
    # Startup/Generate/Change tab", Window_AddChild), opened by CShowMaterialsGeneratorAction
    (0x502960, 0x505d30, 'User interface'),
    # the font list ("Courier", "ShipFont", "ZaxFont", "OmFont", "OtherFont") and its loader
    (0x506b90, 0x506f70, 'Rendering'),
    # HUD bars: read the CHudBarOptions of the custom rules (0x5a1fa0)
    (0x507f20, 0x508000, 'User interface'),
    # HUD bars, same layout as 0x507f20/0x507f40
    (0x5084a0, 0x508730, 'User interface'),
    # generic math without game state, called from AI, World, Objects, Rendering, Sound: fast sqrt,
    # sin/cos/tan tables (filled by Game_Startup), atan2, floor/ceil, segment intersection, log2,
    # ClampAngleToRange (reimplemented)
    (0x508c50, 0x509890, 'Core'),
    # mission objective display: vtable CMissionObjectiveDisplayOptions, called by CQuestGameType and
    # 0x50ff80
    (0x509890, 0x509fc0, 'Quests and dialogue'),
    # builds a window (Window_AddChild and the window toolkit)
    (0x5156e0, 0x515870, 'User interface'),
    # CModel accessors (activity array +0xc/+0x10, Turn Speed +0x64, sequence names), callers
    # CEntity*/DrawRotationSequenceLayers
    (0x517200, 0x517520, 'World'),
    # the model list (ModelList, its file of an object)
    (0x517820, 0x5179b0, 'World'),
    # the model preview of the window toolkit (WindowModelPreview, vtable 0x5fae90: an entity made from the
    # model, its animations 'Walk', 'Run Energy', 'Deploy', ...; ui_dialogs.cpp, p5_leftovers.cpp) and its
    # drawing static ModelPreview::Draw 0x517c60 with its two atexit destructors (ui_c3_misc.cpp); was World
    # by an older range that took the animation names for entity creation (2026-11-01)
    (0x5179b0, 0x517e70, 'User interface'),
    (0x517f20, 0x518260, 'User interface'),
    # 0x518260 (ResourceHandle_Ctor) is called by CModel::CModel; the sequence name list of vtable 0x5faea4
    (0x518260, 0x5182d0, 'World'),
    # the mouse cursor object: position +0x10/+0x14, SetCursorPos, LoadCursorA/SetCursor
    (0x5184e0, 0x518e00, 'User interface'),
    # net game window and server list ("Net Game", "UI/GameSpy Logo",
    # "hostname"/"numplayers"/"mapname", "DirectPlayAddress_0", "-connect"/"-host"), calls the GameSpy
    # SDK
    (0x518e00, 0x51a040, 'Multiplayer'),
    # CMusicMixer helpers: vtables CMusicMixer/CMusicMixerData, music track list for
    # CSwitchMusicTrackAction, threaded sound stream 0x587060-0x587700
    (0x51e360, 0x51ea10, 'Sound'),
    # DirectPlay helper lists: callers DPlayGame_EnumServiceProviders, DPlay_MessageHandler,
    # DPlayProvider/DPlayGame dtors, NetLocal
    (0x51ea10, 0x51f180, 'Multiplayer'),
    # GameSpy answers, player/team lists, session base, traffic graph, packet ("Network packet, ...")
    (0x51f180, 0x521010, 'Multiplayer'),
    # tail of CBeginNonInteractiveSequenceAction (0x522b00 is called by its slot 18)
    (0x522ab0, 0x522b30, 'Actions'),
    # CProxyFrame/COffsetClippedProxyFrame: vtable CProxyFrame (0x525000, 0x525040), offset forwarding
    # to the inner frame (0x525180, 0x5251d0)
    (0x524f50, 0x525410, 'Rendering'),
    # called only by the sound manager (0x57ca10, 0x57cb40 ... 0x57d550), stream buffer pool 0x57cdb0
    (0x525410, 0x525780, 'Sound'),
    # only caller CEntityOm slot 104
    (0x526600, 0x5266d0, 'Objects'),
    # palette/blend tables: callers InitBlendTables, CPaletteHolder, CFrame
    (0x5266d0, 0x5266e0, 'Rendering'),
    # NameEntry_FreeName, reimplemented with the name lists
    (0x5266e0, 0x526700, 'Core'),
    # palette and blend tables (callers InitBlendTables, CFrame, CScreenFrame, Pixel16_FromRGB)
    (0x526700, 0x5284c0, 'Rendering'),
    # CGenericParticleSystemAI constructor/destructor (envelope vtables) and helpers, called by its
    # Create/slots
    (0x52dd20, 0x52e220, 'AI'),
    # A* search over the way point map ("Path classes and the A* search ...")
    (0x5354b0, 0x535b90, 'World'),
    # the point list's drawing (same section)
    (0x536480, 0x536490, 'Rendering'),
    # its two atexit destructors (referenced from 0x5364bb/0x5364f7 in the drawing); their neighbours made them
    # World until the drawing became Rendering (2026-11-01)
    (0x5365b0, 0x5365d0, 'Rendering'),
    # the way point map's quadtree (same section)
    (0x5392f0, 0x539af0, 'World'),
    # way point map: called by 0x538f80, calls 0x538f60/0x53a040
    (0x53a8c0, 0x53a940, 'World'),
    # CPlasmaTileMap: "Num Textures", "Elevations Row", "Seeds Row", ground frames; callers
    # CLayerSaveData, World_DrawView, CLandscapeAI
    (0x53f660, 0x542170, 'World'),
    # CPlayerControlAI constructor/destructor and the walking control (callers
    # CPlayerWalkingControlAI)
    (0x5422b0, 0x542580, 'AI'),
    # 2D polygons ("POLY2D"; Polygon_Assign): geometry and collision of CFreeRangePoly,
    # CPathCollideChecker, Entity_PlaceNear, entity bounds
    (0x545620, 0x547fd0, 'World'),
    # among them the polygon outline drawing of the debug views (DrawPolygonOutline, view_draw.cpp)
    (0x546d40, 0x546f50, 'Rendering'),
    # polygon helpers called only by 0x547230
    (0x548430, 0x5484a0, 'World'),
    # CRenderablePolygon rasterizer: entry 0x549050 from its slot 1, MMX check, span fill variants,
    # RenderPoly_QueueForDraw
    (0x5484a0, 0x54d130, 'Rendering'),
    # FieldNameList12 deleting destructor, reimplemented with the name lists
    (0x54fa70, 0x54fba0, 'Core'),
    # CPursueAI destructor (AIBase_Dtor, caller CChasePursueAI slot 1)
    (0x54fe70, 0x54ff70, 'AI'),
    # quest stat helpers (CQuestStat, CQuestStatColumnInfo, 0x553c00)
    (0x5525b0, 0x552b50, 'Quests and dialogue'),
    # CQuestCounterData/CQuestCounterParScore class getters
    (0x5557f0, 0x555a50, 'Quests and dialogue'),
    # CQuestPercentageParScore/CQuestPercentageData class getters
    (0x55aa30, 0x55aca0, 'Quests and dialogue'),
    # random numbers (generators seeded with GetTickCount, Random_Below), used by every subsystem;
    # generic like 0x508c50
    (0x55c100, 0x55c6f0, 'Core'),
    # the packet's token map, callers NetPacket_*Token
    (0x55d1f0, 0x55d210, 'Multiplayer'),
    # loads a definition record on demand (0x80-byte records as in DefinitionRecord_Init 0x5a4060:
    # busy flag +0x7f, value +0x78)
    (0x55e090, 0x55e1c0, 'Core'),
    # CRenderEffect getter, constructor, destructors (callers CHueChangeEffect, CSimpleWarpEffect)
    (0x55e1c0, 0x55e350, 'Rendering'),
    # CRenderEffectBehavior getter, constructor, destructors (callers CEntityBehaviorWarpEffect)
    (0x55e350, 0x55e470, 'AI'),
    # block reader constructor (BlockReader_Ctor, vtable 0x5feb4c)
    (0x55e470, 0x55e490, 'Multiplayer'),
    # class CVerticalCut ("Cut Offset", "Strip Sorting Y Offset"), used by CRotationSequence 0x55ed20
    (0x55e870, 0x55eae0, 'Rendering'),
    # load/save game screen ("SaveGames/", "QuickSave.sav", "Loading SaveGame Stats", CJListBox),
    # opened by CZaxMenu_OnClick
    (0x562970, 0x564f20, 'User interface'),
    # per-player score board records: filled from NetGame_PlayerAt/NetGame_TeamAt by 0x5bae00
    # ("Multiplayer Flag"), written/read as net packets (0x566ea0, 0x566fd0), drawn by
    # CMultiPlayerGameType::DrawMiniScoreBoard and the CTF/death match game types
    (0x5663f0, 0x5669d0, 'Multiplayer'),
    # one-liner (mov eax, ecx) shared by unrelated callers (folded code), blocks the neighbours
    (0x5664b0, 0x5664c0, ''),
    # one-liner shared by Files_SearchDisk, ZipNode_List, the sound streams and the game types (folded
    # code)
    (0x5669d0, 0x5669e0, ''),
    # score board records, continued
    (0x566a80, 0x566e40, 'Multiplayer'),
    # one-liner shared by World_DrawDirtyTiles, CEntityBase and the score board (folded code)
    (0x566e40, 0x566e50, ''),
    # score board records, continued (net write 0x566ea0, net read 0x566fd0)
    (0x566e50, 0x567630, 'Multiplayer'),
    # CScreenFrame client rectangle helpers (ClientToScreen/GetClientRect; callers CScreenFrame)
    (0x5693a0, 0x5695b0, 'Rendering'),
    # CSegmentedInventoryItem helpers (callers its slots 32/33)
    (0x56a6b0, 0x56a8e0, 'Inventory and weapons'),
    # the image sequence list, its debug window ("!DEBUG! %s") and player at the end of the CSequence range
    # (band D batch 2026-10-26)
    (0x56bf00, 0x56c650, 'Rendering'),
    # builds the energy HUD render data (vtables CEnergyHUDRenderData/CInventoryHUDRenderData) for
    # CShieldHUDRenderData
    (0x56ec50, 0x56ed30, 'Rendering'),
    (0x581860, 0x5818a0, 'Rendering'),
    # only caller CShieldInventoryHUDRenderer slot 25
    (0x56f8a0, 0x56f8c0, 'Inventory and weapons'),
    # CSimpleLayerPart constructor/destructors (LayerPart_Dtor) and net read/write for
    # CLightningLayerPart
    (0x571680, 0x5717d0, 'World'),
    # CSimpleWarpEffect drawing variants, only caller 0x571c50
    (0x5726c0, 0x573670, 'Rendering'),
    # class CSkeletonTransition ("From State", "To State", "Trigger"), registered by
    # CSkeletonAI::RegisterFields, used by 0x577b60
    (0x574af0, 0x574fa0, 'AI'),
    # only caller CSkeletonAI slot 25
    (0x5788a0, 0x5788c0, 'AI'),
    # a full-screen image shown for 3 s or until a key (the splash of App_Start 0x5a12d0; screen frame, frame
    # conversion, App_PumpMessages, the key table 0x6c1050); band D batch 2026-10-26
    (0x5788f0, 0x5790a0, 'Rendering'),
    # sound manager and sound resources: DirectSound objects 0x713694/0x7136f4, "Sound List",
    # ".wav"/".ogg", stream buffer pool 0x716080 ("The Local.txt table, ...", section 7)
    (0x57be20, 0x57e9e0, 'Sound'),
    # sound options from Zax.CFG ("Master Voume", "SFx Mute", "Sounds Mixing Chanels", "Music Volume")
    # into the resource cache, called by Game_Startup
    (0x57ee90, 0x57efd0, 'Sound'),
    # sound options window (Window_AddChild, "Interface", "Master Voume", "SFx Mute")
    (0x57efd0, 0x57fd00, 'User interface'),
    # FormatLength, part of ZString_Format (reimplemented with ZString)
    (0x585490, 0x5856f0, 'Core'),
    # CStrafeAttackAI's class info getter and its startup registration (classreg_getters.cpp, classreg_d.cpp)
    (0x587000, 0x587060, 'AI'),
    # threaded sound streams ("Use Threaded Stream", "Stream sound", timeSetEvent/timeKillEvent),
    # callers SoundStream_*, CMusicMixer
    (0x587060, 0x587a00, 'Sound'),
    # C string helpers (Text_*, Hex_*), callers ZString_*, the object reader, Ini_*
    (0x587a00, 0x588190, 'Core'),
    # swapped layer file names: flushing and expiring cached maps ("Levels", ".zax", "Attempting to
    # load expired map", "Invalid Entry in CSwappedLayerFilenameMappingTable")
    (0x589270, 0x58a0f0, 'World'),
    # temp file names of the swapped layers (File_TempName), callers CSwappedLayerFilenameMapping*
    (0x58b7c0, 0x58b840, 'World'),
    # the frames' image file reader and writer (callers CFrame slot 63, CFont slot 28, the mini map and the
    # screenshots; pixel conversions per depth, RLE, FileBuf); band D batch 2026-10-26
    (0x58c6f0, 0x58d840, 'Rendering'),
    # class CUserDefinedClassRef ("Class Definition" resource field) and its resource list (vtable
    # 0x602170: ResourceList_Find, NamedList_FindOrFail)
    (0x58fe20, 0x5908a0, 'Core'),
    # the build numbers 1308/1306 and the version texts ("%s %s %d", "Zax Version 1.0 Build 1308"), callers
    # the net game (packets, GameSpy "gamever"/"gamebuild") and the main menu (quest_data batch 2026-10-26)
    (0x5908a0, 0x5909b0, 'Game flow'),
    # display mode: "Screen Width/Height/Bit Depth" from Zax.CFG and the command line, "Couldn't set
    # resolution", sets it through CScreenFrame 0x567f20
    (0x590ce0, 0x591020, 'Rendering'),
    # video options window ("555"/"565", "Screen Brightness", "%d X %d", window toolkit)
    (0x591020, 0x591a90, 'User interface'),
    # a list box's data source over a named list (vtable 0x602590, constructor 0x592350; callers the list
    # box CJListBox 0x4041f0/0x404520)
    (0x592350, 0x592410, 'User interface'),
    # voice log panel ("~CVoiceLogPanel", "Replay", CJListBox, Window_AddChild, its sort callback 0x5959d0)
    # and its entries (vtable 0x602640): the page, not the data (the elements 0x592410-0x592a90 are
    # Quests and dialogue by class); was Quests and dialogue until 2026-10-26
    (0x5948b0, 0x595ac0, 'User interface'),
    # voice-over playback ("Play Voiceovers", threaded stream 0x587310), callers CPlayVOAction,
    # CSayQuip, CZaxDialogAction
    (0x595ac0, 0x595cd0, 'Sound'),
    # CWarpEffect constructor/destructors and net read/write (callers CSimpleWarpEffect)
    (0x596490, 0x5965d0, 'Rendering'),
    # CWindowedBrowsableImage ("~CWindowedBrowsableImage", window toolkit 0x40a8a0/0x40b190)
    (0x599330, 0x5994d0, 'User interface'),
    # the sound objects' base constructors and destructor (vtables 0x602da8/0x602dac; callers
    # SoundObject_Ctor 0x4e2220 and 0x4e2b80)
    (0x5994d0, 0x599560, 'Sound'),
    # the key mapping Input_VirtualKeyToGameKey and the message pump App_PumpMessages
    (0x599580, 0x599b80, 'User interface'),
    # the application object's constructor and destructors (CZaxApp, g_App 0x713f14)
    (0x59a670, 0x59ab00, 'Game flow'),
    # a text block (type 3) sent to a player (0x59b230) and the chat from the server to every player
    # (0x59b260, ChatFromServerReader)
    (0x59b230, 0x59b370, 'Multiplayer'),
    # a chat line shown on the screen (the font list's text 0x506c30)
    (0x59b370, 0x59b3a0, 'User interface'),
    # the pending map switch (CSwitchMapAction::Switch)
    (0x59b3a0, 0x59b400, 'Game flow'),
    # the game object after World_OpenMainMenu: menus, net game start, default key bindings, players
    # ("%s's Player"), custom rules setup, "Weapon AutoSwitch", map flush/expire, NIS fast forward,
    # quick save, "Load Game Data"
    (0x59baf0, 0x5a13a0, 'Game flow'),
    # within it: the inventory window opened and closed (user interface) and the status text drawn (band D
    # batch 2026-10-26)
    (0x5a0760, 0x5a0850, 'User interface'),
    (0x5a0a30, 0x5a0c60, 'Rendering'),
    # CZaxCustomRules destructor
    (0x5a13a0, 0x5a13d0, 'Game flow'),
    # default option objects created by the custom rules (CMaterialsGeneratorDisplayOptions,
    # CInventoryDisplayOptions, CDialogDisplayOptions, ...)
    (0x5a1cd0, 0x5a1ee0, 'Game flow'),
    # default option objects of the custom rules, continued
    (0x5a1f40, 0x5a26f0, 'Game flow'),
    # game start ("Normal"), NIS start/end ("Attempting to start NonInteractiveSequence ...")
    (0x5a2940, 0x5a2bf0, 'Game flow'),
    # path helpers of the game start (callers World_StartNewGame_Base, CZaxSaveGameStats,
    # CSelectMapDialog_OnOk)
    (0x5a3180, 0x5a33e0, 'Game flow'),
    # constructor/destructor of the net local class with vtable 0x602fa8 (NetLocal_Ctor/Dtor)
    (0x5a33e0, 0x5a3480, 'Multiplayer'),
    # save game stats helpers (callers CZaxSaveGameStats, the mission actions)
    (0x5a3480, 0x5a3650, 'Game flow'),
    # definition list loaders ("Quips", "can", "DIFICULTY" property files; ResourceLists_Add,
    # FileSpec, Files_*)
    (0x5a3650, 0x5a3f50, 'Core'),
    # definition record init and pointer/string array reserves (pool_get/alloc/free)
    (0x5a4060, 0x5a4270, 'Core'),
    # inventory counts per category, callers the inventory detail renderers
    (0x5b2aa0, 0x5b2d20, 'Inventory and weapons'),
    # the HUD render data's setters (CInventoryHUDRenderData +8/+0xc, CEnergyHUDRenderData +0x14/+0x18, changed
    # bits) and an item icon drawn for the HUD renderers (band D batch 2026-10-26)
    (0x5aca10, 0x5aca50, 'Rendering'),
    (0x5acab0, 0x5acc00, 'Rendering'),
    # a net player's item record (the scoreboard's icons; ZaxNet_PlayerOf, ZaxNetRecord); band D batch 2026-10-26
    (0x5b7f00, 0x5b8010, 'Multiplayer'),
    # CZaxInventoryTypeDefinition constructor/destructor and a helper of CZaxInventoryItemDefinition
    (0x5b9b30, 0x5b9bb0, 'Inventory and weapons'),
    # CZaxLayerSaveData constructor
    (0x5b9ef0, 0x5b9f20, 'World'),
    # methods of the net local class with vtable 0x602fa8 (slots 0x5ba9c0, 0x5bad90: GameSpy
    # "gamename"/"gamever"), NetGame_PlayerAt
    (0x5ba810, 0x5bae00, 'Multiplayer'),
    # fills the score board records from the net game players and teams ("Multiplayer Flag",
    # NetGame_PlayerAt, NetGame_TeamAt)
    (0x5bae00, 0x5bb060, 'Multiplayer'),
    # CZaxSaveData constructor
    (0x5bb6c0, 0x5bb730, 'Game flow'),
    # save games ("SaveGames/%s.sav", "QuickSave", "Ore Deposits", "Crystals") and their names
    (0x5bc2a0, 0x5bccb0, 'Game flow'),
    # registers the client block readers ("CStartGameInfoBlockReader",
    # "CChatMessageFromServerBlockReader", ...)
    (0x5bd8b0, 0x5bdc90, 'Multiplayer'),
    # menu base ("Open Menu"), callers CDifficultyMenu_ctor, CZaxMenu_ctor
    (0x5bdc90, 0x5be050, 'User interface'),
    # between CDifficultyMenu and CZaxMenu: menu destructor and the start buttons ("QuestMode",
    # "Intro")
    (0x5bfa90, 0x5bfc40, 'User interface'),
    # the multiplayer chat window ("~ChatWindow", window toolkit, an edit field); was Multiplayer until the band D
    # batch 2026-10-26
    (0x5c1040, 0x5c11c0, 'User interface'),
    # Pack and its counterpart: zlib compress/uncompress for TempFileField
    (0x5c3b40, 0x5c3bf0, 'Core'),
]

# Class name (regular expression, first match wins) -> subsystem
SUBSYSTEMS = [
    # path finding data of the way point map and the level's object grid; they reached Core only
    # through 'List' or their parent CObject ("Path classes ...", "CSortList2D, ...")
    (r'^C(PathCollideChecker|PathLineSegment|PathPointListListCache|PointList|SortList2D)$', 'World'),
    (r'MultiPlayer|Network|NetGame|DPlay|Lobby|Chat', 'Multiplayer'),
    (r'AI$|AIBase|Behavior|Brain|Fidget|Patrol|Attack|Chase|Flee|Wander|Guard|Pursue', 'AI'),
    (r'Action|ActionQueue|Trigger|Relay|Script', 'Actions'),
    (r'Quest|Dialog|Objective|VoiceLog|Mission', 'Quests and dialogue'),
    # the HUD's item renderers and their render data, the inventory screen's detail renderers, the compass and
    # the auto-map options (DECOMPILATION.md section 9, "The HUD"); before 'Inventory' and 'HUD'
    (r'InventoryHUDRender|HUDRenderData|InventoryDetailRender|^CCompassRender|^CHudBarOptions$', 'Rendering'),
    (r'Inventory|Item|Weapon|Ammo|Energy|Pickup|Goodie|Materials', 'Inventory and weapons'),
    (r'Particle|Effect|Render|Blend|Frame|Sequence|Light|Shadow|Font|Palette|DIB|Bitmap|Image', 'Rendering'),
    (r'Window|Menu|Button|Display|HUD|Hud|Screen|ListBox|Tab(?!le)|Cursor|Interface|Panel', 'User interface'),
    (r'Sound|Music|Mixer|Audio|Voice|Envelope', 'Sound'),
    (r'Entity|Character|Projectile|Explosion|Door', 'Objects'),
    (r'Layer|Level|World|Map|Poly|WayPoint|Way|Tile|Collision|Area|Zone|Camera|Model', 'World'),
    (r'SaveGame|Save|Stats|Difficulty|GameType|Game|Team|Player|Rules', 'Game flow'),
    (r'Object|Collection|ClassInfo|String|List|Array|Map|Hash|File|Zip|Stream|Holder|Cache|Data', 'Core'),
]

# Names without a class (exe_symbols.csv): prefix -> subsystem
PREFIXES = [
    (r'^(ClassInfo|ClassExtraList|PtrArray|Array[0-9]+_|StringArray_|ZString|FieldDesc|operator_new|pool_|File|Files|FileBuf|Zip|NameSet|Cpu_|DirList|Path_)',
     'Core'),
    # name families of the Core batches (field kinds, streams, named lists, object table, definition lists)
    (r'^(\w*Field|FieldKinds|FieldColor|FieldNameList|DynamicPropert(y|ies)|ExtraList|DiskStream|Stream|'
     r'ObjectTable|NamedList|ListView|RecordList|EnumTable(NoAlt)?|ResourceAliases|DefinitionList|Definition|'
     r'CategoryList|Categories|DamageTypeList|DamageTypes|CannedObjectList|ResourceCache|Resource|ResourceLists?)'
     r'(_|::)', 'Core'),
    (r'^(DPlay|DPlaySession|GameSpy|NetGame|NetSession|NetSessionBase|NetPacket|NetTrafficGraph|BlockReader)_',
     'Multiplayer'),
    (r'^(World|WayMap|Map_|SortCell_|SortList_|LayerSaveData|Entity_Place|Entity_CreateFromModel)', 'World'),
    (r'^(Blend|Amplify|BlackBias|TransparentMask|Draw16|Draw|RotSeq|RenderPoly|Entity_Draw|'
     r'DrawRotation|InitBlend|RegisterBlend|Frame_|CBlend|g_Blend|g_Fade|g_Draw|g_NoCut|HudMiniMap)', 'Rendering'),
    (r'^(CMainMenu|CZaxMenu|CDifficultyMenu|CSelectMap|Button|Window|GetMainMenu)', 'User interface'),
    (r'^(Weapon|Zax_Drop|UndeployedInventory|GiveItems)', 'Inventory and weapons'),
    (r'^(CompanionAI|EntityBehavior|PickupAI|AutoGunAI)', 'AI'),
    (r'^(Entity|EntityAnimated|EntityDestructable)', 'Objects'),
    (r'^(Sound)', 'Sound'),
    (r'^(Game_|DebugLog|KeyBinding|g_App|g_World|g_MainMenu)', 'Game flow'),
]


def subsystem_of_class(name, parent):
    chain = []
    n = name
    while n:
        chain.append(n)
        n = parent.get(n)
    for c in chain:
        for rx, sub in SUBSYSTEMS:
            if re.search(rx, c):
                return sub
    return None


def main(argv):
    raw = [r for r in csv.reader(open(ROOT / 'out' / 'ghidra' / 'functions_raw.tsv'), delimiter='\t')]
    classes = {r['name']: r for r in csv.DictReader(open(ROOT / 'docs' / 'classes.csv'))}
    parent = {n: r['parent'] or None for n, r in classes.items()}
    symbols = {int(r['address'], 16): r for r in csv.DictReader(open(ROOT / 'docs' / 'exe_symbols.csv'))}
    reimpl = {}
    p = ROOT / 'docs' / 'reimpl.csv'
    if p.exists():
        reimpl = {int(r['address'], 16): r['status'] for r in csv.DictReader(open(p))}

    rows = []
    for adr, size, ns, name, thunk, callers, callees in raw:
        a = int(adr, 16)
        if ns.endswith('.DLL') or ns == '<EXTERNAL>':
            continue                       # import thunks
        r = {'address': f'0x{a:x}', 'size': SIZE_FIXES.get(a, int(size)), 'class': ns if ns in classes else '',
             'name': name, 'callers': int(callers), 'callees': int(callees)}
        sub, how = None, ''
        for lo, hi, t in RANGES:
            if lo <= a < hi:
                sub, how = t, 'range'
        library = sub is not None
        explicit = False
        # a symbol whose source begins with "Rendering, not reimplemented:" (or User interface, Sound) belongs
        # there whatever its class (a draw slot of a game class; docs/DECOMPILATION.md section 8, band B)
        if not sub and a in symbols:
            m = re.match(r'(Rendering|User interface|Sound), not reimplemented:', symbols[a]['source'])
            if m:
                sub, how, explicit = m.group(1), 'name', True
        if not sub and r['class']:
            sub, how = subsystem_of_class(r['class'], parent), 'class'
        if not sub and a in symbols:
            nm = symbols[a]['name']
            if symbols[a]['class']:
                sub = subsystem_of_class(symbols[a]['class'], parent)
            for rx, t in PREFIXES:
                if not sub and re.search(rx, nm):
                    sub = t
            how = 'name' if sub else ''
        correction = None
        for lo, hi, t in CORRECTIONS:
            if lo <= a < hi:
                correction = t
        names = [name] + ([symbols[a]['name']] if a in symbols else [])
        keep = any(re.search(CORRECTION_KEEPS, n) for n in names)
        if correction is not None and not library and not explicit and (not sub or (sub != correction and not keep)):
            sub, how = correction, 'range'
        r['subsystem'], r['assignment'] = sub or '', how
        if a in reimpl:
            r['status'] = reimpl[a]
        elif a in symbols or (r['class'] and not name.startswith('vf')):
            r['status'] = 'named'
        elif r['class']:
            r['status'] = 'assigned'
        else:
            r['status'] = 'open'
        rows.append(r)

    # Neighbours: between two functions with the same subsystem (game code only)
    rows.sort(key=lambda r: int(r['address'], 16))
    known = [i for i, r in enumerate(rows) if r['subsystem'] or r['assignment'] == 'range']
    for k1, k2 in zip(known, known[1:]):
        t1, t2 = rows[k1]['subsystem'], rows[k2]['subsystem']
        if t1 and t1 == t2 and k2 - k1 > 1 and not t1.startswith(('Library', 'Copy protection')):
            for i in range(k1 + 1, k2):
                rows[i]['subsystem'], rows[i]['assignment'] = t1, 'neighbours'

    cols = ['address', 'size', 'subsystem', 'assignment', 'class', 'name', 'status', 'callers', 'callees']
    with open(ROOT / 'docs' / 'functions.csv', 'w', newline='') as fh:
        w = csv.DictWriter(fh, cols)
        w.writeheader()
        w.writerows(rows)

    by = defaultdict(Counter)
    size = Counter()
    for r in rows:
        t = r['subsystem'] or '(unknown)'
        by[t][r['status']] += 1
        by[t]['_n'] += 1
        size[t] += r['size']
    total = sum(size.values())
    print(f'{len(rows)} functions, {total:,} bytes')
    for t in sorted(by, key=lambda t: -size[t]):
        c = by[t]
        print(f'  {t:28} {c["_n"]:5}  {size[t] * 100 / total:5.1f} %  '
              f'named {c["named"]:4}  assigned {c["assigned"]:4}  open {c["open"]:4}  '
              f'reimplemented {c["reimplemented"] + c["verified"]}')


if __name__ == '__main__':
    main(sys.argv)
