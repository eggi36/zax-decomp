// The control configuration, the game's INI helpers, the player options and the CD key check
// 0x46b4e0-0x46ffa0 (phase 3, game flow; docs/DECOMPILATION.md section 8).
//
// The control configuration (0x1c8 bytes; the key bindings of the world's slot 41, read from and written to
// Zax.CFG; names ours) [confirmed: constructor 0x46ef80, destructor 0x46f2f0, the functions below]:
//   +0x00, +0x0c, +0x64 three bit sets {words (operator new), bit count, changed byte, received byte}: the
//     actions held, ..., sent over the network (0x46ee50 reads, 0x46ef00 writes),
//   +0x18 a callback (fastcall: the configuration, the binding) when a binding lost its keys,
//   +0x1c, +0x2c two pool pointer arrays (vtable 0x5ee138, 0x5ee15c after the destructor): keys that may
//     not be bound (1, 0x3b, 0x44 Escape, F1, F10) and mouse bits that may not be bound,
//   +0x3c the number of bindings, +0x40 the bindings (0x40 bytes each, operator new), +0x44 and +0x48 two
//     saved copies, +0x4c "mouse allowed" (player 0 with Zax.CFG), +0x4d a joystick is set, +0x4e, +0x4f,
//     +0x5c/+0x60, +0x70 the joystick (CJoystick, operator new 0x100; its name +0x48), +0x74, +0x75 a key is
//     being captured, +0x78 the binding as it was before the capture, +0xb8 the joystick's name (0x100
//     characters), +0x1b8 the time of the next joystick check (timeGetTime + 2 s), +0x1bc "use Zax.CFG",
//     +0x1c0 the player (0 the first: no "Player %d " prefix), +0x1c4 active, +0x1c5.
//   A binding: +0 key, +4 second key, +8 joystick bits, +0xc mouse bits, +0x10 its kind (1 the default, 2
//     ignored by the conflict test), +0x14, +0x18, +0x20 a byte, +0x24/+0x28 its INI key's two words, +0x2c
//     the item it uses, +0x34/+0x38/+0x3c what other bindings share (joystick and mouse bits, a second
//     key).
// The INI object 0x6bd330 (Zax.CFG): the file name at +0, the default section at +0x100.
// The player options (0x6bd2f8: the name (ZString), two colours, "Weapon AutoSwitch"), the default name
// 0x6bd2f0.
//
// Return values: as the original where it defines them (the bit test, the capture's and the joystick
// check's al, the CD key check's al); void in use otherwise [assumption].
// Left out (user interface): the CD key dialog 0x46b050-0x46b420 and the network options dialog
// 0x46d010-0x46d450.
#include "class_info.h"
#include "exe_base.h"

namespace zax {

namespace {

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }

// ---- Functions of the exe ----------------------------------------------------------------------------------

constexpr DWORD INI = 0x6bd330, INI_SECTION = 0x6bd430, KEYS = 0x6c1050;
void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_dtor(void *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
char *zs_get_buffer(void *z, DWORD n) { return fn<char *(THISCALL *)(void *, DWORD)>(0x4e1590)(z, n); }
DWORD zs_release_buffer(void *z, DWORD n) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4e16d0)(z, n); }
void zs_set_text(void *z, const char *s) { fn<void(THISCALL *)(void *, const char *)>(0x4df010)(z, s); }
template <typename... A> void zs_format(void *z, DWORD fmt, A... a) { fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, at<const char *>(fmt), a...); }
template <typename... A> int crt_sprintf(char *out, DWORD fmt, A... a) { return fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(out, at<const char *>(fmt), a...); }
template <typename... A> int crt_sscanf(const char *in, DWORD fmt, A... a) { return fn<int(__cdecl *)(const char *, const char *, ...)>(0x5d1539)(in, at<const char *>(fmt), a...); }
// The INI functions (fastcall file, section; then the stack arguments)
DWORD ini_get_int(DWORD section, const char *key, DWORD def)
{
    return fn<DWORD(__fastcall *)(void *, const char *, const char *, DWORD)>(0x4a4670)(glob<void *>(INI), at<const char *>(section), key, def);
}
DWORD ini_write_int(DWORD section, const char *key, DWORD v)
{
    return fn<DWORD(__fastcall *)(void *, const char *, const char *, DWORD)>(0x4a4690)(glob<void *>(INI), at<const char *>(section), key, v);
}
DWORD ini_get_string(void *ini, const char *section, const char *key, char *buf, DWORD n)
{
    return fn<DWORD(__fastcall *)(void *, const char *, const char *, char *, DWORD)>(0x4a4790)(ini, section, key, buf, n);
}
DWORD ini_write_string(const char *section, const char *key, const char *text)
{
    return fn<DWORD(__fastcall *)(void *, const char *, const char *, const char *)>(0x4a47c0)(glob<void *>(INI), section, key, text);
}
DWORD player_error_sound() { return fn<DWORD(THISCALL *)(void *, const char *)>(0x57e820)(glob<void *>(0x713708), glob<const char *>(0x60a4d8)); }
DWORD time_ms() { return fn<DWORD(__cdecl *)()>(0x4d2460)(); }   // timeGetTime
// 0x4dde40 (a 3-byte add and a jump, not hookable): the joystick's name
const char *joystick_name(void *j) { return fn<const char *(THISCALL *)(void *)>(0x4dde40)(j); }
// 0x470e60 (a pointer array; value pointer, start): the index of the value, -1 when it is none
DWORD array_find(void *a, const DWORD *v, DWORD start) { return fn<DWORD(THISCALL *)(void *, const DWORD *, DWORD)>(0x470e60)(a, v, start); }
// The inlined find from -1: the index of the value in the array's data (+4, count +8), -1 when it is none
DWORD array_index(BYTE *a, DWORD v)
{
    const DWORD n = dw(a, 8);
    const DWORD *d = reinterpret_cast<const DWORD *>(bp(dw(a, 4)));
    for (DWORD i = 0; i < n; i++)
        if (d[i] == v)
            return i;
    return 0xffffffffu;
}
void str_copy(char *dst, const char *src)
{
    while ((*dst++ = *src++))
        ;
}
DWORD str_length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}

