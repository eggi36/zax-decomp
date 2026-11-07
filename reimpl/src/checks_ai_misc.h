// Shared parts of the checks of the AI modules of 0x568000-0x577fff (checks_ai_simple.cpp,
// checks_ai_shield.cpp, checks_ai_skeleton.cpp), in the game thread, on top of the fake world of
// checks_ai_world.h and the class machinery of checks_inventory_common.h.
//
//  - A slot test names the class of the object under test; the object is a copy of a template made by
//    the class's Create (or of a real AI of the level), its fields set by the class's filler, every
//    pointer into the game replaced by fakes (a copy must not reach real objects).
//  - Real data: the AIs of the classes on the level's parts and the AIs their fields point to
//    (aiw::part_ais), at most a cap per class in a deterministic spread, through the class's slots.
#pragma once
#include "checks_ai_world.h"
#include "checks_inventory_common.h"

namespace zax::aimisc {

using namespace aiw;

// A fake of the extra objects (O_EXTRA ...)
inline BYTE *extra(int i) { return obj(O_EXTRA + i); }
// A pointer array's data in an extra object: n entries, each a fake of the type or 0 one time in
// null_one_in (0: never)
inline BYTE *fake_pointers(int extra_index, DWORD n, Gen &g, int type_first, int type_count, int null_one_in)
{
    BYTE *d = extra(extra_index);
    for (DWORD i = 0; i < n; i++)
        fl<DWORD>(d, i * 4) = null_one_in && g.next() % (DWORD)null_one_in == 0 ? 0 : ptrv(obj(type_first + (int)(g.next() % (DWORD)type_count)));
    return d;
}
// A message object in an extra object: vtable 0x5eeb64, the id (one of `ids` or random), the byte 0,
// then the given dwords (+0xc, +0x10, +0x14, +0x18)
inline BYTE *fake_message(int extra_index, Gen &g, const DWORD *ids, int nids)
{
    BYTE *m = extra(extra_index);
    fl<DWORD>(m, 0) = 0x5eeb64;
    const DWORD k = g.next() % (DWORD)(nids + 1);
    fl<DWORD>(m, 4) = k < (DWORD)nids ? *at<DWORD *>(ids[k]) : g.next();
    fl<BYTE>(m, 8) = 0;
    return m;
}

// One class of a module: its table entry, the filler of the object under test, its slots
struct SlotClass {
    invcheck::ClassDef def;
    DWORD vtable;
    void (*fill)(Gen &g, BYTE *s, bool real);   // the fields (real: a copy of a real AI, only pointers replaced)
};
struct ClassSlot {
    int cls;
    SlotTest t;
};

// The state of a module's slot run
struct Module {
    const SlotClass *classes;
    int nclasses;
    const ClassSlot *slots;
    int nslots;
    BYTE *templates[24];
    int self_class;
    const BYTE *real_self;
    const BYTE *real_part;
};
inline Module *g_module;

inline void make_self(Gen &g)
{
    Module &m = *g_module;
    const SlotClass &c = m.classes[m.self_class];
    BYTE *s = obj(O_SELF);
    memcpy(s, m.real_self ? m.real_self : m.templates[m.self_class], c.def.size);
    fl<DWORD>(s, 4) = 0;     // no dynamic properties, no broadcaster
    fl<DWORD>(s, 0xc) = 0;
    if (m.real_part)
        for (int i = 0; i < N_ENTITY; i++) {
            BYTE *e = obj(O_ENTITY + i);
            memcpy(e, m.real_part, OBJ);
            fl<DWORD>(e, 0) = ptrv(g_fvt[T_ENTITY]);
            fl<DWORD>(e, 0x48) = ptrv(obj(O_LAYER));
        }
    for (int i = 0; i < N_OTHER; i++)
        fl<DWORD>(obj(O_OTHER + i), 0) = ptrv(g_fvt[T_OTHER]);
    c.fill(g, s, m.real_self != nullptr);
}

inline void make_templates(Module &m)
{
    for (int c = 0; c < m.nclasses; c++)
        m.templates[c] = m.classes[c].def.create ? static_cast<BYTE *>(at<void *(__cdecl *)()>(m.classes[c].def.create)()) : nullptr;
}
inline void free_templates(Module &m)
{
    for (int c = 0; c < m.nclasses; c++)
        if (m.templates[c])
            vcall<void *>(m.templates[c], 1, 1);
}

inline void run_slots(Module &m, Tally &t, const Policy &policy)
{
    g_module = &m;
    for (int k = 0; k < m.nslots; k++) {
        const ClassSlot &s = m.slots[k];
        m.self_class = s.cls;
        m.real_self = nullptr;
        m.real_part = nullptr;
        for (unsigned seed = 1; seed <= (unsigned)scaled(s.t.count); seed++)
            run_slot(t, s.t, seed * 2654435761u + s.t.address, make_self, policy);
    }
}

inline BYTE *g_parts[8192];

// The level's AIs of the module's classes (copies) through their slots: at most `cap` of a class (the
// first, the last and every k-th in level order), 3 seeds each
inline void run_real(Module &m, const char *module, Tally &registered, Tally &t, const Policy &policy, int cap)
{
    g_module = &m;
    for (int c = 0; c < m.nclasses; c++) {
        const invcheck::ClassDef &d = m.classes[c].def;
        const CClassInfo *ci = *at<CClassInfo **>(d.var);
        char text[160];
        wsprintfA(text, "[%s: class info %x, size %x]", d.name, ptrv(ci), ci ? ci->size : 0);
        registered.check(ci && ci->size == d.size && at<CClassInfo *(__cdecl *)()>(d.getters[0])() == ci &&
                             at<CClassInfo *(__cdecl *)()>(d.getters[1])() == ci,
                         d.name, text);
    }
    BYTE **parts = g_parts;
    const int n = level_parts(parts, sizeof g_parts / sizeof g_parts[0]);
    if (n < 0) {
        log("  %s real data: no level grid found", module);
        return;
    }
    // first pass: count per class; second: every k-th
    int total[24] = {}, taken[24] = {}, seen[24] = {}, ran = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < n; i++) {
            const BYTE *part = parts[i];
            BYTE *ais[64];
            const int count = part_ais(part, parts, n, ais, 64);
            for (int k = 0; k < count; k++) {
                const CClassInfo *ci = class_of(ais[k]);
                for (int c = 0; c < m.nclasses; c++) {
                    const SlotClass &sc = m.classes[c];
                    if (!ci || ci != *at<CClassInfo **>(sc.def.var) || !readable(ais[k], sc.def.size))
                        continue;
                    if (pass == 0) {
                        total[c]++;
                        t.check(fl<DWORD>(ais[k], 0) == sc.vtable, sc.def.name, "[vtable]");
                        continue;
                    }
                    if (!spread_pick(seen[c]++, total[c], cap))
                        continue;
                    taken[c]++;
                    m.self_class = c;
                    m.real_self = ais[k];
                    m.real_part = readable(part, OBJ) ? part : nullptr;
                    for (int s = 0; s < m.nslots; s++)
                        if (m.slots[s].cls == c)
                            for (unsigned seed = 1; seed <= 3; seed++) {
                                run_slot(t, m.slots[s].t, seed * 40503u + ptrv(ais[k]), make_self, policy);
                                ran++;
                            }
                    m.real_self = nullptr;
                    m.real_part = nullptr;
                }
            }
        }
    char line[400];
    int len = wsprintfA(line, "  %s real data: %d parts;", module, n);
    for (int c = 0; c < m.nclasses && len < 340; c++)
        if (total[c])
            len += wsprintfA(line + len, " %s %d (%d run)", m.classes[c].def.name, total[c], taken[c]);
    wsprintfA(line + len, "; %d cases", ran);
    log("%s", line);
}

// The class tests of the module (invcheck: getters, Create/CreateArray and slot 1, RegisterFields, the
// registered class infos)
inline void run_classes(Module &m, int (*hooks)(Hook *), const char *module, Tally &objects, Tally &getters, Tally &fields, Tally &real)
{
    invcheck::g_hook_table = hooks;
    if (!invcheck::arena_setup()) {
        objects.check(false, module, "[no arena]");
        return;
    }
    invcheck::ClassDef defs[24];
    for (int c = 0; c < m.nclasses; c++)
        defs[c] = m.classes[c].def;
    invcheck::test_classes(objects, getters, fields, real, defs, m.nclasses);
}

}  // namespace zax::aimisc
