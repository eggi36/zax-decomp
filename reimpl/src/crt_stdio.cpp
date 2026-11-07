// The C runtime's streams and low-level files: fopen/_fsopen/_openfile/_getstream/_sopen, fclose,
// fread, fwrite, fseek, ftell, _flsbuf, _filbuf, _ungetc_lk, _flush, _fflush_lk, flsall, _getbuf,
// _stbuf/_ftbuf, fprintf, _fileno; _read, _write, _lseek, _close, _commit, _chsize, _setmode_lk, the
// handle table (_alloc_osfhnd, _set_osfhnd, _free_osfhnd, _get_osfhandle, _lock_fhandle,
// _unlock_fhandle, _isatty); remove, rename (docs/DECOMPILATION.md 10, "C runtime").
//
// In the exe the names of 0x5d1a3c/0x5d1a6b and 0x5d1b75/0x5d1ba4 were swapped (functions.csv:
// "fread"/"_fread_lk" at 0x5d1a3c/0x5d1a6b): 0x5d1a3c writes (_write, _flsbuf) and is fwrite,
// 0x5d1b75 reads (_read, _filbuf) and is fread.
#include "crt_common.h"

namespace zax::crt {

namespace {

constexpr DWORD NSTREAM = 0x717540, PIOB = 0x716530, CFLUSH = 0x7158f0, COMMODE = 0x715c14, FMODE = 0x715c50,
                UMASKVAL = 0x71587c, APP_TYPE = 0x6298d8, STDBUF = 0x715994;
constexpr DWORD STDOUT_ = 0x629900, STDERR_ = 0x629920;
constexpr DWORD IAT_GET_FILE_TYPE = 0x5ea048;
constexpr DWORD PIOINFO_END = PIOINFO + 4 * 64;   // __pioinfo has 64 blocks of 32 handles

int nhandle() { return *glob<int *>(NHANDLE); }
bool valid(int fh) { return (unsigned)fh < (unsigned)nhandle() && (ioinfo(fh)->osfile & FOPEN); }
void bad_handle()
{
    *errno_ptr() = 9;
    *doserrno_ptr() = 0;
}
DWORD last_error() { return api<DWORD(WINAPI *)()>(iat::GetLastError)(); }

// zeros for _chsize_lk (the exe uses a cleared 4 KB buffer on its stack; the DLL has no stack probes)
const BYTE ZEROS[0x1000] = {};

}  // namespace

// ---- the handle table -----------------------------------------------------------------------------------

// _lock_fhandle 0x5d8d73 / _unlock_fhandle 0x5d8dd2: the handle's critical section, made on first use
void __cdecl lock_fhandle(int fh)
{
    IoInfo *p = ioinfo(fh);
    if (!p->lockinitflag) {
        lock(LOCKTAB_LOCK);
        if (!p->lockinitflag) {
            api<void(WINAPI *)(void *)>(iat::InitializeCriticalSection)(p->lock);
            p->lockinitflag++;
        }
        unlock(LOCKTAB_LOCK);
    }
    api<void(WINAPI *)(void *)>(iat::EnterCriticalSection)(ioinfo(fh)->lock);
}
void __cdecl unlock_fhandle(int fh) { api<void(WINAPI *)(void *)>(iat::LeaveCriticalSection)(ioinfo(fh)->lock); }

// _get_osfhandle 0x5d8d31
HANDLE __cdecl get_osfhandle(int fh)
{
    if (valid(fh))
        return ioinfo(fh)->osfhnd;
    bad_handle();
    return INVALID_HANDLE_VALUE;
}

// _isatty 0x5dc35e: a character device
int __cdecl isatty_(int fh) { return (unsigned)fh >= (unsigned)nhandle() ? 0 : ioinfo(fh)->osfile & FDEV; }

// _alloc_osfhnd 0x5d8b13: the first free slot (its lock taken and kept), or a new block of 32 handles
int __cdecl alloc_osfhnd()
{
    int fh = -1;
    lock(0x12);   // _OSFHND_LOCK
    IoInfo **block = glob<IoInfo **>(PIOINFO);
    for (int i = 0; block < glob<IoInfo **>(PIOINFO) + 64 && fh == -1; block++, i++) {   // PIOINFO_END
        if (!*block) {
            IoInfo *p = (IoInfo *)exe_malloc(0x480);
            if (p) {
                *glob<int *>(NHANDLE) += 32;
                *block = p;
                for (IoInfo *q = p; q < *block + 32; q++) {
                    q->osfile = 0;
                    q->osfhnd = INVALID_HANDLE_VALUE;
                    q->lockinitflag = 0;
                    q->pipech = 10;
                }
                fh = i * 32;
                lock_fhandle(fh);
            }
            break;
        }
        for (IoInfo *p = *block; p < *block + 32; p++) {
            if (p->osfile & FOPEN)
                continue;
            if (!p->lockinitflag) {
                lock(LOCKTAB_LOCK);
                if (!p->lockinitflag) {
                    api<void(WINAPI *)(void *)>(iat::InitializeCriticalSection)(p->lock);
                    p->lockinitflag++;
                }
                unlock(LOCKTAB_LOCK);
            }
            api<void(WINAPI *)(void *)>(iat::EnterCriticalSection)(p->lock);
            if (p->osfile & FOPEN) {
                api<void(WINAPI *)(void *)>(iat::LeaveCriticalSection)(p->lock);
                continue;
            }
            p->osfhnd = INVALID_HANDLE_VALUE;
            fh = (int)(p - *block) + i * 32;
            break;
        }
    }
    unlock(0x12);
    return fh;
}

// _set_osfhnd 0x5d8c36 / _free_osfhnd 0x5d8cb2: console programs also set the standard handles
int __cdecl set_osfhnd(int fh, HANDLE value)
{
    if ((unsigned)fh < (unsigned)nhandle() && ioinfo(fh)->osfhnd == INVALID_HANDLE_VALUE) {
        if (*glob<int *>(APP_TYPE) == 1 && fh <= 2)
            api<BOOL(WINAPI *)(DWORD, HANDLE)>(iat::SetStdHandle)((DWORD)(-10 - fh), value);
        ioinfo(fh)->osfhnd = value;
        return 0;
    }
    bad_handle();
    return -1;
}
int __cdecl free_osfhnd(int fh)
{
    if ((unsigned)fh < (unsigned)nhandle() && (ioinfo(fh)->osfile & FOPEN) && ioinfo(fh)->osfhnd != INVALID_HANDLE_VALUE) {
        if (*glob<int *>(APP_TYPE) == 1 && fh <= 2)
            api<BOOL(WINAPI *)(DWORD, HANDLE)>(iat::SetStdHandle)((DWORD)(-10 - fh), nullptr);
        ioinfo(fh)->osfhnd = INVALID_HANDLE_VALUE;
        return 0;
    }
    bad_handle();
    return -1;
}

// ---- low-level I/O --------------------------------------------------------------------------------------

// _lseek_lk 0x5d88e4: SetFilePointer; -1 counts as failed only when GetLastError says so
long __cdecl lseek_lk(int fh, long pos, int method)
{
    HANDLE h = get_osfhandle(fh);
    if (h == INVALID_HANDLE_VALUE) {
        *errno_ptr() = 9;
        return -1;
    }
    const long r = (long)api<DWORD(WINAPI *)(HANDLE, long, long *, DWORD)>(iat::SetFilePointer)(h, pos, nullptr, (DWORD)method);
    const DWORD err = r == -1 ? last_error() : 0;
    if (err) {
        dosmaperr(err);
        return -1;
    }
    ioinfo(fh)->osfile &= (BYTE)~FEOFLAG;
    return r;
}

int __cdecl read_lk(int fh, char *buf, unsigned cnt);
int __cdecl write_lk(int fh, const char *buf, unsigned cnt);
int __cdecl close_lk(int fh);
int __cdecl chsize_lk(int fh, long size);

// The locking wrappers _lseek 0x5d887f, _read 0x5d8641, _write 0x5d7d45, _close 0x5d7ba7
long __cdecl lseek_(int fh, long pos, int method)
{
    if (!valid(fh)) {
        bad_handle();
        return -1;
    }
    lock_fhandle(fh);
    const long r = lseek_lk(fh, pos, method);
    unlock_fhandle(fh);
    return r;
}
int __cdecl read_(int fh, char *buf, unsigned cnt)
{
    if (!valid(fh)) {
        bad_handle();
        return -1;
    }
    lock_fhandle(fh);
    const int r = read_lk(fh, buf, cnt);
    unlock_fhandle(fh);
    return r;
}
int __cdecl write_(int fh, const char *buf, unsigned cnt)
{
    if (!valid(fh)) {
        bad_handle();
        return -1;
    }
    lock_fhandle(fh);
    const int r = write_lk(fh, buf, cnt);
    unlock_fhandle(fh);
    return r;
}
int __cdecl close_(int fh)
{
    if (!valid(fh)) {
        bad_handle();
        return -1;
    }
    lock_fhandle(fh);
    const int r = close_lk(fh);
    unlock_fhandle(fh);
    return r;
}

// _read_lk 0x5d86a6: text mode turns CR LF into LF and stops at Ctrl-Z; a CR at the end of the block
// peeks at the next byte (pipes and devices keep it in pipech, files seek back)
int __cdecl read_lk(int fh, char *buf, unsigned cnt)
{
    using ReadFileFn = BOOL(WINAPI *)(HANDLE, void *, DWORD, DWORD *, void *);
    int bytesread = 0;
    char *p = buf;
    if (!cnt)
        return 0;
    IoInfo *io = ioinfo(fh);
    if (io->osfile & FEOFLAG)
        return 0;
    if ((io->osfile & (FPIPE | FDEV)) && io->pipech != 10) {
        cnt--;
        *buf = (char)io->pipech;
        p = buf + 1;
        bytesread = 1;
        ioinfo(fh)->pipech = 10;
    }
    DWORD os_read;
    if (!api<ReadFileFn>(iat::ReadFile)(ioinfo(fh)->osfhnd, p, cnt, &os_read, nullptr)) {
        const DWORD err = last_error();
        if (err == 5) {
            *errno_ptr() = 9;
            *doserrno_ptr() = 5;
            return -1;
        }
        if (err == 0x6d)   // ERROR_BROKEN_PIPE: end of file
            return 0;
        dosmaperr(err);
        return -1;
    }
    bytesread += (int)os_read;
    io = ioinfo(fh);
    if (!(io->osfile & FTEXT))
        return bytesread;
    if (os_read && *buf == '\n')
        io->osfile |= FCRLF;
    else
        io->osfile &= (BYTE)~FCRLF;
    char *src = buf, *dst = buf, *end = buf + bytesread;
    while (src < end) {
        if (*src == 0x1a) {
            if (!(ioinfo(fh)->osfile & FDEV))
                ioinfo(fh)->osfile |= FEOFLAG;
            break;
        }
        if (*src != '\r') {
            *dst++ = *src++;
            continue;
        }
        if (src < end - 1) {
            if (src[1] == '\n') {
                src += 2;
                *dst++ = '\n';
            } else {
                *dst++ = '\r';
                src++;
            }
            continue;
        }
        src++;
        char peek;
        const BOOL ok = api<ReadFileFn>(iat::ReadFile)(ioinfo(fh)->osfhnd, &peek, 1, &os_read, nullptr);
        if ((!ok && last_error()) || !os_read) {
            *dst++ = '\r';
            continue;
        }
        if (ioinfo(fh)->osfile & (FDEV | FPIPE)) {
            if (peek == '\n')
                *dst++ = '\n';
            else {
                *dst++ = '\r';
                ioinfo(fh)->pipech = (BYTE)peek;
            }
        } else if (dst == buf && peek == '\n')
            *dst++ = '\n';
        else {
            lseek_lk(fh, -1, 1);
            if (peek != '\n')
                *dst++ = '\r';
        }
    }
    return (int)(dst - buf);
}

// _write_lk 0x5d7daa: append mode seeks to the end first; text mode writes LF as CR LF through a 1 KB
// buffer; the number of bytes of the caller's buffer
int __cdecl write_lk(int fh, const char *buf, unsigned cnt)
{
    using WriteFileFn = BOOL(WINAPI *)(HANDLE, const void *, DWORD, DWORD *, void *);
    int lfcount = 0, charcount = 0;
    DWORD dosretval = 0, written;
    if (!cnt)
        return 0;
    if (ioinfo(fh)->osfile & FAPPEND)
        lseek_lk(fh, 0, 2);
    if (ioinfo(fh)->osfile & FTEXT) {
        const char *p = buf;
        while ((unsigned)(p - buf) < cnt) {
            char lfbuf[0x402];
            char *q = lfbuf;
            while ((unsigned)(p - buf) < cnt) {
                const char ch = *p++;
                if (ch == '\n') {
                    lfcount++;
                    *q++ = '\r';
                }
                *q++ = ch;
                if (q - lfbuf >= 0x400)
                    break;
            }
            const DWORD n = (DWORD)(q - lfbuf);
            if (!api<WriteFileFn>(iat::WriteFile)(ioinfo(fh)->osfhnd, lfbuf, n, &written, nullptr)) {
                dosretval = last_error();
                break;
            }
            charcount += (int)written;
            if ((int)written < (int)n)
                break;
        }
    } else {
        if (api<WriteFileFn>(iat::WriteFile)(ioinfo(fh)->osfhnd, buf, cnt, &written, nullptr)) {
            dosretval = 0;
            charcount = (int)written;
        } else
            dosretval = last_error();
    }
    if (charcount)
        return charcount - lfcount;
    if (dosretval) {
        if (dosretval == 5) {
            *errno_ptr() = 9;
            *doserrno_ptr() = 5;
        } else
            dosmaperr(dosretval);
        return -1;
    }
    if ((ioinfo(fh)->osfile & FDEV) && *buf == 0x1a)
        return 0;
    *errno_ptr() = 28;   // ENOSPC
    *doserrno_ptr() = 0;
    return -1;
}

// _close_lk 0x5d7c04: stdout and stderr on the same handle close nothing
int __cdecl close_lk(int fh)
{
    DWORD err = 0;
    if (get_osfhandle(fh) != INVALID_HANDLE_VALUE) {
        if ((fh == 1 || fh == 2) && get_osfhandle(1) == get_osfhandle(2))
            err = 0;
        else if (!api<BOOL(WINAPI *)(HANDLE)>(iat::CloseHandle)(get_osfhandle(fh)))
            err = last_error();
    }
    free_osfhnd(fh);
    ioinfo(fh)->osfile = 0;
    if (err) {
        dosmaperr(err);
        return -1;
    }
    return 0;
}

// _commit 0x5d7cb2: FlushFileBuffers
int __cdecl commit(int fh)
{
    if (!valid(fh)) {
        *errno_ptr() = 9;
        return -1;
    }
    lock_fhandle(fh);
    int r;
    if (ioinfo(fh)->osfile & FOPEN) {
        const DWORD err = api<BOOL(WINAPI *)(HANDLE)>(iat::FlushFileBuffers)(get_osfhandle(fh)) ? 0 : last_error();
        if (!err)
            r = 0;
        else {
            *doserrno_ptr() = err;
            *errno_ptr() = 9;
            r = -1;
        }
    } else {
        *errno_ptr() = 9;
        r = -1;
    }
    unlock_fhandle(fh);
    return r;
}

// _setmode_lk 0x5d8df4: binary (0x8000) or text (0x4000); the old mode
int __cdecl setmode_lk(int fh, int mode)
{
    IoInfo *io = ioinfo(fh);
    const BYTE old = io->osfile & FTEXT;
    if (mode == 0x8000)
        io->osfile &= (BYTE)~FTEXT;
    else if (mode == 0x4000)
        io->osfile |= FTEXT;
    else {
        *errno_ptr() = 22;
        return -1;
    }
    return old ? 0x4000 : 0x8000;
}

// _chsize_lk 0x5d1f21: grows with zeros (binary mode for the time), shrinks with SetEndOfFile; the file
// position stays
int __cdecl chsize_lk(int fh, long size)
{
    int retval = 0;
    const long place = lseek_lk(fh, 0, 1);
    if (place == -1)
        return -1;
    const long filend = lseek_lk(fh, 0, 2);
    if (filend == -1)
        return -1;
    long extend = size - filend;
    if (extend > 0) {
        const int oldmode = setmode_lk(fh, 0x8000);
        do {
            const int n = write_lk(fh, (const char *)ZEROS, (unsigned)(extend < 0x1000 ? extend : 0x1000));
            if (n == -1) {
                if (*doserrno_ptr() == 5)
                    *errno_ptr() = 13;
                retval = -1;
                break;
            }
            extend -= n;
        } while (extend > 0);
        setmode_lk(fh, oldmode);
    } else if (extend < 0) {
        lseek_lk(fh, size, 0);
        retval = api<BOOL(WINAPI *)(HANDLE)>(iat::SetEndOfFile)(get_osfhandle(fh)) ? 0 : -1;
        if (retval == -1) {
            *errno_ptr() = 13;
            *doserrno_ptr() = last_error();
        }
    }
    lseek_lk(fh, place, 0);
    return retval;
}

// _chsize 0x5d1ec8
int __cdecl chsize(int fh, long size)
{
    if (!valid(fh)) {
        *errno_ptr() = 9;
        return -1;
    }
    lock_fhandle(fh);
    const int r = chsize_lk(fh, size);
    unlock_fhandle(fh);
    return r;
}

// _sopen 0x5dc74d
int __cdecl sopen(const char *path, int oflag, int shflag, int pmode)
{
    struct {
        DWORD length;
        void *descriptor;
        BOOL inherit;
    } sa = {0xc, nullptr, 0};
    BYTE fileflags;
    if (oflag & 0x80) {   // _O_NOINHERIT
        sa.inherit = 0;
        fileflags = FNOINHERIT;
    } else {
        fileflags = 0;
        sa.inherit = 1;
    }
    if (!(oflag & 0x8000) && ((oflag & 0x4000) || *glob<int *>(FMODE) != 0x8000))
        fileflags |= FTEXT;
    DWORD access, share, create, attrib;
    auto invalid = [] {
        *errno_ptr() = 22;
        *doserrno_ptr() = 0;
        return -1;
    };
    switch (oflag & 3) {
    case 0: access = 0x80000000; break;
    case 1: access = 0x40000000; break;
    case 2: access = 0xc0000000; break;
    default: return invalid();
    }
    switch (shflag) {
    case 0x10: share = 0; break;
    case 0x20: share = 1; break;
    case 0x30: share = 2; break;
    case 0x40: share = 3; break;
    default: return invalid();
    }
    switch (oflag & 0x700) {
    case 0: case 0x400: create = 3; break;
    case 0x100: create = 4; break;
    case 0x200: case 0x600: create = 5; break;
    case 0x300: create = 2; break;
    case 0x500: case 0x700: create = 1; break;
    default: return invalid();
    }
    attrib = 0x80;
    if ((oflag & 0x100) && !((pmode & ~*glob<int *>(UMASKVAL)) & 0x80))
        attrib = 1;
    if (oflag & 0x40) {
        attrib |= 0x4000000;
        access |= 0x10000;
    }
    if (oflag & 0x1000)
        attrib |= 0x100;
    if (oflag & 0x20)
        attrib |= 0x8000000;
    else if (oflag & 0x10)
        attrib |= 0x10000000;
    const int fh = alloc_osfhnd();
    if (fh == -1) {
        *errno_ptr() = 24;
        *doserrno_ptr() = 0;
        return -1;
    }
    int result = -1;
    HANDLE h = api<HANDLE(WINAPI *)(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE)>(iat::CreateFileA)(
        path, access, share, &sa, create, attrib, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        dosmaperr(last_error());
    else {
        const DWORD type = api<DWORD(WINAPI *)(HANDLE)>(IAT_GET_FILE_TYPE)(h);
        if (!type) {
            api<BOOL(WINAPI *)(HANDLE)>(iat::CloseHandle)(h);
            dosmaperr(last_error());
        } else {
            if (type == 2)
                fileflags |= FDEV;
            else if (type == 3)
                fileflags |= FPIPE;
            set_osfhnd(fh, h);
            fileflags |= FOPEN;
            const BYTE devpipe = fileflags & (FDEV | FPIPE);
            ioinfo(fh)->osfile = fileflags;
            result = fh;
            if (!devpipe && (fileflags & FTEXT) && (oflag & 2)) {
                // a Ctrl-Z at the end of a text file opened for reading and writing is cut off
                const long pos = lseek_lk(fh, -1, 2);
                if (pos == -1) {
                    if (*doserrno_ptr() != 0x83)
                        result = -2;
                } else {
                    char ch = 0;
                    if (!read_lk(fh, &ch, 1) && ch == 0x1a && chsize_lk(fh, pos) == -1)
                        result = -2;
                    else if (lseek_lk(fh, 0, 0) == -1)
                        result = -2;
                }
                if (result == -2) {
                    close_(fh);
                    result = -1;
                }
            }
            if (result != -1 && !devpipe && (oflag & 8))
                ioinfo(fh)->osfile |= FAPPEND;
        }
    }
    unlock_fhandle(fh);
    return result;
}

// ---- streams --------------------------------------------------------------------------------------------

// _getbuf 0x5dc31a: a 4 KB buffer, or the 2-byte _charbuf without memory
void __cdecl getbuf(File *f)
{
    ++*glob<int *>(CFLUSH);
    f->base = (char *)exe_malloc(0x1000);
    if (f->base) {
        f->flag |= IOMYBUF;
        f->bufsiz = 0x1000;
    } else {
        f->flag |= IONBF;
        f->base = (char *)&f->charbuf;
        f->bufsiz = 2;
    }
    f->ptr = f->base;
    f->cnt = 0;
}

// _flush 0x5d18b3: writes what a buffered writing stream holds
int __cdecl flush(File *f)
{
    int r = 0;
    if ((f->flag & 3) == IOWRT && (f->flag & (IOMYBUF | IOYOURBUF))) {
        const int n = (int)(f->ptr - f->base);
        if (n > 0) {
            if (write_(f->file, f->base, (unsigned)n) == n) {
                if (f->flag & IORW)
                    f->flag &= ~IOWRT;
            } else {
                f->flag |= IOERR;
                r = -1;
            }
        }
    }
    f->ptr = f->base;
    f->cnt = 0;
    return r;
}

// _fflush_lk 0x5d1885
int __cdecl fflush_lk(File *f)
{
    if (flush(f))
        return -1;
    if (f->flag & IOCOMMIT)
        return commit(f->file) ? -1 : 0;
    return 0;
}

// flsall 0x5d1918: 1 flushes every open stream (the count), 0 the writing ones (0 or -1)
int __cdecl flsall(int flushflag)
{
    int count = 0, err = 0;
    lock(2);
    for (int i = 0; i < *glob<int *>(NSTREAM); i++) {
        File *f = (*glob<File ***>(PIOB))[i];
        if (!f || !(f->flag & (IOREAD | IOWRT | IORW)))
            continue;
        lock_file2(i, (BYTE *)f);
        f = (*glob<File ***>(PIOB))[i];
        if (f->flag & (IOREAD | IOWRT | IORW)) {
            if (flushflag == 1) {
                if (fflush_lk(f) != -1)
                    count++;
            } else if (flushflag == 0 && (f->flag & IOWRT) && fflush_lk(f) == -1)
                err = -1;
        }
        unlock_file2(i, (BYTE *)(*glob<File ***>(PIOB))[i]);
    }
    unlock(2);
    return flushflag == 1 ? count : err;
}

// _flsbuf 0x5d459f: the character that did not fit; a stream without a buffer gets one (stdout and
// stderr on a console stay unbuffered)
int __cdecl flsbuf(int ch, File *f)
{
    int flag = f->flag;
    const int fh = f->file;
    if (!(flag & (IOWRT | IORW)) || (flag & IOSTRG)) {
        f->flag = flag | IOERR;
        return -1;
    }
    if (flag & IOREAD) {
        f->cnt = 0;
        if (!(flag & IOEOF)) {
            f->flag = flag | IOERR;
            return -1;
        }
        f->ptr = f->base;
        f->flag = flag & ~IOREAD;
    }
    f->flag = (f->flag & ~IOEOF) | IOWRT;
    f->cnt = 0;
    int charcount, written = 0;
    if (!(f->flag & (IOMYBUF | IOYOURBUF | IONBF)) &&
        (((DWORD)(uintptr_t)f != glob_addr(STDOUT_) && (DWORD)(uintptr_t)f != glob_addr(STDERR_)) || !isatty_(fh)))
        getbuf(f);
    if (f->flag & (IOMYBUF | IOYOURBUF)) {
        charcount = (int)(f->ptr - f->base);
        f->ptr = f->base + 1;
        f->cnt = f->bufsiz - 1;
        if (charcount > 0)
            written = write_(fh, f->base, (unsigned)charcount);
        else if (ioinfo(fh)->osfile & FAPPEND)
            lseek_(fh, 0, 2);
        *f->base = (char)ch;
    } else {
        charcount = 1;
        char c = (char)ch;
        written = write_(fh, &c, 1);
    }
    if (written != charcount) {
        f->flag |= IOERR;
        return -1;
    }
    return ch & 0xff;
}

// _filbuf 0x5d8565: the next character from a refilled buffer; string streams end here
int __cdecl filbuf(File *f)
{
    int flag = f->flag;
    if (!(flag & (IOREAD | IOWRT | IORW)) || (flag & IOSTRG))
        return -1;
    if (flag & IOWRT) {
        f->flag = flag | IOERR;
        return -1;
    }
    f->flag = flag | IOREAD;
    if (!(f->flag & (IOMYBUF | IOYOURBUF | IONBF)))
        getbuf(f);
    else
        f->ptr = f->base;
    f->cnt = read_(f->file, f->base, (unsigned)f->bufsiz);
    if (!f->cnt || f->cnt == -1) {
        f->flag |= f->cnt ? IOERR : IOEOF;
        f->cnt = 0;
        return -1;
    }
    if (!(f->flag & (IOWRT | IORW)) && (ioinfo(f->file)->osfile & (FTEXT | FEOFLAG)) == (FTEXT | FEOFLAG))
        f->flag |= IOCTRLZ;
    if (f->bufsiz == 0x200 && (f->flag & IOMYBUF) && !(f->flag & IOSETVBUF))
        f->bufsiz = 0x1000;
    f->cnt--;
    return (BYTE)*f->ptr++;
}

// _ungetc_lk 0x5dc6df: a string stream only takes back the character it holds there
int __cdecl ungetc_lk(int ch, File *f)
{
    if (ch == -1)
        return -1;
    if (!(f->flag & IOREAD) && !((f->flag & IORW) && !(f->flag & IOWRT)))
        return -1;
    if (!f->base)
        getbuf(f);
    if (f->ptr == f->base) {
        if (f->cnt)
            return -1;
        f->ptr++;
    }
    if (f->flag & IOSTRG) {
        if (*--f->ptr != (char)ch) {
            f->ptr++;
            return -1;
        }
    } else
        *--f->ptr = (char)ch;
    f->cnt++;
    f->flag = (f->flag & ~IOEOF) | IOREAD;
    return ch & 0xff;
}

// _freebuf 0x5d7c87
void __cdecl freebuf(File *f)
{
    if ((f->flag & (IOREAD | IOWRT | IORW)) && (f->flag & IOMYBUF)) {
        exe_free(f->base);
        f->flag &= ~(IOMYBUF | IOSETVBUF);
        f->ptr = f->base = nullptr;
        f->cnt = 0;
    }
}

// _getstream 0x5d815c: a free stream of __piob (made on demand, 0x38 bytes with its critical section),
// returned locked
File *__cdecl getstream()
{
    File *found = nullptr;
    lock(2);
    for (int i = 0; i < *glob<int *>(NSTREAM); i++) {
        File **slot = &(*glob<File ***>(PIOB))[i];
        if (!*slot) {
            *slot = (File *)exe_malloc(0x38);
            if (!*slot)
                break;
            BYTE *cs = (BYTE *)*slot + 0x20;
            api<void(WINAPI *)(void *)>(iat::InitializeCriticalSection)(cs);
            api<void(WINAPI *)(void *)>(iat::EnterCriticalSection)((BYTE *)(*glob<File ***>(PIOB))[i] + 0x20);
            found = (*glob<File ***>(PIOB))[i];
            break;
        }
        if ((*slot)->flag & (IOREAD | IOWRT | IORW))
            continue;
        lock_file2(i, (BYTE *)*slot);
        File *f = (*glob<File ***>(PIOB))[i];
        if (!(f->flag & (IOREAD | IOWRT | IORW))) {
            found = f;
            break;
        }
        unlock_file2(i, (BYTE *)f);
    }
    if (found) {
        found->file = -1;
        found->cnt = 0;
        found->flag = 0;
        found->base = nullptr;
        found->ptr = nullptr;
        found->tmpfname = nullptr;
    }
    unlock(2);
    return found;
}

// _openfile 0x5d7fec: the mode text ("r", "w", "a", then +, b, t, c, n, S, R, T, D); a repeated or
// conflicting letter ends the parsing
File *__cdecl openfile(const char *name, const char *mode, int shflag, File *f)
{
    int streamflag = *glob<int *>(COMMODE), modeflag;
    bool commodeset = false, scanset = false;
    switch (*mode) {
    case 'a': modeflag = 0x109; streamflag |= IOWRT; break;
    case 'r': modeflag = 0; streamflag |= IOREAD; break;
    case 'w': modeflag = 0x301; streamflag |= IOWRT; break;
    default: return nullptr;
    }
    bool more = true;
    for (char c; (c = *++mode) && more;) {
        switch (c) {
        case '+':
            if (modeflag & 2) more = false;
            else {
                modeflag = (modeflag & ~1) | 2;
                streamflag = (streamflag & ~3) | IORW;
            }
            break;
        case 'D':
            if (modeflag & 0x40) more = false;
            else modeflag |= 0x40;
            break;
        case 'R':
            if (scanset) more = false;
            else {
                scanset = true;
                modeflag |= 0x10;
            }
            break;
        case 'S':
            if (scanset) more = false;
            else {
                scanset = true;
                modeflag |= 0x20;
            }
            break;
        case 'T':
            if (modeflag & 0x1000) more = false;
            else modeflag |= 0x1000;
            break;
        case 'b':
            if (modeflag & 0xc000) more = false;
            else modeflag |= 0x8000;
            break;
        case 'c':
            if (commodeset) more = false;
            else {
                commodeset = true;
                streamflag |= IOCOMMIT;
            }
            break;
        case 'n':
            if (commodeset) more = false;
            else {
                commodeset = true;
                streamflag &= ~IOCOMMIT;
            }
            break;
        case 't':
            if (modeflag & 0xc000) more = false;
            else modeflag |= 0x4000;
            break;
        default:
            more = false;
        }
    }
    const int fh = sopen(name, modeflag, shflag, 0x1a4);
    if (fh < 0)
        return nullptr;
    ++*glob<int *>(CFLUSH);
    f->flag = streamflag;
    f->cnt = 0;
    f->ptr = nullptr;
    f->base = nullptr;
    f->tmpfname = nullptr;
    f->file = fh;
    return f;
}

// _fsopen 0x5d19f8, fopen 0x5d1a29 (_SH_DENYNO)
File *__cdecl fsopen(const char *name, const char *mode, int shflag)
{
    File *f = getstream();
    if (!f)
        return nullptr;
    File *r = openfile(name, mode, shflag, f);
    unlock_file((BYTE *)f);
    return r;
}
File *__cdecl fopen_(const char *name, const char *mode) { return fsopen(name, mode, 0x40); }

// _fclose_lk 0x5d1839, fclose 0x5d1808 (a string stream is only cleared)
int __cdecl fclose_lk(File *f)
{
    int r = -1;
    if (f->flag & (IOREAD | IOWRT | IORW)) {
        r = flush(f);
        freebuf(f);
        if (close_(f->file) < 0)
            r = -1;
        else if (f->tmpfname) {
            exe_free(f->tmpfname);
            f->tmpfname = nullptr;
        }
    }
    f->flag = 0;
    return r;
}
int __cdecl fclose_(File *f)
{
    if (f->flag & IOSTRG) {
        f->flag = 0;
        return -1;
    }
    lock_file((BYTE *)f);
    const int r = fclose_lk(f);
    unlock_file((BYTE *)f);
    return r;
}

// fwrite's body 0x5d1a6b (named _fread_lk): through the buffer, whole buffer sizes directly, the rest
// character by character through _flsbuf; the number of whole elements written
size_t __cdecl fwrite_lk(const void *data, size_t size, size_t num, File *f)
{
    const char *buf = (const char *)data;
    const size_t total = size * num;
    size_t count = total;
    if (!count)
        return 0;
    unsigned bufsize = (f->flag & (IOMYBUF | IOYOURBUF | IONBF)) ? (unsigned)f->bufsiz : 0x1000;
    while (count) {
        const int buffered = f->flag & (IOMYBUF | IOYOURBUF);
        if (buffered && f->cnt) {
            const size_t n = count < (unsigned)f->cnt ? count : (unsigned)f->cnt;
            memmove_(f->ptr, buf, n);
            f->cnt -= (int)n;
            f->ptr += n;
            count -= n;
            buf += n;
        } else if (count >= bufsize) {
            if (buffered && flush(f))
                return (total - count) / size;
            const size_t n = bufsize ? count - count % bufsize : count;
            const int w = write_(f->file, buf, (unsigned)n);
            if (w == -1) {
                f->flag |= IOERR;
                return (total - count) / size;
            }
            buf += w;
            count -= (unsigned)w;
            if ((unsigned)w < n) {
                f->flag |= IOERR;
                return (total - count) / size;
            }
        } else {
            if (flsbuf((signed char)*buf, f) == -1)
                return (total - count) / size;
            buf++;
            count--;
            bufsize = (unsigned)f->bufsiz;
            if ((int)bufsize <= 0)
                bufsize = 1;
        }
    }
    return num;
}

// fread's body 0x5d1ba4 (named _fwrite_lk)
size_t __cdecl fread_lk(void *data, size_t size, size_t num, File *f)
{
    char *buf = (char *)data;
    const size_t total = size * num;
    size_t count = total;
    if (!count)
        return 0;
    unsigned bufsize = (f->flag & (IOMYBUF | IOYOURBUF | IONBF)) ? (unsigned)f->bufsiz : 0x1000;
    while (count) {
        if ((f->flag & (IOMYBUF | IOYOURBUF | IONBF)) && f->cnt) {
            const size_t n = count < (unsigned)f->cnt ? count : (unsigned)f->cnt;
            memmove_(buf, f->ptr, n);
            count -= n;
            f->cnt -= (int)n;
            f->ptr += n;
            buf += n;
        } else if (count >= bufsize) {
            const size_t n = bufsize ? count - count % bufsize : count;
            const int r = read_(f->file, buf, (unsigned)n);
            if (!r) {
                f->flag |= IOEOF;
                return (total - count) / size;
            }
            if (r == -1) {
                f->flag |= IOERR;
                return (total - count) / size;
            }
            count -= (unsigned)r;
            buf += r;
        } else {
            const int c = filbuf(f);
            if (c == -1)
                return (total - count) / size;
            *buf++ = (char)c;
            count--;
            bufsize = (unsigned)f->bufsiz;
        }
    }
    return num;
}

// fwrite 0x5d1a3c, fread 0x5d1b75: the stream locked around the body
size_t __cdecl fwrite_(const void *data, size_t size, size_t num, File *f)
{
    lock_file((BYTE *)f);
    const size_t r = fwrite_lk(data, size, num, f);
    unlock_file((BYTE *)f);
    return r;
}
size_t __cdecl fread_(void *data, size_t size, size_t num, File *f)
{
    lock_file((BYTE *)f);
    const size_t r = fread_lk(data, size, num, f);
    unlock_file((BYTE *)f);
    return r;
}

long __cdecl ftell_lk(File *f);

// _fseek_lk 0x5d1cb8, fseek 0x5d1c8c
int __cdecl fseek_lk(File *f, long offset, int whence)
{
    if (!(f->flag & (IOREAD | IOWRT | IORW)) || (whence != 0 && whence != 1 && whence != 2)) {
        *errno_ptr() = 22;
        return -1;
    }
    f->flag &= ~IOEOF;
    if (whence == 1) {
        offset += ftell_lk(f);
        whence = 0;
    }
    flush(f);
    if (f->flag & IORW)
        f->flag &= ~(IOWRT | IOREAD);
    else if ((f->flag & IOREAD) && (f->flag & IOMYBUF) && !(f->flag & IOSETVBUF))
        f->bufsiz = 0x200;
    return lseek_(f->file, offset, whence) == -1 ? -1 : 0;
}
int __cdecl fseek_(File *f, long offset, int whence)
{
    lock_file((BYTE *)f);
    const int r = fseek_lk(f, offset, whence);
    unlock_file((BYTE *)f);
    return r;
}

// _ftell_lk 0x5d1d67, ftell 0x5d1d45: the file position less what the buffer still holds (text mode
// counts every LF of the buffer as CR LF)
long __cdecl ftell_lk(File *f)
{
    const int fd = f->file;
    if (f->cnt < 0)
        f->cnt = 0;
    long filepos = lseek_(fd, 0, 1);
    if (filepos < 0)
        return -1;
    if (!(f->flag & (IOMYBUF | IOYOURBUF)))
        return filepos - f->cnt;
    long offset = (long)(f->ptr - f->base);
    if (f->flag & (IOWRT | IOREAD)) {
        if (ioinfo(fd)->osfile & FTEXT)
            for (const char *p = f->base; p < f->ptr; p++)
                if (*p == '\n')
                    offset++;
    } else if (!(f->flag & IORW)) {
        *errno_ptr() = 22;
        return -1;
    }
    if (!filepos)
        return offset;
    if (f->flag & IOREAD) {
        if (!f->cnt)
            offset = 0;
        else {
            long rdcnt = (long)(f->ptr - f->base) + f->cnt;
            if (ioinfo(fd)->osfile & FTEXT) {
                if (lseek_(fd, 0, 2) == filepos) {
                    for (const char *p = f->base, *end = f->base + rdcnt; p < end; p++)
                        if (*p == '\n')
                            rdcnt++;
                    if (f->flag & IOCTRLZ)
                        rdcnt++;
                } else {
                    lseek_(fd, filepos, 0);
                    if ((unsigned long)rdcnt <= 0x200 && (f->flag & IOMYBUF) && !(f->flag & IOSETVBUF))
                        rdcnt = 0x200;
                    else
                        rdcnt = f->bufsiz;
                    if (ioinfo(fd)->osfile & FCRLF)
                        rdcnt++;
                }
            }
            filepos -= rdcnt;
        }
    }
    return filepos + offset;
}
long __cdecl ftell_(File *f)
{
    lock_file((BYTE *)f);
    const long r = ftell_lk(f);
    unlock_file((BYTE *)f);
    return r;
}

// _stbuf 0x5d7f35 / _ftbuf 0x5d7fc2: stdout and stderr on a console get a temporary buffer for one
// printf call
int __cdecl stbuf(File *f)
{
    if (!isatty_(f->file))
        return 0;
    int index;
    if ((DWORD)(uintptr_t)f == glob_addr(STDOUT_))
        index = 0;
    else if ((DWORD)(uintptr_t)f == glob_addr(STDERR_))
        index = 1;
    else
        return 0;
    ++*glob<int *>(CFLUSH);
    if (f->flag & (IOMYBUF | IOYOURBUF | IONBF))
        return 0;
    char **buf = at<char **>(glob_addr(STDBUF) + 4 * (DWORD)index);
    if (!*buf && !(*buf = (char *)exe_malloc(0x1000))) {
        f->base = f->ptr = (char *)&f->charbuf;
        f->bufsiz = f->cnt = 2;
    } else {
        f->bufsiz = 0x1000;
        f->base = f->ptr = *buf;
        f->cnt = 0x1000;
    }
    f->flag |= IOWRT | IOYOURBUF | IOFLRTN;
    return 1;
}
void __cdecl ftbuf(int flag, File *f)
{
    if (flag && (f->flag & IOFLRTN)) {
        flush(f);
        f->flag &= ~(IOYOURBUF | IOFLRTN);
        f->bufsiz = 0;
        f->ptr = f->base = nullptr;
    }
}

// fprintf 0x5d19bc
int __cdecl fprintf_(File *f, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    lock_file((BYTE *)f);
    const int buffing = stbuf(f);
    const int r = output(f, format, (char *)ap);
    ftbuf(buffing, f);
    unlock_file((BYTE *)f);
    va_end(ap);
    return r;
}

// _fileno 0x5de2e0
int __cdecl fileno_(File *f) { return f->file; }

// remove 0x5d17a6, rename 0x5d2046
int __cdecl remove_(const char *path)
{
    const DWORD err = api<BOOL(WINAPI *)(const char *)>(iat::DeleteFileA)(path) ? 0 : last_error();
    if (err) {
        dosmaperr(err);
        return -1;
    }
    return 0;
}
int __cdecl rename_(const char *from, const char *to)
{
    const DWORD err = api<BOOL(WINAPI *)(const char *, const char *)>(iat::MoveFileA)(from, to) ? 0 : last_error();
    if (err) {
        dosmaperr(err);
        return -1;
    }
    return 0;
}

}  // namespace zax::crt

namespace zax {

using namespace crt;

ZAX_HOOKS(crt_stdio)
{
    const Hook list[] = {
        {0x5d8d73, (void *)&lock_fhandle, "_lock_fhandle", 5, {0x8b, 0x44, 0x24, 0x04, 0x53, 0x8b, 0xc8, 0x83}},
        {0x5d8dd2, (void *)&unlock_fhandle, "_unlock_fhandle", 6, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0xc8, 0x83, 0xe0}},
        {0x5d8d31, (void *)&get_osfhandle, "_get_osfhandle", 10, {0x8b, 0x44, 0x24, 0x04, 0x3b, 0x05, 0x00, 0x65}},
        {0x5dc35e, (void *)&isatty_, "_isatty", 10, {0x8b, 0x44, 0x24, 0x04, 0x3b, 0x05, 0x00, 0x65}},
        {0x5d8b13, (void *)&alloc_osfhnd, "_alloc_osfhnd", 5, {0x51, 0x51, 0x53, 0x55, 0x56, 0x57, 0x6a, 0x12}},
        {0x5d8c36, (void *)&set_osfhnd, "_set_osfhnd", 5, {0x8b, 0x4c, 0x24, 0x04, 0x56, 0x3b, 0x0d, 0x00}},
        {0x5d8cb2, (void *)&free_osfhnd, "_free_osfhnd", 5, {0x8b, 0x4c, 0x24, 0x04, 0x56, 0x3b, 0x0d, 0x00}},
        {0x5d88e4, (void *)&lseek_lk, "_lseek_lk", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x56, 0xe8}},
        {0x5d887f, (void *)&lseek_, "_lseek", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x3b, 0x35, 0x00}},
        {0x5d8641, (void *)&read_, "_read", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x3b, 0x35, 0x00}},
        {0x5d86a6, (void *)&read_lk, "_read_lk", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x83, 0x65}},
        {0x5d7d45, (void *)&write_, "_write", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x3b, 0x35, 0x00}},
        {0x5d7daa, (void *)&write_lk, "_write_lk", 9, {0x55, 0x8b, 0xec, 0x81, 0xec, 0x14, 0x04, 0x00}},
        {0x5d7ba7, (void *)&close_, "_close", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x3b, 0x35, 0x00}},
        {0x5d7c04, (void *)&close_lk, "_close_lk", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x56, 0xe8}},
        {0x5d7cb2, (void *)&commit, "_commit", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x3b, 0x1d, 0x00}},
        {0x5d8df4, (void *)&setmode_lk, "_setmode_lk", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xc8, 0x83}},
        {0x5d1f21, (void *)&chsize_lk, "_chsize_lk", 8, {0x55, 0x8b, 0xec, 0xb8, 0x04, 0x10, 0x00, 0x00}},
        {0x5d1ec8, (void *)&chsize, "_chsize", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x3b, 0x35, 0x00}},
        {0x5dc74d, (void *)&sopen, "_sopen", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x1c, 0x8b, 0x4d}},
        {0x5dc31a, (void *)&getbuf, "_getbuf", 6, {0xff, 0x05, 0xf0, 0x58, 0x71, 0x00, 0x68, 0x00}},
        {0x5d18b3, (void *)&flush, "_flush", 6, {0x53, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x33, 0xdb}},
        {0x5d1885, (void *)&fflush_lk, "_fflush_lk", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x56, 0xe8, 0x23}},
        {0x5d1918, (void *)&flsall, "flsall", 5, {0x53, 0x56, 0x57, 0x6a, 0x02, 0x33, 0xdb, 0x33}},
        {0x5d459f, (void *)&flsbuf, "_flsbuf", 5, {0x55, 0x8b, 0xec, 0x53, 0x56, 0x8b, 0x75, 0x0c}},
        {0x5d8565, (void *)&filbuf, "_filbuf", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x8b, 0x46, 0x0c}},
        {0x5dc6df, (void *)&ungetc_lk, "_ungetc_lk", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x83, 0xfb, 0xff}},
        {0x5d7c87, (void *)&freebuf, "_freebuf", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x8b, 0x46, 0x0c}},
        {0x5d815c, (void *)&getstream, "_getstream", 5, {0x53, 0x56, 0x57, 0x6a, 0x02, 0x33, 0xdb, 0x33}},
        {0x5d7fec, (void *)&openfile, "_openfile", 5, {0x55, 0x8b, 0xec, 0x51, 0x51, 0x53, 0x56, 0x8b}},
        {0x5d19f8, (void *)&fsopen, "_fsopen", 6, {0x56, 0xe8, 0x5e, 0x67, 0x00, 0x00, 0x8b, 0xf0}, 2},
        {0x5d1a29, (void *)&fopen_, "fopen", 6, {0x6a, 0x40, 0xff, 0x74, 0x24, 0x0c, 0xff, 0x74}},
        {0x5d1839, (void *)&fclose_lk, "_fclose_lk", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x83, 0xcf}},
        {0x5d1808, (void *)&fclose_, "fclose", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x83, 0xcf}},
        {0x5d1a6b, (void *)&fwrite_lk, "_fwrite_lk", 5, {0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57, 0x8b}},
        {0x5d1ba4, (void *)&fread_lk, "_fread_lk", 5, {0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57, 0x8b}},
        {0x5d1a3c, (void *)&fwrite_, "fwrite", 7, {0x55, 0x8b, 0xec, 0x56, 0xff, 0x75, 0x14, 0xe8}},
        {0x5d1b75, (void *)&fread_, "fread", 7, {0x55, 0x8b, 0xec, 0x56, 0xff, 0x75, 0x14, 0xe8}},
        {0x5d1cb8, (void *)&fseek_lk, "_fseek_lk", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x8b, 0x46}},
        {0x5d1c8c, (void *)&fseek_, "fseek", 7, {0x55, 0x8b, 0xec, 0x56, 0xff, 0x75, 0x08, 0xe8}},
        {0x5d1d67, (void *)&ftell_lk, "_ftell_lk", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x53, 0x56}},
        {0x5d1d45, (void *)&ftell_, "ftell", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x56, 0xe8}},
        {0x5d7f35, (void *)&stbuf, "_stbuf", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0xff, 0x76, 0x10}},
        {0x5d7fc2, (void *)&ftbuf, "_ftbuf", 5, {0x83, 0x7c, 0x24, 0x04, 0x00, 0x56, 0x74, 0x20}},
        {0x5d19bc, (void *)&fprintf_, "fprintf", 6, {0x53, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x57, 0x56}},
        {0x5de2e0, (void *)&fileno_, "_fileno", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x40, 0x10, 0xc3}},
        {0x5d17a6, (void *)&remove_, "remove", 10, {0xff, 0x74, 0x24, 0x04, 0xff, 0x15, 0x4c, 0xa1}},
        {0x5d2046, (void *)&rename_, "rename", 8, {0xff, 0x74, 0x24, 0x08, 0xff, 0x74, 0x24, 0x08}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
