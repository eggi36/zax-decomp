// Zip archive streams reimplemented (see zip_stream.h).
#include "zip_stream.h"

namespace zax {

// ZString functions of the exe (reimplemented in zstring.cpp, called through the exe so Disable= applies)
static void zstring_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zstring_ctor_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
static void zstring_dtor(ZString *z) { fn<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zstring_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static DWORD zstring_length(const ZString *z) { return fn<DWORD(THISCALL *)(const ZString *)>(0x4e13b0)(z); }
static char *zstring_get_buffer(ZString *z, DWORD n) { return fn<char *(THISCALL *)(ZString *, DWORD)>(0x4e1590)(z, n); }
static void zstring_release_buffer(ZString *z, int n) { fn<void(THISCALL *)(ZString *, int)>(0x4e16d0)(z, n); }
#define zstring_format zax::code<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)

// C runtime and helpers of the exe
static int crt_strnicmp(const char *a, const char *b, DWORD n) { return fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5dd9c0)(a, b, n); }
static void crt_qsort(void *p, DWORD n, DWORD size, DWORD compare)
{
    fn<void(__cdecl *)(void *, DWORD, DWORD, DWORD)>(0x5d097c)(p, n, size, fn_addr_dyn(compare));
}
static void crt_exit(int code) { fn<void(__cdecl *)(int)>(0x5d0e36)(code); }
static void error_box(const char *caption, const char *text) { fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(caption, text); }
static void path_folder(char *s) { fn<void(__fastcall *)(char *)>(0x4a45b0)(s); }
// 0x4a3c80: the current time as DOS date and time (GetSystemTimeAsFileTime, FileTimeToDosDateTime)
static void dos_now(WORD *date, WORD *time) { fn<void(__fastcall *)(WORD *, WORD *)>(0x4a3c80)(date, time); }
// 0x4a26c0: CRC-32 of a whole stream (Size, Seek(0), reads of up to 64 KB)
static DWORD stream_crc(ZStream *s) { return fn<DWORD(THISCALL *)(ZStream *)>(0x4a26c0)(s); }
// Base of the streams: constructor 0x4a23a0, destructor 0x4a2410
static void stream_ctor(ZStream *s) { fn<ZStream *(THISCALL *)(ZStream *)>(0x4a23a0)(s); }
static void stream_dtor(ZStream *s) { fn<void(THISCALL *)(ZStream *)>(0x4a2410)(s); }

// The file buffer (file_buffer.cpp), through the exe
static CFileBuf *buf_ctor(CFileBuf *f) { return fn<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(f); }
static void buf_dtor(CFileBuf *f) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(f); }
static bool buf_open(CFileBuf *f, const char *path, DWORD flags) { return fn<bool(THISCALL *)(CFileBuf *, const char *, DWORD)>(0x4a1840)(f, path, flags); }
static DWORD buf_write(CFileBuf *f, const void *p, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, const void *, DWORD)>(0x4a19d0)(f, p, n); }
static DWORD buf_read(CFileBuf *f, void *p, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, void *, DWORD)>(0x4a1b00)(f, p, n); }
static bool buf_move(CFileBuf *f, DWORD dst, DWORD src, DWORD n) { return fn<bool(THISCALL *)(CFileBuf *, DWORD, DWORD, DWORD)>(0x4a1b30)(f, dst, src, n); }
static DWORD buf_skip(CFileBuf *f, DWORD delta) { return fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a20c0)(f, delta); }
static DWORD buf_seek(CFileBuf *f, DWORD position) { return fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a20e0)(f, position); }
static DWORD buf_tell(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2180)(f); }
static DWORD buf_size(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2190)(f); }
static DWORD buf_set_size(CFileBuf *f, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a21a0)(f, n); }
static void buf_write_dword(CFileBuf *f, DWORD v) { fn<CFileBuf *(THISCALL *)(CFileBuf *, DWORD)>(0x4a21c0)(f, v); }
static void buf_read_dword(CFileBuf *f, DWORD *v) { fn<CFileBuf *(THISCALL *)(CFileBuf *, DWORD *)>(0x4a2270)(f, v); }

