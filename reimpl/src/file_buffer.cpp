// File buffer and file functions reimplemented (see file_buffer.h).
#include "file_buffer.h"
#include "zip_archive.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies
static void zstring_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zstring_dtor(ZString *z) { fn<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zstring_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static void zstring_assign_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
static void zstring_set_path(ZString *z, const char *dir, const char *name)
{
    fn<void(THISCALL *)(ZString *, const char *, const char *)>(0x4dfa50)(z, dir, name);
}
static int crt_strncmp(const char *a, const char *b, DWORD n) { return fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5d17d0)(a, b, n); }
static int crt_remove(const char *path) { return fn<int(__cdecl *)(const char *)>(0x5d17a6)(path); }   // DeleteFileA, -1 on failure
static int *crt_errno() { return fn<int *(__cdecl *)()>(0x5d1014)(); }
// 0x587ff0: hex digits to bytes (at most n, or half the text)
static void hex_decode(void *out, const char *hex, DWORD n) { fn<void(__fastcall *)(void *, const char *, DWORD)>(0x587ff0)(out, hex, n); }
// 0x4a4220: out = dir + '/' (unless it ends with '/' or '\\', or is empty) + name
static void path_join(char *out, const char *dir, const char *name) { fn<void(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
// 0x4a3ce0: sprintf(out, pattern, i) for i = 0, 1, ... until FindFirstFileA finds nothing; returns -1
static DWORD unique_name(char *out, const char *pattern) { return fn<DWORD(__fastcall *)(char *, const char *)>(0x4a3ce0)(out, pattern); }
// 0x4a3d70: a file that is not read-only, or a missing one in an existing folder (path_util.cpp)
static bool disk_exists(const char *path) { return fn<bool(__fastcall *)(const char *)>(0x4a3d70)(path); }
// 0x4a4000: renames the file to path + ".BAK" (path_util.cpp)
static DWORD backup_file(const char *path, DWORD n) { return fn<DWORD(__fastcall *)(const char *, DWORD)>(0x4a4000)(path, n); }
// 0x4a2440: the stream of a path; 0x505e40: the whole file as a memory stream
static ZStream *file_open(const char *path, DWORD flags) { return fn<ZStream *(__fastcall *)(const char *, DWORD)>(0x4a2440)(path, flags); }
static ZStream *memory_open(const char *path, DWORD flags) { return fn<ZStream *(__fastcall *)(const char *, DWORD)>(0x505e40)(path, flags); }
static bool zip_exists(const char *path, BYTE *on_disk)
{
    void *list = fn<void *(__cdecl *)()>(0x5c1650)();
    return fn<bool(THISCALL *)(void *, const char *, BYTE *)>(0x5c1d00)(list, path, on_disk);
}

// Calls into this module through the exe (Disable= applies)
static bool buf_open(CFileBuf *f, const char *path, DWORD flags) { return fn<bool(THISCALL *)(CFileBuf *, const char *, DWORD)>(0x4a1840)(f, path, flags); }
static DWORD buf_close(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2100)(f); }
static void buf_dtor(CFileBuf *f) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(f); }
static bool buf_copy_to(CFileBuf *f, CFileBuf *dst, void(STDCALL *progress)(float))
{
    return fn<bool(THISCALL *)(CFileBuf *, CFileBuf *, void(STDCALL *)(float))>(0x4a1c40)(f, dst, progress);
}

static DWORD text_length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}
static void copy_text(char *dst, const char *src)
{
    while ((*dst++ = *src++))
        ;
}

static DWORD &files_open() { return *glob<DWORD *>(FILES_OPEN); }
static void count_open()
{
    DWORD n = ++files_open();
    if ((int)n > (int)*glob<DWORD *>(FILES_OPEN_MAX))
        *glob<DWORD *>(FILES_OPEN_MAX) = n;
}

