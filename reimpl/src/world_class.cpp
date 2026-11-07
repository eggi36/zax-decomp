// The world class of Zax.exe reimplemented (see world_class.h).
#include "world_class.h"
#include "dir_list.h"

namespace zax {

namespace {

using LD = long double;

// ---- Functions and constants of the exe; through their exe addresses so Disable= applies -------------

float kf(GlobKey address) { return *glob<const float *>(address); }
void *world() { return *glob<void **>(G_WORLD); }
void *app() { return *glob<void **>(0x713f14); }

void *crt_memmove(void *d, const void *s, DWORD n) { return fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
int crt_stricmp_(const char *a, const char *b) { return fn<int(__cdecl *)(const char *, const char *)>(0x5d3640)(a, b); }
char *crt_strrchr(char *s, int c) { return fn<char *(__cdecl *)(char *, int)>(0x5d0d90)(s, c); }
DWORD text_length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}

const char *zs_cstr(void *z) { return fn<const char *(THISCALL *)(void *)>(0x4e13a0)(z); }
DWORD zs_not_empty(void *z) { return fn<DWORD(THISCALL *)(void *)>(0x4e13e0)(z); }
void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_copy(void *z, const void *o) { fn<void *(THISCALL *)(void *, const void *)>(0x4dedf0)(z, o); }
void zs_dtor(void *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
void zs_assign(void *z, const void *o) { fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(z, o); }
void zs_assign_text(void *z, const char *s) { fn<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }
char *zs_buffer(void *z, DWORD n) { return fn<char *(THISCALL *)(void *, DWORD)>(0x4e1590)(z, n); }

void section_begin(GlobKey text) { fn<void(__fastcall *)(const char *)>(0x4a2650)(glob<const char *>(text)); }
void section_end() { fn<void(__cdecl *)()>(0x4a2670)(); }
DWORD set_extension(char *path, GlobKey ext) { return fn<DWORD(__fastcall *)(char *, const char *)>(0x4a3fb0)(path, glob<const char *>(ext)); }
DWORD path_join(char *out, GlobKey dir, const char *name) { return fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, glob<const char *>(dir), name); }
DWORD ini_int(GlobKey name, DWORD def)
{
    return fn<DWORD(__fastcall *)(DWORD, DWORD, const char *, DWORD)>(0x4a4670)(glob_addr(0x6bd330), glob_addr(0x6bd430), glob<const char *>(name), def);
}

BYTE is_kind(void *o, CClassInfo *c) { return (BYTE)fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
CClassInfo *class_49a930() { return fn<CClassInfo *(__cdecl *)()>(0x49a930)(); }
CClassInfo *class_49b810() { return fn<CClassInfo *(__cdecl *)()>(0x49b810)(); }
CClassInfo *class_entity() { return fn<CClassInfo *(__cdecl *)()>(0x487620)(); }
CClassInfo *class_420950() { return fn<CClassInfo *(__cdecl *)()>(0x420950)(); }

Layer *new_layer(void *frame)
{
    void *m = pool_alloc(0x1d0);
    return m ? fn<Layer *(THISCALL *)(void *, void *)>(0x4e9e00)(m, frame) : nullptr;
}
const char *layer_file_name(Layer *l) { return fn<const char *(THISCALL *)(Layer *)>(0x4eb670)(l); }
DWORD is_listed(Layer *l, void *part) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x579e70)(l->data.grid, part, 1); }
void part_position(void *part, float *out) { fn<void(THISCALL *)(void *, float *)>(0x4fb0a0)(part, out); }
// Floats are passed by their bits (a float argument could go through an x87 load that quiets NaNs)
DWORD fbits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}
void part_move_center(void *part, DWORD x, DWORD y) { fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x4fb680)(part, x, y); }

// fstp dword: rounded to float through memory
float fstp_float(LD v)
{
    float r;
    __asm__ volatile("fstps %0" : "=m"(r) : "t"(v) : "st");
    return r;
}
double fstp_double(LD v)
{
    double r;
    __asm__ volatile("fstpl %0" : "=m"(r) : "t"(v) : "st");
    return r;
}

// The player records of the net layer (slot 44: its slot 23 is player i, slot 11 the count)
void *players(World *w) { return vcall<void *>(w, 44); }
BYTE *player(World *w, DWORD i) { return vcall<BYTE *>(players(w), 23, i); }
DWORD player_count(void *p) { return vcall<DWORD>(p, 11); }

// A layer list index as the inlined accessor reads it: -1, past the count or negative give null
Layer *layer_at(World *w, int i)
{
    if (i == -1 || i >= (int)w->layers.count || i < 0)
        return nullptr;
    return static_cast<Layer *>(w->layers.data[i]);
}

// Removes entry i of a pointer array by moving the rest down (the original's memmove with the size
// computed as (count + (i + 1) * 0x3fffffff) * 4)
void array_remove_at(LayerArray *a, DWORD i)
{
    const DWORD count = a->count;
    if (i < count) {
        const DWORD next = i + 1;
        if (next < count)
            crt_memmove(&a->data[i], &a->data[next], (next * 0x3fffffff + count) * 4);
        a->count--;
    }
}

// The index of a pointer in the layer list, -1 for null or a pointer not in it (the inlined search; its
// "last element" shortcut compares the count with -1 unsigned and never runs)
int layer_index_of(World *w, void *layer)
{
    if (!layer)
        return -1;
    for (DWORD i = 0; i < w->layers.count; i++)
        if (w->layers.data[i] == layer)
            return (int)i;
    return -1;
}

// Deletes the player characters from the last (slot 103 takes each out of its layer first); the list
// keeps its count
void delete_characters(World *w)
{
    for (int i = (int)w->characters.count - 1; i >= 0; i--) {
        void *c = w->characters.data[i];
        void *l = *reinterpret_cast<void **>(static_cast<BYTE *>(c) + 0x48);
        if (l)
            vcall<DWORD>(w, 103, c, l);
        c = w->characters.data[i];
        if (c)
            vcall<void *>(c, 1, 1);
    }
}

// Deletes every layer from the last and empties the list
void delete_layers(World *w)
{
    while (w->layers.count) {
        const DWORD i = w->layers.count - 1;
        void *l = w->layers.data[i];
        if (l)
            vcall<void *>(l, 1, 1);
        array_remove_at(&w->layers, i);
    }
}

void begin_loading(World *w)
{
    if (!w->loading) {
        if (w->game_type && (BYTE)vcall<DWORD>(w->game_type, 12))
            fn<void(__cdecl *)()>(0x58ea40)();   // an empty function
        *glob<BYTE *>(LOADING_SCREEN) = 1;
        void *frame = fn<void *(THISCALL *)(World *)>(0x4f3d50)(w);
        fn<void(THISCALL *)(void *)>(0x579030)(frame);
        fn<void(__cdecl *)()>(0x4f3ba0)();
        fn<void(__cdecl *)()>(0x4f3ba0)();
        fn<void(__cdecl *)()>(0x4f3ba0)();
        *glob<DWORD *>(LOADING_STEPS) = 0;
        fn<void(__fastcall *)(DWORD)>(0x4163a0)(fn_addr(0x4f3ba0));   // the read hook: the loading screen's step
    }
    w->loading++;
}

