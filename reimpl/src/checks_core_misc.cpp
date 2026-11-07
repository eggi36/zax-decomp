// Checks of the remaining object system functions (core_misc.cpp) against the original, in the game
// thread. Side 0 runs with all hooks of the module taken out, side 1 through the reimplementation.
//  - CClassInfo destructors on fake class infos (field descriptors and the extra list are fakes whose
//    slot 0 records its call; pool children arrays of 0-6 entries; some fields null): both deleting
//    destructors (freeing or not) and the base destructor: the recorded calls, the object afterwards, the
//    byte 0x6bdc90 (saved and restored).
//  - DisplayName for every registered class with ten suffixes, on an empty and on a filled string.
//  - The static registration stub with a class already registered (no change) and with a new fake class
//    (added at the end, then removed again through the registry's slot 20).
//  - The field descriptor setters on fake descriptors (texts, eax, the flag).
//  - The string kind's copy constructor on every registered string descriptor; the value pointer and set
//    from text on fresh CHueRemapHolder objects (dynamic values, descriptors without a setter); the int and
//    float kinds' get and set on real descriptors without a setter and on copies with a recording setter
//    (ints from the whole range, floats with zeros, NaNs (a signalling one comes back quiet), infinities,
//    denormals); the zeroed array for 0-40 elements.
//  - The class getters (ClassInfo_Register replaced by a stand-in; class and parent variables empty, the
//    parent set, the class set): eax and the registrations; the cast to CSphereOfZinData with null, its own
//    objects and objects of other classes.
//  - CItemTypeData's deleting destructor on objects from its Create/CreateArray (0-3 elements, freeing or
//    not), the trigger array's on fakes, CHueRemapHolder's constructor on patterned pool blocks, the
//    resource list's and the definition list base's deleting destructors and the unload of one entry on
//    fake lists (records with fake objects, a fake name tree), and 0x47e540 on strings sharing buffers.
#include "checks_core_common.h"
#include "core_misc.h"
#include "class_registry.h"