// Calls into this module and the zip directory through the exe (Disable= applies)
static bool dir_load(ZZipDir *d) { return fn<bool(THISCALL *)(ZZipDir *)>(0x5c2240)(d); }
static ZZipEntry *dir_find(ZZipDir *d, const char *name, DWORD *pos)
{
    return fn<ZZipEntry *(THISCALL *)(ZZipDir *, const char *, DWORD *)>(0x5c2d50)(d, name, pos);
}
static ZZipEntry *dir_move_to_end(ZZipDir *d, ZZipEntry *e) { return fn<ZZipEntry *(THISCALL *)(ZZipDir *, ZZipEntry *)>(0x5c2080)(d, e); }
static void dir_write_end(ZZipDir *d, DWORD offset) { fn<DWORD(THISCALL *)(ZZipDir *, DWORD)>(0x5c2dd0)(d, offset); }
static void dir_write_local_header(ZZipDir *d, const ZZipEntry *e) { fn<DWORD(THISCALL *)(ZZipDir *, const ZZipEntry *)>(0x5c30d0)(d, e); }
static bool stream_open(ZZipStream *s, DWORD offset, ZZipEntry *e, CFileBuf *f, const char *archive, ZZipDir *d, DWORD mode)
{
    return fn<bool(THISCALL *)(ZZipStream *, DWORD, ZZipEntry *, CFileBuf *, const char *, ZZipDir *, DWORD)>(0x5c35c0)(
        s, offset, e, f, archive, d, mode);
}
static void stream_close(ZZipStream *s) { fn<void(THISCALL *)(ZZipStream *)>(0x5c3440)(s); }
static bool writer_open(ZZipWriter *w, const char *archive, const char *name)
{
    return fn<bool(THISCALL *)(ZZipWriter *, const char *, const char *)>(0x5c38d0)(w, archive, name);
}
static void writer_close(ZZipWriter *w) { fn<void(THISCALL *)(ZZipWriter *)>(0x5c3700)(w); }

static DWORD text_length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}

static CFileBuf *dir_file(ZZipDir *d) { return static_cast<CFileBuf *>(d->file); }
// The name of an entry without the mount folder
static const char *archive_name(const ZZipEntry *e) { return e->name + text_length(e->name) - e->name_length; }
// Where the data of an entry starts, and where the entry ends
static DWORD data_start(const ZZipEntry *e) { return e->extra_length + e->name_length + e->offset + 0x1e; }
static DWORD entry_end(const ZZipEntry *e) { return e->extra_length + e->name_length + e->offset + e->packed_size + 0x1e; }

// The message of a lazy directory that cannot be loaded; ends the game
static void fatal_empty(ZZipDir *d)
{
    ZString msg;
    zstring_ctor(&msg);
    zstring_format(&msg, glob<const char *>(0x627cb0), d->path);   // "There is something wrong with %s.  It may be an empty archive."
    error_box(glob<const char *>(0x627c9c), zstring_c_str(&msg));   // "FATAL ERROR IN ZIP"
    crt_exit(-1);
}

// ---- The directory ---------------------------------------------------------------------------------

// 0x5c2080: moves everything behind an entry to its place (CFileBuf::Move), fixes the offsets, gives the
// entry the offset behind the last one (its old data stays where it was, now overwritten) and sorts
// again; returns the entry, now the last one. The last entry is returned unchanged.
ZZipEntry *THISCALL ZZipDirOps::MoveEntryToEnd(ZZipEntry *e)
{
    DWORD i = (DWORD)(e - entries), last = count - 1;
    if (i == last)
        return e;
    DWORD next = i + 1, start = entries[next].offset, delta = start - e->offset;
    buf_move(dir_file(this), e->offset, start, entry_end(&entries[last]) - start);
    for (DWORD k = next; k <= last; k++)
        entries[k].offset -= delta;
    e->offset = entry_end(&entries[count - 1]);
    if (count > 1) {
        crt_qsort(entries, count, sizeof(ZZipEntry), fn_addr(0x5c2220));
        for (DWORD k = 0; k < count; k++)
            sorted[k] = k;
        *glob<ZZipEntry **>(ZIP_SORT_ENTRIES) = entries;
        crt_qsort(sorted, count, 4, fn_addr(0x5c21e0));
    }
    return &entries[count - 1];
}