void end_loading(World *w)
{
    if (--w->loading == 0) {
        *glob<BYTE *>(LOADING_SCREEN) = 0;
        fn<void(__fastcall *)(DWORD)>(0x4163a0)(0);
    }
}

// The packet writers of net_session: the getters return the reader (the original pushes the packet before
// calling them; they leave it for the writer)
void send_block(FnKey getter, void *packet)
{
    void *reader = fn<void *(__cdecl *)()>(getter)();
    fn<DWORD(THISCALL *)(void *, void *)>(0x5ac330)(reader, packet);
}

// The explosion block to every player that sees the point (player 0 always): the player record, then
// the seven values (0x4f8a40)
void broadcast_explosion(World *self, DWORD a1, DWORD a2, DWORD x, DWORD y, DWORD a5, DWORD a6, DWORD a7)
{
    void *p = vcall<void *>(world(), 44);
    for (DWORD i = 0; i < player_count(p); i++) {
        DWORD pos[2] = {x, y};
        if (i == 0 || (BYTE)vcall<DWORD>(self, 53, i, pos)) {
            void *pl = vcall<void *>(p, 23, i);
            void *reader = fn<void *(__cdecl *)()>(0x4f8990)();
            fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4f8a40)(reader, pl, a1, a2, x, y, a5, a6,
                                                                                                         a7);
        }
    }
}

}  // namespace

// ---- Constructor and destructor ----------------------------------------------------------------------------

World *THISCALL World::Ctor(World *self, DWORD a1, DWORD a2, DWORD a3)
{
    fn<void *(THISCALL *)(World *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4cd780)(self, 0x3c360b61, 0x3d088889, 0x3e2aaaab, a2,
                                                                                            a1, glob_addr(0x618194), a3);   // "Startup.wav"
    BYTE *holder = reinterpret_cast<BYTE *>(&self->holder_vftable);
    fn<void *(THISCALL *)(void *)>(0x416610)(holder);
    self->characters.vftable = vt(VT_CHARACTER_ARRAY);
    self->characters.data = nullptr;
    self->characters.count = 0;
    self->characters.capacity = 0;
    self->holder_vftable = vt(VT_CHARACTER_HOLDER);
    fn<void(THISCALL *)(void *)>(0x4173c0)(holder);
    BYTE *lh = reinterpret_cast<BYTE *>(self->layer_holder);
    fn<void *(THISCALL *)(void *)>(0x416610)(lh);
    self->layers.data = nullptr;
    self->layers.count = 0;
    self->layers.capacity = 0;
    self->layers.vftable = vt(VT_LAYER_LIST);
    self->layer_holder[0] = vt(VT_LAYER_HOLDER);
    fn<void(THISCALL *)(void *)>(0x4173c0)(lh);
    reinterpret_cast<DWORD *>(&self->playing_time)[0] = 0;
    reinterpret_cast<DWORD *>(&self->running_time)[0] = 0;
    reinterpret_cast<DWORD *>(&self->playing_time)[1] = 0;
    reinterpret_cast<DWORD *>(&self->running_time)[1] = 0;
    self->unique_id = 0;
    self->saved_characters.data = nullptr;
    self->saved_characters.count = 0;
    self->saved_characters.capacity = 0;
    self->saved_characters.vftable = vt(VT_SAVED_CHARACTERS);
    self->vftable = vt(VT_WORLD);
    self->saving = 0;
    self->loading = 0;
    fn<void(__fastcall *)(DWORD)>(0x419ea0)(fn_addr(0x4f0ac0));   // the Properties.txt path of a model (static list 0x62e6a8)
    self->traffic_graph = nullptr;
    *glob<World **>(G_WORLD) = self;
    self->game_type = nullptr;
    void *m = pool_alloc(0x34);
    self->mixer = m ? fn<void *(THISCALL *)(void *)>(0x51e360)(m) : nullptr;
    self->rules = nullptr;
    self->byte2f8 = 0;
    self->show_polygons = ini_int(0x618184, 0) != 0;   // "Show Polygons"
    self->show_grid = ini_int(0x618178, 0) != 0;       // "Show Grid"
    self->byte312 = 0;
    m = pool_alloc(0x1c);
    self->file_buffer = m ? fn<void *(THISCALL *)(void *)>(0x4a1780)(m) : nullptr;
    m = pool_alloc(0x20);
    void *stream = m ? fn<void *(THISCALL *)(void *)>(0x505f00)(m) : nullptr;
    fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1970)(self->file_buffer, stream, 0);
    m = pool_alloc(0x40);
    self->screen_frame = m ? fn<void *(THISCALL *)(void *)>(0x4a8a90)(m) : nullptr;
    void *display = self->display;
    const DWORD depth = fn<DWORD(__cdecl *)()>(0x55d030)();
    const DWORD height = vcall<DWORD>(display, 39);
    const DWORD width = vcall<DWORD>(display, 45);
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(self->screen_frame, width, height, depth);
    m = pool_alloc(0x40);
    self->zoom_frame = m ? fn<void *(THISCALL *)(void *)>(0x4a8a90)(m) : nullptr;
    const int h = (int)vcall<DWORD>(self->display, 39);
    self->view[2] = (int)vcall<DWORD>(self->display, 45);
    self->view[0] = 0;
    self->view[1] = 0;
    self->view[3] = h - 0x38;
    m = heap_alloc(0x1e84);
    self->timers = m ? fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x506730)(m, 0xb4, 0x41) : nullptr;
    reinterpret_cast<DWORD *>(&self->mission_time)[0] = 0;
    self->loading_frames[0] = nullptr;
    self->loading_frames[1] = nullptr;
    self->loading_frames[2] = nullptr;
    self->running = 0;
    self->level_loaded = 0;
    reinterpret_cast<DWORD *>(&self->mission_time)[1] = 0;
    self->loading_frames[3] = nullptr;
    return self;
}

