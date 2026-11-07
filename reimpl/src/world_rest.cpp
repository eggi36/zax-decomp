// The rest of the world class of Zax.exe and the small World functions around it reimplemented (see
// world_rest.h).
#include "world_rest.h"
#include "definitions.h"
#include "file_search.h"
#include "polygon.h"
#include "x87.h"

namespace zax {

namespace {

using LD = long double;

// ---- Functions and constants of the exe; through their exe addresses so Disable= applies -------------

void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_ctor_text(void *z, const char *s) { fn<void *(THISCALL *)(void *, const char *)>(0x4dee00)(z, s); }
void zs_dtor(void *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
const char *zs_cstr(void *z) { return fn<const char *(THISCALL *)(void *)>(0x4e13a0)(z); }
void zs_assign_text(void *z, const char *s) { fn<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }
void zs_append(void *z, const char *s) { fn<void *(THISCALL *)(void *, const char *)>(0x4e1be0)(z, s); }
char *zs_get_buffer(void *z, DWORD n) { return fn<char *(THISCALL *)(void *, DWORD)>(0x4e1590)(z, n); }
void zs_release_buffer(void *z, int n) { fn<void(THISCALL *)(void *, int)>(0x4e16d0)(z, n); }
void zs_set_path(void *z, const char *dir, const char *name) { fn<void(THISCALL *)(void *, const char *, const char *)>(0x4dfa50)(z, dir, name); }
void zs_format2(void *z, GlobKey fmt, DWORD a, DWORD b) { fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, glob<const char *>(fmt), a, b); }
void zs_format1(void *z, GlobKey fmt, DWORD a) { fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, glob<const char *>(fmt), a); }

void section_begin(GlobKey text) { fn<void(__fastcall *)(const char *)>(0x4a2650)(glob<const char *>(text)); }
void section_end() { fn<void(__cdecl *)()>(0x4a2670)(); }
int crt_stricmp_(const char *a, const char *b) { return fn<int(__cdecl *)(const char *, const char *)>(0x5d3640)(a, b); }
int crt_atexit_raw(DWORD f) { return fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(f)); }
void crt_qsort(void *base, DWORD n, DWORD size, DWORD cmp) { fn<void(__cdecl *)(void *, DWORD, DWORD, DWORD)>(0x5d097c)(base, n, size, fn_addr_dyn(cmp)); }
void path_join(char *out, const char *dir, const char *name) { fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
char *path_name_(char *path) { return fn<char *(__fastcall *)(char *)>(0x4a4570)(path); }
void path_set_extension(char *path, const char *ext) { fn<DWORD(__fastcall *)(char *, const char *)>(0x4a3fb0)(path, ext); }
void text_cut_prefix(char *text, const char *prefix) { fn<DWORD(__fastcall *)(char *, const char *)>(0x587a00)(text, prefix); }

BYTE is_kind(void *o, CClassInfo *c) { return (BYTE)fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
CClassInfo *class_of(FnKey getter) { return fn<CClassInfo *(__cdecl *)()>(getter)(); }
constexpr DWORD CI_ENTITY_ANIMATED = 0x48dcc0, CI_SEQUENCE = 0x56ab50, CI_FREE_RANGE_POLY = 0x4b5c60,
                CI_WAY_POINTS_POLYGON = 0x598d90, CI_ENTITY = 0x487620, CI_49B810 = 0x49b810, CI_UPPER_BODY = 0x494180,
                CI_LAYER_PART = 0x4f9bf0, CI_DOOR_AI = 0x481710, CI_48BDF0 = 0x48bdf0, CI_4903F0 = 0x4903f0,
                CI_49A930 = 0x49a930;


constexpr DWORD ERROR_REPORTER = 0x6c02cc;

const char *part_name(void *part) { return fn<const char *(THISCALL *)(void *)>(0x4fbf20)(part); }
void *object_clone(void *o) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
void *entity_model(void *e) { return fn<void *(THISCALL *)(void *)>(0x48b290)(e); }
void delete_part(Layer *l, void *part) { fn<void(THISCALL *)(Layer *, void *)>(0x4ebd60)(l, part); }
void *create_from_model(DWORD model, CClassInfo *c, DWORD creator)
{
    return fn<void *(THISCALL *)(void *, DWORD, CClassInfo *, DWORD, DWORD, DWORD)>(0x5176f0)(glob<void *>(MODEL_LIST), model, c, creator,
                                                                                            0, 0xffffffff);
}

// Floats are passed by their bits (a float argument could go through an x87 load that quiets NaNs)
DWORD fbits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}
template <typename T> T &field(void *p, DWORD offset) { return *reinterpret_cast<T *>(static_cast<BYTE *>(p) + offset); }

// The player records of the net layer (slot 44: its slot 23 is player i, slot 11 the count)
void *players(World *w) { return vcall<void *>(w, 44); }
BYTE *player(World *w, DWORD i) { return vcall<BYTE *>(players(w), 23, i); }
DWORD player_count(World *w) { return vcall<DWORD>(players(w), 11); }
// The game type's player i (slot 1 of the world is the game type; NetGame_PlayerAt 0x51f440)
void *net_player(World *w, DWORD i) { return fn<void *(THISCALL *)(void *, DWORD)>(0x51f440)(vcall<void *>(w, 1), i); }

// A layer list index as the inlined accessor reads it: -1, past the count or negative give null
Layer *layer_at(World *w, int i)
{
    if (i == -1 || i >= (int)w->layers.count || i < 0)
        return nullptr;
    return static_cast<Layer *>(w->layers.data[i]);
}
Layer *player_layer(World *w, DWORD i) { return layer_at(w, field<int>(player(w, i), 0xdc)); }

void set_view_target(World *w, DWORD x, DWORD y, int mode, DWORD i)
{
    fn<void(THISCALL *)(World *, DWORD, DWORD, int, DWORD)>(0x4f5dd0)(w, x, y, mode, i);
}
void set_zoom(World *w, DWORD zoom, DWORD i) { fn<void(THISCALL *)(World *, DWORD, DWORD)>(0x4f6770)(w, zoom, i); }
void frame_resize(void *frame, int w, int h) { fn<void(THISCALL *)(void *, int, int)>(0x4aa550)(frame, w, h); }
void rebuild_frames(World *w) { fn<void(THISCALL *)(World *)>(0x4f2800)(w); }

// The zoom frame gets the view rectangle's size times the zoom (fild of the unsigned difference, __ftol)
DWORD view_width(const World *w) { return (DWORD)w->view[2] - (DWORD)w->view[0]; }
DWORD view_height(const World *w) { return (DWORD)w->view[3] - (DWORD)w->view[1]; }
void resize_zoom_frame(World *w, float zoom)
{
    const int h = x87_ftol(fild64((long long)view_height(w)) * fld(zoom));
    const int wd = x87_ftol(fild64((long long)view_width(w)) * fld(zoom));
    frame_resize(w->zoom_frame, wd, h);
}

// A definition list entry as the callers inline LoadEntry: the object of entry i (index 0 when i is out
// of range), loaded through slot 37 unless it is loaded or being loaded; null without records or while
// the entry is being loaded.
void *list_entry(ZDefinitionList *l, DWORD i)
{
    if (!l->entries)
        return nullptr;
    const DWORD j = i < l->num_entries ? i : 0;
    ZDefinitionRecord *r = &l->entries[j];
    if (r->loading)
        return nullptr;
    if (!r->object && !r->loading) {
        r->loading = 1;
        char path[0x100];
        path_join(path, l->folder, vcall<const char *>(l, 1, j));
        char *p = path_name_(path);
        void *o;
        if (p == path) {
            o = vcall<void *>(l, 37, glob<const char *>(EMPTY_TEXT), p, &l->entries[j].object);
        } else {
            p[-1] = 0;
            o = vcall<void *>(l, 37, path, p, &l->entries[j].object);
        }
        l->entries[j].object = o;
        l->entries[j].loading = 0;
    }
    return l->entries[j].object;
}

// Every entry of a definition list loaded (0x4f10f0's loop)
void preload_list(ZDefinitionList *l)
{
    for (DWORD i = 0; i < l->num_entries; i++)
        if (l->entries)
            list_entry(l, i);
}

// A function-local static ZString of the exe: constructed on first use (guard bit 0 set first), its
// destructor registered with atexit
void *static_text(GlobKey z, GlobKey guard, GlobKey text, DWORD at_exit)
{
    BYTE &g = *glob<BYTE *>(guard);
    if (!(g & 1)) {
        g |= 1;
        zs_ctor_text(glob<void *>(z), glob<const char *>(text));
        crt_atexit_raw(at_exit);
    }
    return glob<void *>(z);
}

// The error message of the explosion spawns for a model whose entity is no CEntityAnimated; the entity is
// deleted and replaced by a new CEntityAnimated (0x48e920) of the model
void *replace_wrong_entity(void *e, DWORD model, DWORD creator, bool set_creator)
{
    const char *name = fn<const char *(THISCALL *)(void *, DWORD)>(0x591d00)(glob<void *>(MODEL_LIST), model);
    ZString s;
    zs_ctor(&s);
    CClassInfo *c = vcall<CClassInfo *>(e, 2);
    zs_format2(&s, 0x6187e8, (DWORD)(uintptr_t)name, *reinterpret_cast<DWORD *>(reinterpret_cast<BYTE *>(c) + 0xc));
    void *rep = *glob<void **>(ERROR_REPORTER);
    vcall<DWORD>(rep, 17, zs_cstr(&s));
    vcall<void *>(e, 1, 1);
    void *p = pool_alloc(0xe4);
    void *n = p ? fn<void *(THISCALL *)(void *)>(0x48e920)(p) : nullptr;
    if (set_creator)
        vcall<DWORD>(n, 69, creator);
    vcall<DWORD>(n, 93, model);
    zs_dtor(&s);
    return n;
}

// The flags and the placement shared by both explosion spawns
void place_explosion(void *e, DWORD x, DWORD y, DWORD a5, DWORD a7, bool both_hidden_flags)
{
    vcall<DWORD>(e, 41, x, y);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x48b6b0)(e, a5);
    if (both_hidden_flags) {
        vcall<DWORD>(e, 58, 0x20000000);
        vcall<DWORD>(e, 58, 0x40000000);
    }
    vcall<DWORD>(e, 58, 0x20000);
    vcall<DWORD>(e, 59, 0x200000);
    vcall<DWORD>(e, 59, 0x40000);
    vcall<DWORD>(e, 104, a7, 1);
}