// The stream calls with the exe's null checks around them
static DWORD stream_read(CFileBuf *f, void *p, DWORD n)
{
    if (!f->stream)
        return 0;
    if (n < SMALL_READ)
        f->small_reads++;
    return vcall<DWORD>(f->stream, STREAM_READ, p, n);
}
static DWORD stream_write(CFileBuf *f, const void *p, DWORD n) { return f->stream ? vcall<DWORD>(f->stream, STREAM_WRITE, p, n) : 0; }
static void stream_seek(CFileBuf *f, DWORD position)
{
    if (f->stream)
        vcall<bool>(f->stream, STREAM_SEEK, position);
}
static DWORD stream_size(CFileBuf *f) { return f->stream ? vcall<DWORD>(f->stream, STREAM_SIZE) : 0; }

// A file buffer on the stack as the file functions build it inline
static void local_buf(CFileBuf *f)
{
    f->vftable = vt(FILE_BUF_VTABLE);
    f->stream = nullptr;
    f->indent = f->flags = f->small_reads = f->advanced = 0;
    f->option18 = 0;
}

// ---- CFileBuf ------------------------------------------------------------------------------------

CFileBuf *THISCALL CFileBuf::Ctor()
{
    local_buf(this);
    return this;
}

// Close (slot 1) and the destructors: count down, close the stream (slot 9) and delete it. The result
// is that of the stream's deleting destructor (0 without a stream).
DWORD THISCALL CFileBuf::Close()
{
    if (!stream)
        return 0;
    files_open()--;
    DWORD r = vcall<DWORD>(stream, STREAM_CLOSE);
    if (stream) {
        r = vcall<DWORD>(stream, STREAM_DELETE, 1u);
        stream = nullptr;
    }
    return r;
}

DWORD THISCALL CFileBuf::Dtor()
{
    vftable = vt(FILE_BUF_VTABLE);
    return Close();
}

CFileBuf *THISCALL CFileBuf::DeletingDtor(DWORD flags)
{
    Dtor();
    if (flags & 1)
        pool_free(this, sizeof(CFileBuf));
    return this;
}

// 0x4a1840: "~CFileBuf 0x" + 8 hex digits takes over the stream at that address; flag 0x200 reads the
// file into a memory stream (0x505e40, without the flag); otherwise File_Open, with 0xa0 for absolute
// paths (starting with '\\' or '/', or with a drive letter). Close (slot 1) runs before the new stream
// is set; an empty path returns false without it.
bool THISCALL CFileBuf::Open(const char *path, DWORD open_flags)
{
    advanced = 0;
    option18 = 0;
    small_reads = 0;
    const char *prefix = *glob<const char **>(FILE_BUF_PREFIX);
    DWORD n = text_length(prefix);
    if (path && crt_strncmp(path, prefix, n) == 0 && text_length(path + n) >= 8) {
        ZStream *s = nullptr;
        hex_decode(&s, path + n, 4);
        if (s) {
            advanced = 0;
            option18 = 0;
            small_reads = 0;
            vcall<DWORD>(this, 1);
            stream = s;
            flags = open_flags;
            count_open();
            return true;
        }
    }
    ZStream *s;
    if (open_flags & 0x200) {
        s = memory_open(path, open_flags & ~0x200u);
        advanced = 0;
        option18 = 0;
        small_reads = 0;
        vcall<DWORD>(this, 1);
    } else {
        if (text_length(path) >= 2 && (path[0] == '\\' || path[0] == '/' || path[1] == ':'))
            open_flags |= 0xa0;
        if (!path || !*path)
            return false;
        vcall<DWORD>(this, 1);
        s = file_open(path, open_flags);
    }
    stream = s;
    flags = open_flags;
    if (!s)
        return false;
    count_open();
    return true;
}

bool THISCALL CFileBuf::Attach(ZStream *s, DWORD open_flags)
{
    advanced = 0;
    option18 = 0;
    small_reads = 0;
    vcall<DWORD>(this, 1);
    stream = s;
    flags = open_flags;
    if (!s)
        return false;
    count_open();
    return true;
}

