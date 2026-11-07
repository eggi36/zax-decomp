// Checks of the CPU check and the INI file wrappers (platform.cpp) against the original, in the game
// thread (side 0 through the trampolines, side 1 through the hooked addresses).
//  - CPU: the signature and the MMX test (whole eax), the CPU class for every family and model and random
//    signatures in the global (saved and restored), Cpu_HasMmx detected (al for both flag values) and not
//    yet detected (eax and the three globals).
//  - INI files in a temp folder, one file per side: 400 random sequences of the six get/write wrappers
//    (sections with leading white space (never an empty one: writing to the section "" ends the game
//    inside kernel32 in the VM, through the original as well), keys and values with spaces, ints from the whole range, floats
//    with zeros, NaNs, infinities, denormals and huge values, strings with buffers of 1-40 bytes and a
//    default in the buffer); every return value (Ini_GetFloat: the double's bits), the buffers and the
//    files' bytes compared. Ini_FileName with 12 names and both flags (the text).
//  - Real data, read only: every key of every section of the game's Zax.CFG (".\Zax.CFG", the name made by
//    Ini_FileName) read with Ini_GetInt, Ini_GetFloat and Ini_GetString through both sides.
#include "checks_core_common.h"
#include "platform.h"
#include "init_list.h"

WINIMPORT DWORD WINAPI GetTempPathA(DWORD size, char *out);
WINIMPORT BOOL WINAPI CreateDirectoryA(const char *path, void *sec);
WINIMPORT BOOL WINAPI RemoveDirectoryA(const char *path);
WINIMPORT BOOL WINAPI DeleteFileA(const char *path);
WINIMPORT BOOL WINAPI ReadFile(HANDLE f, void *buf, DWORD n, DWORD *read, void *overlapped);
WINIMPORT char *WINAPI lstrcpynA(char *dst, const char *src, int n);