// ---- The INI helpers ---------------------------------------------------------------------------------------

// 0x46ec00 (thiscall: the INI object; name): the file name copied to +0. eax: its length + 1.
DWORD THISCALL Ini_SetFile(char *self, const char *name)
{
    const DWORD n = str_length(name) + 1;
    for (DWORD i = 0; i < n; i++)
        self[i] = name[i];
    return n;
}
// 0x46ec30 (thiscall: the INI object; key, out ZString): the key of the default section (+0x100), at most
// 0x100 characters, the ZString's text the default. eax: ReleaseBuffer's.
DWORD THISCALL Ini_GetDefault(BYTE *self, const char *key, void *out)
{
    char *buf = zs_get_buffer(out, 0x100);
    ini_get_string(self, reinterpret_cast<const char *>(self + 0x100), key, buf, 0x100);
    return zs_release_buffer(out, 0xffffffff);
}
// 0x46ec70 (thiscall: the INI object; section, key, out ZString). eax: ReleaseBuffer's.
DWORD THISCALL Ini_GetText(BYTE *self, const char *section, const char *key, void *out)
{
    char *buf = zs_get_buffer(out, 0x100);
    ini_get_string(self, section, key, buf, 0x100);
    return zs_release_buffer(out, 0xffffffff);
}

// ---- The CD key ----------------------------------------------------------------------------------------------

constexpr DWORD CD_KEY = 0x60f80c, LAST_ERROR = 0x60f828;
// 0x46b4e0 (ecx: out ZString): the CD key from Zax.CFG. eax: 0x46ec30's.
DWORD ini_get_default(const char *key, void *out) { return fn<DWORD(THISCALL *)(void *, const char *, void *)>(0x46ec30)(glob<void *>(INI), key, out); }
DWORD __fastcall CdKey_Get(void *out) { return ini_get_default(glob<const char *>(CD_KEY), out); }
// 0x46b500, 0x46b520 (ecx: text): the CD key, the last CD key error message into Zax.CFG
DWORD __fastcall CdKey_Set(const char *text) { return ini_write_string(glob<const char *>(INI_SECTION), glob<const char *>(CD_KEY), text); }
DWORD __fastcall CdKey_SetLastError(const char *text) { return ini_write_string(glob<const char *>(INI_SECTION), glob<const char *>(LAST_ERROR), text); }
// 0x46b540 (fastcall: the key (ZString, rewritten), out: the key's kind or null): upper case, O→0, I→1,
// L→1, B→8, S→5, digits only; 20 digits are two numbers v = (first ten << 32) | last ten; the kind is bits
// 16-19 of v, the key is valid when bits 20-59 of v modulo 0xffff, xor 0x9249, equal the low 16 bits of the
// last ten. The key comes back with a '-' after every 4 characters. al: valid.
DWORD __fastcall CdKey_Check(void *key, DWORD *kind)
{
    alignas(4) BYTE copy[4];
    fn<void *(THISCALL *)(void *, const void *)>(0x4dedf0)(copy, key);
    fn<void(THISCALL *)(void *)>(0x4e11d0)(copy);
    using Replace = void(THISCALL *)(void *, DWORD, DWORD);
    fn<Replace>(0x4e12b0)(copy, 'O', '0');
    fn<Replace>(0x4e12b0)(copy, 'I', '1');
    fn<Replace>(0x4e12b0)(copy, 'L', '1');
    fn<Replace>(0x4e12b0)(copy, 'B', '8');
    fn<Replace>(0x4e12b0)(copy, 'S', '5');
    fn<void(THISCALL *)(void *, const char *)>(0x4e1000)(copy, glob<const char *>(0x60f850) /* "0123456789" */);
    const char *s = zs_c_str(copy);
    BYTE valid = 0;
    if (str_length(s) == 20) {
        char buf[24];
        str_copy(buf, s);
        DWORD last = 0, first = 0;
        crt_sscanf(buf + 10, glob_addr(0x60f848) /* "%010u" */, &last);
        buf[10] = 0;
        crt_sscanf(buf, glob_addr(0x60f848), &first);
        const unsigned long long v = (unsigned long long)first << 32 | last;
        if (kind)
            *kind = (DWORD)(v >> 16) & 0xf;
        const unsigned long long t = v >> 20;
        const long long dividend = (long long)((t & 0xffffffffull) | ((t >> 32) & 0xff) << 32);
        const long long r = fn<long long(__stdcall *)(long long, long long)>(0x5d1570)(dividend, 0xffff);   // __allrem
        valid = (WORD)last == (WORD)((DWORD)r ^ 0xffff9249u);
    }
    for (DWORD i = 4; fn<DWORD(THISCALL *)(void *)>(0x4e13b0)(copy) > i; i += 5)
        fn<void(THISCALL *)(void *, const char *, DWORD)>(0x4e0d30)(copy, glob<const char *>(0x60f634) /* "-" */, i);
    fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(key, copy);
    zs_dtor(copy);
    return valid;
}
// 0x46b6d0: the CD key of Zax.CFG is valid. al.
DWORD __cdecl CdKey_IsValid()
{
    alignas(4) BYTE key[4];
    zs_ctor(key);
    ini_get_default(glob<const char *>(CD_KEY), key);
    const DWORD r = fn<DWORD(__fastcall *)(void *, DWORD *)>(0x46b540)(key, nullptr);
    zs_dtor(key);
    return r & 0xff;
}