// The client's "client ready" block (the reader 0x4f7910 written with player 0's packet)
void send_client_ready(void *net)
{
    void *rec = vcall<void *>(net, 23, 0);
    void *packet = vcall<void *>(net, 12, 0);
    void *reader = fn<void *(__cdecl *)()>(0x4f7910)();
    fn<DWORD(THISCALL *)(void *, void *, void *)>(0x4f7950)(reader, packet, rec);
}

void read_all_blocks(void *packet)
{
    while ((BYTE)fn<DWORD(THISCALL *)(void *)>(0x520f50)(packet))
        fn<DWORD(THISCALL *)(void *)>(0x5200d0)(packet);
}

// One player's view during the multiplayer frame: the view target (copied through the FPU, y by its
// bits) shaken by the layer, the view moved there for the frame
struct ShakenView {
    float pos[2];
    DWORD saved[2];
    BYTE shaken;
};
void shake_view(World *w, Layer *l, DWORD i, ShakenView &v)
{
    BYTE *rec = player(w, i);
    const float x = fstp(fld(field<float>(rec, 0xd0)));
    const DWORD y = field<DWORD>(rec, 0xd4);
    v.pos[0] = x;
    memcpy(&v.pos[1], &y, 4);
    v.saved[0] = fbits(x);
    v.saved[1] = y;
    v.shaken = (BYTE)fn<DWORD(THISCALL *)(Layer *, float *)>(0x4e9f70)(l, v.pos);
}

}  // namespace

// ---- The world class --------------------------------------------------------------------------------

// 0x4f0230 (slot 56): the server runs every player's view (shaken for the frame), the packets and the
// game type; a client reads its packets and draws the frame state
void THISCALL WorldRest::MultiplayerFrame(World *self, DWORD, DWORD draw_views)
{
    void *net = players(self);
    if ((BYTE)vcall<DWORD>(self->game_type, 12)) {
        const DWORD n = vcall<DWORD>(net, 11);
        if (n) {
            BYTE *c0 = static_cast<BYTE *>(self->characters.data[0]);
            if (c0)
                c0[0x128] = 1;
            Layer *l = player_layer(self, 0);
            self->flag29c = 1;
            ShakenView v;
            shake_view(self, l, 0, v);
            if (v.shaken)
                set_view_target(self, fbits(v.pos[0]), fbits(v.pos[1]), 0, 0);
            vcall<DWORD>(net, 27, 0);
            BYTE *r = vcall<BYTE *>(net, 23, 0);
            int rect[4];
            if (field<int>(net_player(self, 0), 0x34) > 0 && (BYTE)draw_views) {
                fn<int(THISCALL *)(Layer *, BYTE *, int *)>(0x4ea660)(l, r + 0xc0, rect);
                fn<void(THISCALL *)(Layer *, BYTE *, DWORD, int *)>(0x4ee760)(l, r + 0x3c, 0, rect);
                int &left = field<int>(net_player(self, 0), 0x34);
                if (left > 0)
                    left--;
            }
            read_all_blocks(r + 0x3c);
            if (!(BYTE)fn<DWORD(__cdecl *)()>(0x4e9d80)())
                *glob<void **>(0x6bde9c) = self->characters.count ? self->characters.data[0] : nullptr;
            fn<DWORD(THISCALL *)(void *)>(0x520750)(r + 0x3c);
            if (v.shaken)
                set_view_target(self, v.saved[0], v.saved[1], 0, 0);
            if (c0)
                c0[0x128] = 0;
            self->flag29c = 0;
            for (DWORD i = 1; i < n; i++) {
                BYTE *c = static_cast<BYTE *>(self->characters.data[i]);
                if (c)
                    c[0x128] = 1;
                Layer *li = player_layer(self, i);
                shake_view(self, li, i, v);
                BYTE shaken = v.shaken;
                if (shaken)
                    set_view_target(self, fbits(v.pos[0]), fbits(v.pos[1]), 0, i);
                vcall<DWORD>(net, 27, i);
                BYTE *ri = vcall<BYTE *>(net, 23, i);
                if (ri[0x39] && field<int>(net_player(self, i), 0x34) > 0) {
                    Layer *lv = player_layer(self, i);
                    fn<int(THISCALL *)(Layer *, BYTE *, int *)>(0x4ea660)(lv, ri + 0xc0, rect);
                    fn<void(THISCALL *)(Layer *, BYTE *, DWORD, int *)>(0x4ee760)(lv, ri + 0x3c, i, rect);
                    int &left = field<int>(net_player(self, i), 0x34);
                    if (left > 0)
                        left--;
                    shaken = v.shaken;
                    ri[0x39] = 0;
                }
                vcall<DWORD>(self->game_type, 14, i, ri + 0x3c);
                if (shaken)
                    set_view_target(self, v.saved[0], v.saved[1], 0, i);
                if (c)
                    c[0x128] = 0;
            }
        }
        *glob<DWORD *>(0x6c28f4) = 0;
        return;
    }
    Layer *l = player_layer(self, 0);
    BYTE *packet = vcall<BYTE *>(net, 23, 0) + 0x3c;
    if ((BYTE)fn<DWORD(THISCALL *)(void *)>(0x520f50)(packet)) {
        do
            fn<DWORD(THISCALL *)(void *)>(0x5200d0)(packet);
        while ((BYTE)fn<DWORD(THISCALL *)(void *)>(0x520f50)(packet));
        fn<DWORD(THISCALL *)(void *)>(0x520750)(packet);
    }
    BYTE *lb = reinterpret_cast<BYTE *>(l);
    if (lb[0x1cc]) {
        *reinterpret_cast<DWORD *>(&self->time) = 0;
        self->byte2f8 = 1;
    } else if (lb[0x1cd]) {
        fn<DWORD(THISCALL *)(Layer *, DWORD)>(0x4ea6e0)(l, 0);
    } else {
        BYTE &g = *glob<BYTE *>(0x6c20f8);
        if (!(g & 1)) {
            g |= 1;
            fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(glob<void *>(0x6c24c8), 0, 0, 0, 0xff);
            crt_atexit_raw(0x4f0700);
        }
        vcall<DWORD>(self->screen_frame, 28, glob<void *>(0x62e578), glob<void *>(0x6c24c8));
    }
    send_client_ready(net);
    lb[0x1cc] = 0;
    *glob<DWORD *>(0x6c28f4) = 0;
}

// 0x4f0ac0 (fastcall, registered in the static list 0x62e6a8): the Properties.txt path of a sequence's
// entry in the "Sequences" list, else in the "Animated Textures" list: "<list folder>/-<entry name>/
// Properties.txt" (the entry loaded first, 0x55e090). eax: 1 when found, else 0.
DWORD __fastcall WorldRest::PropertiesPath(void *object, void *out)
{
    const DWORD lists[2] = {glob_addr(SEQUENCE_LIST), glob_addr(ANIMATED_TEXTURE_LIST)};
    for (DWORD k = 0; k < 2; k++) {
        ZDefinitionList *l = glob_dyn<ZDefinitionList *>(lists[k]);
        if (!is_kind(object, class_of(CI_SEQUENCE)) || !object)
            continue;
        DWORD i = 0;
        for (; i < l->num_entries; i++)
            if (l->entries[i].object == object)
                break;
        if (i >= l->num_entries)
            continue;
        zs_assign_text(out, vcall<const char *>(l, 5));
        zs_append(out, glob<const char *>(0x61804c));   // "/-"
        const char *name = nullptr;
        if (l->entries) {
            const DWORD j = i < l->num_entries ? i : 0;
            if (!l->entries[j].loading && !l->entries[j].object)
                fn<void *(THISCALL *)(ZDefinitionList *, DWORD)>(0x55e090)(l, j);
            name = l->entries[i].name;
        }
        zs_append(out, name);
        zs_append(out, glob<const char *>(0x61803c));   // "/Properties.txt"
        return 1;
    }
    return 0;
}

// 0x4f10f0 (slot 45): every entry of the "Sequences" and "Animated Textures" lists loaded
void THISCALL WorldRest::PreloadDefinitions(World *)
{
    preload_list(glob<ZDefinitionList *>(SEQUENCE_LIST));
    preload_list(glob<ZDefinitionList *>(ANIMATED_TEXTURE_LIST));
}

// 0x4f3200 (slot 52): the zoom keys of a player (dt by its bits)
void THISCALL WorldRest::ZoomKeys(World *self, DWORD dt, DWORD i)
{
    vcall<DWORD>(net_player(self, i), 1);
    BYTE *rec = player(self, i);
    const BYTE keys = rec[0xe0];
    if (keys & 1) {
        const LD z = fld(field<float>(player(self, i), 0xd8));
        if (!c03(z, cst(glob_addr(ZOOM_MIN)))) {
            const LD t = z - cst(glob_addr(C_EPS2));
            float f = fstp(t);
            if (c0(t, cst(glob_addr(ZOOM_MIN))))
                f = *glob<const float *>(ZOOM_MIN);
            set_zoom(self, fbits(f), i);
        }
    }
    if (rec[0xe0] & 2) {
        const LD z = fld(field<float>(player(self, i), 0xd8));
        if (c0(z, cst(glob_addr(ZOOM_MAX)))) {
            const LD t = z + cst(glob_addr(C_EPS2));
            float f = fstp(t);
            if (!c03(t, cst(glob_addr(ZOOM_MAX))))
                f = *glob<const float *>(ZOOM_MAX);
            set_zoom(self, fbits(f), i);
        }
    }
    if (rec[0xe0] & 4)
        set_zoom(self, 0x3f800000, i);
    if (rec[0xe0] & 3)
        return;
    float f = raw_float(field<DWORD>(player(self, i), 0xd8));
    if (c3(fld(f), cst(glob_addr(C_ONE))))
        return;
    const float fdt = raw_float(dt);
    bool step_up = false;
    if (!c03(fld(f), cst(glob_addr(C_ONE)))) {
        if (!c03((cst(glob_addr(ZOOM_MAX)) - cst(glob_addr(C_ONE))) * cst(glob_addr(C_QUARTER)) + cst(glob_addr(C_ONE)), fld(f))) {
            const LD t = fld(f) - fld(fdt) * cst(glob_addr(0x5f14ec));
            f = fstp(t);
            if (c0(t, cst(glob_addr(C_ONE))))
                f = 1.0f;
            set_zoom(self, fbits(f), i);
            return;
        }
    }
    if (c0(fld(f), cst(glob_addr(C_ONE))) && c0((cst(glob_addr(C_ONE)) - cst(glob_addr(ZOOM_MIN))) * cst(glob_addr(0x5f14e8)) + cst(glob_addr(ZOOM_MIN)), fld(f)))
        step_up = true;
    if (step_up) {
        const LD t = fld(fdt) * cst(glob_addr(0x5f14ec)) + fld(f);
        f = fstp(t);
        if (!c03(t, cst(glob_addr(C_ONE))))
            f = 1.0f;
    }
    set_zoom(self, fbits(f), i);
}