void THISCALL World::Dtor(World *self)
{
    self->vftable = vt(VT_WORLD);
    fn<void(THISCALL *)(DWORD)>(0x57e240)(glob_addr(0x6c2480));
    if (self->rules) {
        vcall<void *>(self->rules, 1, 1);
        self->rules = nullptr;
    }
    // The player characters still in a layer are taken out of it (into the saved list)
    for (DWORD li = 0; li < self->layers.count; li++) {
        Layer *l = static_cast<Layer *>(self->layers.data[li]);
        if (!l)
            continue;
        const DWORD n = self->characters.count;
        vcall<DWORD>(&self->saved_characters, 1);
        for (DWORD j = 0; j < n; j++) {
            void *c = self->characters.data[j];
            if ((BYTE)is_listed(l, c)) {
                vcall<DWORD>(self, 103, c, l);
                vcall<DWORD>(&self->saved_characters, 2, &c);
            }
        }
    }
    delete_characters(self);
    if (void *g = self->traffic_graph) {
        fn<void(THISCALL *)(void *)>(0x51f960)(g);
        heap_free(g);
        self->traffic_graph = nullptr;
    }
    for (void *&f : self->loading_frames)
        if (f) {
            vcall<void *>(f, 1, 1);
            f = nullptr;
        }
    if (void *t = self->timers) {
        fn<void(THISCALL *)(void *)>(0x5067b0)(t);
        heap_free(t);
        self->timers = nullptr;
    }
    if (self->screen_frame) {
        vcall<void *>(self->screen_frame, 1, 1);
        self->screen_frame = nullptr;
    }
    if (self->zoom_frame) {
        vcall<void *>(self->zoom_frame, 1, 1);
        self->zoom_frame = nullptr;
    }
    if (self->file_buffer) {
        vcall<void *>(self->file_buffer, 0, 1);
        self->file_buffer = nullptr;
    }
    if (fn<void *(THISCALL *)(World *)>(0x4cdf30)(self))
        vcall<DWORD>(fn<void *(THISCALL *)(World *)>(0x4cdf30)(self), 7);
    if (self->mixer) {
        vcall<void *>(self->mixer, 1, 1);
        self->mixer = nullptr;
    }
    delete_layers(self);
    *glob<World **>(G_WORLD) = nullptr;
    if (self->game_type) {
        vcall<void *>(self->game_type, 0, 1);
        self->game_type = nullptr;
    }
    // The embedded arrays and the two holders
    self->saved_characters.vftable = vt(VT_SAVED_CHARACTERS_BASE);
    if (self->saved_characters.data) {
        pool_get_free(self->saved_characters.data, self->saved_characters.capacity * 4);
        self->saved_characters.data = nullptr;
        self->saved_characters.capacity = 0;
    }
    BYTE *lh = reinterpret_cast<BYTE *>(self->layer_holder);
    self->saved_characters.count = 0;
    self->layer_holder[0] = vt(VT_LAYER_HOLDER);
    fn<void(THISCALL *)(void *)>(0x4173f0)(lh);
    self->layers.vftable = vt(VT_LAYER_LIST_BASE);
    if (self->layers.data) {
        pool_get_free(self->layers.data, self->layers.capacity * 4);
        self->layers.data = nullptr;
        self->layers.capacity = 0;
    }
    self->layers.count = 0;
    fn<void(THISCALL *)(void *)>(0x4166d0)(lh);
    BYTE *holder = reinterpret_cast<BYTE *>(&self->holder_vftable);
    self->holder_vftable = vt(VT_CHARACTER_HOLDER);
    for (int i = (int)self->characters.count - 1; i >= 0; i--) {
        fn<void(THISCALL *)(void *, void *, DWORD, DWORD)>(0x4fc200)(self->characters.data[i], holder, 0xffffffff, glob_addr(0x62e558));
        array_remove_at(&self->characters, (DWORD)i);
    }
    fn<void(THISCALL *)(void *)>(0x4173f0)(holder);
    self->characters.vftable = vt(VT_CHARACTER_ARRAY_BASE);
    if (self->characters.data) {
        heap_free(self->characters.data);
        self->characters.data = nullptr;
        self->characters.capacity = 0;
    }
    self->characters.count = 0;
    fn<void(THISCALL *)(void *)>(0x4166d0)(holder);
    fn<void(THISCALL *)(World *)>(0x4cdcc0)(self);
}

World *THISCALL World::DeletingDtor(World *self, DWORD flags)
{
    fn<void(THISCALL *)(World *)>(0x4f0c70)(self);
    if (flags & 1)
        heap_free(self);
    return self;
}

// ---- Layers ------------------------------------------------------------------------------------------------

Layer *THISCALL World::CurrentLayer(World *self)
{
    if (!self->layers.count)
        return nullptr;
    return layer_at(self, *reinterpret_cast<int *>(player(self, 0) + 0xdc));
}

Layer *THISCALL World::PlayerLayer(World *self, DWORD i) { return layer_at(self, *reinterpret_cast<int *>(player(self, i) + 0xdc)); }

Layer *THISCALL World::FindLayer(World *self, const char *name)
{
    Layer *found = nullptr;
    const DWORD n = text_length(name) + 1;
    char *copy = static_cast<char *>(heap_alloc(n));
    memcpy(copy, name, n);
    if (char *dot = crt_strrchr(copy, '.'))
        *dot = 0;
    for (DWORD i = 0; i < self->layers.count && !found; i++) {
        Layer *l = static_cast<Layer *>(self->layers.data[i]);
        if (l && crt_stricmp_(layer_file_name(l), copy) == 0)
            found = static_cast<Layer *>(self->layers.data[i]);
    }
    if (copy)
        heap_free(copy);
    return found;
}

void THISCALL World::AddLayer(World *self, Layer *layer)
{
    if (layer)
        for (DWORD i = 0; i < self->layers.count; i++)
            if (self->layers.data[i] == layer)
                return;
    for (DWORD i = 0; i < self->layers.count; i++)
        if (!self->layers.data[i]) {
            self->layers.data[i] = layer;
            return;
        }
    vcall<DWORD>(&self->layers, 2, &layer);
}

void THISCALL World::UnloadLayer(World *self, Layer *layer)
{
    const int i = layer_index_of(self, layer);
    self->layers.data[i] = nullptr;   // not found: the dword before the data (as in the original)
    for (int k = (int)self->layers.count - 1; k >= 0; k--) {
        if (self->layers.data[k])
            break;
        array_remove_at(&self->layers, (DWORD)k);
    }
    if (layer)
        vcall<void *>(layer, 1, 1);
}

Layer *THISCALL World::LoadLayer(World *self, const char *name, DWORD a2)
{
    char path[256];
    const DWORD found = vcall<DWORD>(self, 68, name, a2, path);
    Layer *l = new_layer(self->screen_frame);
    if (!(BYTE)vcall<DWORD>(self, 77, l, path, name, found & 0xff) && l) {
        vcall<void *>(l, 1, 1);
        return nullptr;
    }
    return l;
}

DWORD THISCALL World::SaveLayer(World *self, Layer *layer, DWORD a2, DWORD mode)
{
    char path[256];
    const char *name = layer_file_name(layer);
    const DWORD found = vcall<DWORD>(self, 68, name, a2, path);
    if (!a2)
        fn<DWORD(__fastcall *)(const char *)>(0x4a1540)(path);
    return vcall<DWORD>(self, 75, layer, path, mode, found & 0xff);
}

