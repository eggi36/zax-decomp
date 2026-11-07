// The quest game type and the quest statistics of Zax.exe (phase 3, game logic; 0x5516a0-0x55ac50): for
// the 21 classes their Create, CreateArray, class getters (slot 2 and the copies), RegisterFields and
// deleting destructors (slot 1), the game type's slots and helpers, the statistics' slots (mission end,
// value texts, point value, hidden tests) and helpers, CIncrementCounterStatAction's slot 23. Left out:
// the statistic's drawing (CQuestStat slot 24, 0x5533e0: Rendering) and the deleting destructor of
// CQuestStatColumnInfo (0x426bf0, in the inventory batch's range).
//
// Classes [confirmed: constructors, destructors, RegisterFields, docs/fields.csv]:
//  - CQuestGameType (0x44 bytes, class 0x712304, vtable 0x5fdda0, parent CGameType 0x4ce850, its
//    constructor 0x4ceaa0 and destructor 0x4ceb20): +0x18 "Stat/Column Options List" (new[] array of
//    CQuestStatColumnInfo by value: vtable 0x5fde64, +0x1c data, +0x20 count, +0x24 capacity), +0x28
//    "Stat/Row Options List" (pointer array of CQuestStat: vtable 0x5fde58, +0x2c data, +0x30 count,
//    +0x34 capacity; element descriptor "Stat Options" kept in 0x712308), +0x38 "Stat/Par Score Label
//    Format" (LocalEntry), +0x40 "Stat/Scroll Rate" (float, "Pixels/Tick", shown as "Pixels/Second").
//    The stats are indexed by the statistic list 0x712278 (18 names, "Ore Collected" ...).
//  - CQuestStatColumnInfo (0x28, 0x712300, 0x5ee174, parent CObject): +8 "Column Heading", +0x10
//    "Column Value Format" (LocalEntry each), +0x18 "Heading Font" (font list 0x6be1e0), +0x1c "X
//    Offset", +0x20 "Width", +0x24 "Justification" (0x6be1b8).
//  - CQuestGameTypeSpecificData (0x18, 0x712330, 0x5fde70): +8 "Stat Data List" (pointer array of
//    CQuestStatData, vtable 0x5fdecc, +0xc data, +0x10 count, +0x14 capacity; one entry per statistic).
//    The current one is +0x30 of what the application's slot 97 gives (the match state's place).
//  - CQuestStatParScoreData (8, 0x712320, 0x5fdf34) and CQuestStatData (8, 0x712324, 0x5fdfec):
//    abstract bases of the par scores (in CMission's "Par Stat Data List") and of the data.
//  - CQuestStat (0x24, 0x712328, 0x5fe0d8, abstract): +8 "Hide This Stat", +9 "Hide Par Score" (bytes),
//    +0xc "Point Value Weighting", +0x10 "Par Point Offset Weighting" (floats), +0x14 "Stat Label"
//    (LocalEntry), +0x1c "Stat Label Font", +0x20 "Stat Value Font". CQuestTimeStatBase (0x5fe46c,
//    abstract) between it and the time stats. The five statistics: CQuestCounterStat (0x5fe048, data
//    CQuestCounterData 0x14 / par CQuestCounterParScore 0xc: ints), CQuestFloatAccumulatorStat
//    (0x5fe290, data 0x14 / par 0xc: floats), CQuestTimeStat (0x5fe3d8) and CQuestTotalTimeStat
//    (0x5fe500) (data CQuestTimeData 0x10 / par CQuestTimeParScore 0xc), CQuestPercentageStat
//    (0x5fe64c, data CQuestPercentageData 0x20 / par CQuestPercentageParScore 0x10: numerator and
//    denominator).
//  - CIncrementCounterStatAction (0x10, 0x712340, 0x5fe168, parent CAction): +8 "Statistic To
//    Increment" (index into 0x712278), +0xc "Amount To Increment By".
#pragma once
#include "class_info.h"