// 0x4f3ac0: the path of player 0's level (slot 68 of its file name) into a ZString
void THISCALL WorldRest::CurrentLevelPath(World *self, void *out)
{
    if (!self->layers.count)
        return;
    Layer *l = player_layer(self, 0);
    if (!l)
        return;
    char *buf = zs_get_buffer(out, 0x100);
    const char *name = fn<const char *(THISCALL *)(Layer *)>(0x4eb670)(l);
    vcall<DWORD>(self, 68, name, 0, buf);
    zs_release_buffer(out, -1);
}

// 0x4f3e70 (slot 74): reads a save game: every layer deleted (before and after the timers and the loading
// screen's sort list are reset), the layer holder and the character holder read (binary or text), player
// 0's layer assigned to every player (slot 78), the frames rebuilt, every player's views put on its
// layer's view position with zoom 1, "client ready" and "client sync" sent. al: both reads succeeded.
DWORD THISCALL WorldRest::ReadSaveGame(World *self, void *stream, DWORD binary)
{
    const BYTE old = self->saving;
    self->saving = 1;
    section_begin(0x618584);   // "Read Save Game"
    auto remove_all = [self]() {
        while (self->layers.count)
            fn<DWORD(THISCALL *)(LayerArray *, DWORD)>(0x4eea50)(&self->layers, self->layers.count - 1);
    };
    remove_all();
    fn<DWORD(THISCALL *)(void *)>(0x506f70)(self->timers);
    void *s = fn<void *(THISCALL *)(World *)>(0x4f3d50)(self);
    fn<DWORD(THISCALL *)(void *)>(0x579030)(s);
    remove_all();
    BYTE *rec0 = player(*glob<World **>(G_WORLD), 0);
    const DWORD read = binary & 0xff ? fn_addr(0x416fe0) : fn_addr(0x416af0);
    BYTE ok = (BYTE)fn_dyn<DWORD(THISCALL *)(void *, void *)>(read)(self->layer_holder, stream) & 1;
    const DWORD idx = field<DWORD>(rec0, 0xdc);
    if (idx < self->layers.count)
        vcall<DWORD>(self, 78, self->layers.data[idx], 0xffffffff);
    ok &= (BYTE)fn_dyn<DWORD(THISCALL *)(void *, void *)>(read)(&self->holder_vftable, stream);
    section_end();
    self->level_loaded = self->layers.count != 0;
    if (self->layers.count)
        rebuild_frames(self);
    for (DWORD i = 0; i < player_count(self); i++) {
        BYTE *p = player(self, i);
        if (!player_layer(self, i))
            continue;
        BYTE *l = reinterpret_cast<BYTE *>(player_layer(self, i));
        field<DWORD>(p, 0xc0) = field<DWORD>(l, 0x80);
        field<DWORD>(p, 0xc4) = field<DWORD>(l, 0x84);
        l = reinterpret_cast<BYTE *>(player_layer(self, i));
        field<DWORD>(p, 0xc8) = field<DWORD>(l, 0x80);
        field<DWORD>(p, 0xcc) = field<DWORD>(l, 0x84);
        l = reinterpret_cast<BYTE *>(player_layer(self, i));
        field<DWORD>(p, 0xd0) = field<DWORD>(l, 0x80);
        field<DWORD>(p, 0xd4) = field<DWORD>(l, 0x84);
        field<DWORD>(p, 0xd8) = 0x3f800000;
    }
    void *net = players(self);
    send_client_ready(net);
    void *packet = vcall<void *>(net, 12, 0);
    void *reader = fn<void *(__cdecl *)()>(0x4f78a0)();
    fn<DWORD(THISCALL *)(void *, void *)>(0x5ac330)(reader, packet);
    self->saving = old;
    return ok;
}

// 0x4f4190 (slot 27): the base class's slot (0x4ce4f0), then the levels' resources precached again
// (kind 0: player 0's level, kind 1: every level and the rules); when one of them asks for it (+0x1c8),
// every resource list releases what it marks (slot 8 / slot 12). The first character is taken out of
// player 0's level meanwhile unless keep_character.
void THISCALL WorldRest::ReloadResources(World *self, DWORD kind, DWORD keep_character)
{
    fn<DWORD(THISCALL *)(World *, DWORD, DWORD)>(0x4ce4f0)(self, kind, keep_character);
    void *c = self->characters.count ? self->characters.data[0] : nullptr;
    Layer *l = self->layers.count ? fn<Layer *(THISCALL *)(World *, DWORD)>(0x4f10a0)(self, 0) : nullptr;
    BYTE flag = 0;
    if (!(BYTE)keep_character)
        fn<void(THISCALL *)(Layer *, void *)>(0x4ebcb0)(l, c);
    bool reload = true;
    if (kind == 0) {
        Layer *a = self->layers.count ? fn<Layer *(THISCALL *)(World *, DWORD)>(0x4f10a0)(self, kind) : nullptr;
        vcall<DWORD>(a, 7);
        Layer *b = self->layers.count ? fn<Layer *(THISCALL *)(World *, DWORD)>(0x4f10a0)(self, 0) : nullptr;
        flag = reinterpret_cast<BYTE *>(b)[0x1c8];
    } else if (kind == 1) {
        for (DWORD i = 0; i < self->layers.count; i++)
            if (self->layers.data[i]) {
                vcall<DWORD>(self->layers.data[i], 7);
                flag |= static_cast<BYTE *>(self->layers.data[i])[0x1c8];
            }
        vcall<DWORD>(self->rules, 7);
    } else {
        reload = false;
    }
    if (reload && flag) {
        auto lists = []() { return fn<BYTE *(__cdecl *)()>(0x4a35d0)(); };
        auto list_at = [&](DWORD off) { return *reinterpret_cast<void **>(*reinterpret_cast<BYTE **>(lists() + 8) + off + 4); };
        DWORD off = 0;
        for (DWORD i = 0; i < vcall<DWORD>(lists(), 22); i++, off += 0x10) {
            for (DWORD j = 0; j < vcall<DWORD>(list_at(off), 22); j++)
                if ((BYTE)vcall<DWORD>(list_at(off), 8, j))
                    vcall<DWORD>(list_at(off), 12, j);
        }
    }
    if (!(BYTE)keep_character)
        fn<DWORD(THISCALL *)(Layer *, void *)>(0x4eb6f0)(l, c);
}

// 0x4f5660 (slot 87): the fixups of one part after loading: "UnNamed" loses its name, dynamic AIs are
// adopted, way point polygons get their flags cleared and a free range poly named "add to way points"
// is replaced by a way point polygon; an entity gets the model's flags, loses AIs the model's activities
// replace, gets the default entity's child list (a CEntityAnimated child of a 0x49b810 entity replaced by
// the default's first child) and door AI, and a model-defined death behavior "<model> Default".
void THISCALL WorldRest::FixPart(World *self, Layer *layer, void *part)
{
    vcall<DWORD>(part, 58, 0x200000);
    if (part_name(part) && !crt_stricmp_(part_name(part), glob<const char *>(0x618798)))   // "UnNamed"
        fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x417660)(part, glob<const char *>(0x60b080), 0, 0);   // "Name"
    fn<DWORD(THISCALL *)(void *)>(0x4fbba0)(part);
    if (is_kind(part, class_of(CI_FREE_RANGE_POLY))) {
        if (is_kind(part, class_of(CI_WAY_POINTS_POLYGON))) {
            void *wp = part && is_kind(part, class_of(CI_WAY_POINTS_POLYGON)) ? part : nullptr;
            fn<DWORD(THISCALL *)(void *)>(0x5992b0)(wp);
        } else if (part_name(part) && !crt_stricmp_(part_name(part), glob<const char *>(0x618784))) {   // "add to way points"
            void *p = pool_alloc(0x78);
            void *wp = p ? fn<void *(THISCALL *)(void *)>(0x599290)(p) : nullptr;
            vcall<DWORD>(wp, 20, part);
            fn<DWORD(THISCALL *)(void *)>(0x5992b0)(wp);
            vcall<DWORD>(self, 104, wp, layer);
            delete_part(layer, part);
            return;
        }
    }
    if (!is_kind(part, class_of(CI_ENTITY)))
        return;
    BYTE *model = static_cast<BYTE *>(entity_model(part));
    if (!model)
        return;
    BYTE *def = field<BYTE *>(model, 0x74);
    if (!(field<DWORD>(part, 0x1c) & 0xffff) && def) {
        const DWORD mask = *glob<DWORD *>(0x6c29a8);
        if (field<DWORD>(def, 0x1c) & mask)
            vcall<DWORD>(part, 58, mask);
    }
    const DWORD n = field<DWORD>(model, 0x10);
    for (DWORD i = 0; i < n; i++) {
        void *a = fn<void *(THISCALL *)(void *, DWORD)>(0x5174f0)(model, i);
        if (!a)
            continue;
        void *f = fn<void *(THISCALL *)(void *, CClassInfo *, DWORD)>(0x4fbae0)(part, vcall<CClassInfo *>(a, 2), 0);
        if (f && f != a)
            fn<DWORD(THISCALL *)(void *, void *)>(0x4fbd90)(part, f);
    }
    if (field<void *>(model, 0x74)) {
        void *list = field<void *>(field<void *>(model, 0x74), 8);
        if (list) {
            if (!field<void *>(part, 8)) {
                fn<DWORD(THISCALL *)(void *, void *)>(0x4fc7e0)(part, object_clone(list));
                ZString s;
                zs_ctor(&s);
                const char *name = part_name(part) ? part_name(part) : glob<const char *>(EMPTY_TEXT);
                const char *what = vcall<const char *>(part, 61);
                zs_format2(&s, 0x618738, (DWORD)(uintptr_t)name, (DWORD)(uintptr_t)what);
                vcall<DWORD>(self, 16, zs_cstr(&s));
                zs_dtor(&s);
            }
            if (is_kind(part, class_of(CI_49B810))) {
                void *first = *field<void **>(field<void *>(part, 8), 0xc);
                if (first && is_kind(first, class_of(CI_ENTITY_ANIMATED)) && !is_kind(first, class_of(CI_UPPER_BODY))) {
                    fn<DWORD(THISCALL *)(void *, void *)>(0x4f9b70)(field<void *>(part, 8), first);
                    void *src = *field<void **>(field<void *>(field<void *>(model, 0x74), 8), 0xc);
                    void *c = object_clone(src);
                    if (!c || !is_kind(c, class_of(CI_LAYER_PART)))
                        c = nullptr;
                    fn<DWORD(THISCALL *)(void *, void *)>(0x4fc710)(part, c);
                }
            }
        }
        void *d = field<void *>(model, 0x74);
        void *door = vcall<void *>(d, 64, class_of(CI_DOOR_AI), 0);
        if (door && !vcall<void *>(part, 64, class_of(CI_DOOR_AI), 0)) {
            fn<DWORD(THISCALL *)(void *, void *)>(0x4fbc50)(part, object_clone(door));
            ZString s;
            zs_ctor(&s);
            const char *name = part_name(part) ? part_name(part) : glob<const char *>(EMPTY_TEXT);
            const char *what = vcall<const char *>(part, 61);
            zs_format2(&s, 0x6186f0, (DWORD)(uintptr_t)name, (DWORD)(uintptr_t)what);
            vcall<DWORD>(self, 16, zs_cstr(&s));
            zs_dtor(&s);
        }
    }
    if (!part || !is_kind(part, class_of(CI_48BDF0)))
        return;
    if (c03(fld(field<float>(model, 0x20)), cst(glob_addr(C_ZERO))))
        return;
    vcall<DWORD>(part, 58, 0x100000);
    ZDefinitionList *deaths = glob<ZDefinitionList *>(DEATH_BEHAVIORS);
    void *a = list_entry(deaths, field<DWORD>(part, 0x8c));
    void *b = list_entry(deaths, *glob<DWORD *>(0x5fe8f8));
    if (a != b)
        return;
    char buf[0x100];
    lstrcpyA(buf, fn<const char *(THISCALL *)(void *)>(0x5174a0)(model));
    lstrcatA(buf, glob<const char *>(0x6186e4));   // " Default"
    const int idx = vcall<int>(deaths, 2, buf, 0xffffffff);
    if (idx != -1 && idx > 0)
        field<int>(part, 0x8c) = idx;
}