// 0x5c2dd0: end signature and record at the current position (the directory from directory_offset to
// here), then the file ends there; the result is that of SetSize
DWORD THISCALL ZZipDirOps::WriteEnd(DWORD directory_offset)
{
    CFileBuf *f = dir_file(this);
    ZipEndRecord r;
    r.disk = 0;
    r.directory_disk = 0;
    r.count = (WORD)count;
    r.total = (WORD)count;
    r.directory_size = buf_tell(f) - directory_offset;
    r.directory_offset = directory_offset;
    r.comment_length = 0;
    buf_write(f, &end_signature, 4);
    buf_write(f, &r, sizeof r);
    return buf_set_size(f, buf_tell(f));
}

// 0x5c2e50: a stream on a file of the archive, or null. The folder of the path must start with the mount
// folder; mode 0x8000 only takes entries marked by the last listing. Mode 1 (write) needs check_disk and
// no open stream; it opens the archive for writing (open flags | 6), moves the entry to the end, empties
// it (sizes, CRC, method, extra field; date and time now) and writes its local header.
ZStream *THISCALL ZZipDirOps::OpenFile(const char *path, DWORD mode)
{
    if (!loaded && !dir_load(this))
        fatal_empty(this);
    ZString folder;
    zstring_ctor_text(&folder, path);
    path_folder(zstring_get_buffer(&folder, 0));
    zstring_release_buffer(&folder, -1);
    const char *f = zstring_c_str(&folder);
    DWORD folder_length = zstring_length(&folder);
    ZString mount;
    zstring_ctor_text(&mount, prefix);
    DWORD mount_length = zstring_length(&mount);
    if (folder_length < mount_length || crt_strnicmp(f, zstring_c_str(&mount), mount_length) != 0) {
        zstring_dtor(&mount);
        zstring_dtor(&folder);
        return nullptr;
    }
    zstring_dtor(&mount);
    ZZipEntry *e = count && path && *path ? dir_find(this, path, nullptr) : nullptr;
    if (!e || ((mode & 0x8000) && !e->listed)) {
        zstring_dtor(&folder);
        return nullptr;
    }
    if (mode & 1) {
        if (!check_disk || unknown1c) {
            zstring_dtor(&folder);
            return nullptr;
        }
        if (!loaded && !dir_load(this))
            fatal_empty(this);
        CFileBuf *file_buf = dir_file(this);
        if (!file_buf->stream || (file_buf->flags & 2) != 2) {
            vcall<DWORD>(file_buf, 1);
            if (!buf_open(file_buf, this->path, open_flags | 6)) {
                zstring_dtor(&folder);
                return nullptr;
            }
        }
        if ((DWORD)(e - entries) != count - 1)
            e = dir_move_to_end(this, e);
        e->packed_size = 0;
        e->size = 0;
        dos_now(&e->date, &e->time);
        e->crc = 0;
        e->method = 0;
        e->extra_length = 0;
        dir_write_local_header(this, e);
        unknown01 = 1;
    }
    auto *s = static_cast<ZZipStream *>(pool_alloc(sizeof(ZZipStream)));
    if (s) {
        stream_ctor(s);
        s->vftable = vt(ZIP_STREAM_VTABLE);
        s->position = 0;
        s->entry = nullptr;
        s->file = nullptr;
    }
    if (!stream_open(s, data_start(e), e, dir_file(this), this->path, this, mode) && s) {
        vcall<ZZipStream *>(s, STREAM_DELETE, 1u);
        s = nullptr;
    }
    zstring_dtor(&folder);
    return s;
}

// 0x5c30d0: signature, the local header from the entry (the two sizes in swapped order, equal for stored
// files) and the name without the mount folder, at the entry's offset; no extra field
DWORD THISCALL ZZipDirOps::WriteLocalHeader(const ZZipEntry *e)
{
    CFileBuf *f = dir_file(this);
    buf_seek(f, e->offset);
    DWORD signature = ZIP_LOCAL;
    buf_write(f, &signature, 4);
#pragma pack(push, 2)
    struct {
        WORD version, flags, method, time, date;
        DWORD crc, packed_size, size;
        WORD name_length, extra_length;
    } h;
#pragma pack(pop)
    static_assert(sizeof h == 0x1a);
    h.version = e->version_made;
    h.flags = e->flags;
    h.method = e->method;
    h.time = e->time;
    h.date = e->date;
    h.crc = e->crc;
    h.packed_size = e->size;
    h.size = e->packed_size;
    h.name_length = e->name_length;
    h.extra_length = e->extra_length;
    buf_write(f, &h, sizeof h);
    return buf_write(f, archive_name(e), h.name_length);
}