namespace zax {

int hooks_core_misc(Hook *out);

namespace {

using namespace core_checks;

Sides g_s;

// ---- Fakes with recording slots ----------------------------------------------------------------------------

struct Fake {
    DWORD vftable;
    DWORD id;
};
void *THISCALL fake_slot0(Fake *f, DWORD flags)
{
    g_s.ev(1, f->id, flags);
    return f;
}
void *THISCALL fake_slot1(Fake *f, DWORD flags)
{
    g_s.ev(2, f->id, flags);
    return f;
}
void *THISCALL fake_slot7(Fake *f, DWORD flags)
{
    g_s.ev(7, f->id, flags);
    return f;
}
DWORD g_fake_vt[8];
Fake g_fakes[64];
void init_fakes()
{
    for (DWORD &e : g_fake_vt)
        e = 0;
    g_fake_vt[0] = ptrv((void *)&fake_slot0);
    g_fake_vt[1] = ptrv((void *)&fake_slot1);
    g_fake_vt[7] = ptrv((void *)&fake_slot7);
    for (DWORD i = 0; i < 64; i++)
        g_fakes[i] = {ptrv(g_fake_vt), i + 1};
}

// ---- CClassInfo ------------------------------------------------------------------------------------------

void class_info_state(const CClassInfo *c)
{
    g_s.st(c->vftable);
    g_s.st(ptrv(c->fields));
    g_s.st(ptrv(c->extra));
    g_s.st(c->children.vftable);
    g_s.st(ptrv(c->children.data));
    g_s.st(c->children.count);
    g_s.st(c->children.capacity);
    g_s.st(c->num_fields);
}

void check_class_info_dtors(Tally &t)
{
    init_fakes();
    SavedBytes saved;
    save_bytes(saved, EXTRA_LIST_DELETING, 1);
    for (int k = 0; k < 120; k++) {
        const int which = k % 3;   // 0 deleting (0x48), 1 base deleting (0x40), 2 base destructor
        both_sides(g_s, hooks_core_misc, [&](int side) {
            (void)side;
            g_seed = 0x4152a0u + (unsigned)k * 977u;
            *at<BYTE *>(EXTRA_LIST_DELETING) = 0;
            const DWORD csize = which == 0 ? 0x48 : 0x40;
            auto *c = static_cast<CClassInfo *>(pool_alloc(csize));
            memset(c, 0x3c, csize);
            c->vftable = 0x5ec83c;
            const DWORD nf = (random() >> 8) % 6;
            c->num_fields = nf;
            c->fields = nf || (random() >> 8) % 2 ? static_cast<CFieldDesc **>(heap_alloc(nf * 4 + 4)) : nullptr;
            for (DWORD i = 0; i < nf; i++)
                c->fields[i] = (random() >> 8) % 4 ? reinterpret_cast<CFieldDesc *>(&g_fakes[i]) : nullptr;
            c->extra = (random() >> 8) % 2 ? reinterpret_cast<CExtraList *>(&g_fakes[40]) : nullptr;
            const DWORD cap = (random() >> 8) % 7;
            c->children = {0x5ec7c0, cap ? static_cast<void **>(pool_get_alloc(cap * 4)) : nullptr, cap ? (random() >> 8) % (cap + 1) : 0, cap};
            const DWORD flags = which == 2 ? 0 : (random() >> 8) % 2;
            DWORD r = 0;
            if (which == 0)
                r = ptrv(at<CClassInfo *(THISCALL *)(CClassInfo *, DWORD)>(0x415820)(c, flags)) - ptrv(c);
            else if (which == 1)
                r = ptrv(at<CClassInfo *(THISCALL *)(CClassInfo *, DWORD)>(0x415270)(c, flags)) - ptrv(c);
            else
                at<DWORD(THISCALL *)(CClassInfo *)>(0x4152a0)(c);
            g_s.st(r);
            g_s.st(*at<BYTE *>(EXTRA_LIST_DELETING));
            if (!flags) {
                class_info_state(c);
                pool_free(c, csize);
            }
        });
        char d[48];
        wsprintfA(d, "class info destructor %d (kind %d)", k, which);
        g_s.check(t, d);
    }
    restore_bytes(saved);
}

void check_display_names(Tally &t, CClassInfo **cls, int n)
{
    static const char *const suffixes[] = {nullptr, "", "Action", "AI", "Data", "action", "s", "Definition", "CAction", "Info"};
    for (int i = 0; i < n; i++) {
        if (!cls[i])
            continue;
        for (const char *suffix : suffixes)
            for (int filled = 0; filled < 2; filled++) {
                StringState s[2];
                for (int side = 0; side < 2; side++) {
                    ZString out;
                    if (filled)
                        out.CtorText("previous text of the string");
                    else
                        out.Ctor();
                    using Fn = DWORD(THISCALL *)(CClassInfo *, ZString *, const char *);
                    (side ? at<Fn>(0x4153b0) : original<Fn>(0x4153b0))(cls[i], &out, suffix);
                    s[side] = state(out, nullptr);
                    out.Dtor();
                }
                char d[140];
                wsprintfA(d, "[DisplayName %.40s '%.12s': '%.30s' '%.30s']", cls[i]->name, suffix ? suffix : "(null)", s[0].text, s[1].text);
                t.check(memcmp(&s[0], &s[1], sizeof s[0]) == 0, "", d);
            }
    }
}

CClassInfo *g_getter_result;
CClassInfo *__cdecl fake_getter() { return g_getter_result; }

void check_static_register(Tally &t, CClassInfo **cls, int n)
{
    CClassRegistry *reg = at<CClassRegistry *(__cdecl *)()>(0x415d20)();
    alignas(4) static BYTE fake[2][sizeof(CClassInfo)];
    for (int k = 0; k < 40; k++) {
        DWORD r[2], num[2], last[2];
        for (int side = 0; side < 2; side++) {
            const bool fresh = k % 2 == 1;
            CClassInfo *c = cls[(k * 37) % n];
            if (fresh) {
                memcpy(fake[side], c, sizeof(CClassInfo));
                auto *f = reinterpret_cast<CClassInfo *>(fake[side]);
                f->name = k % 4 == 1 ? "ZzReimplCheckClass" : "zzreimplcheckclass2";
                f->alt_name = nullptr;
                f->module = 0x77;
                c = f;
            }
            g_getter_result = c;
            const DWORD before = reg->num;
            const BYTE sorted = reg->sorted;
            BYTE self[4];
            using Fn = void *(THISCALL *)(void *, CClassInfo *(__cdecl *)());
            r[side] = ptrv((side ? at<Fn>(0x417690) : original<Fn>(0x417690))(self, &fake_getter)) - ptrv(self);
            num[side] = reg->num - before;
            last[side] = reg->num && reg->entries[reg->num - 1] == c ? 1 + c->module : 0;
            if (fresh && reg->num > before) {
                vcall<void>(reg, 20, reg->num - 1);
                reg->sorted = sorted;   // the same entries in the same order again
            }
        }
        char d[96];
        wsprintfA(d, "[static register %d: %x %x, added %u %u, last %x %x]", k, r[0], r[1], num[0], num[1], last[0], last[1]);
        t.check(r[0] == r[1] && num[0] == num[1] && last[0] == last[1], "", d);
    }
}

void check_desc_setters(Tally &t)
{
    static const char *const texts[] = {"", "Seconds", "Help text of a field, rather long so the string grows", "Old Name", "x"};
    for (int k = 0; k < 60; k++) {
        unsigned h[2];
        for (int side = 0; side < 2; side++) {
            g_seed = 0x419020u + (unsigned)k * 13u;
            CFieldDesc d;
            memset(&d, 0, sizeof d);
            d.help.Ctor();
            d.alt_name.Ctor();
            d.units.Ctor();
            h[side] = 2166136261u;
            for (int i = 0; i < 4; i++) {
                const DWORD op = (random() >> 8) % 4;
                const char *s = texts[(random() >> 8) % 5];
                using Fn = ZString *(THISCALL *)(CFieldDesc *, const char *);
                const DWORD a = op == 0 ? 0x419020 : op == 1 ? 0x419030 : op == 2 ? 0x419060 : 0x419040;
                DWORD r = 0;
                if (op < 3)
                    r = ptrv((side ? at<Fn>(a) : original<Fn>(a))(&d, s)) - ptrv(&d);
                else
                    (side ? at<void(THISCALL *)(CFieldDesc *)>(a) : original<void(THISCALL *)(CFieldDesc *)>(a))(&d);
                const StringState s1 = state(d.help, nullptr), s2 = state(d.alt_name, nullptr), s3 = state(d.units, nullptr);
                h[side] = fnv(fnv(fnv(fnv(fnv(h[side], &r, 4), &s1, sizeof s1), &s2, sizeof s2), &s3, sizeof s3), &d.inherited, 1);
            }
            d.help.Dtor();
            d.alt_name.Dtor();
            d.units.Dtor();
        }
        t.check(h[0] == h[1], "", "[field descriptor setters]");
    }
}

// ---- Field values ------------------------------------------------------------------------------------------

void *new_object() { return at<void *(__fastcall *)(int, int)>(0x4d4510)(0, 0); }   // CHueRemapHolder::Create
void delete_object(void *o) { vcall<void *>(o, 1, 1u); }

DWORD g_setter_calls, g_setter_value;
DWORD THISCALL recording_int_setter(void *, DWORD v)
{
    g_setter_calls++;
    g_setter_value = v;
    return 0x5e7;
}
float THISCALL recording_float_setter(void *, DWORD v)
{
    g_setter_calls++;
    g_setter_value = v;
    return 1.5f;
}

void check_string_values(Tally &t, CClassInfo **cls, int n)
{
    int copies = 0, values = 0;
    for (int i = 0; i < n; i++) {
        if (!cls[i])
            continue;
        for (DWORD f = cls[i]->num_inherited; f < cls[i]->num_fields; f++) {
            auto *d = reinterpret_cast<CStringFieldDesc *>(cls[i]->fields[f]);
            if (!d || d->vftable != 0x5ec91c)
                continue;
            // copy constructor
            {
                unsigned h[2];
                for (int side = 0; side < 2; side++) {
                    const DWORD refs = d->default_value.buffer->refs;
                    auto *c = static_cast<CStringFieldDesc *>(heap_alloc(sizeof(CStringFieldDesc)));
                    memset(c, 0x77, sizeof *c);
                    using Fn = CStringFieldDesc *(THISCALL *)(CStringFieldDesc *, const CStringFieldDesc *);
                    const DWORD r = ptrv((side ? at<Fn>(0x415ef0) : original<Fn>(0x415ef0))(c, d)) - ptrv(c);
                    const DWORD refs_after = d->default_value.buffer->refs - refs;
                    h[side] = fnv(fnv(2166136261u, &r, 4), &refs_after, 4);
                    h[side] = fnv(fnv(fnv(h[side], &c->vftable, 4), &c->offset, 12), &c->f28, 1);
                    h[side] = fnv(fnv(h[side], &c->setter, 4), &c->inherited, 1);
                    h[side] = fnv_text(fnv_text(fnv_text(h[side], c->name.CStr()), c->help.CStr()), c->default_value.CStr());
                    h[side] = fnv_text(fnv_text(fnv_text(h[side], c->group.CStr()), c->alt_name.CStr()), c->units.CStr());
                    vcall<void *>(c, 0, 1u);
                }
                char dd[96];
                wsprintfA(dd, "[string copy %.30s.%.30s]", cls[i]->name, d->name.CStr());
                t.check(h[0] == h[1], "", dd);
                copies++;
            }
            if (d->setter || values >= 40)
                continue;
            // value pointer and set from text on fresh objects. A dynamic value is a block of the value's size
            // (+0x14) addressed with the descriptor's offset, so the test uses a raw copy with offset 0.
            alignas(4) static BYTE clone[sizeof(CStringFieldDesc)];
            memcpy(clone, d, sizeof clone);
            CStringFieldDesc *const real = d;
            d = reinterpret_cast<CStringFieldDesc *>(clone);
            d->offset = 0;
            static const char *const texts[] = {"", "Text", "A longer text for the dynamic value"};
            for (const char *s : texts) {
                unsigned h[2];
                for (int side = 0; side < 2; side++) {
                    void *o = new_object();
                    using ValueFn = ZString *(THISCALL *)(CStringFieldDesc *, void *);
                    using SetFn = DWORD(THISCALL *)(CStringFieldDesc *, void *, const char *);
                    ValueFn value = side ? at<ValueFn>(0x415f50) : original<ValueFn>(0x415f50);
                    const int was_default = value(d, o) == &d->default_value;
                    h[side] = fnv(2166136261u, &was_default, 4);
                    (side ? at<SetFn>(0x415f80) : original<SetFn>(0x415f80))(d, o, s);
                    ZString *v = value(d, o);
                    const int is_default = v == &d->default_value;
                    h[side] = fnv_text(fnv(h[side], &is_default, 4), v->CStr());
                    delete_object(o);
                }
                char dd[96];
                wsprintfA(dd, "[string value %.30s.%.30s '%.10s']", cls[i]->name, real->name.CStr(), s);
                t.check(h[0] == h[1], "", dd);
            }
            values++;
        }
    }
    log("  string descriptors: %d copied, %d with values", copies, values);
}

template <bool FLOAT> void number_case(Tally &t, CFieldDesc *real, const char *cname, bool fake_setter)
{
    // a raw copy with offset 0 (see check_string_values)
    alignas(4) static BYTE clone_bytes[0x44];
    memcpy(clone_bytes, real, FLOAT ? sizeof(CFloatFieldDesc) : sizeof(CIntFieldDesc));
    CIntFieldDesc *d = reinterpret_cast<CIntFieldDesc *>(clone_bytes);
    d->offset = 0;
    if (fake_setter)
        d->setter = ptrv(FLOAT ? (void *)&recording_float_setter : (void *)&recording_int_setter);
    static const DWORD values[] = {0, 1, 0xffffffff, 0x80000000, 0x7fffffff, 0x7f800000, 0xff800000, 0x7fc00000, 0x7fa00000,
                                   0xffa00001, 0x00000001, 0x807fffff, 0x3f800000, 0x42c80000};
    for (DWORD v : values) {
        unsigned h[2];
        for (int side = 0; side < 2; side++) {
            g_setter_calls = 0;
            g_setter_value = 0;
            void *o = new_object();
            const DWORD get_a = FLOAT ? 0x54ede0 : 0x54d630, set_a = FLOAT ? 0x54ee10 : 0x54d660;
            DWORD before, after;
            if (FLOAT) {
                using GetFn = float(THISCALL *)(CIntFieldDesc *, void *);
                GetFn get = side ? at<GetFn>(get_a) : original<GetFn>(get_a);
                before = bits(get(d, o));
                using SetFn = DWORD(THISCALL *)(CIntFieldDesc *, void *, DWORD);
                (side ? at<SetFn>(set_a) : original<SetFn>(set_a))(d, o, v);   // eax: the setter's (a float function's)
                after = bits(get(d, o));
                h[side] = 1;
            } else {
                using GetFn = DWORD(THISCALL *)(CIntFieldDesc *, void *);
                GetFn get = side ? at<GetFn>(get_a) : original<GetFn>(get_a);
                before = get(d, o);
                using SetFn = DWORD(THISCALL *)(CIntFieldDesc *, void *, DWORD);
                const DWORD r = (side ? at<SetFn>(set_a) : original<SetFn>(set_a))(d, o, v);
                after = get(d, o);
                h[side] = fake_setter ? r : 1;
            }
            h[side] = fnv(fnv(fnv(fnv(h[side], &before, 4), &after, 4), &g_setter_calls, 4), &g_setter_value, 4);
            delete_object(o);
        }
        char dd[120];
        wsprintfA(dd, "[%s %.30s.%.30s setter %d value %x]", FLOAT ? "float" : "int", cname, real->name.CStr(), fake_setter, v);
        t.check(h[0] == h[1], "", dd);
    }
}

void check_number_values(Tally &t, CClassInfo **cls, int n)
{
    int ints = 0, floats = 0;
    for (int i = 0; i < n && (ints < 12 || floats < 12); i++) {
        if (!cls[i])
            continue;
        for (DWORD f = cls[i]->num_inherited; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (!d)
                continue;
            const bool is_int = d->vftable == 0x5fd440 || d->vftable == 0x5fd66c, is_float = d->vftable == 0x5fdb68;
            if (is_int && ints < 12 && !reinterpret_cast<CIntFieldDesc *>(d)->setter) {
                number_case<false>(t, d, cls[i]->name, false);
                number_case<false>(t, d, cls[i]->name, true);
                ints++;
            } else if (is_float && floats < 12 && !reinterpret_cast<CFloatFieldDesc *>(d)->setter) {
                number_case<true>(t, d, cls[i]->name, false);
                number_case<true>(t, d, cls[i]->name, true);
                floats++;
            }
        }
    }
    log("  number descriptors: %d int, %d float", ints, floats);
    for (DWORD k = 0; k <= 40; k++) {
        bool same = true;
        DWORD *p[2];
        for (int side = 0; side < 2; side++) {
            using Fn = DWORD *(THISCALL *)(void *, DWORD, DWORD, DWORD);
            p[side] = (side ? at<Fn>(0x54d440) : original<Fn>(0x54d440))(nullptr, k, 7, 9);
            for (DWORD i = 0; i < k; i++)
                same = same && p[side][i] == 0;
        }
        same = same && (p[0] != nullptr) == (p[1] != nullptr);
        heap_free(p[0]);
        heap_free(p[1]);
        t.check(same, "", "[zeroed array]");
    }
}

// ---- Class getters ---------------------------------------------------------------------------------------

DWORD g_blocks[8];
int g_nblocks;
DWORD block_id(DWORD p)
{
    for (int i = 0; i < g_nblocks; i++)
        if (g_blocks[i] == p)
            return 0xb000 + (DWORD)i;
    return p;
}
CClassInfo *THISCALL fake_register(CClassInfo *self, CClassInfo **var, const char *name, float version, CClassInfo *parent,
                                   DWORD size, DWORD abstract, void *create, void *create_array, void *register_fields)
{
    if (g_nblocks < 8)
        g_blocks[g_nblocks++] = ptrv(self);
    g_s.ev(0x415780, block_id(ptrv(self)), ptrv(var), ptrv(name));
    g_s.ev(bits(version), block_id(ptrv(parent)), size, abstract & 0xff);
    g_s.ev(ptrv(create), ptrv(create_array), ptrv(register_fields));
    memset(self, 0, sizeof *self);
    self->name = name;
    *var = self;
    return self;
}

struct GetterCase {
    DWORD address, var, parent_var;
};
const GetterCase GETTERS[] = {{0x4242b0, 0x62e9fc, 0},        {0x51bf50, 0x6d001c, 0},        {0x5557f0, 0x712338, 0x712324},
                              {0x555890, 0x712334, 0x712320}, {0x557470, 0x712348, 0x712324}, {0x557510, 0x712344, 0x712320},
                              {0x557bd0, 0x712354, 0x712324}, {0x5588d0, 0x712350, 0x712320}, {0x5597d0, 0x712368, 0x712324},
                              {0x55aa30, 0x712364, 0x712320}};

void check_getters(Tally &t)
{
    for (const GetterCase &c : GETTERS)
        for (int variant = 0; variant < 3; variant++) {   // 0 both empty, 1 parent set, 2 class set
            if (variant == 1 && !c.parent_var)
                continue;
            CClassInfo **var = at<CClassInfo **>(c.var), **pvar = c.parent_var ? at<CClassInfo **>(c.parent_var) : nullptr;
            CClassInfo *const saved = *var, *const psaved = pvar ? *pvar : nullptr;
            both_sides(g_s, hooks_core_misc, [&](int side) {
                (void)side;
                g_nblocks = 0;
                *var = variant == 2 ? saved : nullptr;
                if (pvar)
                    *pvar = variant == 1 ? psaved : nullptr;
                CodePatch p;
                patch_jump(p, 0x415780, (void *)&fake_register);
                const DWORD r = at<DWORD(__cdecl *)()>(c.address)();
                unpatch(p);
                g_s.st(block_id(r));
                g_s.st(block_id(ptrv(*var)));
                for (int i = 0; i < g_nblocks; i++)
                    pool_free(at<void *>(g_blocks[i]), sizeof(CClassInfo));
                *var = saved;
                if (pvar)
                    *pvar = psaved;
            });
            char d[64];
            wsprintfA(d, "getter 0x%x variant %d", c.address, variant);
            g_s.check(t, d);
        }
    // the cast to CSphereOfZinData
    void *objects[4] = {nullptr, nullptr, nullptr, nullptr};
    objects[1] = at<void *(__fastcall *)(int, int)>(0x584210)(0, 0);   // CSphereOfZinData::Create
    objects[2] = new_object();                                         // a CHueRemapHolder
    objects[3] = at<void *(__fastcall *)(int, int)>(0x582570)(0, 0);   // CSphereOfZinOptions::Create
    for (void *o : objects) {
        using Fn = void *(__fastcall *)(void *);
        const DWORD a = ptrv(original<Fn>(0x585380)(o)), b = ptrv(at<Fn>(0x585380)(o));
        char d[64];
        wsprintfA(d, "[SphereOfZinData cast %x: %x %x]", ptrv(o), a, b);
        t.check(a == b, "", d);
    }
    // the cast with the class variable empty: its getter registers on the way
    CClassInfo **var = at<CClassInfo **>(0x713818);
    CClassInfo *const saved = *var;
    both_sides(g_s, hooks_core_misc, [&](int side) {
        (void)side;
        g_nblocks = 0;
        *var = nullptr;
        CodePatch p;
        patch_jump(p, 0x415780, (void *)&fake_register);
        g_s.st(ptrv(at<void *(__fastcall *)(void *)>(0x585380)(objects[2])));
        g_s.st(ptrv(at<void *(__fastcall *)(void *)>(0x585380)(nullptr)));
        unpatch(p);
        for (int i = 0; i < g_nblocks; i++)
            pool_free(at<void *>(g_blocks[i]), sizeof(CClassInfo));
        *var = saved;
    });
    g_s.check(t, "SphereOfZinData cast, class variable empty");
    for (void *o : objects)
        if (o)
            delete_object(o);
}

// ---- Destructors and the constructor ------------------------------------------------------------------------

void check_item_type_data(Tally &t)
{
    for (int k = 0; k < 40; k++) {
        both_sides(g_s, hooks_core_misc, [&](int side) {
            (void)side;
            g_seed = 0x424230u + (unsigned)k;
            const bool array = k % 2;
            const DWORD flags = (array ? 2 : 0) | ((random() >> 8) % 2);
            BYTE *o;
            DWORD *block = nullptr;
            int count = 0;
            if (array) {
                count = (int)((random() >> 8) % 4);
                o = static_cast<BYTE *>(at<void *(__fastcall *)(int, int, int)>(0x4241d0)(count, 0, 0));
                if (!o)
                    return;
                block = reinterpret_cast<DWORD *>(o) - 1;
            } else {
                o = static_cast<BYTE *>(at<void *(__fastcall *)(int, int)>(0x424190)(0, 0));
            }
            const DWORD r = ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x424230)(o, flags));
            g_s.st(r == ptrv(o) ? 1 : r == ptrv(block) ? 2 : 3);
            if (!(flags & 1)) {
                for (int i = 0; i < (array ? count : 1); i++) {
                    g_s.st(*reinterpret_cast<DWORD *>(o + i * 0x18));
                    g_s.st(*reinterpret_cast<DWORD *>(o + i * 0x18 + 4));
                }
                if (array)
                    heap_free(block);
                else
                    pool_free(o, 0x18);
            }
        });
        char d[48];
        wsprintfA(d, "CItemTypeData destructor %d", k);
        g_s.check(t, d);
    }
}