// 0x4f5c80: "Fixing legacy data": every part of the layer either deleted (marked for deletion, flag
// 0x10000 after slot 39) or fixed (slot 87); the count of the deleted ones reported (slot 16)
void THISCALL WorldRest::FixLegacyData(World *self, Layer *layer)
{
    LayerArray *list;
    fn<void(THISCALL *)(LayerArray **)>(0x59a470)(&list);
    fn<DWORD(THISCALL *)(void *, LayerArray *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x57a770)(reinterpret_cast<BYTE *>(layer) + 0x50, list, 0, 5,
                                                                                         0, 0, 0);
    DWORD deleted = 0;
    fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x536080)(glob<void *>(0x6c0468), glob<const char *>(0x6187d4), 1, 0);
    for (DWORD i = 0; i < list->count; i++) {
        void *p = list->data[i];
        vcall<DWORD>(p, 39);
        if (field<DWORD>(p, 0x1c) >> 16 & 1) {
            delete_part(layer, p);
            deleted++;
        } else {
            vcall<DWORD>(self, 87, layer, p);
        }
    }
    if (deleted) {
        ZString s;
        zs_ctor(&s);
        zs_format1(&s, 0x6187a0, deleted);   // "Deleted %d LayerParts that were marked for deletion"
        vcall<DWORD>(self, 16, zs_cstr(&s));
        zs_dtor(&s);
    }
    fn<void(THISCALL *)(LayerArray **)>(0x59a510)(&list);
}

// 0x4f5dd0: sets a player's view target (x, y by their bits) and moves its view (see world_rest.h)
void THISCALL WorldRest::SetViewTarget(World *self, DWORD x, DWORD y, int mode, DWORD i)
{
    if (i == 0xffffffff) {
        for (DWORD k = 0; k < player_count(self); k++)
            set_view_target(self, x, y, mode, k);
        return;
    }
    Layer *l = player_layer(self, i);
    if (!l)
        return;
    BYTE *r = player(self, i);
    field<DWORD>(r, 0xc8) = x;
    field<DWORD>(r, 0xcc) = y;
    if (mode == 1 && (!(BYTE)vcall<DWORD>(self, 30) || (BYTE)vcall<DWORD>(self, 31))) {
        const LD dx = fld(field<float>(r, 0xc8)) - fld(field<float>(r, 0xd0));
        const LD dy = fld(field<float>(r, 0xcc)) - fld(field<float>(r, 0xd4));
        float fdy = fstp(dy);
        const float k = fstp(cst(glob_addr(C_ONE)) - cst(glob_addr(C_ONE)) / ((dy * dy + dx * dx) * cst(glob_addr(0x5f9180)) + cst(glob_addr(C_ONE))));
        const LD mx = dx * fld(k);
        fdy = fstp(fld(fdy) * fld(k));
        field<float>(r, 0xd0) = fstp(mx + fld(field<float>(r, 0xd0)));
        field<float>(r, 0xd4) = fstp(fld(fdy) + fld(field<float>(r, 0xd4)));
    } else {
        field<DWORD>(r, 0xd0) = field<DWORD>(r, 0xc8);
        field<DWORD>(r, 0xd4) = field<DWORD>(r, 0xcc);
    }
    const int w = vcall<int>(self->zoom_frame, 45);
    const int h = vcall<int>(self->zoom_frame, 39);
    float *view = &field<float>(r, 0xc0);
    if (mode == 0) {
        view[0] = fstp(fld(field<float>(r, 0xc8)) - fild(w / 2));
        view[1] = fstp(fld(field<float>(r, 0xcc)) - fild(h / 2));
        fn<DWORD(THISCALL *)(Layer *, float *)>(0x4ed7f0)(l, view);
    } else if (mode > 0 && mode <= 2) {
        const BYTE *rules = self->rules;
        fn<DWORD(THISCALL *)(Layer *, float *, DWORD, DWORD, float, float, DWORD, DWORD)>(0x4ed6e0)(
            l, view, field<DWORD>(r, 0xd0), field<DWORD>(r, 0xd4), fstp(fild(w)), fstp(fild(h)),
            *reinterpret_cast<const DWORD *>(rules + 0x10), *reinterpret_cast<const DWORD *>(rules + 0x14));
    }
    if (i == 0)
        fn<DWORD(THISCALL *)(Layer *, DWORD, DWORD)>(0x4ed8f0)(l, field<DWORD>(r, 0xc0), field<DWORD>(r, 0xc4));
}

// 0x4f6320: an explosion (a CEntityAnimated of the model) at (x, y) in the layer, playing its "Idle"
// animation, optionally with the model's sound
void THISCALL WorldRest::SpawnExplosion(World *self, Layer *layer, DWORD model, DWORD x, DWORD y, DWORD a5, DWORD sound,
                                        DWORD a7)
{
    if (!model)
        return;
    void *e = create_from_model(model, class_of(CI_ENTITY_ANIMATED), 0);
    if (!e)
        return;
    if (!is_kind(e, class_of(CI_ENTITY_ANIMATED)))
        e = replace_wrong_entity(e, model, 0, false);
    place_explosion(e, x, y, a5, a7, true);
    fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x490060)(e, static_text(0x6c207c, 0x6c2700, 0x60f8d0, 0x4f6520), 0x3f800000);
    vcall<DWORD>(self, 104, e, layer);
    if (!(BYTE)sound)
        return;
    if (!field<void *>(entity_model(e), 0xa4))
        return;
    DWORD pos[2] = {x, y};
    fn<DWORD(THISCALL *)(Layer *, void *, DWORD *, DWORD, DWORD, DWORD)>(0x4eaca0)(layer, field<void *>(entity_model(e), 0xa4), pos, 0, 0,
                                                                                  0xff);
}

// 0x4f6530: the same as an entity of a creator (a9), with an AI of 0x4cc7a0 instead of the "Idle"
// animation when with_ai, the model's sound object added to the layer when add. eax: the entity.
void *THISCALL WorldRest::SpawnExplosionEntity(World *self, Layer *layer, DWORD model, DWORD x, DWORD y, DWORD a5, DWORD add,
                                               DWORD a7, DWORD with_ai, DWORD a9)
{
    if (!model)
        return nullptr;
    void *e = create_from_model(model, class_of(CI_ENTITY_ANIMATED), a9);
    if (!e)
        return nullptr;
    if (!is_kind(e, class_of(CI_ENTITY_ANIMATED)))
        e = replace_wrong_entity(e, model, a9, true);
    place_explosion(e, x, y, a5, a7, false);
    if ((BYTE)with_ai) {
        void *p = pool_alloc(0x40);
        fn<DWORD(THISCALL *)(void *, void *)>(0x4fbc50)(e, p ? fn<void *(THISCALL *)(void *)>(0x4cc7a0)(p) : nullptr);
    } else {
        fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x490060)(e, static_text(0x6c24c0, 0x6c25f8, 0x60f8d0, 0x4f6730), 0x3f800000);
    }
    vcall<DWORD>(self, 104, e, layer);
    if ((BYTE)add)
        fn<DWORD(THISCALL *)(void *, void *)>(0x4fc8a0)(e, field<void *>(entity_model(e), 0xa4));
    return e;
}

// 0x4f6770: sets a player's zoom (by its bits), limited so the zoomed view fits player 0's tile map
void THISCALL WorldRest::SetZoom(World *self, DWORD zoom_bits, DWORD i)
{
    BYTE *rec = player(self, i);
    if (!self->layers.count)
        return;
    Layer *l = fn<Layer *(THISCALL *)(World *, DWORD)>(0x4f10a0)(self, 0);
    if (!l)
        return;
    float zoom = raw_float(zoom_bits);
    if (BYTE *tm = field<BYTE *>(l, 0x7c)) {
        const DWORD w1 = field<DWORD>(tm, 0x2cfc) - 1;
        const DWORD h1 = (field<DWORD>(tm, 0x2d00) - 1) & ~0x7fu;
        const LD a = fild64((long long)h1);
        const LD bz = fild64((long long)view_width(self)) * fld(zoom);
        if (c0(fild64((long long)w1), bz))
            zoom = fstp(fild64((long long)(w1 & ~0x7fu)) / fild((int)view_width(self)));
        if (!c03(fild64((long long)view_height(self)) * fld(zoom), a))
            zoom = fstp(a / fild64((long long)view_height(self)));
    }
    const float diff = fstp(fld(field<float>(rec, 0xd8)) - fld(zoom));
    if (c03(fld(raw_float(fbits(diff) & 0x7fffffff)), cst(glob_addr(C_EPS4))))
        return;
    field<float>(rec, 0xd8) = zoom;
    float cx = field<float>(rec, 0xc0), cy = field<float>(rec, 0xc4);
    const DWORD hw = vcall<DWORD>(self->zoom_frame, 45) >> 1;
    cx = fstp(fild64((long long)hw) + fld(cx));
    const DWORD hh = vcall<DWORD>(self->zoom_frame, 39) >> 1;
    cy = fstp(fild64((long long)hh) + fld(cy));
    resize_zoom_frame(self, zoom);
    set_view_target(self, fbits(cx), fbits(cy), 0, i);
    rebuild_frames(self);
}