namespace zax {

// One class of the module, for the checks: name, class variable, Create, CreateArray, RegisterFields
// (0 = none of its own), class getter (slot 2), deleting destructor (0 = not reimplemented), size,
// vtable; abstract classes have the exe's generic Create 0x46b760
struct QuestClassDef {
    const char *name;
    // key-typed (batch 6-7), so the table's addresses go through the address layer; only the checks read them, in the
    // DLL, where every key is the DWORD address itself
    GlobKey var;
    CodeKey create, create_array, register_fields, getter, dtor;
    DWORD size;
    VtKey vtable;
};

constexpr QuestClassDef QUEST_STAT_CLASSES[] = {
    {"CQuestStatColumnInfo", 0x712300, 0x5516a0, 0x5516e0, 0x5517b0, 0x551750, 0, 0x28, 0x5ee174},
    {"CQuestGameType", 0x712304, 0x551c70, 0x551cd0, 0x551f30, 0x551ed0, 0x551d70, 0x44, 0x5fdda0},
    {"CQuestStatParScoreData", 0x712320, 0, 0, 0, 0x552c40, 0x554140, 0x8, 0x5fdf34},
    {"CQuestStatData", 0x712324, 0, 0, 0, 0x552cf0, 0x5544f0, 0x8, 0x5fdfec},
    {"CQuestStat", 0x712328, 0, 0, 0x552e00, 0x552da0, 0x554d30, 0x24, 0x5fe0d8},
    {"CQuestGameTypeSpecificData", 0x712330, 0x553c50, 0x553ca0, 0x553e60, 0x553e00, 0x553d20, 0x18, 0x5fde70},
    {"CQuestCounterParScore", 0x712334, 0x554170, 0x5541b0, 0x5542d0, 0x555890, 0x554220, 0xc, 0x5fded8},
    {"CQuestCounterData", 0x712338, 0x554520, 0x554560, 0x554680, 0x5557f0, 0x5545d0, 0x14, 0x5fdf90},
    {"CQuestCounterStat", 0x71233c, 0x554d80, 0x554dd0, 0, 0x554f20, 0x554e50, 0x24, 0x5fe048},
    {"CIncrementCounterStatAction", 0x712340, 0x555a50, 0x555a80, 0x555bc0, 0x555b60, 0x555ae0, 0x10, 0x5fe168},
    {"CQuestFloatAccumulatorParScore", 0x712344, 0x556000, 0x556040, 0x556160, 0x557510, 0x5560b0, 0xc, 0x5fe1d8},
    {"CQuestFloatAccumulatorData", 0x712348, 0x556380, 0x5563c0, 0x5564e0, 0x557470, 0x556430, 0x14, 0x5fe234},
    {"CQuestFloatAccumulatorStat", 0x71234c, 0x5569e0, 0x556a30, 0, 0x556b80, 0x556ab0, 0x24, 0x5fe290},
    {"CQuestTimeParScore", 0x712350, 0x557710, 0x557750, 0x557870, 0x5588d0, 0x5577c0, 0xc, 0x5fe320},
    {"CQuestTimeData", 0x712354, 0x557a90, 0x557ad0, 0x557c80, 0x557bd0, 0x557b30, 0x10, 0x5fe37c},
    {"CQuestTimeStatBase", 0x712358, 0, 0, 0, 0x558010, 0x558a60, 0x24, 0x5fe46c},
    {"CQuestTimeStat", 0x71235c, 0x558ab0, 0x558b00, 0, 0x558c60, 0x558b80, 0x24, 0x5fe3d8},
    {"CQuestTotalTimeStat", 0x712360, 0x558e60, 0x558eb0, 0, 0x559010, 0x558f30, 0x24, 0x5fe500},
    {"CQuestPercentageParScore", 0x712364, 0x5591b0, 0x5591f0, 0x559300, 0x55aa30, 0x559250, 0x10, 0x5fe594},
    {"CQuestPercentageData", 0x712368, 0x559690, 0x5596d0, 0x559880, 0x5597d0, 0x559730, 0x20, 0x5fe5f0},
    {"CQuestPercentageStat", 0x71236c, 0x559eb0, 0x559f00, 0, 0x55a050, 0x559f80, 0x24, 0x5fe64c},
};

// The other copies of the class getters (all reimplemented; the slot 2 ones are in QUEST_STAT_CLASSES)
struct QuestGetterCopy {
    CodeKey address;
    GlobKey var;
};
constexpr QuestGetterCopy QUEST_GETTER_COPIES[] = {
    {0x551c20, 0x712304}, {0x552bf0, 0x712320}, {0x552ca0, 0x712324}, {0x552d50, 0x712328}, {0x553c00, 0x712330},
    {0x5540a0, 0x712334}, {0x554450, 0x712338}, {0x554ae0, 0x71233c}, {0x555a00, 0x712340}, {0x555f60, 0x712344},
    {0x5562e0, 0x712348}, {0x556940, 0x71234c}, {0x557670, 0x712350}, {0x5579f0, 0x712354}, {0x557f70, 0x712358},
    {0x558970, 0x71235c}, {0x558d70, 0x712360}, {0x559110, 0x712364}, {0x5595f0, 0x712368}, {0x559e10, 0x71236c},
};

// Globals of the module: the cached quest game type (found by the name "QuestMode" in the game type
// list 0x6c02e8), the element descriptor of "Stat/Row Options List", the statistic name list, the two
// item types of slot 40 (guard bits in 0x712294)
constexpr DWORD QUEST_GAME_TYPE_CACHE = 0x71232c;
constexpr DWORD QUEST_STAT_OPTIONS_FIELD = 0x712308;
constexpr DWORD QUEST_STAT_LIST = 0x712278;
constexpr DWORD QUEST_ITEM_GUARD = 0x712294;
constexpr DWORD QUEST_ITEM_CRYSTALS = 0x712290;
constexpr DWORD QUEST_ITEM_ORE_DEPOSITS = 0x7122b4;

}  // namespace zax