void check_small_dtors(Tally &t)
{
    init_fakes();
    // the trigger array
    for (int k = 0; k < 12; k++) {
        both_sides(g_s, hooks_core_misc, [&](int side) {
            (void)side;
            DWORD *a = static_cast<DWORD *>(heap_alloc(16));
            a[0] = 0x601374;
            a[1] = k % 3 ? ptrv(&g_fakes[k]) : 0;
            a[2] = (DWORD)k;
            a[3] = (DWORD)k * 2;
            const DWORD flags = k % 2;
            g_s.st(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x5853f0)(a, flags)) == ptrv(a));
            if (!flags) {
                g_s.st_bytes(a, 16);
                heap_free(a);
            }
        });
        g_s.check(t, "trigger array destructor");
    }
    // CHueRemapHolder's constructor
    for (int k = 0; k < 8; k++) {
        both_sides(g_s, hooks_core_misc, [&](int side) {
            (void)side;
            BYTE *o = static_cast<BYTE *>(pool_alloc(0x20));
            memset(o, 0x11 * (k + 1), 0x20);
            g_s.st(ptrv(at<void *(THISCALL *)(void *)>(0x4d4cc0)(o)) == ptrv(o));
            g_s.st_bytes(o, 0x20);
            delete_object(o);
        });
        g_s.check(t, "CHueRemapHolder constructor");
    }
    // the resource list and the definition list base
    for (int k = 0; k < 60; k++) {
        const int which = k % 3;   // 0 resource list, 1 definition list base destructor, 2 unload
        both_sides(g_s, hooks_core_misc, [&](int side) {
            (void)side;
            g_seed = 0x41e830u + (unsigned)k * 101u;
            const DWORD size = which == 0 ? 0x30 : 0x140;
            BYTE *l = static_cast<BYTE *>(pool_alloc(size));
            memset(l, 0, size);
            at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, 0, 0x80, 0, nullptr, 0);
            auto dw = [l](DWORD off) -> DWORD & { return *reinterpret_cast<DWORD *>(l + off); };
            dw(0x1c) = 0x5ed558;   // empty aliases
            const DWORD nrec = which ? (random() >> 8) % 5 : 0;
            if (which) {
                if (nrec || (random() >> 8) % 2) {
                    DWORD *block = static_cast<DWORD *>(heap_alloc(nrec * 0x80 + 4));
                    block[0] = nrec;
                    memset(block + 1, 0x5c, nrec * 0x80);
                    for (DWORD i = 0; i < nrec; i++)
                        *reinterpret_cast<DWORD *>(reinterpret_cast<BYTE *>(block + 1) + i * 0x80 + 0x78) =
                            (random() >> 8) % 3 ? ptrv(&g_fakes[i]) : 0;
                    dw(0x30) = ptrv(block + 1);
                }
                dw(0x34) = nrec;
                dw(0x13c) = (random() >> 8) % 2 ? ptrv(&g_fakes[50]) : 0;
            }
            const DWORD flags = which == 2 ? 0 : (random() >> 8) % 2;
            if (which == 0) {
                g_s.st(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x41e830)(l, flags)) == ptrv(l));
            } else if (which == 1) {
                g_s.st(ptrv(at<void *(THISCALL *)(void *, DWORD)>(0x41e940)(l, flags)) == ptrv(l));
            } else {
                const DWORD i = nrec ? (random() >> 8) % nrec : 0;
                if (nrec || !dw(0x30)) {
                    const DWORD r = at<DWORD(THISCALL *)(void *, DWORD)>(0x41ec30)(l, i);
                    g_s.st(r == dw(0x30));
                    if (dw(0x30))
                        g_s.st_bytes(at<BYTE *>(dw(0x30)), nrec * 0x80);
                }
                at<void *(THISCALL *)(void *, DWORD)>(0x41e940)(l, 0);   // clean up through the reimplementation's twin
            }
            if (!flags) {
                g_s.st(dw(0));
                g_s.st(dw(0x30));
                g_s.st(dw(0x34));
                g_s.st(dw(0x13c));
                g_s.st(dw(0x1c));
                pool_free(l, size);
            }
        });
        char d[48];
        wsprintfA(d, "list destructor %d (kind %d)", k, which);
        g_s.check(t, d);
    }
    // 0x47e540
    for (int k = 0; k < 8; k++) {
        unsigned h[2];
        for (int side = 0; side < 2; side++) {
            ZString keep;
            keep.CtorText("shared text");
            ZString three[4];
            three[0].CopyCtor(&keep);
            three[1].CtorText("untouched");
            three[2].CtorText(k % 2 ? "" : "own text");
            three[3].CopyCtor(k % 3 ? &keep : &three[2]);
            using Fn = DWORD(THISCALL *)(ZString *);
            (side ? at<Fn>(0x47e540) : original<Fn>(0x47e540))(three);
            const StringState s = state(keep, nullptr), u = state(three[1], nullptr);
            h[side] = fnv(fnv(2166136261u, &s, sizeof s), &u, sizeof u);
            three[1].Dtor();
            keep.Dtor();
        }
        t.check(h[0] == h[1], "", "[three strings destroyed]");
    }
}

}  // namespace

ZAX_CHECKS(core_misc)
{
    {
        Tally t{"core misc: CClassInfo destructors, display name, registration stub, descriptor setters"};
        check_class_info_dtors(t);
        check_display_names(t, cls, n);
        check_static_register(t, cls, n);
        check_desc_setters(t);
        t.report();
    }
    {
        Tally t{"core misc: field values"};
        check_string_values(t, cls, n);
        check_number_values(t, cls, n);
        t.report();
    }
    {
        Tally t{"core misc: class getters, SphereOfZinData cast"};
        check_getters(t);
        t.report();
    }
    {
        Tally t{"core misc: destructors, CHueRemapHolder constructor"};
        check_item_type_data(t);
        check_small_dtors(t);
        t.report();
    }
}

}  // namespace zax