// 0x4f6980: the view rectangle (null: the screen frame without the 0x38-pixel bar at the bottom; a given
// rectangle is cut at the screen frame's width) and the zoom frame sized for player 0's zoom
void THISCALL WorldRest::SetViewRect(World *self, const int *rect)
{
    BYTE *rec = player(self, 0);
    if (rect) {
        for (int k = 0; k < 4; k++)
            self->view[k] = rect[k];
        if (self->view[2] > vcall<int>(self->screen_frame, 45))
            self->view[2] = vcall<int>(self->screen_frame, 45);
    } else {
        const int h = vcall<int>(self->screen_frame, 39);
        const int w = vcall<int>(self->screen_frame, 45);
        self->view[0] = 0;
        self->view[1] = 0;
        self->view[2] = w;
        self->view[3] = h;
        self->view[3] += -0x38;
    }
    resize_zoom_frame(self, field<float>(rec, 0xd8));
}

// ---- The definition list loaders --------------------------------------------------------------------

namespace {

void spec_ctor(ZFileSpec *s) { fn<ZFileSpec *(THISCALL *)(ZFileSpec *)>(0x4a09f0)(s); }
void spec_set(ZFileSpec *s, const char *folder, const char *ext) { fn<DWORD(THISCALL *)(ZFileSpec *, const char *, const char *)>(0x4a0a60)(s, folder, ext); }
void spec_set_folders(ZFileSpec *s, const char *folder, DWORD on) { fn<DWORD(THISCALL *)(ZFileSpec *, const char *, DWORD)>(0x4a0aa0)(s, folder, on); }
void spec_set_mode(ZFileSpec *s, DWORD mode) { fn<DWORD(THISCALL *)(ZFileSpec *, DWORD)>(0x4a0b20)(s, mode); }
void spec_dtor(ZFileSpec *s)
{
    for (int k = 4; k >= 0; k--)
        zs_dtor(&s->ext[k]);
}
DWORD files_count(ZFileSpec *s, DWORD flags) { return fn<DWORD(__fastcall *)(ZFileSpec *, DWORD)>(0x4a12d0)(s, flags); }
DWORD files_list_names(ZFileSpec *s, void *names, DWORD max, DWORD flags)
{
    return fn<DWORD(__fastcall *)(ZFileSpec *, void *, DWORD, DWORD, DWORD, DWORD)>(0x4a1150)(s, names, 0, 0x80, max, flags);
}

// How a list load builds its records and finds its files (the loaders 0x4f6ba0-0x4f74a0 and the three
// loads inlined into 0x4f13a0 differ in these)
struct ListLoad {
    int spec;                 // 0: files with the extension ext, 1: folders (0x4a0aa0), 2: by 0x46b760 (0: ext, else folders)
    DWORD ext;
    int init_files;           // records of the files: 0 cleared inline, 1 DefinitionRecord_Init each, 2 vector_ctor_iterator
    int init_cache;           // the same for the "Cache/<folder>" files (0 or 2)
    DWORD cache_ext;          // their extension, or 0: from cache_ext_getter (called after the prefix's c_str)
    DWORD cache_ext_getter;
    bool vector_dtor;         // the file specs' ZStrings through the compiler's vector destructor 0x408e70
};

void vector_ctor(void *p, DWORD size, DWORD n, DWORD ctor) { fn<void(STDCALL *)(void *, DWORD, DWORD, DWORD)>(0x402090)(p, size, n, fn_addr_dyn(ctor)); }
void vector_dtor(void *p, DWORD size, DWORD n, DWORD dtor) { fn<void(STDCALL *)(void *, DWORD, DWORD, DWORD)>(0x408e70)(p, size, n, fn_addr_dyn(dtor)); }

// The records of a list of n files: n + 1 records from operator new (count in front)
ZDefinitionRecord *new_records(DWORD n, int init)
{
    const DWORD m = n + 1;
    DWORD *block = static_cast<DWORD *>(heap_alloc(m * 0x80 + 4));
    if (!block)
        return nullptr;
    block[0] = m;
    ZDefinitionRecord *data = reinterpret_cast<ZDefinitionRecord *>(block + 1);
    if (init == 2) {
        vector_ctor(data, 0x80, m, 0x5a4060);
        return data;
    }
    if ((int)(m - 1) >= 0) {
        for (DWORD i = 0; i < m; i++) {
            if (init == 1) {
                fn<void *(THISCALL *)(void *)>(0x5a4060)(&data[i]);
            } else {
                data[i].name[0] = 0;
                data[i].object = nullptr;
                data[i].b7e = 0;
                data[i].in_use = 0;
                data[i].keep = 0;
                data[i].loading = 0;
            }
        }
    }
    return data;
}

void free_spec(ZFileSpec *s, bool vector)
{
    if (vector)
        vector_dtor(s->ext, 4, 5, 0x4defd0);
    else
        spec_dtor(s);
}

// The shared body of the list loads: like the definition lists' Init (definitions.cpp) with the name,
// folder, file kind and search flags as parameters. eax: the number of entries.
DWORD load_list(ZDefinitionList *self, const char *name, const char *folder, DWORD flags, const ListLoad &o)
{
    self->list_name = name;
    void *r = fn<void *(__cdecl *)()>(0x4a35d0)();
    const DWORD index = fn<DWORD(THISCALL *)(void *, const char *, void *, DWORD, DWORD)>(0x4a34d0)(r, name, self, 0, 1);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x523e20)(self, index);
    vcall<DWORD>(self, 40);
    lstrcpyA(self->folder, folder);
    ZString prefix;
    zs_ctor(&prefix);
    zs_set_path(&prefix, folder, glob<const char *>(EMPTY_TEXT));
    ZFileSpec spec;
    spec_ctor(&spec);
    const bool folders = o.spec == 1 || (o.spec == 2 && fn<DWORD(__cdecl *)()>(0x46b760)());
    if (folders)
        spec_set_folders(&spec, zs_cstr(&prefix), 1);
    else
        spec_set(&spec, zs_cstr(&prefix), glob_dyn<const char *>(o.ext));
    spec_set_mode(&spec, 1);
    DWORD n = files_count(&spec, flags);
    self->num_entries = n;
    self->entries = nullptr;
    if (n) {
        self->entries = new_records(n, o.init_files);
        self->num_entries = files_list_names(&spec, self->entries, self->num_entries, flags);
    } else {
        ZString cache;
        zs_ctor(&cache);
        zs_set_path(&cache, fn<const char *(__cdecl *)()>(0x4a1140)(), folder);
        zs_set_path(&prefix, zs_cstr(&cache), glob<const char *>(EMPTY_TEXT));
        ZFileSpec spec2;
        spec_ctor(&spec2);
        if (o.cache_ext_getter) {
            const char *p = zs_cstr(&prefix);
            spec_set(&spec2, p, fn_dyn<const char *(__cdecl *)()>(o.cache_ext_getter)());
        } else {
            spec_set(&spec2, zs_cstr(&prefix), glob_dyn<const char *>(o.cache_ext));
        }
        spec_set_mode(&spec2, 1);
        n = files_count(&spec2, flags);
        self->num_entries = n;
        if (n) {
            self->entries = new_records(n, o.init_cache);
            self->num_entries = files_list_names(&spec2, self->entries, self->num_entries, flags);
        }
        free_spec(&spec2, o.vector_dtor);
        zs_dtor(&cache);
    }
    if (self->num_entries) {
        for (DWORD i = 0; i < self->num_entries; i++) {
            char *e = self->entries[i].name;
            text_cut_prefix(e, zs_cstr(&prefix));
            path_set_extension(e, glob<const char *>(EMPTY_TEXT));
            text_cut_prefix(path_name_(e), glob<const char *>(0x60f634));   // "-"
        }
        crt_qsort(self->entries, self->num_entries, 0x80, 0x5d3640);
        fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(self, 1, 0x80, self->num_entries, self->entries, 0);
    } else {
        self->entries = nullptr;
    }
    void *m = nullptr;
    if (void *p = heap_alloc(0x34))
        m = fn<void *(THISCALL *)(void *, void *)>(0x47e560)(p, self);
    self->monitor = m;
    vcall<DWORD>(self, 27, m);
    const DWORD count = self->num_entries;
    free_spec(&spec, o.vector_dtor);
    zs_dtor(&prefix);
    return count;
}

constexpr ListLoad LOADER_STY = {0, 0x61887c /* "STY" */, 0, 0, 0, 0x41e0e0, false};
constexpr ListLoad LOADER_MODELS = {1, 0, 0, 0, 0, 0x516a20, false};
constexpr ListLoad LOADER_RDS = {0, 0x60b0b0 /* "RDS" */, 0, 0, 0, 0x41e0e0, false};
constexpr ListLoad LOADER_TXT = {0, 0x618880 /* "txt" */, 0, 0, 0x60b094 /* "property" */, 0, false};
constexpr ListLoad LOADER_QUIPS = {0, 0x61fffc /* "Quips" */, 0, 0, 0x60b094 /* "property" */, 0, false};
constexpr ListLoad LOADER_CANNED = {0, 0x60f2c0 /* "can" */, 0, 0, 0x60b094 /* "property" */, 0, false};
constexpr ListLoad LOADER_DIFFICULTY = {0, 0x618fb8 /* "DIFICULTY" */, 0, 0, 0x60b094 /* "property" */, 0, false};
constexpr ListLoad LOAD_FONTS = {1, 0, 1, 2, 0, 0x56b2b0, true};
constexpr ListLoad LOAD_FRAME_OVERLAYS = {0, 0x613e74 /* "FOO" */, 2, 2, 0x60b094 /* "property" */, 0, true};
constexpr ListLoad LOAD_FRAMES = {2, 0x613d8c /* "TGA" */, 2, 2, 0, 0x4a8980, true};