DWORD THISCALL World::LevelExists(World *self, const char *name)
{
    char path[256];
    vcall<DWORD>(self, 68, name, 0, path);
    return fn<DWORD(__fastcall *)(const char *, DWORD)>(0x4a1570)(path, 0);
}

DWORD THISCALL World::LevelPath(World *, const char *name, DWORD, char *out)
{
    char path[256];
    memcpy(path, name, text_length(name) + 1);
    set_extension(path, 0x61857c);   // ".zax"
    path_join(out, 0x6182e8, path);   // "Levels"
    return (set_extension(out, 0x61857c) & ~0xffu) | 1;
}

DWORD THISCALL World::SaveLayerFile(World *self, Layer *layer, const char *path, DWORD mode, DWORD)
{
    vcall<DWORD>(self, 69, layer, &self->saved_characters);
    const DWORD r = fn<DWORD(THISCALL *)(Layer *, const char *, DWORD)>(0x4ebe30)(layer, path, mode);
    const DWORD e = vcall<DWORD>(self, 70, layer, &self->saved_characters, 0);
    return (e & ~0xffu) | (r & 0xff);
}

DWORD THISCALL World::LoadLayerStream(World *self, Layer *layer, void *stream, DWORD fix, const char *name)
{
    section_begin(0x6185a4);   // "LoadNewLayerFP"
    ZString last;
    zs_copy(&last, glob<void *>(LAST_FILE));
    if (!(BYTE)fn<DWORD(THISCALL *)(Layer *, void *)>(0x4ec090)(layer, stream)) {
        zs_dtor(&last);
        return 0;
    }
    self->level_loaded = 1;
    zs_assign(glob<void *>(LAST_FILE), &last);
    if ((BYTE)fix) {
        fn<void(THISCALL *)(World *, Layer *)>(0x4f5c80)(self, layer);
        fn<void(THISCALL *)(Layer *, const char *)>(0x4eb6b0)(layer, name);
    }
    if (vcall<void *>(self, 1) && (BYTE)vcall<DWORD>(vcall<void *>(self, 1), 13))
        fn<void(THISCALL *)(Layer *)>(0x4eda60)(layer);
    section_end();
    zs_dtor(&last);
    return 1;
}

DWORD THISCALL World::LoadLayerFile(World *self, Layer *layer, const char *path, const char *name, DWORD fix)
{
    struct {
        Layer *layer;
        const char *path;
        ZString name;   // the name without its extension
        BYTE fix;
    } info;
    alignas(4) BYTE stream[0x24];   // the object stream (constructor 0x5240b0)
    zs_ctor(&info.name);
    info.layer = layer;
    info.path = path;
    zs_assign_text(&info.name, name);
    set_extension(zs_buffer(&info.name, 0), 0x62e558);
    info.fix = (BYTE)fix;
    *glob<void **>(LOADING_FILE) = &info;
    BYTE result = 0;
    zs_assign_text(glob<void *>(LAST_FILE), path);
    section_begin(0x618594);   // "Loading a map"
    fn<void *(THISCALL *)(void *)>(0x5240b0)(stream);
    fn<void(THISCALL *)(Layer *, const char *)>(0x4eb6b0)(layer, name);
    void *file = fn<void *(__fastcall *)(const char *, DWORD)>(0x505e40)(path, 4);
    if (file && (BYTE)fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x5242f0)(stream, file, 4, (BYTE)fix ? 1 : 0) &&
        (BYTE)vcall<DWORD>(self, 76, layer, stream, fix, name)) {
        if ((BYTE)fix)
            fn<void(THISCALL *)(Layer *)>(0x4ea870)(layer);
        result = 1;
    }
    section_end();
    *glob<void **>(LOADING_FILE) = nullptr;
    fn<void(THISCALL *)(void *)>(0x5241e0)(stream);
    zs_dtor(&info.name);
    return result;
}

void THISCALL World::SetPlayerLayer(World *self, Layer *layer, DWORD i)
{
    if (i == 0xffffffff) {
        for (DWORD j = 0; j < player_count(players(self)); j++)
            vcall<DWORD>(self, 78, layer, j);
        return;
    }
    BYTE *p = player(self, i);
    int idx = layer_index_of(self, layer);
    *reinterpret_cast<int *>(p + 0xdc) = idx;
    if (idx == -1) {
        DWORD empty = 0;
        idx = (int)fn<DWORD(THISCALL *)(LayerArray *, DWORD *, DWORD)>(0x470e60)(&self->layers, &empty, 0xffffffff);
        if (idx == -1)
            idx = (int)vcall<DWORD>(&self->layers, 2, &layer);
        else
            self->layers.data[idx] = layer;
        *reinterpret_cast<int *>(p + 0xdc) = idx;
    }
    if (i == 0)
        fn<void(THISCALL *)(Layer *)>(0x4ed960)(layer);
    fn<void(THISCALL *)(World *, DWORD, DWORD)>(0x4f6770)(self, *reinterpret_cast<DWORD *>(p + 0xd8), 0);
    LayerSaveData *sd = &layer->data;
    if (BYTE *mix = static_cast<BYTE *>(fn<void *(THISCALL *)(LayerSaveData *)>(0x4e7bf0)(sd))) {
        auto set = code<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x51e3e0);
        set(self->mixer, 2, *reinterpret_cast<DWORD *>(mix + 0x14));
        set(self->mixer, 1, *reinterpret_cast<DWORD *>(mix + 0x10));
        set(self->mixer, 0, *reinterpret_cast<DWORD *>(mix + 0xc));
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x51e520)(self->mixer, *reinterpret_cast<DWORD *>(static_cast<BYTE *>(self->mixer) + 0x24));
    }
    fn<DWORD(THISCALL *)(LayerSaveData *, DWORD)>(0x4e6c20)(sd, sd->environment_type);
    fn<LD(THISCALL *)(LayerSaveData *, DWORD)>(0x4e6c80)(sd, *reinterpret_cast<DWORD *>(&sd->environment_size));
}

void THISCALL World::UpdateTileMaps(World *self)
{
    for (DWORD i = 0; i < self->layers.count; i++) {
        Layer *l = static_cast<Layer *>(self->layers.data[i]);
        if (l && l->data.tile_map)
            fn<void(THISCALL *)(void *)>(0x541fb0)(l->data.tile_map);
    }
}

DWORD THISCALL World::RemovePartFrom(World *, void *part, Layer *layer)
{
    if (layer)
        fn<void(THISCALL *)(Layer *, void *)>(0x4ebcb0)(layer, part);
    return 0;
}

DWORD THISCALL World::AddPartTo(World *, void *part, Layer *layer)
{
    if (layer)
        return fn<DWORD(THISCALL *)(Layer *, void *)>(0x4eb6f0)(layer, part);
    return 0;
}