DWORD THISCALL CFileBuf::Write(const void *p, DWORD n) { return stream_write(this, p, n); }
DWORD THISCALL CFileBuf::WriteChar(char c) { return stream_write(this, &c, 1); }
DWORD THISCALL CFileBuf::WriteText(const char *s) { return stream ? vcall<DWORD>(stream, STREAM_WRITE, s, text_length(s)) : 0; }

// 0x4a1a40: the text (unless null), "\r\n" and one tab per indent level; returns the level
DWORD THISCALL CFileBuf::WriteLine(const char *s)
{
    if (s && stream)
        vcall<DWORD>(stream, STREAM_WRITE, s, text_length(s));
    if (stream)
        vcall<DWORD>(stream, STREAM_WRITE, glob<const char *>(0x6137fc), 2u);
    for (DWORD i = 0; i < indent; i++) {
        char tab = 9;
        if (stream)
            vcall<DWORD>(stream, STREAM_WRITE, &tab, 1u);
    }
    return indent;
}

// 0x4a1ab0/0x4a1ae0: one more level writes a tab at once; one less goes back one byte
DWORD THISCALL CFileBuf::Indent()
{
    char tab = 9;
    indent++;
    return stream_write(this, &tab, 1);
}

DWORD THISCALL CFileBuf::Unindent()
{
    if (!indent)
        return 0;
    indent--;
    if (!stream)
        return indent;
    return vcall<DWORD>(stream, STREAM_SKIP, (DWORD)-1);
}

DWORD THISCALL CFileBuf::Read(void *p, DWORD n) { return stream_read(this, p, n); }

// 0x4a1b30: moves n bytes from src to the lower offset dst in chunks of up to 64 KB (ZZipDir moves
// the entries behind one with it). dst == src or n == 0 is true at once; dst > src is false.
bool THISCALL CFileBuf::Move(DWORD dst, DWORD src, DWORD n)
{
    if (!stream)
        return false;
    if (dst == src || !n)
        return true;
    if (dst >= src)
        return false;
    DWORD chunk = n > 0x10000 ? 0x10000 : n;
    BYTE *buf = static_cast<BYTE *>(heap_alloc(chunk));
    for (DWORD rest = n; rest;) {
        DWORD c = chunk < rest ? chunk : rest;
        stream_seek(this, src);
        if (stream_read(this, buf, c) != c)
            goto fail;
        stream_seek(this, dst);
        if (stream_write(this, buf, c) != c)
            goto fail;
        rest -= c;
        src += c;
        dst += c;
    }
    heap_free(buf);
    return true;
fail:
    heap_free(buf);
    return false;
}

// 0x4a1c40: the whole file from the start into dst (from its start) in chunks of up to 512 KB; the
// results of the reads and writes are not checked. progress (stdcall) gets 0, the share done before
// every chunk and 1.
bool THISCALL CFileBuf::CopyTo(CFileBuf *dst, void(STDCALL *progress)(float))
{
    DWORD total = stream_size(this);
    DWORD rest = stream_size(this);
    stream_seek(this, 0);
    stream_seek(dst, 0);
    if (progress)
        progress(0.0f);
    if (rest) {
        BYTE *buf = static_cast<BYTE *>(heap_alloc(rest < 0x80000 ? rest : 0x80000));
        do {
            if (progress)
                progress((float)((long double)(unsigned long long)(total - rest) / (long double)(int)total));
            DWORD c = rest < 0x80000 ? rest : 0x80000;
            stream_read(this, buf, c);
            stream_write(dst, buf, c);
            rest -= c;
        } while (rest);
        if (buf)
            heap_free(buf);
    }
    if (progress)
        progress(1.0f);
    return true;
}