// The loaders called by 0x4f13a0 through their exe addresses (so Disable= applies)
void LoadStyList_(DWORD list)
{
    fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x4f6ba0)(glob_dyn<void *>(list), glob<const char *>(0x61837c),
                                                                            glob<const char *>(0x61837c), 0);   // "Surface Types"
}
void LoadTxtList_(DWORD list)
{
    fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x4f74a0)(glob_dyn<void *>(list), glob<const char *>(0x618308),
                                                                            glob<const char *>(0x618308), 0);   // "Water Warp Effects"
}

void load_entry(DWORD list, DWORD i) { fn<void *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob_dyn<void *>(list), i); }
void load_all(DWORD list)
{
    for (DWORD i = 0; i < glob_dyn<ZDefinitionList *>(list)->num_entries; i++)
        load_entry(list, i);
}

}  // namespace

DWORD THISCALL WorldRest::LoadStyList(void *list, const char *name, const char *folder, DWORD flags)
{
    return load_list(static_cast<ZDefinitionList *>(list), name, folder, flags, LOADER_STY);
}
DWORD THISCALL WorldRest::LoadModelList(void *list, const char *name, const char *folder, DWORD flags)
{
    return load_list(static_cast<ZDefinitionList *>(list), name, folder, flags, LOADER_MODELS);
}
DWORD THISCALL WorldRest::LoadRdsList(void *list, const char *name, const char *folder, DWORD flags)
{
    return load_list(static_cast<ZDefinitionList *>(list), name, folder, flags, LOADER_RDS);
}
DWORD THISCALL WorldRest::LoadTxtList(void *list, const char *name, const char *folder, DWORD flags)
{
    return load_list(static_cast<ZDefinitionList *>(list), name, folder, flags, LOADER_TXT);
}
DWORD THISCALL WorldRest::LoadQuipList(void *list, const char *name, const char *folder, DWORD flags)
{
    return load_list(static_cast<ZDefinitionList *>(list), name, folder, flags, LOADER_QUIPS);
}
DWORD THISCALL WorldRest::LoadCannedList(void *list, const char *name, const char *folder, DWORD flags)
{
    return load_list(static_cast<ZDefinitionList *>(list), name, folder, flags, LOADER_CANNED);
}
DWORD THISCALL WorldRest::LoadDifficultyList(void *list, const char *name, const char *folder, DWORD flags)
{
    return load_list(static_cast<ZDefinitionList *>(list), name, folder, flags, LOADER_DIFFICULTY);
}

// ---- The game data (slot 2) ----------------------------------------------------------------------------

namespace {

// The cursors of "Interface/Cursors" (0x56b950 on each object) and the table entries 0x62e5e4-0x62e63c
// that get them, in the order of the code
struct Cursor {
    DWORD object, name, entry;
};
constexpr Cursor CURSORS[] = {
    {0x6c2640, 0x6184e4, 0x62e5e4}, {0x6c2138, 0x6184c4, 0x62e5e8}, {0x6c1d18, 0x6184b4, 0x62e5fc}, {0x6c2600, 0x6184a4, 0x62e5f8},
    {0x6c2440, 0x618494, 0x62e604}, {0x6c2748, 0x618480, 0x62e600}, {0x6c26b8, 0x618470, 0x62e60c}, {0x6c24d8, 0x61845c, 0x62e608},
    {0x6c2680, 0x61844c, 0x62e5ec}, {0x6c20c0, 0x61843c, 0x62e5f0}, {0x6c2510, 0x61842c, 0x62e5f4}, {0x6c2270, 0x618418, 0x62e610},
    {0x6c2088, 0x618400, 0x62e618}, {0x6c25c0, 0x6183f0, 0x62e614}, {0x6c2708, 0x6183d8, 0x62e61c}, {0x6c1eb0, 0x6183c8, 0x62e624},
    {0x6c2100, 0x6183b4, 0x62e62c}, {0x6c22a8, 0x6183a0, 0x62e634}, {0x6c2588, 0x61838c, 0x62e63c},
};

DWORD category_bit(GlobKey name)
{
    const int i = fn<int(THISCALL *)(void *, const char *, int)>(0x591fc0)(glob<void *>(0x6bd140), glob<const char *>(name), -1);
    return 1u << (i & 31);
}
DWORD ini_int(GlobKey name) { return fn<DWORD(__fastcall *)(DWORD, DWORD, const char *, DWORD)>(0x4a4670)(glob_addr(0x6bd330), glob_addr(0x6bd430), glob<const char *>(name), 0); }

}  // namespace

// 0x4f13a0 (slot 2): the game data: the base class's slot 2, the game type (0x51ec90), the lists of
// fonts, frame overlay options, frames, sequences, the cursors, surface types, item categories, death
// behaviors, damage types and entity categories (the "Player", "Enemy" and "Player Friend" bits
// 0x6c29a0/0x6c29a8/0x6c29bc), textures, water warp effects, voice overs, dialogue scripts, models and
// sequences (preloaded when Zax.ini says so), the rules from Rules.txt, the frames and fonts of the
// interface, the music mix, the loading screen frames and the display. A "Red Flag" or "Blue Flag" item
// category ends the game ("Fatal error").
void THISCALL WorldRest::LoadGameData(World *self, void *app)
{
    fn<DWORD(THISCALL *)(World *, void *)>(0x4cdf70)(self, app);
    void *gt = fn<void *(__fastcall *)(void *, DWORD, DWORD)>(0x51ec90)(vcall<void *>(self, 44), 0, 0xffffffff);
    self->game_type = gt;
    vcall<DWORD>(gt, 1);
    load_list(glob<ZDefinitionList *>(0x6be1e0), glob<const char *>(0x61855c), glob<const char *>(0x61855c), 0, LOAD_FONTS);   // "Fonts"
    load_all(glob_addr(0x6be1e0));
    load_list(glob<ZDefinitionList *>(0x6bf9b8), glob<const char *>(0x618544) /* "Frame Overlay Options" */,
              glob<const char *>(0x618524) /* "Resources/Frame Overlay Options" */, 0, LOAD_FRAME_OVERLAYS);
    load_list(glob<ZDefinitionList *>(0x6bf850), glob<const char *>(0x618510) /* "Resources - Frames" */,
              glob<const char *>(0x618504) /* "Resources" */, 0, LOAD_FRAMES);
    auto load_c40 = [](GlobKey list, GlobKey name, GlobKey folder) {
        fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x4d7c40)(glob<void *>(list), glob<const char *>(name), glob<const char *>(folder), 0);
    };
    auto load_920 = [](GlobKey list, GlobKey name, GlobKey folder) {
        fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x4d7920)(glob<void *>(list), glob<const char *>(name), glob<const char *>(folder), 0);
    };
    load_c40(0x713380, 0x6184ec /* "Resources - Sequences" */, 0x618504 /* "Resources" */);
    for (const Cursor &c : CURSORS) {
        fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x56b950)(glob_dyn<void *>(c.object), glob<const char *>(0x6184d0), glob_dyn<const char *>(c.name), 0);
        if (glob_addr_dyn(c.object) == glob_addr(0x6c2640))
            fn<DWORD(THISCALL *)(void *, void *)>(0x518590)(glob<void *>(0x6cff40), glob<void *>(0x6c2640));
        *glob_dyn<DWORD *>(c.entry) = glob_addr_dyn(c.object);
    }
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4e27a0)(glob<void *>(0x6c1050), 0);
    fn<DWORD(THISCALL *)(void *, void *)>(0x4aa390)(self->screen_frame, fn<void *(__cdecl *)()>(0x433a80)());
    fn<DWORD(THISCALL *)(void *, void *)>(0x4aa390)(self->zoom_frame, fn<void *(__cdecl *)()>(0x433a80)());
    vcall<DWORD>(self, 8);
    LoadStyList_(glob_addr(0x713868));
    load_all(glob_addr(0x713868));
    fn<DWORD(THISCALL *)(void *)>(0x4d7330)(glob<void *>(0x6c0800));
    fn<DWORD(THISCALL *)(void *)>(0x4d6e10)(glob<void *>(0x6c0c08));
    auto find = [](GlobKey name, int hint) { return fn<int(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(0x6c0800), glob<const char *>(name), hint); };
    int found = find(0x618370 /* "Red Flag" */, -1);
    if (found == -1)
        found = find(0x618364 /* "Blue Flag" */, found);
    if (found != -1) {
        fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x6181b0), glob<const char *>(0x6181bc));   // "Fatal error"
        fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
    }
    fn<DWORD(THISCALL *)(void *)>(0x4784e0)(glob<void *>(DEATH_BEHAVIORS));
    fn<DWORD(THISCALL *)(void *)>(0x477220)(glob<void *>(0x6bd760));
    fn<int(THISCALL *)(void *, const char *, int)>(0x591fc0)(glob<void *>(0x6bd760), glob<const char *>(0x610324) /* "Unavoidable" */, -1);
    fn<DWORD(THISCALL *)(void *)>(0x46a940)(glob<void *>(0x6bd140));
    *glob<DWORD *>(0x6c29a0) = category_bit(0x61807c);   // "Player"
    *glob<DWORD *>(0x6c29a8) = category_bit(0x61835c);   // "Enemy"
    *glob<DWORD *>(0x6c29bc) = category_bit(0x61834c);   // "Player Friend"
    BYTE depth[0x40];   // a texture depth object (0x4b1cc0 .. 0x4b1d20)
    fn<void *(THISCALL *)(void *, DWORD)>(0x4b1cc0)(depth, 0);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4b1db0)(depth, field<DWORD>(*glob<void **>(ERROR_REPORTER), 0x24) == 8 ? 0xb : 0xc);
    fn<DWORD(THISCALL *)(void *, void *)>(0x56c300)(glob<void *>(ANIMATED_TEXTURE_LIST), depth);
    fn<DWORD(THISCALL *)(void *, void *)>(0x4b4220)(glob<void *>(0x6c1d50), depth);
    load_c40(ANIMATED_TEXTURE_LIST, 0x618338, 0x618338);   // "Animated Textures"
    load_920(0x6c1d50, 0x61831c /* "Ground Textures" */, 0x61832c /* "Textures" */);
    LoadTxtList_(glob_addr(0x713c80));
    if (ini_int(0x6182f0)) {   // "Pre-load all textures"
        load_all(glob_addr(0x6c1d50));
        load_all(glob_addr(ANIMATED_TEXTURE_LIST));
    }
    fn<DWORD(THISCALL *)(void *, const char *, const char *, const char *, const char *)>(0x57e2f0)(
        glob<void *>(0x6c2480), glob<const char *>(0x6182dc) /* "Voiceovers" */, glob<const char *>(0x6182e8) /* "Levels" */,
        glob<const char *>(0x615658) /* ".wav" */, glob<const char *>(0x615660) /* ".ogg" */);
    fn<DWORD(THISCALL *)(void *, void *)>(0x4b4220)(glob<void *>(0x6c22e0), depth);
    load_920(0x6c22e0, 0x6182b0 /* "User Interface Textures" */, 0x6182c8 /* "Interface/Textures" */);
    fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x4f71a0)(glob<void *>(0x62e7e0), glob<const char *>(0x6182a0) /* "Dialog Scripts" */,
                                                                            glob<const char *>(0x6182e8) /* "Levels" */, 0);
    fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x4f6ea0)(glob<void *>(MODEL_LIST), glob<const char *>(0x618298),
                                                                            glob<const char *>(0x618298), 0);   // "Models"
    ZDefinitionList *models = glob<ZDefinitionList *>(MODEL_LIST);
    if (models->entries) {
        DWORD i = *glob<DWORD *>(0x5fe8f8);
        if (i >= models->num_entries)
            i = 0;
        if (!models->entries[i].loading && !models->entries[i].object)
            fn<void *(THISCALL *)(ZDefinitionList *, DWORD)>(0x55e090)(models, i);
    }
    fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a3800)(fn<void *(__cdecl *)()>(0x4a35d0)(), models, 1);
    fn<DWORD(THISCALL *)(void *)>(0x4b1d80)(depth);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4b1db0)(depth, 0xc);
    fn<DWORD(THISCALL *)(void *, void *)>(0x56c300)(glob<void *>(SEQUENCE_LIST), depth);
    load_c40(SEQUENCE_LIST, 0x61828c, 0x61828c);   // "Sequences"
    if (ini_int(0x618278))   // "Pre-load all models"
        load_all(glob_addr(MODEL_LIST));
    ZString last;
    fn<void *(THISCALL *)(void *, const void *)>(0x4dedf0)(&last, glob<void *>(LAST_FILE));
    zs_assign_text(glob<void *>(LAST_FILE), glob<const char *>(0x61826c));   // "Rules.txt"
    void *rules = fn<void *(__fastcall *)(void *, DWORD, DWORD, void **)>(0x4164c0)(glob<void *>(LAST_FILE), 0, 0, nullptr);
    self->rules = rules && is_kind(rules, class_of(0x560b20)) ? static_cast<BYTE *>(rules) : nullptr;
    fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(glob<void *>(LAST_FILE), &last);
    void *fonts = glob<void *>(0x6be1e0);
    void *courier = fn<void *(THISCALL *)(void *, DWORD)>(0x48d8f0)(fonts, vcall<DWORD>(fonts, 3, glob<const char *>(0x618264) /* "Courier" */, -1));
    void *font = fn<void *(THISCALL *)(void *, DWORD)>(0x48d8f0)(fonts, field<DWORD>(self->rules, 0x1c));
    vcall<DWORD>(self, 11, self->screen_frame, glob<void *>(0x6bf850), glob<void *>(0x6c22e0), font, courier);
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x51e3e0)(self->mixer, 2, field<DWORD>(self->rules, 0x18));
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x51e520)(self->mixer, 2);
    fn<void(THISCALL *)(LayerArray *)>(0x4f6b40)(&self->layers);
    vcall<DWORD>(self, 42, vcall<DWORD>(self, 88));
    vcall<DWORD>(self, 105, self->loading_frames);
    fn<DWORD(THISCALL *)(void *)>(0x578eb0)(app);
    fn<DWORD(THISCALL *)(void *)>(0x5694b0)(self->display);
    void *image = fn<void *(__cdecl *)()>(0x433a80)();
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, void *, DWORD, DWORD)>(0x568660)(self->display, 0, 0, 0, 0, image, 0, 1);
    zs_dtor(&last);
    fn<DWORD(THISCALL *)(void *)>(0x4b1d20)(depth);
}