DWORD __stdcall World::IsLoadingLayer(Layer *layer)
{
    Layer **info = *glob<Layer ***>(LOADING_FILE);
    return info && *info == layer ? 1 : 0;
}

const char *__stdcall World::LoadingName() { return zs_cstr(*glob<BYTE **>(LOADING_FILE) + 8); }

DWORD __stdcall World::LoadingFlag()
{
    BYTE *info = *glob<BYTE **>(LOADING_FILE);
    return with_low_byte((DWORD)(uintptr_t)info, info[0xc]);   // the plain merge lost the upper bytes (reimpl.h)
}

// ---- Game flow -----------------------------------------------------------------------------------------------

void THISCALL World::BeginLoading(World *self) { begin_loading(self); }
void THISCALL World::EndLoading(World *self) { end_loading(self); }

void THISCALL World::StartNewGame(World *self, DWORD a1, const char *map, const char *spawn, DWORD a4)
{
    section_begin(0x618610);   // "Start New Game"
    while (self->layers.count)
        fn<void(THISCALL *)(LayerArray *, DWORD)>(0x4eea50)(&self->layers, self->layers.count - 1);
    fn<void(THISCALL *)(void *)>(0x506f70)(self->timers);
    self->unique_id = 0;
    do {
        reinterpret_cast<DWORD *>(&self->playing_time)[0] = 0;
        reinterpret_cast<DWORD *>(&self->playing_time)[1] = 0;
        self->unique_id = fn<DWORD(THISCALL *)(void *)>(0x4f50d0)(glob<void *>(GAME_RANDOM));
    } while (!self->unique_id);
    reinterpret_cast<DWORD *>(&self->mission_time)[0] = 0;
    reinterpret_cast<DWORD *>(&self->mission_time)[1] = 0;
    if (!map) {
        map = zs_cstr(self->rules + 8);      // "First Map Name"
        spawn = zs_cstr(self->rules + 0xc);  // "First Map Spawn Point"
    }
    begin_loading(self);
    char path[256];
    const DWORD found = vcall<DWORD>(self, 68, map, 0, path);
    Layer *l = new_layer(self->screen_frame);
    if (!(BYTE)vcall<DWORD>(self, 77, l, path, map, found & 0xff) && l) {
        vcall<void *>(l, 1, 1);
        l = nullptr;
    }
    vcall<DWORD>(self, 78, l, 0xffffffff);
    if ((int)fn<DWORD(THISCALL *)(void *)>(0x5a01a0)(app()) == -1)   // the game type "Default For Map"
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x5a32b0)(app(), l->data.default_game);
    vcall<DWORD>(self, 80);
    delete_characters(self);
    if ((BYTE)vcall<DWORD>(self->game_type, 12))
        vcall<DWORD>(self, 107, l, a1, spawn, a4);
    fn<void(THISCALL *)(World *, Layer *)>(0x4f4890)(self, l);
    end_loading(self);
    vcall<DWORD>(self, 78, l, 0xffffffff);
    send_block(0x4f78a0, vcall<void *>(players(self), 12, 0));   // CClientSync
    section_end();
}

void THISCALL World::FinishLevelStart(World *self, Layer *layer)
{
    fn<DWORD(THISCALL *)(Layer *)>(0x4ed5c0)(layer);
    section_begin(0x6185f8);   // "Preload things on Map"
    auto status = code<DWORD(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x536080);
    status(glob_addr(0x6c0468), glob_addr(0x6185d8), 1, 0);   // "Pre-loading referenced assets"
    vcall<DWORD>(layer, 7);
    status(glob_addr(0x6c0468), glob_addr(0x6185b4), 1, 0);   // "Done pre-loading referenced assets"
    section_end();
    void **first = &layer->data.first_load;
    if (void *a = *first) {
        fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, void *, void **)>(0x41a720)(a, layer, 0, 0, 0, &layer->data, first);
        vcall<void *>(a, 1, 1);
        *first = nullptr;
    }
    void *p = players(self);
    void *p0 = vcall<void *>(p, 23, 0);
    void *packet = vcall<void *>(p, 12, 0);
    void *reader = fn<void *(__cdecl *)()>(0x4f7910)();   // CClientReady
    fn<DWORD(THISCALL *)(void *, void *, void *)>(0x4f7950)(reader, packet, p0);
}

void THISCALL World::GameStarted(World *self)
{
    if (!self->running)
        self->running = 1;
    self->time = 0;
    if (!player_count(players(self)))
        vcall<DWORD>(self->game_type, 1);
}

void THISCALL World::EndGame(World *self, DWORD)
{
    self->byte2f8 = 0;
    if (!self->running)
        return;
    fn<void(THISCALL *)(void *)>(0x4f8fb0)(fn<void *(__cdecl *)()>(0x4f8d30)());   // stop the client sounds
    self->running = 0;
    self->level_loaded = 0;
    fn<void(THISCALL *)(void *)>(0x506f70)(self->timers);
    delete_layers(self);
    delete_characters(self);
    void *p = players(self);
    for (DWORD j = 0; j < player_count(p); j++) {
        BYTE *pl = vcall<BYTE *>(p, 23, j);
        pl[0x38] = 0;
        pl[0x39] = 0;
    }
}

void THISCALL World::Update(World *self, float dt)
{
    if (!(BYTE)vcall<DWORD>(self->game_type, 12))
        vcall<DWORD>(players(self), 24);
    vcall<DWORD>(self->game_type, 16);
    if ((BYTE)vcall<DWORD>(self->game_type, 12)) {
        void *p = players(self);
        const DWORD n = player_count(p);
        for (DWORD i = 0; i < n; i++) {
            vcall<DWORD>(p, 27, i);
            void *packet = vcall<void *>(players(self), 12, i);
            auto can_read = code<DWORD(THISCALL *)(void *)>(0x520f50);
            if ((BYTE)can_read(packet)) {
                while ((BYTE)can_read(packet))
                    fn<DWORD(THISCALL *)(void *)>(0x5200d0)(packet);
                fn<DWORD(THISCALL *)(void *)>(0x520750)(packet);
            }
        }
    }
    bool add_mission = true;
    if ((BYTE)vcall<DWORD>(self, 30) && !(BYTE)vcall<DWORD>(self, 31)) {
        // paused: the time only counts up to the limit
        const LD acc = (LD)*glob<float *>(UPDATE_TIME) + (LD)dt;
        *glob<float *>(UPDATE_TIME) = fstp_float(acc);
        add_mission = x87_lt(acc, (LD)kf(0x5f9174));
    } else {
        self->running_time = fstp_double((LD)dt + (LD)self->running_time);
        vcall<DWORD>(self, 55, fbits(dt));
        *glob<DWORD *>(UPDATE_TIME) = 0;
    }
    if (add_mission)
        self->mission_time = fstp_double((LD)dt + (LD)self->mission_time);
    if ((BYTE)vcall<DWORD>(self->game_type, 12)) {
        const DWORD n = player_count(players(self));
        for (DWORD i = 0; i < n; i++)
            vcall<DWORD>(self, 52, fbits(dt), i);
    }
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x506a10)(self->timers, fbits(dt));
    self->time = fstp_float((LD)dt + (LD)self->time);
}