// ---- The player options --------------------------------------------------------------------------------------

constexpr DWORD NET_GAME = 0x60fa48, PLAYER_NAME = 0x60fa54, COLOR1 = 0x60fa38, COLOR2 = 0x60fa28;
// 0x46ce20: the name from the default (0x6bd2f0), the colours 50 and 100, auto switch on. eax: this.
BYTE *THISCALL Options_Ctor(BYTE *self)
{
    zs_ctor(self);
    fn<void *(THISCALL *)(void *, const char *)>(0x4e1930)(self, zs_c_str(glob<void *>(0x6bd2f0)));
    dw(self, 4) = 0x32;
    dw(self, 8) = 0x64;
    self[0xc] = 1;
    return self;
}
// 0x46ce50: from Zax.CFG ("Net Game": "Player Name" with the default name, "Player Color 1", "Player Color
// 2"; the default section's "Weapon AutoSwitch", 1)
void THISCALL Options_Load(BYTE *self)
{
    fn<void(THISCALL *)(void *, const void *)>(0x4df270)(self, glob<void *>(0x6bd2f0));
    fn<DWORD(THISCALL *)(void *, const char *, const char *, void *)>(0x46ec70)(glob<void *>(INI), glob<const char *>(NET_GAME),
                                                                            glob<const char *>(PLAYER_NAME), self);
    dw(self, 4) = ini_get_int(glob_addr(NET_GAME), glob<const char *>(COLOR1), dw(self, 4));
    dw(self, 8) = ini_get_int(glob_addr(NET_GAME), glob<const char *>(COLOR2), dw(self, 8));
    self[0xc] = ini_get_int(glob_addr(INI_SECTION), glob<const char *>(0x60fa14) /* "Weapon AutoSwitch" */, 1) != 0;
}
// 0x46ced0: the name and the colours into Zax.CFG
void THISCALL Options_Save(BYTE *self)
{
    ini_write_string(glob<const char *>(NET_GAME), glob<const char *>(PLAYER_NAME), zs_c_str(self));
    ini_write_int(glob_addr(NET_GAME), glob<const char *>(COLOR1), dw(self, 4));
    ini_write_int(glob_addr(NET_GAME), glob<const char *>(COLOR2), dw(self, 8));
}

// ---- The bit sets --------------------------------------------------------------------------------------------