// ---- CRC-32 (table 0x627814, start value from 0x592380) ---------------------------------------------

DWORD __fastcall Crc_Final(DWORD crc) { return ~crc; }

DWORD __fastcall Crc_Update(DWORD crc, const void *p, DWORD n)
{
    const BYTE *b = static_cast<const BYTE *>(p);
    const DWORD *table = glob<const DWORD *>(CRC_TABLE);
    for (; n; n--)
        crc = table[(*b++ ^ crc) & 0xff] ^ (crc >> 8);
    return crc;
}

// ---- ZZipStream --------------------------------------------------------------------------------------

// 0x5c35c0: with mode 0x10 a file buffer of its own (pool) opened on the archive (the result is that of
// the open); otherwise the archive's buffer, opened if needed (the open streams count up only if it is
// open), the entry is marked as open and the result is true
bool THISCALL ZZipStream::Open(DWORD data_offset, ZZipEntry *e, CFileBuf *f, const char *archive, ZZipDir *d, DWORD m)
{
    mode = m;
    dir = d;
    position = data_offset;
    entry = e;
    own_file = (m >> 4) & 1;
    if (own_file == 1) {
        auto *b = static_cast<CFileBuf *>(pool_alloc(sizeof(CFileBuf)));
        file = b ? buf_ctor(b) : nullptr;
        return buf_open(file, archive, d->open_flags | 0x404);
    }
    e->unknown2e = 1;
    CFileBuf *df = dir_file(d);
    if (df->stream || buf_open(df, d->path, d->open_flags | 0x404))
        d->unknown1c++;
    file = f;
    return true;
}

ZZipStream *THISCALL ZZipStream::DeletingDtor(DWORD flags)
{
    vftable = vt(ZIP_STREAM_VTABLE);
    stream_close(this);
    stream_dtor(this);
    if (flags & 1)
        pool_free(this, sizeof(ZZipStream));
    return this;
}

// 0x5c3220: writes at the position; growing past the size makes both sizes of the entry the position
DWORD THISCALL ZZipStream::Write(const void *p, DWORD n)
{
    if (!entry)
        return 0;
    buf_seek(file, position);
    DWORD w = buf_write(file, p, n);
    position += w;
    DWORD size = vcall<DWORD>(this, STREAM_SIZE);
    if (vcall<DWORD>(this, STREAM_TELL) > size) {
        entry->size = vcall<DWORD>(this, STREAM_TELL);
        entry->packed_size = entry->size;
    }
    return w;
}

// 0x5c3290: at most up to the end of the entry
DWORD THISCALL ZZipStream::Read(void *p, DWORD n)
{
    if (!entry)
        return 0;
    DWORD start = data_start(entry);
    if (position - start + n > entry->size)
        n = start - position + entry->size;
    if (!n)
        return 0;
    buf_seek(file, position);
    DWORD r = buf_read(file, p, n);
    position += r;
    return r;
}

// 0x5c3320: the new position must lie inside the data (signed); the end itself is not allowed. al is the
// result; the rest of eax is what the original leaves there (the entry, the end, the end), as SetSize
// passes it on.
DWORD THISCALL ZZipStream::Skip(DWORD delta)
{
    if (!entry)
        return 0;
    DWORD p = delta + position, start = data_start(entry), end = start + entry->size;
    if ((int)p < (int)start)
        return (DWORD)(uintptr_t)entry & 0xffffff00;
    if ((int)p >= (int)end)
        return end & 0xffffff00;
    position = p;
    return (end & 0xffffff00) | 1;
}

// 0x5c3380: 0 to the size (signed); eax as the original's (the entry, or the new position)
DWORD THISCALL ZZipStream::Seek(DWORD p)
{
    if (!entry)
        return 0;
    if ((int)p < 0 || (int)p > (int)entry->size)
        return (DWORD)(uintptr_t)entry & 0xffffff00;
    position = data_start(entry) + p;
    return (position & 0xffffff00) | 1;
}