void THISCALL World::UpdateViews(World *self, float dt)
{
    void *p = players(self);
    const DWORD n = player_count(p);
    struct ViewList {
        DWORD vftable;
        int *data;
        DWORD count, capacity;
    } *views = glob<ViewList *>(VIEW_LIST);
    BYTE *guard = glob<BYTE *>(0x6c2780);
    if (!(*guard & 1)) {
        *guard |= 1;
        views->data = nullptr;
        views->count = 0;
        views->capacity = 0;
        views->vftable = vt(0x5f8dc4);
        crt_atexit(code<void(__cdecl *)()>(0x4f39a0));
    }
    views->count = 0;
    for (DWORD li = 0; li < self->layers.count; li++) {
        Layer *l = static_cast<Layer *>(self->layers.data[li]);
        if (!l)   // (the original also tests the grid's address, l + 0x50)
            continue;
        for (DWORD j = 0; j < n; j++) {
            if (layer_at(self, *reinterpret_cast<int *>(player(self, j) + 0xdc)) != l)
                continue;
            BYTE *pl = vcall<BYTE *>(p, 23, j);
            DWORD need = views->count + 1;
            if (views->capacity < need) {
                const DWORD grown = (DWORD)x87_ftol((LD)(unsigned long long)views->capacity * kf(0x5ea5f4));
                if (grown > need)
                    need = grown;
                fn<void(THISCALL *)(ViewList *, DWORD)>(0x5a41b0)(views, need);
            }
            int *e = views->data + views->count * 2;
            views->count++;
            e[0] = x87_fistp(*reinterpret_cast<float *>(pl + 0xc0));
            e[1] = x87_fistp(*reinterpret_cast<float *>(pl + 0xc4));
        }
        fn<void(THISCALL *)(Layer *, void *, DWORD)>(0x4ea350)(l, views, fbits(dt));
        views->count = 0;
    }
}

DWORD THISCALL World::WriteState(World *self, void *stream, DWORD binary)
{
    const BYTE saving = self->saving;
    self->saving = 1;
    const DWORD write = (BYTE)binary ? 0x416f50 : 0x4168f0;   // Object_WriteBinary / Object_WriteText
    fn_dyn<DWORD(THISCALL *)(void *, void *)>(write)(self->layer_holder, stream);
    const DWORD r = fn_dyn<DWORD(THISCALL *)(void *, void *)>(write)(&self->holder_vftable, stream);
    self->saving = saving;
    return r;
}

// ---- Players and characters --------------------------------------------------------------------------------------

void *THISCALL World::Character(World *self, DWORD i) { return i < self->characters.count ? self->characters.data[i] : nullptr; }
void *THISCALL World::FirstCharacter(World *self) { return self->characters.count ? self->characters.data[0] : nullptr; }
void THISCALL World::DeleteCharacters(World *self) { delete_characters(self); }

void THISCALL World::TakeCharacters(World *self, Layer *layer, LayerArray *out)
{
    const DWORD n = self->characters.count;
    vcall<DWORD>(out, 1);
    for (DWORD i = 0; i < n; i++) {
        void *c = self->characters.data[i];
        if ((BYTE)is_listed(layer, c)) {
            vcall<DWORD>(self, 103, c, layer);
            vcall<DWORD>(out, 2, &c);
        }
    }
}

void THISCALL World::PutCharacters(World *self, Layer *layer, LayerArray *list, DWORD notify)
{
    const DWORD n = list->count;
    for (DWORD i = 0; i < n; i++) {
        void *c = list->data[i];
        if (!c || !is_kind(c, class_49a930()))
            c = nullptr;
        vcall<DWORD>(self, 104, c, layer);
        if ((BYTE)notify)
            vcall<DWORD>(c, 62, layer);
    }
}

void THISCALL World::PlaceAt(World *, void *part, Layer *, void *reference, const float *offset)
{
    DWORD pos[2];
    if (reference) {
        part_position(reference, reinterpret_cast<float *>(pos));
    } else {
        pos[0] = 0;
        pos[1] = 0;
    }
    if (offset) {
        pos[0] = fbits(fstp_float((LD)*reinterpret_cast<float *>(&pos[0]) + offset[0]));
        pos[1] = fbits(fstp_float((LD)*reinterpret_cast<float *>(&pos[1]) + offset[1]));
    }
    part_move_center(part, pos[0], pos[1]);
    if (reference) {
        CClassInfo *c = class_420950();
        if (void *ai = vcall<void *>(reference, 64, c, (DWORD)0))
            fn<DWORD(THISCALL *)(void *, void *, void *)>(0x420d50)(ai, reference, part);
    }
}

void THISCALL World::MoveToSpawnPoint(World *self, void *part, Layer *layer, const char *name, const float *offset)
{
    if (!name || !*name) {
        vcall<DWORD>(self, 17, glob<const char *>(0x618620));   // "ERROR: Tried to move an entity but ..."
        return;
    }
    if (crt_stricmp_(name, glob<const char *>(0x614b30)) == 0) {   // "$0,0"
        vcall<DWORD>(self, 82, part, layer, (DWORD)0, offset);
        return;
    }
    LayerArray *list;
    fn<void *(THISCALL *)(LayerArray **)>(0x59a470)(&list);
    fn<DWORD(THISCALL *)(void *, LayerArray *, const char *, DWORD, DWORD, DWORD)>(0x57aa80)(layer->data.grid, list, name, 0, 0, 0);
    if (!list->count) {
        void *reporter = *glob<void **>(0x6c02cc);
        vcall<DWORD>(reporter, 17, glob<const char *>(0x6186b8));   // "Could not find specified spawn point entity"
        vcall<DWORD>(*glob<void **>(0x6c02cc), 17, name);
        fn<DWORD(THISCALL *)(LayerArray **)>(0x59a510)(&list);
        return;
    }
    DWORD off[2] = {0, 0};
    if (offset) {
        off[0] = reinterpret_cast<const DWORD *>(offset)[0];
        off[1] = reinterpret_cast<const DWORD *>(offset)[1];
    }
    BYTE *tried = static_cast<BYTE *>(heap_alloc(list->count));
    for (DWORD i = 0; i < list->count; i++)
        tried[i] = 0;
    auto random_in = [&]() {
        return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x55c4e0)(glob<void *>(SPAWN_RANDOM), 0, list->count - 1);
    };
    DWORD idx = 0;
    BYTE free_found = 0;
    bool untried = list->count != 0;
    while (untried) {
        idx = random_in();
        float p[2];
        part_position(list->data[idx], p);
        p[0] = fstp_float((LD)*reinterpret_cast<float *>(&off[0]) + p[0]);
        p[1] = fstp_float((LD)*reinterpret_cast<float *>(&off[1]) + p[1]);
        part_move_center(part, fbits(p[0]), fbits(p[1]));
        auto blocker = [&]() {
            CClassInfo *prefer = class_49b810();
            return fn<void *(THISCALL *)(Layer *, void *, CClassInfo *, DWORD, DWORD)>(0x4eb1c0)(layer, part, prefer, 0x840000, 0x2000000);
        };
        BYTE blocked = 0;
        if (blocker()) {
            CClassInfo *c = class_49b810();
            blocked = is_kind(blocker(), c) ? 1 : 0;
        }
        tried[idx] = blocked;
        free_found = blocked == 0;
        if (free_found)
            break;
        untried = false;
        for (DWORD j = 0; j < list->count; j++) {
            untried = tried[j] == 0;
            if (untried)
                break;
        }
    }
    if (tried)
        heap_free(tried);
    if (!free_found)
        idx = random_in();
    vcall<DWORD>(self, 82, part, layer, list->data[idx], off);
    fn<DWORD(THISCALL *)(LayerArray **)>(0x59a510)(&list);
}

