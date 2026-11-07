// Memory stream reimplemented (see memory_stream.h).
#include "memory_stream.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies
static void stream_ctor(ZStream *s) { fn<ZStream *(THISCALL *)(ZStream *)>(0x4a23a0)(s); }
static DWORD stream_dtor(ZStream *s) { return fn<DWORD(THISCALL *)(ZStream *)>(0x4a2410)(s); }
static void reserve(MemoryStream *s, DWORD n) { fn<void(THISCALL *)(MemoryStream *, DWORD)>(0x506250)(s, n); }
// The constructors call Close through the vtable's memory (call [0x5f9e20]), not virtually
static void close_direct(MemoryStream *s) { (*at<void(THISCALL **)(MemoryStream *)>(vt(MEMORY_STREAM_VTABLE) + 4 * STREAM_CLOSE))(s); }

// The fields as the constructors and destructors clear them
static void clear(MemoryStream *s)
{
    s->vftable = vt(MEMORY_STREAM_VTABLE);
    s->position = s->size = s->capacity = 0;
    s->owns = 0;
    s->data = nullptr;
}
static void free_data(MemoryStream *s)
{
    if (s->owns && s->data)
        heap_free(s->data);
    s->data = nullptr;
    s->owns = 0;
}

MemoryStream *THISCALL MemoryStream::Ctor()
{
    stream_ctor(this);
    clear(this);
    close_direct(this);
    position = size = capacity = 0;
    owns = 0;
    reserve(this, 0);
    return this;
}

MemoryStream *THISCALL MemoryStream::CtorData(BYTE *p, DWORD n)
{
    stream_ctor(this);
    clear(this);
    close_direct(this);
    position = 0;
    size = capacity = n;
    owns = 1;
    data = p;
    return this;
}

DWORD THISCALL MemoryStream::Dtor()
{
    vftable = vt(MEMORY_STREAM_VTABLE);
    position = size = capacity = 0;
    free_data(this);
    return stream_dtor(this);
}

MemoryStream *THISCALL MemoryStream::DeletingDtor(DWORD flags)
{
    Dtor();
    if (flags & 1)
        pool_free(this, sizeof(MemoryStream));
    return this;
}

// 0x506110: eax keeps what the caller left in it (al = 0) unless the stream owned data
void THISCALL MemoryStream::Close()
{
    position = size = capacity = 0;
    free_data(this);
}

// 0x506250: grows to at least n (and at least 1.5 times and 4 more than before) if the stream owns its
// buffer or has none; the old bytes up to the size are copied
void THISCALL MemoryStream::Reserve(DWORD n)
{
    if (capacity >= n || (capacity && !owns))
        return;
    DWORD grown = capacity + (capacity >> 1);
    if (grown < capacity + 4)
        grown = capacity + 4;
    if (n < grown)
        n = grown;
    BYTE *p = static_cast<BYTE *>(heap_alloc(n));
    capacity = n;
    if (size)
        memcpy(p, data, size);
    if (data) {
        heap_free(data);
        data = nullptr;
    }
    data = p;
    owns = 1;
}

void THISCALL MemoryStream::Reset(DWORD n)
{
    vcall<void>(this, STREAM_CLOSE);
    position = size = capacity = 0;
    owns = 0;
    reserve(this, n);
}

// 0x506040: grows the size to the end of the write
DWORD THISCALL MemoryStream::Write(const void *p, DWORD n)
{
    if (position + n > size) {
        reserve(this, position + n);
        size = position + n;
    }
    if (n) {
        memcpy(data + position, p, n);
        position += n;
    }
    return n;
}

DWORD THISCALL MemoryStream::Read(void *p, DWORD n)
{
    if (position + n > size)
        n = size - position;
    if (n) {
        memcpy(p, data + position, n);
        position += n;
    }
    return n;
}

// 0x5060e0/0x506100: always true, the upper bytes are those of the argument
DWORD THISCALL MemoryStream::Skip(DWORD delta)
{
    position += delta;
    return (delta & 0xffffff00) | 1;
}

DWORD THISCALL MemoryStream::Seek(DWORD p)
{
    position = p;
    return (p & 0xffffff00) | 1;
}

// 0x506140: the bytes up to the next '\r' (or the end) if they fit into n - 1; then one '\r' and one
// '\n' are skipped
DWORD THISCALL MemoryStream::ReadLine(char *p, DWORD n)
{
    DWORD self = (DWORD)(uintptr_t)this & 0xffffff00;
    if (position >= size)
        return self;
    const BYTE *start = data + position, *end = data + size, *c = start;
    while (*c != '\r' && c < end)
        c++;
    DWORD length = (DWORD)(c - start);
    if (length + 1 >= n)
        return self;
    memcpy(p, start, length);
    p[length] = 0;
    position += length;
    if (position < size && data[position] == '\r')
        position++;
    if (position < size && data[position] == '\n')
        position++;
    return self | 1;
}

