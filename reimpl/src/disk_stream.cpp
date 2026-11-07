// Disk stream, File_Open, the line reader, the CRC of a stream and the "~CFileBuf 0x" paths
// reimplemented (see disk_stream.h).
#include "disk_stream.h"
#include "zip_archive.h"

namespace zax {

// C runtime of the exe (its FILE * and file handles)
static void *crt_fopen(const char *path, const char *mode) { return fn<void *(__cdecl *)(const char *, const char *)>(0x5d1a29)(path, mode); }
static DWORD crt_fwrite(const void *p, DWORD size, DWORD n, void *f) { return fn<DWORD(__cdecl *)(const void *, DWORD, DWORD, void *)>(0x5d1a3c)(p, size, n, f); }
static DWORD crt_fread(void *p, DWORD size, DWORD n, void *f) { return fn<DWORD(__cdecl *)(void *, DWORD, DWORD, void *)>(0x5d1b75)(p, size, n, f); }
static int crt_fseek(void *f, DWORD offset, int origin) { return fn<int(__cdecl *)(void *, DWORD, int)>(0x5d1c8c)(f, offset, origin); }
static DWORD crt_ftell(void *f) { return fn<DWORD(__cdecl *)(void *)>(0x5d1d45)(f); }
static DWORD crt_fclose(void *f) { return fn<DWORD(__cdecl *)(void *)>(0x5d1808)(f); }
static int crt_fileno(void *f) { return fn<int(__cdecl *)(void *)>(0x5de2e0)(f); }
static DWORD crt_chsize(int fd, DWORD n) { return fn<DWORD(__cdecl *)(int, DWORD)>(0x5d1ec8)(fd, n); }
static int crt_strncmp(const char *a, const char *b, DWORD n) { return fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5d17d0)(a, b, n); }