namespace zax {

namespace {

using namespace core_checks;

Sides g_s;

template <typename F> F pick(int side, DWORD address) { return side ? at<F>(address) : original<F>(address); }

// ---- CPU ---------------------------------------------------------------------------------------------------

void check_cpu(Tally &t)
{
    compare_call<DWORD(__cdecl *)()>(t, 0x5154e0, "Cpu_Signature");
    compare_call<DWORD(__cdecl *)()>(t, 0x515560, "Cpu_DetectMmx");
    SavedBytes saved;
    save_bytes(saved, CPU_SIGNATURE, 8);
    DWORD &sig = *at<DWORD *>(CPU_SIGNATURE);
    for (DWORD v = 0; v < 0x1000; v += 0x10)
        for (DWORD extra : {0u, 0x3005u, 0xf000u}) {
            sig = v | extra;
            compare_call<DWORD(__cdecl *)()>(t, 0x515620, "Cpu_Class");
        }
    g_seed = 0x515620;
    for (int k = 0; k < scaled(200); k++) {
        sig = random();
        compare_call<DWORD(__cdecl *)()>(t, 0x515620, "Cpu_Class random");
    }
    for (BYTE flag : {(BYTE)0, (BYTE)1, (BYTE)0x80}) {
        *at<BYTE *>(CPU_DETECTED) = 1;
        *at<BYTE *>(CPU_MMX) = flag;
        const DWORD a = original<DWORD(__cdecl *)()>(0x5156b0)() & 0xff, b = at<DWORD(__cdecl *)()>(0x5156b0)() & 0xff;
        t.check(a == b, "", "[Cpu_HasMmx detected]");
    }
    DWORD r[2], g[2][2];
    for (int side = 0; side < 2; side++) {
        memset(at<BYTE *>(CPU_SIGNATURE), 0xcc, 6);
        *at<BYTE *>(CPU_DETECTED) = 0;
        r[side] = pick<DWORD(__cdecl *)()>(side, 0x5156b0)();
        g[side][0] = sig;
        g[side][1] = *at<DWORD *>(CPU_MMX) & 0xffff;
    }
    char d[96];
    wsprintfA(d, "[Cpu_HasMmx first: %x %x, sig %x %x, flags %x %x]", r[0], r[1], g[0][0], g[1][0], g[0][1], g[1][1]);
    t.check(r[0] == r[1] && g[0][0] == g[1][0] && g[0][1] == g[1][1], "", d);
    restore_bytes(saved);
}

// ---- INI files ---------------------------------------------------------------------------------------------

unsigned file_hash(const char *path)
{
    HANDLE f = CreateFileA(path, 0x80000000 /* GENERIC_READ */, 1, nullptr, 3 /* OPEN_EXISTING */, 0, nullptr);
    if (f == (HANDLE)(intptr_t)-1)
        return 0;
    static BYTE buf[0x4000];
    DWORD n = 0;
    ReadFile(f, buf, sizeof buf, &n, nullptr);
    CloseHandle(f);
    return fnv(fnv(2166136261u, &n, 4), buf, n);
}

const char *const SECTIONS[] = {"Video", "  Video", "\tVideo", "Sound Options", " \t Sound Options", "Keys", "keys", "\t  Keys"};
const char *const KEYS[] = {"Width", "width", "Volume", "Key 1", "Gamma Ramp", "Last Name", "x", "Player Name"};
const char *const TEXTS[] = {"", "Zax", "  spaced  ", "Reflexive Entertainment", "12345", "-7", "3.5e3", "a;b=c",
                             "A rather long value that is longer than forty characters for the buffer"};

float pick_float()
{
    static const DWORD special[] = {0, 0x80000000, 0x7f800000, 0xff800000, 0x7fc00000, 0x7fa00000, 0x00000001, 0x007fffff,
                                    0x7f7fffff, 0x3f800000, 0xbf800000, 0x3dcccccd, 0x4cbebc20, 0x2edbe6ff, 0x47c35000};
    const DWORD r = (random() >> 8) % 3;
    DWORD bitsv = r == 0 ? special[(random() >> 8) % 15] : r == 1 ? random() : bits((float)((int)(random() % 20001) - 10000) / 16.0f);
    float f;
    memcpy(&f, &bitsv, 4);
    return f;
}

void check_ini_sequences(Tally &t, const char *dir)
{
    char file[2][MAX_PATH];
    for (int side = 0; side < 2; side++)
        wsprintfA(file[side], "%sside%d.ini", dir, side);
    for (int k = 0; k < 400; k++) {
        g_s.reset();
        for (int side = 0; side < 2; side++) {
            g_s.side = side;
            DeleteFileA(file[side]);
            g_seed = 0x4a4670u + (unsigned)k * 31337u;
            const int ops = 2 + (int)((random() >> 8) % 12);
            for (int i = 0; i < ops; i++) {
                const char *sec = SECTIONS[(random() >> 8) % 8];
                const char *key = KEYS[(random() >> 8) % 8];
                const DWORD op = (random() >> 8) % 6;
                DWORD r = 0;
                if (op == 0) {
                    const int v = (random() >> 8) % 2 ? (int)random() : (int)((random() >> 8) % 2001) - 1000;
                    r = pick<DWORD(__fastcall *)(const char *, const char *, const char *, int)>(side, 0x4a4690)(file[side], sec, key, v);
                } else if (op == 1) {
                    const int def = (int)((random() >> 8) % 201) - 100;
                    r = pick<DWORD(__fastcall *)(const char *, const char *, const char *, int)>(side, 0x4a4670)(file[side], sec, key, def);
                } else if (op == 2) {
                    r = pick<DWORD(__fastcall *)(const char *, const char *, const char *, float)>(side, 0x4a4740)(file[side], sec, key,
                                                                                                      pick_float());
                } else if (op == 3) {
                    const double v = pick<double(__fastcall *)(const char *, const char *, const char *, float)>(side, 0x4a46e0)(
                        file[side], sec, key, pick_float());
                    g_s.st_bytes(&v, 8);
                } else if (op == 4) {
                    r = pick<DWORD(__fastcall *)(const char *, const char *, const char *, const char *)>(side, 0x4a47c0)(
                        file[side], sec, key, TEXTS[(random() >> 8) % 9]);
                } else {
                    char buf[48];
                    memset(buf, 0x55, sizeof buf);
                    const DWORD size = 1 + (random() >> 8) % 40;
                    const char *def = TEXTS[(random() >> 8) % 8];
                    lstrcpynA(buf, def, (int)size);
                    r = pick<DWORD(__fastcall *)(const char *, const char *, const char *, char *, DWORD)>(side, 0x4a4790)(
                        file[side], sec, key, buf, size);
                    g_s.st_bytes(buf, sizeof buf);
                }
                g_s.st(r);
            }
            g_s.st(file_hash(file[side]));
        }
        char d[48];
        wsprintfA(d, "ini sequence %d", k);
        g_s.check(t, d);
    }
    DeleteFileA(file[0]);
    DeleteFileA(file[1]);
}

void check_file_names(Tally &t)
{
    const char *const names[] = {"Zax", "", "Zax-Launcher", "a b", "C:\\Temp\\x", "..\\up", "long name with spaces and more text",
                                 "%s", "%d%n", "Data/Zax", "ZAX", "-"};
    for (const char *nm : names)
        for (BYTE cfg = 0; cfg < 2; cfg++) {
            StringState s[2];
            DWORD r[2];
            for (int side = 0; side < 2; side++) {
                ZString z;
                ZString *out = &z;   // constructed by the function (copy constructor)
                memset(out, 0, sizeof *out);
                r[side] = ptrv(pick<ZString *(__fastcall *)(ZString *, const char *, BYTE)>(side, 0x4a4610)(out, nm, cfg)) - ptrv(out);
                s[side] = state(*out, nullptr);
                out->Dtor();
            }
            char d[120];
            wsprintfA(d, "[Ini_FileName '%.40s' %u: '%.30s' '%.30s']", nm, cfg, s[0].text, s[1].text);
            t.check(r[0] == r[1] && memcmp(&s[0], &s[1], sizeof s[0]) == 0, "", d);
        }
}

// Every key of every section of the game's settings, read through both sides (nothing is written)
void check_real_cfg(Tally &t)
{
    ZString name;
    memset(&name, 0, sizeof name);
    at<ZString *(__fastcall *)(ZString *, const char *, BYTE)>(0x4a4610)(&name, "Zax", 1);
    const char *file = name.CStr();
    static char sections[0x2000], keys[0x2000];
    const DWORD ns = GetPrivateProfileStringA(nullptr, nullptr, "", sections, sizeof sections, file);
    int count = 0;
    for (const char *sec = sections; sec < sections + ns && *sec; sec += lstrlenA(sec) + 1) {
        const DWORD nk = GetPrivateProfileStringA(sec, nullptr, "", keys, sizeof keys, file);
        for (const char *key = keys; key < keys + nk && *key; key += lstrlenA(key) + 1) {
            count++;
            for (int what = 0; what < 3; what++) {
                unsigned h[2];
                for (int side = 0; side < 2; side++) {
                    if (what == 0) {
                        const DWORD v = pick<DWORD(__fastcall *)(const char *, const char *, const char *, int)>(side, 0x4a4670)(file, sec, key, -12345);
                        h[side] = v;
                    } else if (what == 1) {
                        const double v = pick<double(__fastcall *)(const char *, const char *, const char *, float)>(side, 0x4a46e0)(file, sec, key, 0.25f);
                        h[side] = fnv(2166136261u, &v, 8);
                    } else {
                        char buf[64];
                        memset(buf, 0x55, sizeof buf);
                        buf[0] = 0;
                        const DWORD r = pick<DWORD(__fastcall *)(const char *, const char *, const char *, char *, DWORD)>(side, 0x4a4790)(file, sec, key, buf, 60);
                        h[side] = fnv(fnv(2166136261u, buf, sizeof buf), &r, 4);
                    }
                }
                char d[120];
                wsprintfA(d, "[Zax.CFG %.30s/%.30s kind %d]", sec, key, what);
                t.check(h[0] == h[1], "", d);
            }
        }
    }
    log("  Zax.CFG: %d keys in %s", count, file);
    name.Dtor();
}

}  // namespace

ZAX_CHECKS(platform)
{
    (void)cls;
    (void)n;
    {
        Tally t{"platform: CPU"};
        check_cpu(t);
        t.report();
    }
    {
        Tally t{"platform: INI files"};
        char dir[MAX_PATH];
        const DWORD len = GetTempPathA(MAX_PATH - 40, dir);
        lstrcpyA(dir + len, "ZaxReimplIni\\");
        CreateDirectoryA(dir, nullptr);
        check_ini_sequences(t, dir);
        check_file_names(t);
        RemoveDirectoryA(dir);
        t.report();
    }
    {
        Tally t{"platform: real Zax.CFG"};
        check_real_cfg(t);
        t.report();
    }
}

}  // namespace zax