// ---- CZaxWorld ----------------------------------------------------------------------------------------

// 0x59b7f0 (slot 79 of CZaxWorld): the level cache (the swapped layer file name table, unless it is
// busy), the other map list and the statistics are reset, the chosen game type entry told (slot 23),
// then the base StartNewGame, the error reporter's slot 27 and the flagged resource lists' calls
void THISCALL WorldRest::ZaxStartNewGame(World *self, DWORD a1, const char *map, const char *spawn, DWORD a4)
{
    BYTE *z = reinterpret_cast<BYTE *>(self);
    BYTE *table = field<BYTE *>(z, 0x33c);
    if (!table[0x18] && !table[0x19]) {
        fn<DWORD(THISCALL *)(void *)>(0x58a030)(table);
        if (void *p = field<void *>(table, 0x24)) {
            heap_free(p);
            field<DWORD>(table, 0x24) = 0;
            field<DWORD>(table, 0x2c) = 0;
        }
        field<DWORD>(table, 0x28) = 0;
        if (void *p = field<void *>(table, 0x34)) {
            heap_free(p);
            field<DWORD>(table, 0x34) = 0;
            field<DWORD>(table, 0x3c) = 0;
        }
        field<DWORD>(table, 0x38) = 0;
        fn<DWORD(THISCALL *)(void *)>(0x58ade0)(table + 0x40);
        table[0x18] = 0;
        table[0x19] = 0;
    }
    fn<DWORD(THISCALL *)(void *)>(0x41f660)(field<void *>(z, 0x354));
    void *stats = field<void *>(z, 0x34c);
    const DWORD keep = field<DWORD>(stats, 0x28);
    vcall<DWORD>(stats, 19);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x5bb780)(field<void *>(z, 0x34c), keep);
    const DWORD gt = field<DWORD>(z, 0x58c);
    if (gt != 0xffffffff)
        vcall<DWORD>(list_entry(glob<ZDefinitionList *>(GAME_TYPE_LIST), gt), 23);
    fn<void(THISCALL *)(World *, DWORD, const char *, const char *, DWORD)>(0x4f4e70)(self, a1, map, spawn, a4);
    vcall<DWORD>(*glob<void **>(ERROR_REPORTER), 27, 1, 1);
    fn<DWORD(THISCALL *)(void *)>(0x4a3760)(fn<void *(__cdecl *)()>(0x4a35d0)());
}

// ---- The layer -----------------------------------------------------------------------------------------

// 0x4ed350: the part drawn at a point: the parts of the layer's grid around it (a query of the given kind
// with a5 | 0x1000000) fill the draw queues (slot 23 of every part that is no free range poly
// and not of the skipped class, active and visible: 0x20000 and 0x800000), the queues are sorted and
// asked from the top (entry slot 4); otherwise the free range polys answer (slot 38). queues: 0 both, 1
// the ground, 2 the objects. eax: the part, 0 when none.
void *THISCALL WorldRest::PartAt(Layer *self, const int *point, DWORD queues, CClassInfo *skip, DWORD a4, DWORD a5, DWORD a6)
{
    const int x = point[0], y = point[1];
    int rect[4] = {x, y, x + 1, y + 1};
    LayerArray *list;
    fn<void(THISCALL *)(LayerArray **)>(0x59a470)(&list);
    fn<DWORD(THISCALL *)(void *, LayerArray *, int *, DWORD, DWORD, DWORD, DWORD)>(0x57a770)(reinterpret_cast<BYTE *>(self) + 0x50, list, rect,
                                                                                         queues, a4, a5 | 0x1000000, a6);
    fn<DWORD(THISCALL *)(LayerArray *)>(0x59a2d0)(list);
    if (queues == 0 || queues == 1)
        fn<DWORD(THISCALL *)(void *)>(0x49ccb0)(glob<void *>(DRAW_QUEUE_GROUND));
    if (queues == 0 || queues == 2)
        fn<DWORD(THISCALL *)(void *)>(0x49ccb0)(glob<void *>(DRAW_QUEUE_OBJECTS));
    const int n = (int)list->count;
    for (int i = 0; i < n; i++) {
        void *p = list->data[i];
        if (is_kind(p, class_of(CI_FREE_RANGE_POLY)))
            continue;
        p = list->data[i];
        if (skip && is_kind(p, skip))
            continue;
        const DWORD flags = field<DWORD>(p, 0x1c);
        if (!(flags & 0x20000) || !(flags & 0x800000))
            continue;
        DWORD extra[3] = {0, 0, 0};
        if (queues == 0)
            vcall<DWORD>(p, 23, glob<void *>(DRAW_QUEUE_GROUND), glob<void *>(DRAW_QUEUE_OBJECTS), rect, 0, extra);
        else if (queues == 1)
            vcall<DWORD>(p, 23, glob<void *>(DRAW_QUEUE_GROUND), nullptr, rect, 0, extra);
        else if (queues == 2)
            vcall<DWORD>(p, 23, nullptr, glob<void *>(DRAW_QUEUE_OBJECTS), rect, 0, extra);
    }
    auto ask = [&](DWORD queue) -> void * {
        fn<DWORD(THISCALL *)(void *)>(0x49ca40)(glob_dyn<void *>(queue));
        for (int k = (int)fn<DWORD(THISCALL *)(void *)>(0x482de0)(glob_dyn<void *>(queue)) - 1; k >= 0; k--) {
            BYTE *e = fn<BYTE *(THISCALL *)(void *, int)>(0x566e40)(glob_dyn<void *>(queue), k);
            if ((BYTE)vcall<DWORD>(e, 4, x, y))
                return e;
        }
        return nullptr;
    };
    void *hit = nullptr;
    if (queues == 0 || queues == 2)
        hit = ask(glob_addr(DRAW_QUEUE_OBJECTS));
    if (!hit && (queues == 0 || queues == 1))
        hit = ask(glob_addr(DRAW_QUEUE_GROUND));
    if (hit) {
        void *part = field<void *>(hit, 0x10);
        fn<void(THISCALL *)(LayerArray **)>(0x59a510)(&list);
        return part;
    }
    for (int i = 0; i < n; i++) {
        void *p = list->data[i];
        if (skip && is_kind(p, skip))
            continue;
        if (!is_kind(p, class_of(CI_FREE_RANGE_POLY)))
            continue;
        if ((BYTE)vcall<DWORD>(p, 38, x, y, queues)) {
            fn<void(THISCALL *)(LayerArray **)>(0x59a510)(&list);
            return p;
        }
    }
    fn<void(THISCALL *)(LayerArray **)>(0x59a510)(&list);
    return nullptr;
}