// Functions of the exe; through their exe addresses so Disable= applies
static void path_join(char *out, const char *dir, const char *name) { fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
static DWORD find_exists(const char *path) { return fn<DWORD(__fastcall *)(const char *)>(0x4a3cb0)(path); }
static DWORD create_path(const char *path) { return fn<DWORD(__fastcall *)(const char *)>(0x4a3ec0)(path); }
static void zstring_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zstring_dtor(ZString *z) { fn<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zstring_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static void split_path(const char *path, ZString *dir, ZString *name)
{
    fn<void(__fastcall *)(const char *, ZString *, ZString *)>(0x4e0770)(path, dir, name);
}
static ZStream *zip_open(const char *path, DWORD flags)
{
    void *list = fn<void *(__cdecl *)()>(0x5c1650)();
    return fn<ZStream *(THISCALL *)(void *, const char *, DWORD)>(0x5c1c70)(list, path, flags);
}
static ZStream *zip_writer(const char *archive, const char *name) { return fn<ZStream *(__fastcall *)(const char *, const char *)>(0x5c3660)(archive, name); }
static DWORD disk_open(ZStream *s, const char *path, DWORD flags) { return fn<DWORD(THISCALL *)(ZStream *, const char *, DWORD)>(0x4a2760)(s, path, flags); }
// 0x587ff0: hex digits to bytes; its eax is passed on by 0x4a2a70
static DWORD hex_decode(void *out, const char *hex, DWORD n) { return fn<DWORD(__fastcall *)(void *, const char *, DWORD)>(0x587ff0)(out, hex, n); }
static DWORD crc_start() { return fn<DWORD(__cdecl *)()>(0x592380)(); }   // -1
static DWORD crc_update(DWORD crc, const void *p, DWORD n) { return fn<DWORD(__fastcall *)(DWORD, const void *, DWORD)>(0x5c31b0)(crc, p, n); }
static DWORD crc_final(DWORD crc) { return fn<DWORD(__fastcall *)(DWORD)>(0x5c31a0)(crc); }

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
static char *find_char(char *s, char c)
{
    for (;; s++) {
        if (*s == c)
            return s;
        if (!*s)
            return nullptr;
    }
}

// ---- DiskStream ------------------------------------------------------------------------------------

DiskStream *THISCALL DiskStream::Ctor()
{
    vftable = vt(STREAM_VTABLE);
    handle = nullptr;
    owned = 0;
    return this;
}

// Close and the destructor: fclose if owned; the result is fclose's, the handle itself when it is not
// owned, 0 without one
DWORD THISCALL DiskStream::Close()
{
    DWORD r = (DWORD)(uintptr_t)handle;
    if (handle) {
        if (owned)
            r = crt_fclose(handle);
        handle = nullptr;
        owned = 0;
    }
    return r;
}

DWORD THISCALL DiskStream::Dtor()
{
    vftable = vt(STREAM_VTABLE);
    return Close();
}

DiskStream *THISCALL DiskStream::DeletingDtor(DWORD flags)
{
    Dtor();
    if (flags & 1)
        pool_free(this, sizeof(DiskStream));
    return this;
}

DWORD THISCALL DiskStream::Open(const char *path, DWORD flags)
{
    char full[260];
    void *f;
    if ((flags & 1) && (flags & 2)) {
        if (flags & 0x20)
            copy_text(full, path);
        else
            path_join(full, glob<const char *>(DATA_FOLDER), path);
        f = crt_fopen(full, glob<const char *>(MODE_WB));
        if (!f && !(find_exists(full) & 0xff) && (flags & 0x100)) {
            ZString dir, name;
            zstring_ctor(&dir);
            zstring_ctor(&name);
            split_path(full, &dir, &name);
            create_path(zstring_c_str(&dir));
            f = crt_fopen(full, glob<const char *>(MODE_WB));
            zstring_dtor(&name);
            zstring_dtor(&dir);
        }
    } else {
        f = nullptr;
        const char *mode = at<const char *>(flags & 2 ? MODE_RPB : MODE_RB);
        if (!(flags & 0x20)) {
            path_join(full, glob<const char *>(DATA_FOLDER), path);
            f = crt_fopen(full, mode);
        }
        if (!f)
            f = crt_fopen(path, mode);
    }
    handle = f;
    owned = f != nullptr;
    return ((DWORD)(uintptr_t)f & 0xffffff00) | owned;
}

DWORD THISCALL DiskStream::Write(const void *p, DWORD n) { return handle ? crt_fwrite(p, 1, n, handle) : 0; }
DWORD THISCALL DiskStream::Read(void *p, DWORD n) { return handle ? crt_fread(p, 1, n, handle) : 0; }
DWORD THISCALL DiskStream::Skip(DWORD delta) { return handle ? crt_fseek(handle, delta, 1) == 0 : 0; }
DWORD THISCALL DiskStream::Seek(DWORD position) { return handle ? crt_fseek(handle, position, 0) == 0 : 0; }
DWORD THISCALL DiskStream::Tell() { return handle ? crt_ftell(handle) : 0xffffffff; }

// 0x4a2ba0: ftell, fseek to the end, ftell, fseek back
DWORD THISCALL DiskStream::Size()
{
    if (!handle)
        return 0;
    DWORD position = crt_ftell(handle);
    crt_fseek(handle, 0, 2);
    DWORD size = crt_ftell(handle);
    crt_fseek(handle, position, 0);
    return size;
}

// 0x4a2bf0: _chsize on the file's handle (0 or -1); without a file the null handle
DWORD THISCALL DiskStream::SetSize(DWORD n) { return handle ? crt_chsize(crt_fileno(handle), n) : 0; }

// ---- The line reader -------------------------------------------------------------------------------

// 0x4a2ad0: reads n - 1 bytes from the current position, ends the line at the first '\r' or '\n' and
// seeks behind the line end: "\r\n" and "\n\r" count as one, "\r\r" and "\n\n" as two; "\r" or "\n"
// followed by "\r\n" or "\n\n" is one line end of three bytes. Without a line end in the bytes read it
// seeks back to the start and returns al = 0. The upper bytes are those of Seek's result.
DWORD THISCALL LineReader::ReadLine(char *p, DWORD n)
{
    DWORD start = vcall<DWORD>(this, STREAM_TELL);
    DWORD r = vcall<DWORD>(this, STREAM_READ, p, n - 1);
    p[r] = 0;
    char *end = find_char(p, '\r');
    char *lf = find_char(p, '\n');
    if (!end || (lf && lf < end))
        end = lf;
    if (!end)
        return vcall<DWORD>(this, STREAM_SEEK, start) & 0xffffff00;
    char c = *end;
    *end = 0;
    char next = end[1];
    char *rest = end + 1;
    if (next == '\n' || next == '\r') {
        if (c == '\r' && rest[1] == '\n')
            rest += 2;
        else if (next != c)
            rest++;
    }
    return (vcall<DWORD>(this, STREAM_SEEK, start + (DWORD)(rest - p)) & 0xffffff00) | 1;
}

// ---- File_Open -------------------------------------------------------------------------------------

// A disk stream (inline constructor) opened with DiskStream::Open, deleted again if that fails
static ZStream *open_on_disk(const char *path, DWORD flags, bool *opened)
{
    DiskStream *s = static_cast<DiskStream *>(pool_alloc(sizeof(DiskStream)));
    if (s) {
        s->vftable = vt(STREAM_VTABLE);
        s->handle = nullptr;
        s->owned = 0;
    }
    *opened = disk_open(s, path, flags) & 0xff;
    if (*opened || !s)
        return s;
    vcall<void *>(s, STREAM_DELETE, 1u);
    return nullptr;
}

ZStream *__fastcall File_Open(const char *path, DWORD flags)
{
    ZStream *s = nullptr;
    bool zip = *glob<BYTE *>(0x6bdf64) && !(flags & 0x20);
    bool opened;
    if (flags & 2) {
        if (flags & 1) {
            if (find_char(const_cast<char *>(path), ';')) {
                char archive[256];
                copy_text(archive, path);
                char *name = find_char(archive, ';');
                *name = 0;
                s = zip_writer(archive, name + 1);
                if (s)
                    s->owned = 1;
                return s;
            }
            if (zip) {
                s = zip_open(path, flags);
                if (s) {
                    s->owned = 1;
                    return s;
                }
            }
        }
        if (flags & 0x40)
            return s;
        s = open_on_disk(path, flags, &opened);
        if (s)
            s->owned = 1;
        return s;
    }
    if (!(flags & 4))
        return nullptr;
    if (zip) {
        s = zip_open(path, flags | 0x8000);
        if (s) {
            s->owned = 1;
            return s;
        }
    }
    if (!(flags & 0x40)) {
        s = open_on_disk(path, flags, &opened);
        if (s) {
            s->owned = 1;
            return s;
        }
    }
    if (zip) {
        s = zip_open(path, flags);
        if (s)
            s->owned = 1;
    }
    return s;
}

// ---- CRC and "~CFileBuf 0x" paths ------------------------------------------------------------------

// 0x4a26c0: CRC-32 of the whole stream from the start (reads of up to 64 KB, results not checked)
DWORD __fastcall Stream_Crc(ZStream *s)
{
    DWORD rest = vcall<DWORD>(s, STREAM_SIZE);
    DWORD chunk = rest > 0x10000 ? 0x10000 : rest;
    BYTE *buf = static_cast<BYTE *>(heap_alloc(chunk));
    vcall<DWORD>(s, STREAM_SEEK, 0u);
    DWORD crc = crc_start();
    while (rest) {
        DWORD c = chunk < rest ? chunk : rest;
        vcall<DWORD>(s, STREAM_READ, buf, c);
        crc = crc_update(crc, buf, c);
        rest -= c;
    }
    heap_free(buf);
    return crc_final(crc);
}

void __fastcall Stream_FileBufPath(ZStream *s, DWORD, ZString *out, const char *name)
{
    char hex[12];
    text_hex(hex, &s, 4);
    fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(out, glob<const char *>(0x61384c), *glob<const char **>(FILE_BUF_PREFIX), hex, name);
}

// 0x4a2a10: the stream of a "~CFileBuf 0x" path (at least 8 hex digits), else null
ZStream *__fastcall Stream_FromFileBufPath(const char *path)
{
    const char *prefix = *glob<const char **>(FILE_BUF_PREFIX);
    DWORD n = text_length(prefix);
    if (!path || crt_strncmp(path, prefix, n) != 0 || text_length(path + n) < 8)
        return nullptr;
    ZStream *s = const_cast<ZStream *>(reinterpret_cast<const ZStream *>(path));   // overwritten in full
    hex_decode(&s, path + n, 4);
    return s;
}

// 0x4a2a70: deletes the stream of a "~CFileBuf 0x" path. The result is whatever eax holds: 0 for a null
// path or fewer than 8 digits, strncmp's result for another path, 0x587ff0's for a null stream, else
// the deleting destructor's.
DWORD __fastcall Stream_DeleteFileBufPath(const char *path)
{
    const char *prefix = *glob<const char **>(FILE_BUF_PREFIX);
    DWORD n = text_length(prefix);
    if (!path)
        return 0;
    int c = crt_strncmp(path, prefix, n);
    if (c != 0)
        return (DWORD)c;
    if (text_length(path + n) < 8)
        return 0;
    ZStream *s = const_cast<ZStream *>(reinterpret_cast<const ZStream *>(path));
    DWORD r = hex_decode(&s, path + n, 4);
    if (!s)
        return r;
    return vcall<DWORD>(s, STREAM_DELETE, 1u);
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(disk_stream)
{
    const Hook list[] = {
        {0x4a23a0, ptr(&DiskStream::Ctor), "Stream::Ctor", 10, {0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x00, 0xdc, 0x40}},
        {0x4a23c0, ptr(&DiskStream::DeletingDtor), "DiskStream::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04, 0xc7, 0x06}},
        {0x4a2410, ptr(&DiskStream::Dtor), "Stream::Dtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04, 0xc7, 0x06}},
        {0x4a2440, ptr(&File_Open), "File_Open", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x53, 0x56}},
        {0x4a26c0, ptr(&Stream_Crc), "Stream_Crc", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4a2760, ptr(&DiskStream::Open), "DiskStream::Open", 6, {0x81, 0xec, 0x08, 0x01, 0x00, 0x00, 0x53, 0x8b}},
        {0x4a28e0, ptr(&DiskStream::Write), "DiskStream::Write", 5, {0x8b, 0x41, 0x04, 0x85, 0xc0, 0x74, 0x18, 0x8b}},
        {0x4a2910, ptr(&DiskStream::Read), "DiskStream::Read", 5, {0x8b, 0x41, 0x04, 0x85, 0xc0, 0x74, 0x18, 0x8b}},
        {0x4a2940, ptr(&DiskStream::Skip), "DiskStream::Skip", 5, {0x8b, 0x41, 0x04, 0x85, 0xc0, 0x74, 0x18, 0x8b}},
        {0x4a2970, ptr(&DiskStream::Seek), "DiskStream::Seek", 5, {0x8b, 0x41, 0x04, 0x85, 0xc0, 0x74, 0x18, 0x8b}},
        {0x4a29a0, ptr(&DiskStream::Close), "DiskStream::Close", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04, 0x85, 0xc0}},
        {0x4a29d0, ptr(&Stream_FileBufPath), "Stream_FileBufPath", 7, {0x83, 0xec, 0x10, 0x89, 0x4c, 0x24, 0x00, 0x6a}},
        {0x4a2a10, ptr(&Stream_FromFileBufPath), "Stream_FromFileBufPath", 7, {0x51, 0x8b, 0x15, 0xec, 0x36, 0x61, 0x00, 0x56}},
        {0x4a2a70, ptr(&Stream_DeleteFileBufPath), "Stream_DeleteFileBufPath", 7, {0x51, 0x8b, 0x15, 0xec, 0x36, 0x61, 0x00, 0x56}},
        {0x4a2ad0, ptr(&LineReader::ReadLine), "Stream_ReadLine", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x07}},
        {0x4a2b80, ptr(&DiskStream::Tell), "DiskStream::Tell", 5, {0x8b, 0x41, 0x04, 0x85, 0xc0, 0x74, 0x0a, 0x50}},
        {0x4a2ba0, ptr(&DiskStream::Size), "DiskStream::Size", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04, 0x85, 0xc0}},
        {0x4a2bf0, ptr(&DiskStream::SetSize), "DiskStream::SetSize", 5, {0x8b, 0x41, 0x04, 0x85, 0xc0, 0x74, 0x17, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