DWORD words(DWORD bits) { return ((bits & 0x1f) ? 1 : 0) + (bits >> 5); }
// 0x46ed70 (bit): set or not. eax: 1 or 0.
DWORD THISCALL Bitset_Test(BYTE *self, DWORD bit) { return (reinterpret_cast<DWORD *>(bp(dw(self, 0)))[bit >> 5] >> (bit & 0x1f)) & 1; }
// 0x46eda0 (other): the words copied, "changed" set when one differed. eax: this.
BYTE *THISCALL Bitset_Assign(BYTE *self, BYTE *other)
{
    for (DWORD i = 0; i < words(dw(self, 4)); i++) {
        DWORD *d = reinterpret_cast<DWORD *>(bp(dw(self, 0))) + i;
        const DWORD v = reinterpret_cast<DWORD *>(bp(dw(other, 0)))[i];
        if (*d != v) {
            *d = v;
            self[8] = 1;
        }
    }
    return self;
}
// 0x46ee00: every word cleared, "changed" set when one was not 0. eax: the bit count >> 5 (left from the
// word count).
DWORD THISCALL Bitset_Clear(BYTE *self)
{
    for (DWORD i = 0; i < words(dw(self, 4)); i++) {
        DWORD *d = reinterpret_cast<DWORD *>(bp(dw(self, 0))) + i;
        if (*d) {
            *d = 0;
            self[8] = 1;
        }
    }
    return dw(self, 4) >> 5;
}
// 0x46ee50 (packet): the first read of a frame (+9 not set) clears the set; the words read are added.
void THISCALL Bitset_Read(BYTE *self, void *packet)
{
    const DWORD n = words(dw(self, 4));
    if (!self[9])
        for (DWORD i = 0; i < words(dw(self, 4)); i++) {
            DWORD *d = reinterpret_cast<DWORD *>(bp(dw(self, 0))) + i;
            if (*d) {
                *d = 0;
                self[8] = 1;
            }
        }
    DWORD v = (DWORD)(uintptr_t)packet;   // the original reads into its packet argument's slot
    for (DWORD i = 0; i < n; i++) {
        fn<DWORD(THISCALL *)(void *, DWORD *)>(0x520550)(packet, &v);
        reinterpret_cast<DWORD *>(bp(dw(self, 0)))[i] |= v;
    }
    self[9] = 1;
}
// 0x46ef00 (packet): the words written. al: 1.
DWORD THISCALL Bitset_Write(BYTE *self, void *packet)
{
    const DWORD n = words(dw(self, 4));
    DWORD r = n;
    for (DWORD i = 0; i < n; i++)
        r = fn<DWORD(THISCALL *)(void *, DWORD)>(0x520940)(packet, reinterpret_cast<DWORD *>(bp(dw(self, 0)))[i]);
    return (r & ~0xffu) | 1;
}

// ---- The control configuration ---------------------------------------------------------------------------------

constexpr DWORD BINDING = 0x40;
BYTE *binding(BYTE *self, DWORD i) { return bp(dw(self, 0x40)) + i * BINDING; }
// 0x46ef50: a binding's defaults (kind 1, the rest 0). eax: this.
BYTE *THISCALL Binding_Init(BYTE *b)
{
    dw(b, 0) = 0;
    dw(b, 4) = 0;
    dw(b, 8) = 0;
    dw(b, 0xc) = 0;
    dw(b, 0x10) = 1;
    dw(b, 0x3c) = 0;
    dw(b, 0x34) = 0;
    dw(b, 0x38) = 0;
    dw(b, 0x30) = 0;
    dw(b, 0x1c) = 0;
    b[0x20] = 0;
    dw(b, 0x24) = 0;
    dw(b, 0x28) = 0;
    return b;
}
// The inlined copy of the bindings (0x46f980, 0x46fa40): a new block with initialised bindings (none
// without memory), the bindings copied into it
BYTE *copy_bindings(BYTE *self)
{
    const DWORD n = dw(self, 0x3c);
    BYTE *c = static_cast<BYTE *>(heap_alloc(n << 6));
    if (c && (int)(n - 1) >= 0)
        for (DWORD i = 0; i < n; i++)
            Binding_Init(c + i * BINDING);
    memcpy(c, bp(dw(self, 0x40)), dw(self, 0x3c) << 6);
    return c;
}
void free_block(BYTE *self, DWORD off)
{
    if (dw(self, off)) {
        heap_free(bp(dw(self, off)));
        dw(self, off) = 0;
    }
}
// "Player %d " for a player other than the first, then the text
void player_prefix(BYTE *self, char *buf)
{
    if (dw(self, 0x1c0))
        crt_sprintf(buf, glob_addr(0x60fc58) /* "Player %d " */, dw(self, 0x1c0) + 1);
}