void THISCALL World::MoveToTeamSpawn(World *self, void *part, Layer *layer, const char *name, const float *offset)
{
    BYTE was = 0;
    if (BYTE *pl = fn<BYTE *(THISCALL *)(void *)>(0x5bac80)(part)) {
        was = pl[0x18];
        pl[0x18] = 1;
    }
    if (!name || !*name) {
        DWORD team = fn<DWORD(THISCALL *)(void *)>(0x4fa960)(part);
        if (team == 0xffffffff)
            team = 0;
        void *type = fn<void *(THISCALL *)(void *)>(0x59ff80)(app());
        if ((BYTE)vcall<DWORD>(type, 36))
            team = fn<DWORD(THISCALL *)(void *, DWORD)>(0x55c470)(glob<void *>(SPAWN_RANDOM), layer->data.teams.count);
        const DWORD n = layer->data.teams.count;
        if (team >= n)
            team = n - 1;
        if (team >= n) {
            name = glob<const char *>(0x62e558);
        } else {
            BYTE *info = static_cast<BYTE *>(layer->data.teams.data[team]);
            name = zs_cstr(info + 8);
            if (was && (BYTE)zs_not_empty(static_cast<BYTE *>(layer->data.teams.data[team]) + 0xc))
                name = zs_cstr(static_cast<BYTE *>(layer->data.teams.data[team]) + 0xc);
        }
    }
    fn<void(THISCALL *)(World *, void *, Layer *, const char *, const float *)>(0x4f53f0)(self, part, layer, name, offset);
}

void THISCALL World::MoveToSpawnHere(World *self, void *part, const char *name, const float *offset)
{
    Layer *l = *reinterpret_cast<Layer **>(static_cast<BYTE *>(part) + 0x48);
    fn<void(THISCALL *)(World *, void *, Layer *, const char *, const float *)>(0x4f53f0)(self, part, l, name, offset);
}

void THISCALL World::MoveAllToSpawns(World *self, LayerArray *parts, LayerArray *names, LayerArray *offsets, LayerArray *layers)
{
    for (DWORD i = 0; i < parts->count; i++) {
        void *p = parts->data[i];
        if (p && is_kind(p, class_49a930())) {
            const float *off = reinterpret_cast<const float *>(offsets->data) + i * 2;
            const char *name = zs_cstr(&names->data[i]);
            fn<void(THISCALL *)(World *, void *, Layer *, const char *, const float *)>(0x4f4950)(self, p, static_cast<Layer *>(layers->data[i]),
                                                                                                name, off);
        }
    }
}

void *THISCALL World::SetSyncRequested(World *self, DWORD i)
{
    BYTE *p = player(self, i);
    p[0x39] = 1;
    return p;
}

void THISCALL World::RequestSync(World *self, DWORD i)
{
    void *p = players(self);
    BYTE *pl = vcall<BYTE *>(p, 23, i);
    pl[0x38] = 1;
    pl[0x39] = 1;
    if (i == 0)
        for (DWORD j = 1; j < player_count(p); j++)
            send_block(0x4f9390, vcall<void *>(p, 13, j));   // CRequestSyncFromClients
}

DWORD THISCALL World::ViewTarget(World *self, float *out, DWORD i)
{
    BYTE *p = player(self, i);
    const DWORD *t = reinterpret_cast<DWORD *>(p + 0xd0);
    reinterpret_cast<DWORD *>(out)[0] = t[0];
    reinterpret_cast<DWORD *>(out)[1] = t[1];
    return t[1];
}

long double THISCALL World::Zoom(World *self, DWORD i) { return *reinterpret_cast<float *>(player(self, i) + 0xd8); }
DWORD THISCALL World::PlayerEntity(World *self, DWORD i) { return *reinterpret_cast<DWORD *>(player(self, i) + 0xe8); }
DWORD THISCALL World::LocalEntity(World *self) { return vcall<DWORD>(self, 89, (DWORD)0); }

int THISCALL World::PlayerViewRectAt(World *self, DWORD i, int *rect)
{
    BYTE *p = vcall<BYTE *>(vcall<void *>(self, 43), 22, i);
    BYTE *p0 = vcall<BYTE *>(vcall<void *>(self, 43), 22, (DWORD)0);
    Layer *l = layer_at(self, *reinterpret_cast<int *>(p0 + 0xdc));
    return fn<int(THISCALL *)(Layer *, const float *, int *)>(0x4ea660)(l, reinterpret_cast<float *>(p + 0xc0), rect);
}

void THISCALL World::PlayerViewRect(World *self, DWORD i, int *rect)
{
    BYTE *p = vcall<BYTE *>(vcall<void *>(self, 43), 22, i);
    int pos[2];
    pos[0] = x87_fistp(*reinterpret_cast<float *>(p + 0xc0));
    pos[1] = x87_fistp(*reinterpret_cast<float *>(p + 0xc4));
    BYTE *q = vcall<BYTE *>(vcall<void *>(self, 43), 22, i);
    Layer *l = layer_at(self, *reinterpret_cast<int *>(q + 0xdc));
    fn<void(THISCALL *)(Layer *, const int *, int *)>(0x4ea110)(l, pos, rect);
}

DWORD THISCALL World::PlayerSees(World *self, DWORD i, const float *pos)
{
    int r[4];
    vcall<DWORD>(self, 54, i, r);
    const int y = x87_ftol(pos[1]);
    const int x = x87_ftol(pos[0]);
    return x >= r[0] && y >= r[1] && x < r[2] && y < r[3] ? 1 : 0;
}