DWORD THISCALL ZZipStream::Tell() { return entry ? position - data_start(entry) : 0xffffffff; }
DWORD THISCALL ZZipStream::Size() { return entry ? entry->size : 0xffffffff; }

// 0x5c3400: no null check; a position behind the new end moves to it (the result is that of Seek,
// otherwise the position)
DWORD THISCALL ZZipStream::SetSize(DWORD n)
{
    entry->size = n;
    entry->packed_size = entry->size;
    DWORD size = vcall<DWORD>(this, STREAM_SIZE);
    DWORD p = vcall<DWORD>(this, STREAM_TELL);
    if (p <= size)
        return p;
    return vcall<DWORD>(this, STREAM_SEEK, vcall<DWORD>(this, STREAM_SIZE));
}

// 0x5c3440: with mode 2 the CRC of the data and the local header. An own file buffer is deleted.
// Otherwise one open stream less; if the directory changed and the archive is open for writing, the
// central directory goes behind the last entry, then the end record, and the archive is closed. With no
// stream left and without keep_open the archive is closed as well.
void THISCALL ZZipStream::Close()
{
    if (!file)
        return;
    if (mode & 2) {
        entry->crc = stream_crc(this);
        dir_write_local_header(dir, entry);
    }
    if (own_file == 1) {
        if (file)
            vcall<CFileBuf *>(file, 0, 1u);
        file = nullptr;
        return;
    }
    ZZipDir *d = dir;
    d->unknown1c--;
    CFileBuf *df = dir_file(d);
    if (d->unknown01 && df->stream && (df->flags & 2) == 2) {
        DWORD offset = entry_end(&d->entries[d->count - 1]);
        buf_seek(df, offset);
        for (DWORD i = 0; i < d->count; i++) {
            const ZZipEntry *e = &d->entries[i];
            DWORD signature = ZIP_CENTRAL;
            buf_write(dir_file(d), &signature, 4);
            const char *name = archive_name(e);
            buf_write(dir_file(d), &e->version_made, 0x2a);
            buf_write(dir_file(d), name, e->name_length);
        }
        dir_write_end(d, offset);
        d->unknown01 = 0;
        vcall<DWORD>(dir_file(d), 1);
    }
    if (!d->unknown1c && !d->keep_open && dir_file(d)->stream)
        vcall<DWORD>(dir_file(d), 1);
    entry->unknown2e = 0;
    file = nullptr;
}

// ---- ZZipWriter --------------------------------------------------------------------------------------

// 0x5c3660 (fastcall archive, name)
ZZipWriter *__fastcall ZipWriter_Create(const char *archive, const char *file_name)
{
    auto *w = static_cast<ZZipWriter *>(pool_alloc(sizeof(ZZipWriter)));
    if (w) {
        stream_ctor(w);
        buf_ctor(&w->file);
        w->vftable = vt(ZIP_WRITER_VTABLE);
        w->directory = nullptr;
    }
    if (!writer_open(w, archive, file_name) && w) {
        vcall<ZZipWriter *>(w, STREAM_DELETE, 1u);
        return nullptr;
    }
    return w;
}

ZZipWriter *THISCALL ZZipWriter::DeletingDtor(DWORD flags)
{
    vftable = vt(ZIP_WRITER_VTABLE);
    writer_close(this);
    buf_dtor(&file);
    stream_dtor(this);
    if (flags & 1)
        pool_free(this, sizeof(ZZipWriter));
    return this;
}

// 0x5c38d0: opens the archive on disk (0x26), needs the end record 0x16 bytes before the end, keeps the
// central directory in memory and writes a local header for the name where it started (version 10,
// everything else 0)
bool THISCALL ZZipWriter::Open(const char *archive, const char *file_name)
{
    char *d = name;
    for (const char *s = file_name; (*d++ = *s++);)
        ;
    if (!buf_open(&file, archive, 0x26))
        return false;
    buf_seek(&file, buf_size(&file) - 0x16);
    DWORD signature = (DWORD)(uintptr_t)file_name;   // the original reads into the slot of this argument
    buf_read_dword(&file, &signature);
    if (signature != ZIP_END && signature != ZIP_END_RE) {
        vcall<DWORD>(&file, 1);
        return false;
    }
    buf_read(&file, &end, sizeof end);
    buf_seek(&file, end.directory_offset);
    if (directory) {
        heap_free(directory);
        directory = nullptr;
    }
    directory = static_cast<BYTE *>(heap_alloc(end.directory_size));
    buf_read(&file, directory, end.directory_size);
    buf_seek(&file, end.directory_offset);
    header_offset = buf_tell(&file);
    buf_write_dword(&file, ZIP_LOCAL);
    WORD h[13] = {10};   // version, flags, method, time, date, crc, both sizes, name length, extra length
    h[11] = (WORD)text_length(name);
    buf_write(&file, h, 0x1a);
    buf_write(&file, name, h[11]);
    data_offset = buf_tell(&file);
    handle = reinterpret_cast<void *>(1);
    return true;
}