// 0x46ef80 (thiscall: use Zax.CFG, player, the default joystick, the number of bindings): the three bit
// sets, the arrays, the bindings; the joystick from Zax.CFG ("[Player %d ]Controler ID Number", default
// the given one + 1; 0x470d20); the keys 1, 0x3b, 0x44 that may not be bound. eax: this.
BYTE *THISCALL Config_Ctor(BYTE *self, DWORD use_cfg, DWORD player, DWORD joystick, DWORD n)
{
    dw(self, 4) = n;
    const DWORD bytes = words(n) << 2;
    dw(self, 0) = (DWORD)(uintptr_t)heap_alloc(bytes);
    fn<DWORD(THISCALL *)(void *)>(0x46ee00)(self);
    self[8] = 0;
    self[9] = 0;
    dw(self, 0x10) = n;
    dw(self, 0xc) = (DWORD)(uintptr_t)heap_alloc(bytes);
    fn<DWORD(THISCALL *)(void *)>(0x46ee00)(self + 0xc);
    self[0x14] = 0;
    self[0x15] = 0;
    dw(self, 0x20) = 0;
    dw(self, 0x24) = 0;
    dw(self, 0x28) = 0;
    dw(self, 0x1c) = vt(0x5ee138);
    dw(self, 0x30) = 0;
    dw(self, 0x34) = 0;
    dw(self, 0x38) = 0;
    dw(self, 0x2c) = vt(0x5ee138);
    dw(self, 0x68) = n;
    dw(self, 0x64) = (DWORD)(uintptr_t)heap_alloc(bytes);
    fn<DWORD(THISCALL *)(void *)>(0x46ee00)(self + 0x64);
    self[0x6c] = 0;
    self[0x6d] = 0;
    Binding_Init(self + 0x78);
    dw(self, 0x18) = 0;
    self[0x1bc] = (BYTE)use_cfg;
    dw(self, 0x1c0) = player;
    self[0x75] = 0;
    dw(self, 0x3c) = n;
    BYTE *b = static_cast<BYTE *>(heap_alloc(n << 6));
    if (b && (int)(n - 1) >= 0)
        for (DWORD i = 0; i < n; i++)
            fn<BYTE *(THISCALL *)(void *)>(0x46ef50)(b + i * BINDING);
    dw(self, 0x40) = (DWORD)(uintptr_t)b;
    dw(self, 0x44) = 0;
    dw(self, 0x48) = 0;
    dw(self, 0x70) = 0;
    self[0x1c5] = 1;
    char key[0x108];
    key[0] = 0;
    player_prefix(self, key);
    str_copy(key + str_length(key), glob<const char *>(0x60fc44) /* "Controler ID Number" */);
    const DWORD id = ini_get_int(glob_addr(INI_SECTION), key, joystick + 1) - 1;
    fn<void(THISCALL *)(void *, DWORD)>(0x470d20)(self, id);
    dw(self, 0x1b8) = time_ms() + 2000;
    self[0x4f] = 1;
    self[0x4c] = !dw(self, 0x1c0) && self[0x1bc];
    self[0x4d] = id != 0xffffffffu;
    self[0x4e] = 0;
    Bitset_Clear(self);
    dw(self, 0x5c) = 0;
    dw(self, 0x60) = 0;
    Bitset_Clear(self + 0x64);
    self[0x74] = 1;
    self[0x1c4] = 1;
    BYTE *keys = self + 0x1c;
    DWORD v = 1;
    if (array_find(keys, &v, 0xffffffff) == 0xffffffffu) {
        DWORD w = v;
        vcall<DWORD>(keys, 2, &w);
    }
    v = 0x3b;
    if (array_find(keys, &v, 0xffffffff) == 0xffffffffu) {
        DWORD w = v;
        vcall<DWORD>(keys, 2, &w);
    }
    if (array_index(keys, 0x44) == 0xffffffffu) {
        DWORD w = 0x44;
        vcall<DWORD>(keys, 2, &w);
    }
    return self;
}
// 0x46f2f0: the blocks freed, the joystick deleted (its name first), the two arrays emptied (their data to
// the pool).
void THISCALL Config_Dtor(BYTE *self)
{
    free_block(self, 0x40);
    free_block(self, 0x44);
    free_block(self, 0x48);
    if (BYTE *j = bp(dw(self, 0x70))) {
        zs_dtor(j + 0x48);
        heap_free(j);
        dw(self, 0x70) = 0;
    }
    free_block(self, 0x64);
    const DWORD arrays[2] = {0x2c, 0x1c};
    for (DWORD a : arrays) {
        dw(self, a) = vt(0x5ee15c);
        if (dw(self, a + 4)) {
            pool_get_free(bp(dw(self, a + 4)), dw(self, a + 0xc) << 2);
            dw(self, a + 4) = 0;
            dw(self, a + 0xc) = 0;
        }
        dw(self, a + 8) = 0;
    }
    free_block(self, 0xc);
    free_block(self, 0);
}
// 0x46f3e0 (key): added to the keys that may not be bound unless it is there. eax: its index or the
// array's Add's.
DWORD THISCALL Config_ForbidKey(BYTE *self, DWORD key)
{
    BYTE *keys = self + 0x1c;
    const DWORD i = array_index(keys, key);
    if (i != 0xffffffffu)
        return i;
    return vcall<DWORD>(keys, 2, &key);
}
// 0x46f430 (binding; the INI key's second word, its first word, kind, +0x14, +0x18, the byte +0x20).
// eax: the binding's offset.
DWORD THISCALL Config_SetBinding(BYTE *self, DWORD i, DWORD word2, DWORD word1, DWORD kind, DWORD a5, DWORD a6, DWORD a7)
{
    const DWORD o = i << 6;
    BYTE *b = bp(dw(self, 0x40)) + o;
    dw(b, 0x24) = word1;
    dw(b, 0x28) = word2;
    b[0x20] = (BYTE)a7;
    dw(b, 0x10) = kind;
    dw(b, 0x14) = a5;
    dw(b, 0x18) = a6;
    return o;
}
// 0x46f480 (binding, item), 0x46f4a0 (binding): the item a binding uses (+0x2c). eax: the offset, the
// item.
DWORD THISCALL Config_SetItem(BYTE *self, DWORD i, DWORD item)
{
    dw(binding(self, 0), (i << 6) + 0x2c) = item;
    return i << 6;
}
DWORD THISCALL Config_GetItem(BYTE *self, DWORD i) { return dw(binding(self, i), 0x2c); }