// 0x4a1d60: same size and the same bytes from the start (chunks of up to 64 KB)
bool THISCALL CFileBuf::Equals(CFileBuf *other)
{
    DWORD size = stream_size(this);
    if (size != stream_size(other))
        return false;
    stream_seek(this, 0);
    stream_seek(other, 0);
    DWORD chunk = size > 0x10000 ? 0x10000 : size;
    BYTE *a = static_cast<BYTE *>(heap_alloc(chunk));
    BYTE *b = static_cast<BYTE *>(heap_alloc(chunk));
    bool same = true;
    for (DWORD rest = size; rest;) {
        DWORD c = chunk < rest ? chunk : rest;
        if (stream_read(this, a, c) != c || stream_read(other, b, c) != c || memcmp(a, b, c) != 0) {
            same = false;
            break;
        }
        rest -= c;
    }
    heap_free(a);
    heap_free(b);
    return same;
}

// 0x4a2010: a length dword and that many bytes into a new block (operator new, terminated); an old
// block is deleted first. The results of the reads are not checked.
bool THISCALL CFileBuf::ReadString(char **out)
{
    if (*out) {
        heap_free(*out);
        *out = nullptr;
    }
    if (!stream)
        return false;
    DWORD n = 0;
    small_reads++;
    vcall<DWORD>(stream, STREAM_READ, &n, 4u);
    *out = static_cast<char *>(heap_alloc(n + 1));
    stream_read(this, *out, n);
    (*out)[n] = 0;
    return true;
}

// The stream's eax is passed on (al is the result)
DWORD THISCALL CFileBuf::ReadLine(char *p, DWORD n) { return stream ? vcall<DWORD>(stream, STREAM_READ_LINE, p, n) : 0; }
DWORD THISCALL CFileBuf::Skip(DWORD delta) { return stream ? vcall<DWORD>(stream, STREAM_SKIP, delta) : 0; }
DWORD THISCALL CFileBuf::Seek(DWORD position) { return stream ? vcall<DWORD>(stream, STREAM_SEEK, position) : 0; }

BYTE THISCALL CFileBuf::SetOption18(BYTE b)
{
    option18 = b;
    return b;
}

// 0x4a2150: no null check
void *THISCALL CFileBuf::Advance(DWORD n)
{
    advanced += n;
    return vcall<void *>(stream, STREAM_ADVANCE, n);
}

void *THISCALL CFileBuf::Detach() { return stream ? vcall<void *>(stream, STREAM_DETACH) : nullptr; }
DWORD THISCALL CFileBuf::Tell() { return stream ? vcall<DWORD>(stream, STREAM_TELL) : 0xffffffff; }
DWORD THISCALL CFileBuf::Size() { return stream_size(this); }
DWORD THISCALL CFileBuf::SetSize(DWORD n) { return stream ? vcall<DWORD>(stream, STREAM_SET_SIZE, n) : 0; }

CFileBuf *THISCALL CFileBuf::WriteDword(DWORD v)
{
    stream_write(this, &v, 4);
    return this;
}

CFileBuf *THISCALL CFileBuf::WriteByte(BYTE v)
{
    stream_write(this, &v, 1);
    return this;
}

CFileBuf *THISCALL CFileBuf::WriteQword(DWORD lo, DWORD hi)
{
    DWORD v[2] = {lo, hi};
    stream_write(this, v, 8);
    return this;
}

CFileBuf *THISCALL CFileBuf::WriteCountedText(const char *s)
{
    DWORD n = text_length(s);
    stream_write(this, &n, 4);
    stream_write(this, s, n);
    return this;
}

CFileBuf *THISCALL CFileBuf::ReadDword(DWORD *v)
{
    stream_read(this, v, 4);
    return this;
}

CFileBuf *THISCALL CFileBuf::ReadByte(BYTE *v)
{
    stream_read(this, v, 1);
    return this;
}

CFileBuf *THISCALL CFileBuf::ReadQword(void *v)
{
    stream_read(this, v, 8);
    return this;
}

// 0x4a2300: a length dword, then the text into out (terminated). A length beyond the rest of the file
// sets errno to ERANGE (0x22) and leaves out alone.
CFileBuf *THISCALL CFileBuf::ReadCountedText(char *out)
{
    *crt_errno() = 0;
    DWORD n = 0;
    stream_read(this, &n, 4);
    DWORD size = stream_size(this);
    DWORD position = stream ? vcall<DWORD>(stream, STREAM_TELL) : 0xffffffff;
    if (n > size - position) {
        *crt_errno() = 0x22;
        return this;
    }
    stream_read(this, out, n);
    out[n] = 0;
    return this;
}

