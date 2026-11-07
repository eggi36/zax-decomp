// Platform helpers: CPU check and INI files (see platform.h).
#include "platform.h"

namespace zax {

namespace {

struct CpuId {
    DWORD a, b, c, d;
};
CpuId cpuid1()
{
    CpuId r;
    __asm__ volatile("cpuid" : "=a"(r.a), "=b"(r.b), "=c"(r.c), "=d"(r.d) : "a"(1));
    return r;
}

const char *skip_white(const char *s) { return fn<const char *(__fastcall *)(const char *)>(0x587aa0)(s); }

// kernel32 through the exe's import table
using GetIntFn = DWORD(STDCALL *)(const char *, const char *, int, const char *);
using WriteStringFn = DWORD(STDCALL *)(const char *, const char *, const char *, const char *);
using GetStringFn = DWORD(STDCALL *)(const char *, const char *, const char *, char *, DWORD, const char *);
GetIntFn get_int() { return *imp_slot<GetIntFn *>(0x5ea0c4); }
WriteStringFn write_string() { return *imp_slot<WriteStringFn *>(0x5ea0c8); }
GetStringFn get_string() { return *imp_slot<GetStringFn *>(0x5ea0cc); }

// sprintf of the C runtime (0x5d0db7)
template <typename T> int crt_sprintf(char *out, const char *fmt, T v) { return fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(out, fmt, v); }

constexpr DWORD TEXT_INT = 0x60a5a0, TEXT_G = 0x60a5a4, TEXT_CFG = 0x613894, TEXT_CD_INI = 0x613888;

}  // namespace

// 0x5154e0
DWORD __cdecl Cpu_Signature() { return cpuid1().a & 0x3fff; }

// 0x515560: true when cpuid reports MMX (then emms as a test of the instruction)
DWORD __cdecl Cpu_DetectMmx()
{
    const CpuId r = cpuid1();
    if (!(r.d & 0x800000))
        return r.a & ~0xffu;
    __asm__ volatile("emms");
    return (r.a & ~0xffu) | 1;
}

// 0x515620
DWORD __cdecl Cpu_Class()
{
    const DWORD sig = *glob<DWORD *>(CPU_SIGNATURE);
    const DWORD family = (sig >> 8) & 0xf;
    if (family >= 1 && family <= 5)
        return 1;
    if (family != 6)
        return 6;
    switch ((sig >> 4) & 0xf) {
    case 1: return 2;
    case 3: return 3;
    case 5: return 4;
    case 7: return 5;
    default: return 6;
    }
}

// 0x5156b0
DWORD __cdecl Cpu_HasMmx()
{
    BYTE &detected = *glob<BYTE *>(CPU_DETECTED);
    if (detected)
        return *glob<BYTE *>(CPU_MMX);
    *glob<DWORD *>(CPU_SIGNATURE) = Cpu_Signature();
    const DWORD r = Cpu_DetectMmx();
    *glob<BYTE *>(CPU_MMX) = (BYTE)r;
    detected = 1;
    return r;
}

// 0x4a4610
ZString *__fastcall Ini_FileName(ZString *out, const char *name, BYTE cfg)
{
    ZString tmp;
    tmp.Ctor();
    fn<ZString *(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(&tmp, at<const char *>(cfg ? TEXT_CFG : TEXT_CD_INI), name);
    out->CopyCtor(&tmp);
    tmp.Dtor();
    return out;
}

// 0x4a4670
DWORD __fastcall Ini_GetInt(const char *file, const char *section, const char *key, int def)
{
    return get_int()(skip_white(section), key, def, file);
}

// 0x4a4690
DWORD __fastcall Ini_WriteInt(const char *file, const char *section, const char *key, int v)
{
    char buf[0x14];
    crt_sprintf(buf, glob<const char *>(TEXT_INT), v);
    return write_string()(skip_white(section), key, buf, file);
}

// 0x4a46e0: the default is written into the buffer that receives the value
double __fastcall Ini_GetFloat(const char *file, const char *section, const char *key, float def)
{
    char buf[0x14];
    crt_sprintf(buf, glob<const char *>(TEXT_G), (double)def);
    get_string()(skip_white(section), key, buf, buf, sizeof buf, file);
    return fn<double(__cdecl *)(const char *)>(0x5d03de)(buf);
}

// 0x4a4740
DWORD __fastcall Ini_WriteFloat(const char *file, const char *section, const char *key, float v)
{
    char buf[0x14];
    crt_sprintf(buf, glob<const char *>(TEXT_G), (double)v);
    return write_string()(skip_white(section), key, buf, file);
}

// 0x4a4790: the buffer's text is the default
DWORD __fastcall Ini_GetString(const char *file, const char *section, const char *key, char *buf, DWORD size)
{
    return get_string()(skip_white(section), key, buf, buf, size, file);
}

// 0x4a47c0
DWORD __fastcall Ini_WriteString(const char *file, const char *section, const char *key, const char *v)
{
    return write_string()(skip_white(section), key, v, file);
}

ZAX_HOOKS(platform)
{
    const Hook list[] = {
        {0x4a4610, (void *)&Ini_FileName, "Ini_FileName", 5, {0x51, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0xf2, 0x8d}},
        {0x4a4670, (void *)&Ini_GetInt, "Ini_GetInt", 5, {0x8b, 0x44, 0x24, 0x08, 0x51, 0x8b, 0x4c, 0x24}},
        {0x4a4690, (void *)&Ini_WriteInt, "Ini_WriteInt", 7, {0x8b, 0x44, 0x24, 0x08, 0x83, 0xec, 0x14, 0x56}},
        {0x4a46e0, (void *)&Ini_GetFloat, "Ini_GetFloat", 7, {0xd9, 0x44, 0x24, 0x08, 0x83, 0xec, 0x14, 0x8d}},
        {0x4a4740, (void *)&Ini_WriteFloat, "Ini_WriteFloat", 7, {0xd9, 0x44, 0x24, 0x08, 0x83, 0xec, 0x14, 0x8d}},
        {0x4a4790, (void *)&Ini_GetString, "Ini_GetString", 5, {0x8b, 0x44, 0x24, 0x0c, 0x51, 0x8b, 0x4c, 0x24}},
        {0x4a47c0, (void *)&Ini_WriteString, "Ini_WriteString", 5, {0x8b, 0x44, 0x24, 0x08, 0x51, 0x8b, 0x4c, 0x24}},
        {0x5154e0, (void *)&Cpu_Signature, "Cpu_Signature", 5, {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0x80, 0xab}},
        {0x515560, (void *)&Cpu_DetectMmx, "Cpu_DetectMmx", 5, {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0x90, 0xab}},
        {0x515620, (void *)&Cpu_Class, "Cpu_Class", 5, {0xa1, 0xc0, 0xfd, 0x6c, 0x00, 0x8b, 0xc8, 0xc1}},
        {0x5156b0, (void *)&Cpu_HasMmx, "Cpu_HasMmx", 5, {0xa0, 0xc5, 0xfd, 0x6c, 0x00, 0x84, 0xc0, 0x75}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