DWORD THISCALL ZZipWriter::Read(void *p, DWORD n) { return buf_read(&file, p, n); }
DWORD THISCALL ZZipWriter::Write(const void *p, DWORD n) { return buf_write(&file, p, n); }
DWORD THISCALL ZZipWriter::Skip(DWORD delta) { return buf_skip(&file, delta); }
DWORD THISCALL ZZipWriter::Seek(DWORD p) { return buf_seek(&file, p + data_offset); }
DWORD THISCALL ZZipWriter::Tell() { return buf_tell(&file) - data_offset; }
DWORD THISCALL ZZipWriter::Size() { return buf_size(&file) - data_offset; }
DWORD THISCALL ZZipWriter::SetSize(DWORD n) { return buf_set_size(&file, n + data_offset); }

// 0x5c3700: if the archive is open, the local header again with the size (no CRC, time or date), the old
// central directory behind the data, a central header for the new file (version made 0x2a, needed 10)
// and the end record with one entry more and the signature "RE\5\6"; then the archive is closed.
void THISCALL ZZipWriter::Close()
{
    if (file.stream) {
        DWORD size = buf_size(&file) - data_offset;
        WORD n = (WORD)text_length(name);
#pragma pack(push, 2)
        struct {
            WORD version, flags, method, time, date;
            DWORD crc, packed_size, size;
            WORD name_length, extra_length;
        } h = {10, 0, 0, 0, 0, 0, size, size, n, 0};
        struct {
            WORD version_made, version, flags, method, time, date;
            DWORD crc, packed_size, size;
            WORD name_length, extra_length, comment_length, disk, internal;
            DWORD external, offset;
        } c = {0x2a, 10, 0, h.method, h.time, h.date, h.crc, h.packed_size, h.size, n, 0, 0, 0, 0, 0, header_offset};
#pragma pack(pop)
        static_assert(sizeof h == 0x1a && sizeof c == 0x2a);
        buf_seek(&file, header_offset);
        buf_write_dword(&file, ZIP_LOCAL);
        buf_write(&file, &h, sizeof h);
        end.directory_offset = buf_size(&file);
        buf_seek(&file, end.directory_offset);
        buf_write(&file, directory, end.directory_size);
        buf_write_dword(&file, ZIP_CENTRAL);
        buf_write(&file, &c, sizeof c);
        buf_write(&file, name, n);
        end.count++;
        end.total++;
        end.directory_size += n + 0x2e;
        buf_write_dword(&file, ZIP_END_RE);
        buf_write(&file, &end, sizeof end);
    }
    vcall<DWORD>(&file, 1);
    if (directory) {
        heap_free(directory);
        directory = nullptr;
    }
    handle = nullptr;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(zip_stream)
{
    const Hook list[] = {
        {0x5c2080, ptr(&ZZipDirOps::MoveEntryToEnd), "ZZipDir::MoveEntryToEnd", 8, {0x83, 0xec, 0x08, 0xb8, 0xab, 0xaa, 0xaa, 0xaa}},
        {0x5c2dd0, ptr(&ZZipDirOps::WriteEnd), "ZZipDir::WriteEnd", 6, {0x83, 0xec, 0x14, 0x56, 0x8b, 0xf1, 0x66, 0xc7}},
        {0x5c2e50, ptr(&ZZipDirOps::OpenFile), "ZZipDir::OpenFile", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x53, 0x56}},
        {0x5c30d0, ptr(&ZZipDirOps::WriteLocalHeader), "ZZipDir::WriteLocalHeader", 5, {0x83, 0xec, 0x1c, 0x53, 0x56, 0x8b, 0x74, 0x24}},
        {0x5c31a0, ptr(&Crc_Final), "Crc_Final", 5, {0x8b, 0xc1, 0xf7, 0xd0, 0xc3, 0x90, 0x90, 0x90}},
        {0x5c31b0, ptr(&Crc_Update), "Crc_Update", 7, {0x8b, 0xc1, 0x56, 0x8b, 0x4c, 0x24, 0x08, 0x8b}},
        {0x5c31f0, ptr(&ZZipStream::DeletingDtor), "ZZipStream::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xd0, 0x60, 0x60}},
        {0x5c3220, ptr(&ZZipStream::Write), "ZZipStream::Write", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x14, 0x85, 0xc0}},
        {0x5c3290, ptr(&ZZipStream::Read), "ZZipStream::Read", 7, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x46, 0x14, 0x85}},
        {0x5c3320, ptr(&ZZipStream::Skip), "ZZipStream::Skip", 6, {0x8b, 0x41, 0x14, 0x56, 0x85, 0xc0, 0x57, 0x74}},
        {0x5c3380, ptr(&ZZipStream::Seek), "ZZipStream::Seek", 6, {0x8b, 0x41, 0x14, 0x56, 0x85, 0xc0, 0x74, 0x2f}},
        {0x5c33c0, ptr(&ZZipStream::Tell), "ZZipStream::Tell", 5, {0x8b, 0x51, 0x14, 0x85, 0xd2, 0x75, 0x04, 0x83}},
        {0x5c33f0, ptr(&ZZipStream::Size), "ZZipStream::Size", 5, {0x8b, 0x41, 0x14, 0x85, 0xc0, 0x75, 0x04, 0x83}},
        {0x5c3400, ptr(&ZZipStream::SetSize), "ZZipStream::SetSize", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x57}},
        {0x5c3440, ptr(&ZZipStream::Close), "ZZipStream::Close", 5, {0x83, 0xec, 0x10, 0x53, 0x55, 0x8b, 0xe9, 0x33}},
        {0x5c35c0, ptr(&ZZipStream::Open), "ZZipStream::Open", 5, {0x8b, 0x44, 0x24, 0x18, 0x56, 0x8b, 0x54, 0x24}},
        {0x5c3660, ptr(&ZipWriter_Create), "ZipWriter_Create", 5, {0x53, 0x56, 0x8b, 0xd9, 0x57, 0x8b, 0xfa, 0xb9}},
        {0x5c36c0, ptr(&ZZipWriter::DeletingDtor), "ZZipWriter::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x08, 0x61, 0x60}},
        {0x5c3700, ptr(&ZZipWriter::Close), "ZZipWriter::Close", 5, {0x83, 0xec, 0x48, 0x55, 0x56, 0x8b, 0xf1, 0x33}},
        {0x5c38d0, ptr(&ZZipWriter::Open), "ZZipWriter::Open", 5, {0x83, 0xec, 0x1c, 0x33, 0xc0, 0x53, 0x56, 0x8b}},
        {0x5c3a70, ptr(&ZZipWriter::Read), "ZZipWriter::Read", 8, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x54, 0x24, 0x04}},
        {0x5c3a90, ptr(&ZZipWriter::Write), "ZZipWriter::Write", 8, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x54, 0x24, 0x04}},
        {0x5c3ab0, ptr(&ZZipWriter::Skip), "ZZipWriter::Skip", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xc1, 0x0c, 0x50}},
        {0x5c3ac0, ptr(&ZZipWriter::Seek), "ZZipWriter::Seek", 6, {0x8b, 0x81, 0x44, 0x01, 0x00, 0x00, 0x8b, 0x54}},
        {0x5c3ae0, ptr(&ZZipWriter::Tell), "ZZipWriter::Tell", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x0c, 0xe8, 0x95}},
        {0x5c3b00, ptr(&ZZipWriter::Size), "ZZipWriter::Size", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x0c, 0xe8, 0x85}},
        {0x5c3b20, ptr(&ZZipWriter::SetSize), "ZZipWriter::SetSize", 6, {0x8b, 0x81, 0x44, 0x01, 0x00, 0x00, 0x8b, 0x54}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
