// The network packet of the multiplayer code and the small classes around it (layouts and the
// functions left out: net_packet.h).
#include "net_packet.h"
#include "exe_base.h"

namespace zax {

namespace {

constexpr DWORD READERS_VTABLE = 0x5fb580, TOKENS_VTABLE = 0x5fb574, READER_VTABLE = 0x5f1dd0;
constexpr DWORD SESSION_VTABLE = 0x5fb568, NAMED_VALUE_VTABLE = 0x5fb564;
constexpr DWORD EMPTY = 0x62e558;   // ""

void zstring_ctor(ZString *s) { zax::fn<void(THISCALL *)(ZString *)>(0x4dec90)(s); }
void zstring_dtor(void *s) { zax::fn<void(THISCALL *)(void *)>(0x4defd0)(s); }
DWORD zstring_assign(ZString *s, const char *t) { return zax::fn<DWORD(THISCALL *)(ZString *, const char *)>(0x4e1930)(s, t); }

// The token map's methods (thiscall on the map at +0x28)
DWORD token_translate(ZPtrArray *map, void *table, DWORD id) { return zax::fn<DWORD(THISCALL *)(ZPtrArray *, void *, DWORD)>(0x55d1f0)(map, table, id); }
// 0x55d200 only sets al; the original returns the whole eax it leaves, so the call goes through a
// helper that loads eax first (the value the original has in eax at that point) and returns all of it
extern "C" DWORD __cdecl zax_call_with_eax(DWORD fn, void *self, DWORD eax, void *a, DWORD b);
__asm__(".text\n"
        ".globl _zax_call_with_eax\n"
        "_zax_call_with_eax:\n"
        "  movl 20(%esp), %eax\n"
        "  pushl %eax\n"
        "  movl 20(%esp), %eax\n"
        "  pushl %eax\n"
        "  movl 16(%esp), %ecx\n"
        "  movl 20(%esp), %eax\n"
        "  calll *12(%esp)\n"
        "  retl\n");
DWORD token_known(ZPtrArray *map, void *table, DWORD id, DWORD eax) { return zax_call_with_eax(fn_addr(0x55d200), map, eax, table, id); }
void token_remember(ZPtrArray *map, void *table, DWORD id) { zax::fn<void(THISCALL *)(ZPtrArray *, void *, DWORD)>(0x46cd50)(map, table, id); }

DWORD text_size(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n + 1;
}

}  // namespace

// ---- Packet ------------------------------------------------------------------------------------------

void NetPacket::Grow(DWORD n)
{
    if (capacity >= length + n)
        return;
    DWORD c = capacity + n;
    if (capacity * 2 > c)
        c = capacity * 2;
    capacity = c;
    BYTE *p = (BYTE *)heap_alloc(c);
    memcpy(p, data, length);
    if (data) {
        heap_free(data);
        data = nullptr;
    }
    data = p;
}

NetPacket *THISCALL NetPacket::Ctor()
{
    readers.vftable = vt(READERS_VTABLE);
    readers.data = nullptr;
    readers.count = 0;
    readers.capacity = 0;
    unknown30 = 0;
    ZPtrArray *t = (ZPtrArray *)heap_alloc(0x10);
    if (t) {
        t->vftable = vt(TOKENS_VTABLE);
        t->data = nullptr;
        t->count = 0;
        t->capacity = 0;
    }
    tokens = t;
    position = 0;
    length = 0;
    consumed = 0;
    capacity = 0;
    data = nullptr;
    current = nullptr;
    reliable = 0;
    return this;
}

void THISCALL NetPacket::Dtor()
{
    if (ZPtrArray *t = tokens) {
        t->vftable = vt(TOKENS_VTABLE);
        if (t->data) {
            heap_free(t->data);
            t->data = nullptr;
            t->capacity = 0;
        }
        t->count = 0;
        heap_free(t);
        tokens = nullptr;
    }
    if (data) {
        heap_free(data);
        data = nullptr;
    }
    readers.vftable = vt(READERS_VTABLE);
    if (readers.data) {
        heap_free(readers.data);
        readers.data = nullptr;
        readers.capacity = 0;
    }
    readers.count = 0;
}

// The reader gets its name and is appended (slot 2 with the address of the argument); slot 2's result
DWORD THISCALL NetPacket::RegisterReader(BlockReader *reader, const char *name)
{
    reader->name = name;
    return vcall<DWORD>(&readers, 2, &reader);
}

// Kinds 0, 1 and 4 mark the packet reliable. An unregistered reader writes nothing and returns the
// reader count; otherwise the result is the new length.
DWORD THISCALL NetPacket::WriteBlockType(BlockReader *reader, DWORD kind)
{
    if (kind == 0 || kind == 1 || kind == 4)
        reliable = 1;
    DWORD index = 0;
    for (;; index++) {
        if (index >= readers.count)
            return index;
        if (readers.data[index] == reader)
            break;
    }
    if (index == 0xffffffff)
        return index;
    if (index > 0xfe) {
        for (DWORD k = (index - 1) / 0xfe; k; k--) {
            Grow(1);
            data[length] = 0xff;
            length++;
            index -= 0xfe;
        }
    }
    Grow(1);
    data[length] = (BYTE)index;
    return ++length;
}

// The reader of the next block type (0xcf when no byte is left); slot 2 of the reader with the packet
// gives the result, without a reader the packet itself
DWORD THISCALL NetPacket::ReadBlock()
{
    DWORD extra = 0;
    BYTE b = 0xcf;
    if (position + 1 <= length) {
        b = data[position];
        position++;
        while (b == 0xff) {
            extra += 0xfe;
            if (position + 1 <= length) {
                b = data[position];
                position++;
            }
        }
    }
    BlockReader *r = (BlockReader *)readers.data[b + extra];
    current = r;
    if (r)
        return vcall<DWORD>(r, 2, this);
    return (DWORD)(uintptr_t)this;
}

// Null or n == 0: nothing, the pointer comes back; otherwise the old length
DWORD THISCALL NetPacket::WriteBytes(const void *p, DWORD n)
{
    if (!p || !n)
        return (DWORD)(uintptr_t)p;
    Grow(n);
    const DWORD at_ = length;
    length = at_ + n;
    memcpy(data + at_, p, n);
    return at_;
}

DWORD THISCALL NetPacket::Skip(DWORD n)
{
    const DWORD p = position + n;
    if (length >= p)
        position = p;
    return p;
}

// The reads: al = 1 with the new position in the upper bytes, al = 0 with the old one
DWORD THISCALL NetPacket::ReadByte(BYTE *out)
{
    if (length < position + 1)
        return position & 0xffffff00;
    *out = data[position];
    return (++position & 0xffffff00) | 1;
}

DWORD THISCALL NetPacket::Read16(DWORD *out)
{
    if (length < position + 0x10)
        return position & 0xffffff00;
    const DWORD *s = (const DWORD *)(data + position);
    out[0] = s[0];
    out[1] = s[1];
    out[2] = s[2];
    out[3] = s[3];
    position += 0x10;
    return (position & 0xffffff00) | 1;
}

DWORD THISCALL NetPacket::ReadWord(WORD *out)
{
    if (length < position + 2)
        return position & 0xffffff00;
    *out = *(const WORD *)(data + position);
    position += 2;
    return (position & 0xffffff00) | 1;
}

DWORD THISCALL NetPacket::ReadDword(DWORD *out)
{
    if (length < position + 4)
        return position & 0xffffff00;
    *out = *(const DWORD *)(data + position);
    position += 4;
    return (position & 0xffffff00) | 1;
}

// Without a byte left *out becomes 1 (the 0xcf default); the upper bytes are the old position
DWORD THISCALL NetPacket::ReadBool(BYTE *out)
{
    const DWORD p = position;
    if (length < p + 1) {
        *out = 1;
        return p & 0xffffff00;
    }
    const BYTE b = data[p];
    position = p + 1;
    *out = b != 0;
    return (p & 0xffffff00) | 1;
}

// The token reads: the value goes through the token map (0x55d1f0); the upper bytes of a success are
// the map's result
DWORD THISCALL NetPacket::ReadDwordToken(DWORD *out, void *table)
{
    if (length < position + 4)
        return position & 0xffffff00;
    *out = *(const DWORD *)(data + position);
    position += 4;
    const DWORD r = token_translate(tokens, table, *out);
    *out = r;
    return (r & 0xffffff00) | 1;
}

DWORD THISCALL NetPacket::ReadByteToken(BYTE *out, void *table)
{
    if (length < position + 1)
        return position & 0xffffff00;
    *out = data[position];
    position++;
    const DWORD r = token_translate(tokens, table, *out);
    *out = (BYTE)r;
    return (r & 0xffffff00) | 1;
}

DWORD THISCALL NetPacket::ReadWordToken(WORD *out, void *table)
{
    if (length < position + 2)
        return position & 0xffffff00;
    *out = *(const WORD *)(data + position);
    position += 2;
    const DWORD r = token_translate(tokens, table, *out);
    *out = (WORD)r;
    return (r & 0xffffff00) | 1;
}

// The text at the position (its end is searched without a bound); "" if it does not end inside
const char *THISCALL NetPacket::ReadText()
{
    const char *s = (const char *)data + position;
    const DWORD p = position + text_size(s);
    if (length < p)
        return glob<const char *>(EMPTY);
    position = p;
    return s;
}

// Like ReadText into a ZString; always al = 1 with the upper bytes of the assignment's result
DWORD THISCALL NetPacket::ReadString(ZString *out)
{
    const char *s = (const char *)data + position;
    const DWORD p = position + text_size(s);
    if (length < p)
        return (zstring_assign(out, glob<const char *>(EMPTY)) & 0xffffff00) | 1;
    position = p;
    return (zstring_assign(out, s) & 0xffffff00) | 1;
}

DWORD THISCALL NetPacket::Reset()
{
    consumed += position;
    length = 0;
    position = 0;
    reliable = 0;
    return 0;
}

// The text with its terminator (null writes ""); the new length
DWORD THISCALL NetPacket::WriteText(const char *s)
{
    if (!s)
        s = glob<const char *>(EMPTY);
    const DWORD n = text_size(s);
    Grow(n);
    memcpy(data + length, s, n);
    return length += n;
}

// The writes return the offset they wrote at
DWORD THISCALL NetPacket::WriteByte(BYTE v)
{
    Grow(1);
    data[length] = v;
    return length++;
}

DWORD THISCALL NetPacket::Write16(const DWORD *p)
{
    Grow(0x10);
    DWORD *d = (DWORD *)(data + length);
    d[0] = p[0];
    d[1] = p[1];
    d[2] = p[2];
    d[3] = p[3];
    length += 0x10;
    return length - 0x10;
}

DWORD THISCALL NetPacket::WriteDword(DWORD v)
{
    Grow(4);
    *(DWORD *)(data + length) = v;
    length += 4;
    return length - 4;
}

DWORD THISCALL NetPacket::WriteWord(WORD v)
{
    Grow(2);
    *(WORD *)(data + length) = v;
    length += 2;
    return length - 2;
}

DWORD THISCALL NetPacket::WriteBool(BYTE v)
{
    Grow(1);
    data[length] = v != 0;
    return length++;
}

// The token writes: the id, then, if the token map does not know it (never: 0x55d200 returns 1), its
// name from slot 1 of the table ("" for null) with the terminator and the map's remember call; the
// result is then the name's size. A known token gives 0x55d200's eax: al = 1 with the upper bytes the
// original had in eax before the call (dword: the address written to, byte: the argument, word: the
// new length).
DWORD THISCALL NetPacket::WriteDwordToken(DWORD v, void *table)
{
    Grow(4);
    BYTE *const dst = data + length;
    *(DWORD *)dst = v;
    length += 4;
    const DWORD known = token_known(tokens, table, v, (DWORD)(uintptr_t)dst);
    if (known & 0xff)
        return known;
    const char *s = vcall<const char *>(table, 1, v);
    if (!s)
        s = glob<const char *>(EMPTY);
    const DWORD n = text_size(s);
    Grow(n);
    memcpy(data + length, s, n);
    length += n;
    token_remember(tokens, table, v);
    return n;
}

DWORD THISCALL NetPacket::WriteByteToken(DWORD v, void *table)
{
    Grow(1);
    data[length] = (BYTE)v;
    length++;
    const DWORD id = v & 0xff;
    const DWORD known = token_known(tokens, table, id, v);
    if (known & 0xff)
        return known;
    const char *s = vcall<const char *>(table, 1, id);
    if (!s)
        s = glob<const char *>(EMPTY);
    const DWORD n = text_size(s);
    Grow(n);
    memcpy(data + length, s, n);
    length += n;
    token_remember(tokens, table, id);
    return n;
}

DWORD THISCALL NetPacket::WriteWordToken(DWORD v, void *table)
{
    Grow(2);
    *(WORD *)(data + length) = (WORD)v;
    length += 2;
    const DWORD id = v & 0xffff;
    const DWORD known = token_known(tokens, table, id, length);
    if (known & 0xff)
        return known;
    const char *s = vcall<const char *>(table, 1, id);
    if (!s)
        s = glob<const char *>(EMPTY);
    const DWORD n = text_size(s);
    Grow(n);
    memcpy(data + length, s, n);
    length += n;
    token_remember(tokens, table, id);
    return n;
}

// Overwrite at an offset (no check); the address written to
DWORD THISCALL NetPacket::PatchDword(DWORD v, DWORD offset)
{
    *(DWORD *)(data + offset) = v;
    return (DWORD)(uintptr_t)(data + offset);
}

DWORD THISCALL NetPacket::PatchWord(WORD v, DWORD offset)
{
    *(WORD *)(data + offset) = v;
    return (DWORD)(uintptr_t)(data + offset);
}

// al: a byte left; the upper bytes are position + 1
DWORD THISCALL NetPacket::CanRead()
{
    const DWORD p = position + 1;
    return (p & 0xffffff00) | (length >= p ? 1 : 0);
}

DWORD THISCALL NetPacket::SkipToEnd()
{
    position = length;
    return length;
}

// ---- Block reader, session base, traffic graph, named value --------------------------------------

BlockReader *THISCALL BlockReader::Ctor()
{
    name = nullptr;
    vftable = vt(READER_VTABLE);
    return this;
}

NetSessionBase *THISCALL NetSessionBase::Ctor()
{
    zstring_ctor(&text);
    vftable = vt(SESSION_VTABLE);
    flag5 = 0;
    flag6 = 1;
    flag4 = 0;
    unknown8 = 0;
    flag18 = 0;
    void *p = heap_alloc(0x10);
    settings = p ? zax::fn<PlayerSettings *(THISCALL *)(void *)>(0x46ce20)(p) : nullptr;
    flag20 = 0;
    unknown34 = 1;
    unknown2c = 0;
    unknown30 = 0;
    unknown10 = 0;
    flag28 = 0;
    return this;
}

void THISCALL NetSessionBase::Dtor()
{
    vftable = vt(SESSION_VTABLE);
    if (PlayerSettings *s = settings) {
        zstring_dtor(s);
        heap_free(s);
        settings = nullptr;
    }
    zstring_dtor(&text);
}

NetSessionBase *THISCALL NetSessionBase::DeletingDtor(DWORD flags)
{
    Dtor();
    if (flags & 1)
        heap_free(this);
    return this;
}

void THISCALL NetSessionBase::ClearFlag18() { flag18 = 0; }

NetTrafficGraph *THISCALL NetTrafficGraph::Ctor()
{
    ++*glob<DWORD *>(NET_GRAPH_COUNT);
    samples = 0x50;
    height = 0x32;
    unknown10 = 1;
    int *s = (int *)heap_alloc(0x140);
    sent = s;
    memset(s, 0, samples * 4);
    int *r = (int *)heap_alloc(samples * 4);
    received = r;
    memset(r, 0, samples * 4);
    return this;
}

void THISCALL NetTrafficGraph::Dtor()
{
    --*glob<DWORD *>(NET_GRAPH_COUNT);
    if (sent) {
        heap_free(sent);
        sent = nullptr;
    }
    if (received) {
        heap_free(received);
        received = nullptr;
    }
}

NamedValue *THISCALL NamedValue::Ctor()
{
    zstring_ctor(&name);
    vftable = vt(NAMED_VALUE_VTABLE);
    value = 0;
    return this;
}

NamedValue *THISCALL NamedValue::DeletingDtor(DWORD flags)
{
    vftable = vt(NAMED_VALUE_VTABLE);
    zstring_dtor(&name);
    if (flags & 1)
        heap_free(this);
    return this;
}

DWORD THISCALL NamedValue::SetValue(DWORD v)
{
    if (value != v)
        value = v;
    return v;
}

ZString *THISCALL NamedValue::SetName(const char *s) { return (ZString *)(uintptr_t)zstring_assign(&name, s); }

// 0x51f6c0-0x51f6f0 jump to slots 24-27 of the object in ecx with the caller's stack (identical code
// folded, no direct callers)
extern "C" void __cdecl zax_jump_slot24();
extern "C" void __cdecl zax_jump_slot25();
extern "C" void __cdecl zax_jump_slot26();
extern "C" void __cdecl zax_jump_slot27();
__asm__(".text\n"
        ".globl _zax_jump_slot24\n"
        "_zax_jump_slot24:\n"
        "  movl (%ecx), %eax\n"
        "  jmpl *0x60(%eax)\n"
        ".globl _zax_jump_slot25\n"
        "_zax_jump_slot25:\n"
        "  movl (%ecx), %eax\n"
        "  jmpl *0x64(%eax)\n"
        ".globl _zax_jump_slot26\n"
        "_zax_jump_slot26:\n"
        "  movl (%ecx), %eax\n"
        "  jmpl *0x68(%eax)\n"
        ".globl _zax_jump_slot27\n"
        "_zax_jump_slot27:\n"
        "  movl (%ecx), %eax\n"
        "  jmpl *0x6c(%eax)\n");

// 0x5205c0: the packet's check of a marker (the save game stats pass 0xabababab), a stub in the
// release: al = 1, the upper bytes of eax are the caller's
extern "C" void __cdecl zax_net_packet_check_marker();
__asm__(".text\n"
        ".globl _zax_net_packet_check_marker\n"
        "_zax_net_packet_check_marker:\n"
        "  movb $1, %al\n"
        "  ret $4\n");

ZAX_HOOKS(net_packet)
{
    const Hook list[] = {
        {0x51f6c0, (void *)&zax_jump_slot24, "JumpSlot24", 5, {0x8b, 0x01, 0xff, 0x60, 0x60, 0xcc, 0xcc, 0xcc}},
        {0x51f6d0, (void *)&zax_jump_slot25, "JumpSlot25", 5, {0x8b, 0x01, 0xff, 0x60, 0x64, 0xcc, 0xcc, 0xcc}},
        {0x51f6e0, (void *)&zax_jump_slot26, "JumpSlot26", 5, {0x8b, 0x01, 0xff, 0x60, 0x68, 0xcc, 0xcc, 0xcc}},
        {0x51f6f0, (void *)&zax_jump_slot27, "JumpSlot27", 5, {0x8b, 0x01, 0xff, 0x60, 0x6c, 0xcc, 0xcc, 0xcc}},
        {0x51f700, ptr(&NamedValue::Ctor), "NamedValue::NamedValue", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x04, 0xe8, 0x85}},
        {0x51f720, ptr(&NamedValue::DeletingDtor), "NamedValue::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x04, 0xc7, 0x06}},
        {0x51f750, ptr(&NamedValue::SetValue), "NamedValue::SetValue", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x08, 0x3b}},
        {0x51f770, ptr(&NamedValue::SetName), "NamedValue::SetName", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xc1, 0x04, 0x50}},
        {0x51f780, ptr(&NetSessionBase::Ctor), "NetSessionBase::NetSessionBase", 7, {0x53, 0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x24, 0xe8}},
        {0x51f7e0, ptr(&NetSessionBase::DeletingDtor), "NetSessionBase::DeletingDtor", 7, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7e, 0x1c, 0xc7}},
        {0x51f830, ptr(&NetSessionBase::Dtor), "NetSessionBase::~NetSessionBase", 7, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7e, 0x1c, 0xc7}},
        {0x51f870, ptr(&NetSessionBase::ClearFlag18), "NetSessionBase::ClearFlag18", 5, {0xc6, 0x41, 0x18, 0x00, 0xc3, 0x90, 0x90, 0x90}},
        {0x51f8d0, ptr(&BlockReader::Ctor), "BlockReader::BlockReader", 9, {0x8b, 0xc1, 0xc7, 0x40, 0x04, 0x00, 0x00, 0x00}},
        {0x51f8e0, ptr(&NetTrafficGraph::Ctor), "NetTrafficGraph::NetTrafficGraph", 8, {0x56, 0x57, 0x8b, 0x3d, 0xe8, 0x00, 0x6d, 0x00}},
        {0x51f960, ptr(&NetTrafficGraph::Dtor), "NetTrafficGraph::~NetTrafficGraph", 6, {0x8b, 0x15, 0xe8, 0x00, 0x6d, 0x00, 0x56, 0x8b}},
        {0x51fe60, ptr(&NetPacket::Ctor), "NetPacket::NetPacket", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x6a, 0x10}},
        {0x51fec0, ptr(&NetPacket::Dtor), "NetPacket::~NetPacket", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x33, 0xdb, 0x8b}},
        {0x51ff30, ptr(&NetPacket::RegisterReader), "NetPacket::RegisterReader", 8, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x44, 0x24, 0x08}},
        {0x51ff50, ptr(&NetPacket::WriteBlockType), "NetPacket::WriteBlockType", 5, {0x8b, 0x44, 0x24, 0x08, 0x55, 0x56, 0x57, 0x85}},
        {0x5200d0, ptr(&NetPacket::ReadBlock), "NetPacket::ReadBlock", 5, {0x51, 0x8b, 0xc1, 0x56, 0x57, 0x33, 0xff, 0x8b}},
        {0x5203d0, ptr(&NetPacket::WriteBytes), "NetPacket::WriteBytes", 5, {0x8b, 0x44, 0x24, 0x04, 0x53, 0x85, 0xc0, 0x8b}},
        {0x520470, ptr(&NetPacket::Skip), "NetPacket::Skip", 7, {0x8b, 0x41, 0x1c, 0x8b, 0x54, 0x24, 0x04, 0x03}},
        {0x520490, ptr(&NetPacket::ReadByte), "NetPacket::ReadByte", 7, {0x8b, 0x41, 0x1c, 0x56, 0x8b, 0x71, 0x20, 0x8d}},
        {0x5204c0, ptr(&NetPacket::Read16), "NetPacket::Read16", 7, {0x8b, 0x41, 0x1c, 0x56, 0x8b, 0x71, 0x20, 0x8d}},
        {0x520510, ptr(&NetPacket::ReadWord), "NetPacket::ReadWord", 7, {0x8b, 0x41, 0x1c, 0x56, 0x8b, 0x71, 0x20, 0x8d}},
        {0x520550, ptr(&NetPacket::ReadDword), "NetPacket::ReadDword", 7, {0x8b, 0x41, 0x1c, 0x56, 0x8b, 0x71, 0x20, 0x8d}},
        {0x520580, ptr(&NetPacket::ReadBool), "NetPacket::ReadBool", 5, {0x8b, 0x41, 0x1c, 0x53, 0x56, 0x8b, 0x71, 0x20}},
        {0x5205d0, ptr(&NetPacket::ReadDwordToken), "NetPacket::ReadDwordToken", 7, {0x8b, 0x41, 0x1c, 0x56, 0x8b, 0x71, 0x20, 0x8d}},
        {0x520620, ptr(&NetPacket::ReadByteToken), "NetPacket::ReadByteToken", 7, {0x8b, 0x41, 0x1c, 0x56, 0x8b, 0x71, 0x20, 0x8d}},
        {0x520670, ptr(&NetPacket::ReadWordToken), "NetPacket::ReadWordToken", 7, {0x8b, 0x41, 0x1c, 0x56, 0x8b, 0x71, 0x20, 0x8d}},
        {0x5206c0, ptr(&NetPacket::ReadText), "NetPacket::ReadText", 5, {0x8b, 0xd1, 0x53, 0x56, 0x57, 0x8b, 0x5a, 0x1c}},
        {0x520700, ptr(&NetPacket::ReadString), "NetPacket::ReadString", 5, {0x8b, 0xd1, 0x53, 0x56, 0x57, 0x8b, 0x5a, 0x1c}},
        {0x520750, ptr(&NetPacket::Reset), "NetPacket::Reset", 6, {0x8b, 0x41, 0x1c, 0x8b, 0x51, 0x24, 0x03, 0xd0}},
        {0x520770, ptr(&NetPacket::WriteText), "NetPacket::WriteText", 5, {0x51, 0x8b, 0x44, 0x24, 0x08, 0x53, 0x55, 0x56}},
        {0x520820, ptr(&NetPacket::WriteByte), "NetPacket::WriteByte", 6, {0x53, 0x8b, 0xd9, 0x8b, 0x43, 0x20, 0x8b, 0x4b}},
        {0x5208a0, ptr(&NetPacket::Write16), "NetPacket::Write16", 6, {0x53, 0x8b, 0xd9, 0x8b, 0x43, 0x20, 0x8b, 0x4b}},
        {0x520940, ptr(&NetPacket::WriteDword), "NetPacket::WriteDword", 6, {0x53, 0x8b, 0xd9, 0x8b, 0x43, 0x20, 0x8b, 0x4b}},
        {0x5209c0, ptr(&NetPacket::WriteWord), "NetPacket::WriteWord", 6, {0x53, 0x8b, 0xd9, 0x8b, 0x43, 0x20, 0x8b, 0x4b}},
        {0x520a50, ptr(&NetPacket::WriteBool), "NetPacket::WriteBool", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x55, 0x8b, 0xe9}},
        {0x520ae0, ptr(&NetPacket::WriteDwordToken), "NetPacket::WriteDwordToken", 6, {0x83, 0xec, 0x08, 0x53, 0x8b, 0xd9, 0x55, 0x56}},
        {0x520c40, ptr(&NetPacket::WriteByteToken), "NetPacket::WriteByteToken", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x8b, 0xe9, 0x56}},
        {0x520da0, ptr(&NetPacket::WriteWordToken), "NetPacket::WriteWordToken", 7, {0x83, 0xec, 0x08, 0x8b, 0x44, 0x24, 0x0c, 0x53}},
        {0x520f10, ptr(&NetPacket::PatchDword), "NetPacket::PatchDword", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x08, 0x03}},
        {0x520f30, ptr(&NetPacket::PatchWord), "NetPacket::PatchWord", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x08, 0x03}},
        {0x520f50, ptr(&NetPacket::CanRead), "NetPacket::CanRead", 6, {0x8b, 0x41, 0x1c, 0x8b, 0x51, 0x20, 0x40, 0x3b}},
        {0x520f60, ptr(&NetPacket::SkipToEnd), "NetPacket::SkipToEnd", 6, {0x8b, 0x41, 0x20, 0x89, 0x41, 0x1c, 0xc3, 0x90}},
        {0x5205c0, (void *)&zax_net_packet_check_marker, "NetPacket::CheckMarker", 5, {0xb0, 0x01, 0xc2, 0x04, 0x00, 0x90, 0x90, 0x90}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
