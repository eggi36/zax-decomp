// Checks of batch C4's small slots and callbacks (c4_misc.cpp) against the original, in the game thread
// (machinery: checks_inventory_common.h; side 0 with the module's hooks out, the C heap from each side's
// arena). Every exe function the code calls is a recording stand-in for the case (the pool's free too, so
// no block of ours reaches the pool); objects and windows are blocks of our own with recording vtables;
// the dialogue script list's records (0x62e810/0x62e814) are swapped for records of our own with script
// objects whose line tables hold real ZStrings.
#include "checks_inventory_common.h"
#include "init_list.h"

namespace zax {

int hooks_c4_misc(Hook *out);

namespace {

using namespace invcheck;

DWORD &D(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
DWORD rnd(DWORD n) { return (random() >> 8) % n; }

CodePatch g_p[16];
int g_np;
void stand_in(DWORD a, void *f) { patch_jump(g_p[g_np++], a, f); }
void begin(int side, BYTE pattern)
{
    side_begin(side);
    log_reset(side);
    arena_on(side, pattern);
}
void end(int side)
{
    while (g_np)
        unpatch(g_p[--g_np]);
    arena_off();
    side_end(side);
}

// Blocks of our own per side: pointers into them compare by offset
BYTE g_obj[2][0x400];
DWORD orel(DWORD v)
{
    const DWORD b = ptrv(g_obj[g_side]);
    return v - b < sizeof g_obj[0] ? 0x0b000000u + (v - b) : rel(v, g_side);
}
template <DWORD A> DWORD THISCALL t0(void *s) { ev(A, orel(ptrv(s))); return result(A & 0xff); }
template <DWORD A> DWORD THISCALL t1(void *s, DWORD a) { ev(A, orel(ptrv(s)), orel(a)); return result(A & 0xff); }
template <DWORD A> DWORD THISCALL t2(void *s, DWORD a, DWORD b) { ev(A, orel(ptrv(s)), orel(a), orel(b)); return result(A & 0xff); }
template <DWORD A> DWORD THISCALL t3(void *s, DWORD a, DWORD b, DWORD c) { ev(A, orel(ptrv(s)), orel(a), orel(b) ^ (c << 4)); return result(A & 0xff); }
template <DWORD A> DWORD THISCALL t5(void *s, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
    ev(A, orel(ptrv(s)), a ^ (b << 8), c ^ (d << 8) ^ (e << 16));
    return result(A & 0xff);
}
template <DWORD A> void *THISCALL ctor2(void *s, DWORD a, DWORD b) { ev(A, orel(a), orel(b)); return s; }
template <DWORD A> DWORD __cdecl c1(DWORD a) { ev(A, orel(a)); return result(A & 0xff); }
template <DWORD A> DWORD __fastcall f1(DWORD c) { ev(A, orel(c)); return result(A & 0xff); }
template <DWORD A> DWORD __fastcall f2(DWORD c, DWORD d) { ev(A, orel(c), orel(d)); return result(A & 0xff); }
// The pool's free while real ZStrings are destroyed too: their buffers (pool blocks of the game) are
// recorded by their size only (and left out of the pool: a few bytes per case)
DWORD __fastcall s_pool_free(DWORD p, DWORD n)
{
    const DWORD own = p - ptrv(g_obj[g_side]) < sizeof g_obj[0];
    ev(0x505, own ? orel(p) : 0xb0f0, n);
    return result(5);
}
DWORD g_vt[128];
void fill_vt()
{
    for (int i = 0; i < 128; i++)
        g_vt[i] = ptrv((void *)&t1<0x100>);
}

// ---- The application's slots ------------------------------------------------------------------------------

void test_app(Tally &t)
{
    fill_vt();
    g_vt[22] = ptrv((void *)&t2<0x122>);
    g_vt[76] = ptrv((void *)&t0<0x176>);
    for (int k = 0; k < scaled(64); k++) {
        DWORD e[2][5], saved = *at<DWORD *>(0x62e5d0);
        BYTE copy[2][0x40];
        const unsigned seed = (unsigned)k * 17u;
        for (int side = 0; side < 2; side++) {
            begin(side, 0x40);
            g_seed = seed;
            BYTE *app = g_obj[side];
            memset(app, 0, 0x100);
            D(app, 0) = ptrv(g_vt);
            D(app, 0x2c) = rnd(4) - 1;
            BYTE *win = g_obj[side] + 0x100;
            D(win, 0) = ptrv(g_vt);
            D(app, 0x34) = rnd(4) ? ptrv(win) : 0;
            stand_in(0x40bf00, (void *)&f2<0x201>);   // CJWindow::SetTextureList
            stand_in(0x40c680, (void *)&t2<0x202>);   // CJWindow::AddToParentFront
            stand_in(0x408f20, (void *)&ctor2<0x203>);   // CScreenWindow::CScreenWindow (returns this)
            stand_in(0x54d130, (void *)&f2<0x204>);   // pool free (not called here)
            e[side][0] = at<DWORD(THISCALL *)(void *)>(0x4cdd70)(app);
            e[side][1] = D(app, 0x34) ? at<DWORD(THISCALL *)(void *)>(0x4cdec0)(app) : 0;   // without a window the caller's eax
            e[side][2] = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4cded0)(app, rnd(2) ? 0x51 : 0, 0x52, 0x53, 0x54,
                                                                                                      rnd(2) ? 0x55 : 0);
            e[side][3] = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4cdf40)(app, 0x61, rnd(2) ? 0x100 : 0x101);
            void *w = at<void *(THISCALL *)(void *, DWORD, DWORD)>(0x4ce640)(app, 0x71, 0x72);
            e[side][4] = w ? 1 : 0;
            memcpy(copy[side], app, 0x40);
            end(side);
            if (w)
                pool_free(w, 0x17c);
            e[side][4] |= *at<DWORD *>(0x62e5d0) << 4;
            *at<DWORD *>(0x62e5d0) = saved;
        }
        t.check(memcmp(e[0], e[1], sizeof e[0]) == 0 && same_region(copy[0], copy[1], 0x40) && same_log(), "application slots");
    }
}

// ---- The dialogue action's slots 23 and 7 ------------------------------------------------------------------

DWORD g_ini_on;
DWORD __fastcall s_ini(DWORD a, DWORD b, const char *name, DWORD def)
{
    ev(0x301, a ^ b, fnv_text(2166136261u, name), def);
    return g_ini_on;
}
BYTE g_scripts[2][4][0x10];
BYTE g_lines[2][4][0x10];
DWORD g_line_table[2][4];
DWORD THISCALL s_load_record(void *list, DWORD i)
{
    ev(0x302, ptrv(list), i);
    BYTE *records = *at<BYTE **>(0x62e810);
    D(records + i * 0x80, 0x78) = ptrv(g_scripts[g_side][i % 4]);
    return result(2);
}
DWORD THISCALL s_precache_name(void *l, const char *name)
{
    ev(0x303, ptrv(l), fnv_text(2166136261u, name));
    return result(3);
}

void test_dialog(Tally &t)
{
    fill_vt();
    g_vt[29] = ptrv((void *)&t5<0x129>);
    for (int k = 0; k < 8; k++) {
        DWORD e[2];
        for (int side = 0; side < 2; side++) {
            BYTE *o = g_obj[side];
            D(o, 0) = ptrv(g_vt);
            begin(side, 0);
            e[side] = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x41d810)(o, (DWORD)k, 2, 3, 4);
            end(side);
        }
        t.check(e[0] == e[1] && same_log(), "dialogue action slot 23");
    }
    static const char *const TEXTS[] = {"Intro01", "Zax_Hello", "", "VO/Line 4"};
    for (int k = 0; k < scaled(80); k++) {
        DWORD e[2], saved[2];
        memcpy(saved, at<void *>(0x62e810), 8);
        const unsigned seed = (unsigned)k * 29u;
        for (int side = 0; side < 2; side++) {
            g_seed = seed;
            for (int i = 0; i < 4; i++) {
                at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(g_lines[side][i] + 8, TEXTS[(i + k) % 4]);
                g_line_table[side][i] = ptrv(g_lines[side][i]);
                D(g_scripts[side][i], 8) = ptrv(g_line_table[side]);
            }
            begin(side, 0x45);
            const DWORD n = 1 + rnd(4);
            BYTE *records = static_cast<BYTE *>(heap_alloc(n * 0x80));
            memset(records, 0, n * 0x80);
            for (DWORD i = 0; i < n; i++)
                D(records + i * 0x80, 0x78) = rnd(2) ? ptrv(g_scripts[side][i % 4]) : 0;
            *at<DWORD *>(0x62e810) = ptrv(records);
            *at<DWORD *>(0x62e814) = n;
            g_ini_on = rnd(5) != 0;
            BYTE *o = g_obj[side];
            D(o, 8) = rnd(n + 1);   // one beyond the count: index 0
            D(o, 0xc) = rnd(4);
            stand_in(0x4167b0, (void *)&t0<0x300>);   // CObject_Precache
            stand_in(0x4a4670, (void *)&s_ini);
            stand_in(0x55e090, (void *)&s_load_record);
            stand_in(0x57e7f0, (void *)&s_precache_name);
            e[side] = at<DWORD(THISCALL *)(void *)>(0x41da10)(o);
            end(side);
            memcpy(at<void *>(0x62e810), saved, 8);
            for (int i = 0; i < 4; i++)
                at<DWORD(THISCALL *)(void *)>(0x4defd0)(g_lines[side][i] + 8);
        }
        char d[60];
        wsprintfA(d, "[precache seed %u: %x / %x]", seed, e[0], e[1]);
        t.check(e[0] == e[1] && same_log(), "dialogue action precache", d);
    }
}