// 0x46f5c0: for every binding (kind 2 left out) what the others share: joystick bits (+0x34), mouse bits
// (+0x38) and the second key of one with the same key (+0x3c). eax: the count.
DWORD THISCALL Config_FindShared(BYTE *self)
{
    const DWORD n = dw(self, 0x3c);
    for (DWORD i = 0; i < dw(self, 0x3c); i++) {
        BYTE *b = binding(self, i);
        dw(b, 0x34) = 0;
        dw(b, 0x38) = 0;
        dw(b, 0x3c) = 0;
        if (dw(b, 0x10) == 2)
            continue;
        for (DWORD j = 0; j < dw(self, 0x3c); j++) {
            if (j == i)
                continue;
            BYTE *o = binding(self, j);
            const DWORD m = dw(b, 8) & dw(o, 8);
            if (m)
                dw(b, 0x34) |= dw(o, 8) ^ m;
            const DWORD m2 = dw(b, 0xc) & dw(o, 0xc);
            if (m2)
                dw(b, 0x38) |= dw(o, 0xc) ^ m2;
            if (dw(b, 0) == dw(o, 0))
                dw(b, 0x3c) = dw(o, 4);
        }
    }
    (void)n;
    return dw(self, 0x3c);
}
// 0x46f4c0 (binding, key, second key, joystick bits, mouse bits): the keys and bits taken from every other
// binding that has the same (the callback +0x18 told of each that lost something); a binding cleared
// completely tells too; then set, the shares found again.
void THISCALL Config_SetKeys(BYTE *self, DWORD i, DWORD key, DWORD key2, DWORD joy, DWORD mouse)
{
    using Callback = void(__fastcall *)(BYTE *, DWORD);
    for (DWORD j = 0; j < dw(self, 0x3c); j++) {
        if (i == j)
            continue;
        BYTE *o = binding(self, j);
        bool lost = false;
        if (dw(o, 0) == key && dw(o, 4) == key2) {
            dw(o, 0) = 0;
            dw(o, 4) = 0;
            if (key)
                lost = true;
        }
        if (dw(o, 8) == joy) {
            dw(o, 8) = 0;
            if (joy)
                lost = true;
        }
        if (dw(o, 0xc) == mouse) {
            dw(o, 0xc) = 0;
            if (mouse)
                lost = true;
        }
        if (lost && dw(self, 0x18))
            reinterpret_cast<Callback>(bp(dw(self, 0x18)))(self, j);
    }
    BYTE *b = binding(self, i);
    if ((dw(b, 0) || dw(b, 4) || dw(b, 8) || dw(b, 0xc)) && !key && !key2 && !joy && !mouse && dw(self, 0x18))
        reinterpret_cast<Callback>(bp(dw(self, 0x18)))(self, i);
    dw(binding(self, 0), (i << 6)) = key;
    dw(binding(self, 0), (i << 6) + 4) = key2;
    dw(binding(self, 0), (i << 6) + 8) = joy;
    dw(binding(self, 0), (i << 6) + 0xc) = mouse;
    fn<DWORD(THISCALL *)(void *)>(0x46f5c0)(self);
}
void set_keys(BYTE *self, DWORD i, DWORD key, DWORD key2, DWORD joy, DWORD mouse)
{
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x46f4c0)(self, i, key, key2, joy, mouse);
}
// 0x46f680 (binding, out "cancelled"): one step of capturing a binding. The first step keeps the binding
// (+0x78) and clears it. The key pressed (other than the binding's), the joystick's bits and, with the
// mouse allowed (+0x4c), the mouse buttons (not those forbidden at +0x2c) and the wheel (bits 8 and
// 0x10 for a kind 1 binding; an error sound otherwise or when forbidden): Delete (0x153) clears the
// binding, Escape (1) puts the old one back ("cancelled"), a forbidden key sounds an error, another key
// becomes the binding's key (the old one its second key). The capture ends (al 1) when all keys are up,
// the joystick released or the mouse buttons released, with the binding set through 0x46f4c0.
DWORD THISCALL Config_Capture(BYTE *self, DWORD i, BYTE *cancelled)
{
    *cancelled = 0;
    BYTE *b = binding(self, i);
    DWORD mouse = 0;
    if (!self[0x75]) {
        memcpy(self + 0x78, b, BINDING);
        self[0x75] = 1;
        fn<void(THISCALL *)(void *, DWORD)>(0x4e2980)(glob<void *>(KEYS), 0);
        dw(b, 0) = 0;
        dw(b, 4) = 0;
        dw(b, 8) = 0;
        dw(b, 0xc) = 0;
    }
    const DWORD key = fn<DWORD(THISCALL *)(void *, DWORD)>(0x4e2940)(glob<void *>(KEYS), dw(b, 0));
    DWORD joy = 0;
    if (dw(self, 0x70))
        joy = fn<DWORD(THISCALL *)(void *)>(0x4ddb80)(bp(dw(self, 0x70))) & 0x0fffffff;
    if (self[0x4c]) {
        mouse = fn<DWORD(THISCALL *)(void *)>(0x51f450)(glob<void *>(0x6cff40));
        if (array_index(self + 0x2c, 1) != 0xffffffffu)
            mouse &= ~1u;
        if (array_index(self + 0x2c, 2) != 0xffffffffu)
            mouse &= ~2u;
        const int wheel = (int)fn<DWORD(THISCALL *)(void *)>(0x518520)(glob<void *>(0x6cff40));
        if (dw(b, 0x10) == 1) {
            if (wheel < 0) {
                if (array_index(self + 0x2c, 8) != 0xffffffffu)
                    player_error_sound();
                else
                    mouse |= 8;
            } else if (wheel > 0) {
                if (array_index(self + 0x2c, 0x10) != 0xffffffffu)
                    player_error_sound();
                else
                    mouse |= 0x10;
            }
        } else if (wheel) {
            player_error_sound();
        }
    }
    if (key == 0x153) {
        set_keys(self, i, 0, 0, 0, 0);
        if (dw(self, 0x18))
            reinterpret_cast<void(__fastcall *)(BYTE *, DWORD)>(bp(dw(self, 0x18)))(self, i);
        self[0x75] = 0;
        return 1;
    }
    if (key == 1) {
        memcpy(binding(self, i), self + 0x78, BINDING);
        self[0x75] = 0;
        *cancelled = 1;
        return 1;
    }
    if (key) {
        if (array_index(self + 0x1c, key) != 0xffffffffu) {
            player_error_sound();
        } else if (!dw(b, 0)) {
            dw(b, 0) = key;
        } else if (!dw(b, 4)) {
            dw(b, 4) = dw(b, 0);
            dw(b, 0) = key;
        }
    }
    if (!fn<DWORD(THISCALL *)(void *)>(0x4e2870)(glob<void *>(KEYS)) && dw(b, 0)) {
        set_keys(self, i, dw(b, 0), dw(b, 4), dw(self, 0x80), dw(self, 0x84));
        self[0x75] = 0;
        return 1;
    }
    dw(b, 8) |= joy;
    if (dw(b, 8) && !joy) {
        set_keys(self, i, dw(self, 0x78), dw(self, 0x7c), dw(b, 8), dw(self, 0x84));
        self[0x75] = 0;
        return 1;
    }
    dw(b, 0xc) |= mouse;
    if (dw(b, 0xc) && !mouse) {
        set_keys(self, i, dw(self, 0x78), dw(self, 0x7c), dw(self, 0x80), dw(b, 0xc));
        self[0x75] = 0;
        return 1;
    }
    return 0;
}
// 0x46f980, 0x46fa40: the bindings copied to +0x44, +0x48; 0x46fa10, 0x46fad0: copied back
void THISCALL Config_Save1(BYTE *self)
{
    free_block(self, 0x44);
    dw(self, 0x44) = (DWORD)(uintptr_t)copy_bindings(self);
}
void THISCALL Config_Save2(BYTE *self)
{
    free_block(self, 0x48);
    dw(self, 0x48) = (DWORD)(uintptr_t)copy_bindings(self);
}
void THISCALL Config_Restore1(BYTE *self) { memcpy(bp(dw(self, 0x40)), bp(dw(self, 0x44)), dw(self, 0x3c) << 6); }
void THISCALL Config_Restore2(BYTE *self) { memcpy(bp(dw(self, 0x40)), bp(dw(self, 0x48)), dw(self, 0x3c) << 6); }
// 0x46fb00: a binding's keys or bits differ from the second copy (+0x48). al.
DWORD THISCALL Config_Changed(BYTE *self)
{
    for (DWORD i = 0; i < dw(self, 0x3c); i++)
        if (memcmp(binding(self, i), bp(dw(self, 0x48)) + i * BINDING, 16) != 0)
            return 1;
    return 0;
}
// The section: the given name, or "[Player %d ]<joystick name> Control Configuration"
void make_section(BYTE *self, void *section, const char *name)
{
    if (name) {
        zs_set_text(section, name);
        return;
    }
    char *buf = zs_get_buffer(section, 0x200);
    buf[0] = 0;
    player_prefix(self, buf);
    crt_sprintf(buf + str_length(buf), glob_addr(0x60fc78) /* "%s Control Configuration" */, self + 0xb8);
    zs_release_buffer(section, 0xffffffff);
}
// 0x46fb60 (section name or null): with Zax.CFG, every binding as "<word2> <word1>" = "key,key2,joy,mouse"
void THISCALL Config_WriteIni(BYTE *self, const char *name)
{
    if (!self[0x1bc])
        return;
    alignas(4) BYTE section[4], key[4], value[4];
    zs_ctor(section);
    zs_ctor(key);
    zs_ctor(value);
    make_section(self, section, name);
    for (DWORD i = 0; i < dw(self, 0x3c); i++) {
        BYTE *b = binding(self, i);
        zs_format(key, glob_addr(0x60fc70) /* "%s %s" */, dw(b, 0x28), dw(b, 0x24));
        zs_format(value, glob_addr(0x60fc64) /* "%d,%d,%d,%d" */, dw(b, 0), dw(b, 4), dw(b, 8), dw(b, 0xc));
        const char *k = zs_c_str(key);
        const char *s = zs_c_str(section);
        ini_write_string(s, k, zs_c_str(value));
    }
    zs_dtor(value);
    zs_dtor(key);
    zs_dtor(section);
}
// 0x46fcc0 (section name or null): with Zax.CFG, the joystick's name (or "NO JOYSTICK", 0x6bd568) kept
// at +0xb8; every binding read back where its key is there (sscanf "%d,%d,%d,%d"; fields it does not find
// keep what the previous binding had); the shares found, the second copy made.
void THISCALL Config_ReadIni(BYTE *self, const char *name)
{
    if (!self[0x1bc])
        return;
    alignas(4) BYTE section[4], key[4], value[4];
    zs_ctor(section);
    zs_ctor(key);
    zs_ctor(value);
    str_copy(reinterpret_cast<char *>(self + 0xb8), dw(self, 0x70) ? joystick_name(bp(dw(self, 0x70))) : zs_c_str(glob<void *>(0x6bd568)));
    make_section(self, section, name);
    DWORD v[4];
    for (DWORD i = 0; i < dw(self, 0x3c); i++) {
        BYTE *b = binding(self, i);
        zs_format(key, glob_addr(0x60fc70) /* "%s %s" */, dw(b, 0x28), dw(b, 0x24));
        fn<void(THISCALL *)(void *)>(0x4df2b0)(value);
        const char *k = zs_c_str(key);
        const char *s = zs_c_str(section);
        ini_get_string(glob<void *>(INI), s, k, zs_get_buffer(value, 0x200), 0x200);
        zs_release_buffer(value, 0xffffffff);
        if (fn<DWORD(THISCALL *)(void *)>(0x4e13e0)(value) & 0xff) {
            crt_sscanf(zs_get_buffer(value, 0x200), glob_addr(0x60fc64) /* "%d,%d,%d,%d" */, &v[0], &v[1], &v[2], &v[3]);
            dw(b, 0) = v[0];
            dw(b, 0xc) = v[3];
            zs_release_buffer(value, 0xffffffff);
            dw(b, 4) = v[1];
            dw(b, 8) = v[2];
        }
    }
    fn<DWORD(THISCALL *)(void *)>(0x46f5c0)(self);
    free_block(self, 0x48);
    dw(self, 0x48) = (DWORD)(uintptr_t)copy_bindings(self);
    zs_dtor(value);
    zs_dtor(key);
    zs_dtor(section);
}
// 0x46ff70: active, every key state cleared. 0x46ff90: inactive.
DWORD THISCALL Config_Activate(BYTE *self)
{
    self[0x1c4] = 1;
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4e27a0)(glob<void *>(KEYS), 0);
}
void THISCALL Config_Deactivate(BYTE *self) { self[0x1c4] = 0; }
// 0x46ffa0: the next joystick check in 2 s; a joystick is initialised again (CJoystick::Init with its
// device); when its name changed the bindings are written and read again. al: changed.
DWORD THISCALL Config_CheckJoystick(BYTE *self)
{
    dw(self, 0x1b8) = time_ms() + 2000;
    BYTE *j = bp(dw(self, 0x70));
    if (!j)
        return 0;
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4dda00)(j, dw(j, 0));
    const char *now = joystick_name(bp(dw(self, 0x70)));
    const char *was = reinterpret_cast<const char *>(self + 0xb8);
    DWORD k = 0;
    while (was[k] == now[k] && was[k])
        k++;
    if (was[k] == now[k])
        return 0;
    fn<void(THISCALL *)(void *, const char *)>(0x46fb60)(self, nullptr);
    fn<void(THISCALL *)(void *, const char *)>(0x46fcc0)(self, nullptr);
    return 1;
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(control_config)
{
    const Hook list[] = {
#include "control_config_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