BYTE *THISCALL MemoryStream::Advance(DWORD n)
{
    BYTE *r = data + position;
    position += n;
    return r;
}

// 0x506230: hands the buffer over (data is cleared before Close, so Close does not delete it)
BYTE *THISCALL MemoryStream::Detach()
{
    BYTE *r = data;
    data = nullptr;
    vcall<void>(this, STREAM_CLOSE);
    return r;
}

// 0x5062d0: same size (slot 7 of both) and the same bytes; other is taken as a memory stream. The
// upper bytes of a false result: this size if the sizes differ, this data if only other has none.
DWORD THISCALL MemoryStream::Equals(MemoryStream *other)
{
    DWORD a = vcall<DWORD>(other, STREAM_SIZE);
    DWORD b = vcall<DWORD>(this, STREAM_SIZE);
    if (b != a)
        return b & 0xffffff00;
    if (!data)
        return 0;
    if (!other->data)
        return (DWORD)(uintptr_t)data & 0xffffff00;
    DWORD n = vcall<DWORD>(this, STREAM_SIZE);
    return memcmp(data, other->data, n) == 0;
}

// 0x505e40: the whole file through a CFileBuf into a new memory stream (flags as for FileBuf_Open)
ZStream *__fastcall File_OpenInMemory(const char *path, DWORD flags)
{
    CFileBuf f;
    fn<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(&f);
    if (!(fn<DWORD(THISCALL *)(CFileBuf *, const char *, DWORD)>(0x4a1840)(&f, path, flags) & 0xff)) {
        fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(&f);
        return nullptr;
    }
    DWORD n = fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2190)(&f);
    BYTE *p = static_cast<BYTE *>(heap_alloc(n));
    fn<DWORD(THISCALL *)(CFileBuf *, void *, DWORD)>(0x4a1b00)(&f, p, n);
    MemoryStream *s = static_cast<MemoryStream *>(pool_alloc(sizeof(MemoryStream)));
    if (s) {
        stream_ctor(s);
        clear(s);
        close_direct(s);
        s->position = 0;
        s->size = s->capacity = n;
        s->data = p;
        s->owns = 1;
    }
    fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(&f);
    return s;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(memory_stream)
{
    const Hook list[] = {
        {0x505e40, ptr(&File_OpenInMemory), "File_OpenInMemory", 5, {0x83, 0xec, 0x1c, 0x56, 0x57, 0x8b, 0xf9, 0x8b}},
        {0x505f00, ptr(&MemoryStream::Ctor), "MemoryStream::Ctor", 9, {0x53, 0x56, 0x8b, 0xf1, 0xe8, 0x97, 0xc4, 0xf9}, 5},
        {0x505f50, ptr(&MemoryStream::DeletingDtor), "MemoryStream::DeletingDtor", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x8a, 0x46}},
        {0x505fb0, ptr(&MemoryStream::CtorData), "MemoryStream::CtorData", 9, {0x53, 0x56, 0x8b, 0xf1, 0xe8, 0xe7, 0xc3, 0xf9}, 5},
        {0x506000, ptr(&MemoryStream::Dtor), "MemoryStream::Dtor", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x8a, 0x46}},
        {0x506040, ptr(&MemoryStream::Write), "MemoryStream::Write", 8, {0x53, 0x8b, 0xd9, 0x55, 0x8b, 0x6c, 0x24, 0x10}},
        {0x5060a0, ptr(&MemoryStream::Read), "MemoryStream::Read", 6, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0xd1, 0x56, 0x57}},
        {0x5060e0, ptr(&MemoryStream::Skip), "MemoryStream::Skip", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x0c, 0x03}},
        {0x506100, ptr(&MemoryStream::Seek), "MemoryStream::Seek", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x0c, 0xb0}},
        {0x506110, ptr(&MemoryStream::Close), "MemoryStream::Close", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x8a, 0x46}},
        {0x506140, ptr(&MemoryStream::ReadLine), "MemoryStream::ReadLine", 7, {0x8b, 0xc1, 0x56, 0x57, 0x8b, 0x50, 0x0c, 0x8b}},
        {0x5061e0, ptr(&MemoryStream::Reset), "MemoryStream::Reset", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x50, 0x24}},
        {0x506210, ptr(&MemoryStream::Advance), "MemoryStream::Advance", 6, {0x8b, 0x51, 0x0c, 0x8b, 0x41, 0x18, 0x56, 0x8b}},
        {0x506230, ptr(&MemoryStream::Detach), "MemoryStream::Detach", 6, {0x8b, 0x01, 0x56, 0x8b, 0x71, 0x18, 0xc7, 0x41}},
        {0x506250, ptr(&MemoryStream::Reserve), "MemoryStream::Reserve", 8, {0x53, 0x8b, 0xd9, 0x56, 0x8b, 0x74, 0x24, 0x0c}},
        {0x5062d0, ptr(&MemoryStream::Equals), "MemoryStream::Equals", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