// ---- File functions --------------------------------------------------------------------------------

// 0x4a1300: true if the file has exactly n bytes and all of them were read
bool __fastcall File_ReadExact(const char *path, void *p, DWORD n)
{
    CFileBuf f;
    local_buf(&f);
    bool ok = false;
    if (buf_open(&f, path, 4)) {
        if (stream_size(&f) == n && stream_read(&f, p, n) == n)
            ok = true;
        buf_close(&f);
    }
    buf_dtor(&f);
    return ok;
}

// 0x4a13f0: true if the file could be opened for writing (the write itself is not checked)
bool __fastcall File_WriteAll(const char *path, const void *p, DWORD n)
{
    CFileBuf f;
    local_buf(&f);
    bool ok = buf_open(&f, path, 3);
    if (ok) {
        stream_write(&f, p, n);
        buf_close(&f);
    }
    buf_dtor(&f);
    return ok;
}

// 0x4a14e0: the path below the data folder (0x6135ec) or, with flag 0x20, as it is; DeleteFileA through
// the C runtime. al = deleted, the rest of eax from the runtime's result (0 or -1).
DWORD __fastcall File_Delete(const char *path, DWORD flags)
{
    char full[256];
    if (flags & 0x20)
        copy_text(full, path);
    else
        path_join(full, glob<const char *>(DATA_FOLDER), path);
    DWORD r = (DWORD)crt_remove(full);
    return (r & 0xffffff00) | (r == 0 ? 1 : 0);
}

DWORD __fastcall File_BackupData(const char *path)
{
    char full[256];
    path_join(full, glob<const char *>(DATA_FOLDER), path);
    return backup_file(full, 0);
}

// 0x4a1570: whether the file can be opened for reading (flags only keeps 0x20; 0x80 added)
bool __fastcall File_CanOpen(const char *path, DWORD flags)
{
    CFileBuf f;
    local_buf(&f);
    bool ok = buf_open(&f, path, (flags & 0x20) | 0x84);
    if (ok)
        buf_close(&f);
    buf_dtor(&f);
    return ok;
}

// 0x4a1650: if an archive is mounted and knows the path, the answer is the byte ZZipList::Exists returns
// (with check_disk whether the archive file exists, else 0); otherwise the disk below the data folder
// (flag 0x20: the path as it is).
bool __fastcall File_Exists(const char *path, DWORD flags)
{
    BYTE on_disk = 0;
    if (*glob<BYTE *>(0x6bdf64) && zip_exists(path, &on_disk))
        return on_disk;
    ZString full;
    zstring_ctor(&full);
    if (flags & 0x20)
        zstring_assign_text(&full, path);
    else
        zstring_set_path(&full, glob<const char *>(DATA_FOLDER), path);
    bool r = disk_exists(zstring_c_str(&full));
    zstring_dtor(&full);
    return r;
}

// 0x4a16e0: the first name pattern % i (i = 0, 1, ...) that does not exist, below the data folder; out
// is relative to it again (flag 0x20: the pattern as it is, out absolute). Returns -1 with flag 0x20,
// otherwise the length of out + 1.
DWORD __fastcall File_UniqueName(char *out, const char *pattern, DWORD flags)
{
    char full[260];
    if (flags & 0x20)
        copy_text(full, pattern);
    else
        path_join(full, glob<const char *>(DATA_FOLDER), pattern);
    DWORD r = unique_name(out, full);
    if (flags & 0x20)
        return r;
    const char *rest = out + text_length(glob<const char *>(DATA_FOLDER)) + 1;
    DWORD n = text_length(rest) + 1;
    for (DWORD i = 0; i < n; i++)   // overlapping, forwards
        out[i] = rest[i];
    return n;
}