// 0x4ed960: makes this layer the one whose ambient sounds play (0x6c1c70): the old layer's sounds
// (+0xf4/+0xf8, objects with the sound at +8) are stopped (slot 11), this layer's started: at their
// position (y negated) when they have an object (+0x10), else plain (slot 2); +0x1a marks them playing
void THISCALL WorldRest::SetSoundLayer(Layer *self)
{
    BYTE *old = *glob<BYTE **>(SOUND_LAYER);
    if (old == reinterpret_cast<BYTE *>(self))
        return;
    if (old) {
        const DWORD n = field<DWORD>(old, 0xf8);
        for (DWORD i = 0; n && i < n; i++) {
            BYTE *s = field<BYTE **>(old, 0xf4)[i];
            if (!s)
                continue;
            if (void *snd = field<void *>(s, 8))
                vcall<DWORD>(snd, 11);
            s[0x1a] = 0;
        }
    }
    *glob<Layer **>(SOUND_LAYER) = self;
    if (!self)
        return;
    BYTE *l = reinterpret_cast<BYTE *>(self);
    const DWORD n = field<DWORD>(l, 0xf8);
    for (DWORD i = 0; n && i < n; i++) {
        BYTE *s = field<BYTE **>(l, 0xf4)[i];
        if (!s || !field<void *>(s, 8))
            continue;
        if (field<void *>(s, 0x14) == *glob<void **>(SOUND_LAYER)) {
            if (void *o = field<void *>(s, 0x10)) {
                float pos[2];
                fn<void(THISCALL *)(void *, float *)>(0x4fb0a0)(o, pos);
                DWORD out[3];   // x, 0, -y
                out[0] = fbits(fstp(fld(pos[0])));
                out[2] = fbits(fstp(-fld(pos[1])));
                out[1] = 0;
                fn<DWORD(THISCALL *)(void *, DWORD *, DWORD)>(0x57b290)(field<void *>(s, 8), out, 0);
            } else {
                vcall<DWORD>(field<void *>(s, 8), 2, 0);
            }
        }
        s[0x1a] = 1;
    }
}

// 0x4e96b0 (slot 1 of the sweep vtable 0x5f8ba8): accepts every part. al: 1.
DWORD THISCALL WorldRest::SweepAccepts(void *, void *, void *) { return 1; }

// 0x4e9920: CLayer's class getter (registered on first use: version 1, 0x1d0 bytes, parent
// COffsetClippedProxyFrame)
CClassInfo *__cdecl WorldRest::LayerClassInfo()
{
    CClassInfo *&v = *glob<CClassInfo **>(0x6c1d04);
    if (!v)
        if (void *p = pool_alloc(0x48)) {
            CClassInfo *parent = fn<CClassInfo *(__cdecl *)()>(0x524450)();
            fn<CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, void *, void *, void *)>(0x415780)(
                p, &v, glob<const char *>(0x617b74), 1.0f, parent, 0x1d0, 0, code<void *>(0x4e99b0), code<void *>(0x4e9a20), code<void *>(0x4e9cf0));
        }
    return v;
}

// 0x4fc8f0: the part's layer (+0x48), passed down its child list (+8: data +0xc, count +0x10)
void THISCALL WorldRest::SetLayer(void *part, void *layer)
{
    field<void *>(part, 0x48) = layer;
    BYTE *list = field<BYTE *>(part, 8);
    if (!list)
        return;
    for (DWORD i = 0; i < field<DWORD>(list, 0x10); i++) {
        if (void *c = field<void **>(list, 0xc)[i])
            fn<void(THISCALL *)(void *, void *)>(0x4fc8f0)(c, layer);
        list = field<BYTE *>(part, 8);
    }
}

// ---- The collide checker ---------------------------------------------------------------------------------

// 0x535dc0 (slot 23): whether the line between two integer points is blocked for the mover: a point sweep
// (0x545a20 with notify 0 and the second byte set) through the layer (0x4e8d40) that ignores parts the
// filter 0x535e70 rejects, with the mover's flag 0x2000000 cleared meanwhile. al: the sweep's answer.
DWORD THISCALL WorldRest::IsBlocked(void *, void *mover, const int *from, const int *to)
{
    float a[2], b[2];
    a[0] = fstp(fild(from[0]));
    a[1] = fstp(fild(from[1]));
    b[0] = fstp(fild(to[0]));
    b[1] = fstp(fild(to[1]));
    Sweep sweep;
    fn<void *(THISCALL *)(Sweep *, const float *, const float *, DWORD, DWORD)>(0x545a20)(&sweep, a, b, 0, 1);
    sweep.b1d = 1;
    const DWORD keep = field<DWORD>(mover, 0x1c) & 0x2000000;
    vcall<DWORD>(mover, 58, 0x2000000);
    BYTE *layer = field<BYTE *>(mover, 0x48);
    const BYTE r = (BYTE)fn<DWORD(THISCALL *)(void *, const float *, Sweep *, void *, void *, DWORD, DWORD, void *, DWORD)>(0x4e8d40)(
        layer + 0x3c, a, &sweep, nullptr, nullptr, 0, 0, mover, fn_addr(0x535e70));
    vcall<DWORD>(mover, 59, 0x2000000);
    vcall<DWORD>(mover, 58, keep);
    return r;
}

// 0x535eb0 (slot 24): the cost factor of a line for the mover: 1 when nothing blocks it (a sweep of the
// mover's shape, slot 48, through the layer, 0x4e8e00), 0 when even the filtered sweep (0x536010) is
// blocked, else 5. st(0).
long double THISCALL WorldRest::Cost(void *, void *mover, const int *from, const int *to)
{
    float pos[2], off[2];
    fn<void(THISCALL *)(void *, float *)>(0x4fb0a0)(mover, pos);
    vcall<DWORD>(mover, 43, off);
    // The start and end shifted by the mover's position less its offset: x kept on the FPU, y stored as a
    // float first; the start's x stays unrounded, its y and the end's are rounded before the subtraction
    const LD dx = fld(pos[0]) - fld(off[0]);
    const float dy = fstp(fld(pos[1]) - fld(off[1]));
    const LD fx = fild(from[0]);
    const float fy = fstp(fild(from[1]));
    const float tx = fstp(fild(to[0]));
    const float ty = fstp(fild(to[1]));
    float a[2], b[2];
    a[0] = fstp(fx - dx);
    a[1] = fstp(fld(fy) - fld(dy));
    b[0] = fstp(fld(tx) - dx);
    b[1] = fstp(fld(ty) - fld(dy));
    Sweep sweep;
    fn<void *(THISCALL *)(Sweep *, const float *, const float *, DWORD, DWORD)>(0x545a20)(&sweep, a, b, 0, 1);
    const DWORD keep = field<DWORD>(mover, 0x1c) & 0x2000000;
    vcall<DWORD>(mover, 58, 0x2000000);
    auto sweep_shape = [&](BYTE *layer, DWORD filter) {
        void *shape = vcall<void *>(mover, 48);
        return (BYTE)fn<DWORD(THISCALL *)(void *, void *, const float *, Sweep *, void *, void *, void *, DWORD, DWORD, DWORD)>(0x4e8e00)(
            layer + 0x3c, shape, a, &sweep, nullptr, nullptr, mover, 0, 0, filter);
    };
    const BYTE blocked = sweep_shape(field<BYTE *>(mover, 0x48), 0);
    vcall<DWORD>(mover, 59, 0x2000000);
    vcall<DWORD>(mover, 58, keep);
    if (!blocked)
        return cst(glob_addr(C_ONE));
    if (sweep_shape(field<BYTE *>(mover, 0x48), fn_addr(0x536010)))
        return cst(glob_addr(C_ZERO));
    return cst(glob_addr(C_FIVE));
}

// 0x536030 (slot 25): whether the mover may stand at a position: no blocker of the layer there
// (0x4eb050 with all 0x840000, none 0x2000000) or only one of class 0x49a930. al.
DWORD THISCALL WorldRest::CanStand(void *, void *mover, const float *pos)
{
    void *b = fn<void *(THISCALL *)(void *, void *, const float *, DWORD, DWORD, DWORD)>(0x4eb050)(field<void *>(mover, 0x48), mover, pos, 0,
                                                                                              0x840000, 0x2000000);
    if (b && is_kind(b, class_of(CI_49A930)))
        return 0;
    return 1;
}

// 0x535e70 (filter of IsBlocked): passes parts that are not of class 0x4903f0 and have no name or an
// empty one. al.
DWORD __fastcall WorldRest::IgnoreUnnamed(void *, void *part)
{
    if (is_kind(part, class_of(CI_4903F0)))
        return 0;
    if (part_name(part) && *part_name(part))
        return 0;
    return 1;
}

// 0x536010 (filter of Cost): passes parts not of class 0x49a930. al.
DWORD __fastcall WorldRest::IgnoreNot49a930(void *, void *part) { return !is_kind(part, class_of(CI_49A930)); }

// ---- Small helpers ------------------------------------------------------------------------------------

// 0x548430: scales a 2D vector to length 1 (nothing for length 0)
void THISCALL WorldRest::Normalize(float *v)
{
    const LD len = fsqrt(fld(v[0]) * fld(v[0]) + fld(v[1]) * fld(v[1]));
    if (c3(len, cst(glob_addr(C_ZERO))))
        return;
    const LD k = cst(glob_addr(C_ONE)) / len;
    v[0] = fstp(k * fld(v[0]));
    v[1] = fstp(k * fld(v[1]));
}

// 0x548470: out = a + b (2D). eax: a.
const float *THISCALL WorldRest::AddVectors(float *out, const float *a, const float *b)
{
    out[0] = fstp(fld(a[0]) + fld(b[0]));
    out[1] = fstp(fld(a[1]) + fld(b[1]));
    return a;
}

// 0x518260: a resource handle (vtable 0x5faea4, no resource). eax: the handle.
void *THISCALL WorldRest::ResourceHandleCtor(DWORD *handle)
{
    handle[0] = vt(0x5faea4);
    handle[1] = 0;
    return handle;
}

// 0x4b5c60: CFreeRangePoly's class getter (version 3, 0x74 bytes, parent CLayerPart)
CClassInfo *__cdecl WorldRest::FreeRangePolyClassInfo()
{
    CClassInfo *&v = *glob<CClassInfo **>(0x6bfb08);
    if (!v)
        if (void *p = pool_alloc(0x48)) {
            CClassInfo *parent = fn<CClassInfo *(__cdecl *)()>(0x4f9bf0)();
            fn<CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, void *, void *, void *)>(0x415780)(
                p, &v, glob<const char *>(0x613e94), 3.0f, parent, 0x74, 0, code<void *>(0x4b5d50), code<void *>(0x4b5d90), code<void *>(0x4b5f00));
        }
    return v;
}

ZAX_HOOKS(world_rest)
{
    using W = WorldRest;
    const Hook list[] = {
#include "world_rest_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