// ---- Destructors, readers, factories, callbacks, forwarders --------------------------------------------------

DWORD THISCALL s_rect(void *self, DWORD *rect)
{
    ev(0x401, orel(ptrv(self)));
    for (int i = 0; i < 4; i++)
        rect[i] = 0x40 + (DWORD)i;
    return result(1);
}
DWORD THISCALL s_draw_rect(void *frame, const DWORD *rect, DWORD colour)
{
    ev(0x402, ptrv(frame), rect[0] ^ rect[3], colour);
    return result(2);
}
void *g_made;
DWORD THISCALL s_set_last(void *o) { g_made = o; ev(0x403, D(o, 0)); return result(3); }

void test_objects(Tally &t)
{
    fill_vt();
    // CEntityBehaviorMoveToPos: arrays of 0-3 with flags 2/3, the object with flags 0/1
    for (DWORD flags = 0; flags < 4; flags++)
        for (DWORD n = 0; n < 4; n++) {
            DWORD e[2];
            for (int side = 0; side < 2; side++) {
                begin(side, 0x50);
                BYTE *block = g_obj[side];
                D(block, 0) = n;
                BYTE *self = block + 4;
                for (DWORD i = 0; i < 4; i++)
                    at<void *(THISCALL *)(void *, const char *)>(0x4dee00)(self + i * 0xbc + 0xb8, "");
                stand_in(0x4173f0, (void *)&t0<0x501>);   // Object_FreeFields
                stand_in(0x536e30, (void *)&t0<0x502>);   // CWayPointPath::Dtor
                stand_in(0x498dc0, (void *)&t0<0x503>);   // CEntityBehaviorIdle::Dtor
                stand_in(0x495360, (void *)&t0<0x504>);   // CEntityBehaviorTurnToAngle::Dtor
                stand_in(0x54d130, (void *)&s_pool_free);
                stand_in(0x5d0320, (void *)&c1<0x506>);   // operator delete
                e[side] = orel(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x42f7a0)(self, flags)));
                end(side);
                if (flags < 2 || n < 4)
                    for (DWORD i = flags < 2 ? 1 : n; i < 4; i++)   // the strings the destructor did not reach
                        at<DWORD(THISCALL *)(void *)>(0x4defd0)(self + i * 0xbc + 0xb8);
            }
            char d[60];
            wsprintfA(d, "[move to pos dtor flags %u n %u: %x / %x]", flags, n, e[0], e[1]);
            t.check(e[0] == e[1] && same_log(), "move to pos destructor", d);
            if (!same_log())
                log_first_difference();
        }
    // the block reader of 0x5f3240 (the made object freed to the pool afterwards)
    for (int k = 0; k < 4; k++) {
        DWORD e[2], made[2][8];
        for (int side = 0; side < 2; side++) {
            begin(side, 0x51);
            g_made = nullptr;
            stand_in(0x490f40, (void *)&t0<0x511>);   // CEntityMovable::CEntityMovable
            stand_in(0x4173c0, (void *)&t0<0x512>);   // Object_InitFields
            stand_in(0x491150, (void *)&f1<0x513>);   // EntityMovable_SetBounceFilter
            stand_in(0x4fd5c0, (void *)&s_set_last);
            stand_in(*at<DWORD *>(0x5f324c + 27 * 4), (void *)&t1<0x514>);
            e[side] = at<DWORD(THISCALL *)(void *, void *)>(0x49a840)(nullptr, g_obj[side]);
            end(side);
            BYTE *o = static_cast<BYTE *>(g_made);
            for (int i = 0; i < 6; i++)
                made[side][i] = o ? D(o, 0x114 + 4 * (DWORD)i) : 0;
            made[side][5] &= 0xff;
            made[side][6] = o ? D(o, 0) : 0;
            made[side][7] = o ? fnv_text(2166136261u, (*(ZStringBuffer **)(o + 0x124))->text) : 0;
            if (o) {
                at<DWORD(THISCALL *)(void *)>(0x4defd0)(o + 0x124);
                pool_free(o, 0x12c);
            }
        }
        t.check(e[0] == e[1] && memcmp(made[0], made[1], sizeof made[0]) == 0 && same_log(), "block reader 0x5f3240");
        if (!same_log())
            log_first_difference();
    }
    // the model preview's factory (operator new from the arena), the small destructors, callbacks
    for (int k = 0; k < 4; k++) {
        DWORD e[2][6];
        BYTE copy[2][0x20];
        for (int side = 0; side < 2; side++) {
            begin(side, 0x52);
            BYTE *o = g_obj[side];
            memset(o, 0x11, 0x40);
            stand_in(0x5179b0, (void *)&t2<0x521>);   // WindowModelPreview::Make
            stand_in(0x54d130, (void *)&f2<0x522>);   // the pool's free
            e[side][0] = rel(at<DWORD(__fastcall *)(DWORD, DWORD)>(0x518230)(0x61, 0x62), side);
            BYTE *h = static_cast<BYTE *>(heap_alloc(8));
            e[side][1] = orel(at<DWORD(THISCALL *)(void *, DWORD)>(0x476ce0)(k & 1 ? h : o, (DWORD)k & 1));
            e[side][2] = orel(at<DWORD(THISCALL *)(void *, DWORD)>(0x599530)(o + 8, (DWORD)k & 1));
            e[side][3] = at<DWORD(THISCALL *)(void *, DWORD)>(0x4bed80)(o + 0x10, 0x4bed + (DWORD)k);
            e[side][4] = at<DWORD(THISCALL *)(void *, DWORD)>(0x52dc80)(o, 3);
            const DWORD saved[2] = {*at<DWORD *>(0x62e960), *at<DWORD *>(0x62e990)};
            at<DWORD(__cdecl *)()>(0x420ed0)();
            at<DWORD(__cdecl *)()>(0x421860)();
            e[side][5] = *at<DWORD *>(0x62e960) ^ (*at<DWORD *>(0x62e990) << 1);
            *at<DWORD *>(0x62e960) = saved[0];
            *at<DWORD *>(0x62e990) = saved[1];
            memcpy(copy[side], o, 0x20);
            end(side);
        }
        t.check(memcmp(e[0], e[1], sizeof e[0]) == 0 && memcmp(copy[0], copy[1], 0x20) == 0 && same_log(), "factories, destructors, callbacks");
        if (!same_log())
            log_first_difference();
    }
    // the forwarders of 0x6020e0
    g_vt[3] = ptrv((void *)&s_rect);
    g_vt[37] = ptrv((void *)&t3<0x137>);
    g_vt[38] = ptrv((void *)&t3<0x138>);
    for (int k = 0; k < 4; k++) {
        DWORD e[2][3];
        for (int side = 0; side < 2; side++) {
            BYTE *o = g_obj[side];
            D(o, 0) = ptrv(g_vt);
            D(o, 0x28) = ptrv(o + 0x40);
            D(o, 0x40) = ptrv(g_vt);
            begin(side, 0);
            stand_in(0x4b3430, (void *)&s_draw_rect);
            e[side][0] = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x58f2d0)(o, 0x6020, (DWORD)k);
            e[side][1] = at<DWORD(THISCALL *)(void *, DWORD)>(0x58f300)(o, (DWORD)k);
            e[side][2] = at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x58f320)(o, (DWORD)k, 5);
            end(side);
        }
        t.check(memcmp(e[0], e[1], sizeof e[0]) == 0 && same_log(), "forwarders");
        if (!same_log())
            log_first_difference();
    }
}

}  // namespace

ZAX_CHECKS(c4_misc)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_c4_misc;
    if (!arena_setup()) {
        log("Check c4 misc: no arena");
        return;
    }
    Tally app{"c4 misc: the application's slots"};
    test_app(app);
    app.report();
    Tally dialog{"c4 misc: the dialogue action's slots"};
    test_dialog(dialog);
    dialog.report();
    Tally objects{"c4 misc: destructors, readers, factories, callbacks, forwarders"};
    test_objects(objects);
    objects.report();
}

}  // namespace zax