// ---- Multiplayer broadcasts --------------------------------------------------------------------------------------

void THISCALL World::BroadcastSimpleSound(World *, DWORD sound)
{
    if (*glob<DWORD *>(0x6c1d0c) || sound == 0xffffffff)
        return;
    void *p = vcall<void *>(world(), 44);
    for (DWORD i = 0; i < player_count(p); i++) {
        void *pl = vcall<void *>(p, 23, i);
        void *packet = vcall<void *>(p, 13, i);
        void *reader = fn<void *(__cdecl *)()>(0x4f8af0)();   // CClientPlaySimpleSound
        fn<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD, DWORD)>(0x4f8b90)(reader, packet, pl, 0, sound, 0);
    }
}

void THISCALL World::BroadcastExplosionAt(World *self, DWORD a1, DWORD a2, void *part, DWORD a4, DWORD a5)
{
    DWORD pos[2];
    vcall<DWORD>(part, 43, pos);
    DWORD extra = 0;
    if (is_kind(part, class_entity()))
        extra = vcall<DWORD>(part, 54);
    broadcast_explosion(self, a1, a2, pos[0], pos[1], extra, a4, a5);
}

void THISCALL World::BroadcastExplosionPoint(World *self, DWORD a1, DWORD a2, const float *pos, DWORD a4, DWORD a5, DWORD a6)
{
    const DWORD *p = reinterpret_cast<const DWORD *>(pos);
    broadcast_explosion(self, a1, a2, p[0], p[1], a4, a5, a6);
}

void THISCALL World::BroadcastExplosion(World *self, DWORD a1, DWORD a2, DWORD x, DWORD y, DWORD a5, DWORD a6, DWORD a7)
{
    broadcast_explosion(self, a1, a2, x, y, a5, a6, a7);
}

void THISCALL World::RegisterReaders(World *self, void *server_packet, void *client_packet)
{
    fn<void(THISCALL *)(void *)>(0x4f93e0)(server_packet);
    fn<void(THISCALL *)(void *)>(0x4f7bf0)(client_packet);
    if (self->game_type)
        vcall<DWORD>(self->game_type, 17, server_packet, client_packet);
}

// ---- Small slots ---------------------------------------------------------------------------------------------------

void *THISCALL World::GameType(World *self) { return self->game_type; }
DWORD THISCALL World::Slot60(World *self) { return vcall<DWORD>(self, 30); }

DWORD THISCALL World::Slot61(World *self)
{
    DWORD r = vcall<DWORD>(self, 30);
    if ((BYTE)r) {
        r = vcall<DWORD>(self, 31);
        if (!(BYTE)r)
            return r;
    }
    return (r & ~0xffu) | 1;
}

DWORD THISCALL World::Slot67(World *, DWORD, DWORD, DWORD, DWORD) { return 0; }
DWORD THISCALL World::Saving(World *self) { return self->saving; }
long double THISCALL World::One(World *) { return kf(0x5ec3f4); }
DWORD THISCALL World::Zero4(World *, DWORD) { return 0; }
DWORD THISCALL World::Slot9(World *self, DWORD a) { return fn<DWORD(THISCALL *)(World *, DWORD)>(0x4ce400)(self, a); }

DWORD THISCALL World::Slot51(World *self, void *obj, DWORD a)
{
    auto f = code<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x470050);
    if (static_cast<BYTE *>(obj)[0x1bc])
        return f(obj, a, 0, vcall<DWORD>(players(self), 12, (DWORD)0));
    return f(obj, a, 0, 0);
}

long double THISCALL World::RunningTime(World *self) { return self->running_time; }
long double THISCALL World::TotalTime(World *self) { return (LD)self->playing_time + (LD)self->mission_time; }

DWORD THISCALL World::Slot95(World *self, DWORD a)
{
    vcall<DWORD>(self, 96, a);
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x5067f0)(self->timers, a);
}

DWORD *THISCALL World::Slot99(World *, DWORD *out)
{
    out[0] = 1;
    out[1] = 1;
    return out;
}

DWORD THISCALL World::Slot38(World *self)
{
    if (void *m = pool_alloc(0x16c)) {
        const DWORD x = vcall<DWORD>(self, 88);
        const DWORD g = (DWORD)(uintptr_t)fn<void *(THISCALL *)(World *)>(0x4cdf30)(self);
        void *o = fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x4713b0)(m, g, x);
        return vcall<DWORD>(self, 23, o, (DWORD)1);
    }
    return vcall<DWORD>(self, 23, (DWORD)0, (DWORD)1);
}

DWORD THISCALL World::KeyEscape(World *self, DWORD key)
{
    if (key == 0x1b && !(BYTE)fn<DWORD(THISCALL *)(void *)>(0x4cdf60)(world()) && (BYTE)vcall<DWORD>(self, 35))
        return 1;
    return 0;
}

long double THISCALL World::Slot62(World *) { return kf(0x5f9178); }
long double THISCALL World::Slot63(World *self) { return vcall<LD>(self, 62) * (LD)kf(0x5f917c); }
long double THISCALL World::Slot64(World *self) { return vcall<LD>(self, 62) * (LD)kf(0x5f05b8); }

long double THISCALL World::Slot65(World *self)
{
    const LD v = vcall<LD>(self, 63);
    return v + v;
}

long double THISCALL World::Slot66(World *self) { return vcall<LD>(self, 62); }

DWORD THISCALL World::Slot15(World *self, DWORD a)
{
    const DWORD w = vcall<DWORD>(self->screen_frame, 45);
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x506b90)(self->timers, a, 0, w);
}

// ---- Helpers of the range -------------------------------------------------------------------------------------------

DWORD THISCALL Random_Next(BYTE *gen)
{
    ++*reinterpret_cast<DWORD *>(gen + 4);
    const DWORD a = *reinterpret_cast<DWORD *>(gen + 0x10);
    *reinterpret_cast<DWORD *>(gen + 0x10) = glob<const DWORD *>(0x620004)[a];
    const DWORD b = *reinterpret_cast<DWORD *>(gen + 0x14);
    *reinterpret_cast<DWORD *>(gen + 0x14) = glob<const DWORD *>(0x620004)[b];
    DWORD *table = reinterpret_cast<DWORD *>(gen + 0x18);
    const DWORD v = table[a] ^ table[b];
    table[a] = v;
    return v;
}

void THISCALL PtrArray_DeleteAll(LayerArray *a)
{
    while (a->count) {
        const DWORD i = a->count - 1;
        if (void *e = a->data[i])
            vcall<void *>(e, 1, 1);
        array_remove_at(a, i);
    }
}

ZAX_HOOKS(world_class)
{
    using W = World;
    const Hook list[] = {
#include "world_class_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