// 0x4a1ea0: copies src (opened with src_flags | 4) to dst (dst_flags | 3); true if both opened
bool __fastcall File_Copy(const char *src, DWORD src_flags, const char *dst, DWORD dst_flags, void(STDCALL *progress)(float))
{
    CFileBuf in, out;
    local_buf(&in);
    bool ok = false;
    if (buf_open(&in, src, src_flags | 4)) {
        local_buf(&out);
        if (buf_open(&out, dst, dst_flags | 3)) {
            if (buf_copy_to(&in, &out, progress))
                ok = true;
            buf_close(&out);
        }
        buf_dtor(&out);
        buf_close(&in);
    }
    buf_dtor(&in);
    return ok;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(file_buffer)
{
    const Hook list[] = {
        {0x4a1300, ptr(&File_ReadExact), "File_ReadExact", 5, {0x83, 0xec, 0x20, 0x53, 0x57, 0x6a, 0x04, 0x33}},
        {0x4a13f0, ptr(&File_WriteAll), "File_WriteAll", 5, {0x83, 0xec, 0x1c, 0x53, 0x56, 0x6a, 0x03, 0x33}},
        {0x4a14e0, ptr(&File_Delete), "File_Delete", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0xf6, 0xc2}},
        {0x4a1540, ptr(&File_BackupData), "File_BackupData", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0xba, 0xec}},
        {0x4a1570, ptr(&File_CanOpen), "File_CanOpen", 6, {0x83, 0xec, 0x1c, 0x83, 0xe2, 0x20, 0x53, 0x80}},
        {0x4a1650, ptr(&File_Exists), "File_Exists", 8, {0x83, 0xec, 0x08, 0xa0, 0x64, 0xdf, 0x6b, 0x00}},
        {0x4a16e0, ptr(&File_UniqueName), "File_UniqueName", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0x53, 0x55}},
        {0x4a1780, ptr(&CFileBuf::Ctor), "CFileBuf::Ctor", 10, {0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x00, 0xd4, 0x40}},
        {0x4a17a0, ptr(&CFileBuf::DeletingDtor), "CFileBuf::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04, 0xc7, 0x06}},
        {0x4a1800, ptr(&CFileBuf::Dtor), "CFileBuf::Dtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04, 0xc7, 0x06}},
        {0x4a1840, ptr(&CFileBuf::Open), "FileBuf_Open", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x57}},
        {0x4a1970, ptr(&CFileBuf::Attach), "CFileBuf::Attach", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xc7, 0x46, 0x14}},
        {0x4a19d0, ptr(&CFileBuf::Write), "CFileBuf::Write", 5, {0x8b, 0x49, 0x04, 0x85, 0xc9, 0x74, 0x12, 0x8b}},
        {0x4a19f0, ptr(&CFileBuf::WriteChar), "CFileBuf::WriteChar", 5, {0x8b, 0x49, 0x04, 0x85, 0xc9, 0x74, 0x0c, 0x8b}},
        {0x4a1a10, ptr(&CFileBuf::WriteText), "CFileBuf::WriteText", 5, {0x8b, 0x51, 0x04, 0x85, 0xd2, 0x74, 0x1f, 0x53}},
        {0x4a1a40, ptr(&CFileBuf::WriteLine), "CFileBuf::WriteLine", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x57, 0x85}},
        {0x4a1ab0, ptr(&CFileBuf::Indent), "CFileBuf::Indent", 9, {0x51, 0x8b, 0x51, 0x08, 0xc6, 0x44, 0x24, 0x03}},
        {0x4a1ae0, ptr(&CFileBuf::Unindent), "CFileBuf::Unindent", 5, {0x8b, 0x41, 0x08, 0x85, 0xc0, 0x74, 0x12, 0x48}},
        {0x4a1b00, ptr(&CFileBuf::Read), "CFileBuf::Read", 5, {0x8b, 0x41, 0x04, 0x85, 0xc0, 0x74, 0x21, 0x8b}},
        {0x4a1b30, ptr(&CFileBuf::Move), "CFileBuf::Move", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x47}},
        {0x4a1c40, ptr(&CFileBuf::CopyTo), "CFileBuf::CopyTo", 6, {0x83, 0xec, 0x18, 0x53, 0x8b, 0xd9, 0x55, 0x56}},
        {0x4a1d60, ptr(&CFileBuf::Equals), "CFileBuf::Equals", 5, {0x83, 0xec, 0x14, 0x53, 0x56, 0x57, 0x8b, 0xf9}},
        {0x4a1ea0, ptr(&File_Copy), "File_Copy", 7, {0x83, 0xec, 0x3c, 0x53, 0x83, 0xca, 0x04, 0x56}},
        {0x4a2010, ptr(&CFileBuf::ReadString), "CFileBuf::ReadString", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x4a20a0, ptr(&CFileBuf::ReadLine), "CFileBuf::ReadLine", 5, {0x8b, 0x49, 0x04, 0x85, 0xc9, 0x74, 0x12, 0x8b}},
        {0x4a20c0, ptr(&CFileBuf::Skip), "CFileBuf::Skip", 5, {0x8b, 0x49, 0x04, 0x85, 0xc9, 0x74, 0x0d, 0x8b}},
        {0x4a20e0, ptr(&CFileBuf::Seek), "CFileBuf::Seek", 5, {0x8b, 0x49, 0x04, 0x85, 0xc9, 0x74, 0x0d, 0x8b}},
        {0x4a2100, ptr(&CFileBuf::Close), "CFileBuf::Close", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04, 0x85, 0xc0}},
        {0x4a2140, ptr(&CFileBuf::SetOption18), "CFileBuf::SetOption18", 7, {0x8a, 0x44, 0x24, 0x04, 0x88, 0x41, 0x18, 0xc2}},
        {0x4a2150, ptr(&CFileBuf::Advance), "CFileBuf::Advance", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x14, 0x03}},
        {0x4a2170, ptr(&CFileBuf::Detach), "CFileBuf::Detach", 5, {0x8b, 0x49, 0x04, 0x85, 0xc9, 0x74, 0x05, 0x8b}},
        {0x4a2180, ptr(&CFileBuf::Tell), "CFileBuf::Tell", 5, {0x8b, 0x49, 0x04, 0x85, 0xc9, 0x74, 0x05, 0x8b}},
        {0x4a2190, ptr(&CFileBuf::Size), "CFileBuf::Size", 5, {0x8b, 0x49, 0x04, 0x85, 0xc9, 0x74, 0x05, 0x8b}},
        {0x4a21a0, ptr(&CFileBuf::SetSize), "CFileBuf::SetSize", 5, {0x8b, 0x49, 0x04, 0x85, 0xc9, 0x74, 0x0a, 0x8b}},
        {0x4a21c0, ptr(&CFileBuf::WriteDword), "CFileBuf::WriteDword", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x04, 0x85, 0xc9}},
        {0x4a21e0, ptr(&CFileBuf::WriteByte), "CFileBuf::WriteByte", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x04, 0x85, 0xc9}},
        {0x4a2200, ptr(&CFileBuf::WriteQword), "CFileBuf::WriteQword", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x04, 0x85, 0xc9}},
        {0x4a2220, ptr(&CFileBuf::WriteCountedText), "CFileBuf::WriteCountedText", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4a2270, ptr(&CFileBuf::ReadDword), "CFileBuf::ReadDword", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x04, 0x85, 0xc9}},
        {0x4a22a0, ptr(&CFileBuf::ReadByte), "CFileBuf::ReadByte", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x04, 0x85, 0xc9}},
        {0x4a22d0, ptr(&CFileBuf::ReadQword), "CFileBuf::ReadQword", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x04, 0x85, 0xc9}},
        {0x4a2300, ptr(&CFileBuf::ReadCountedText), "CFileBuf::ReadCountedText", 5, {0x51, 0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x0a, 0xed}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
