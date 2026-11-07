// Checks of short_hooks.cpp (the short functions hooked through the padding behind them, docs/DECOMPILATION.md
// 10, "The milestone audit") against the original: every function runs twice from the same registers, stack
// arguments, object memory and globals, once with its hook site put back to the original bytes and once
// through the hook; compared are all registers after the return, how many bytes it popped, the object memory
// (a 4 KB arena this points into), the global it uses, the FPU stack (st0 and its depth) and the call or jump
// out of it. The function it jumps or calls to is a recording stand-in for the case (it records eax, ecx,
// edx and three stack arguments, answers a case value and pops what the real function pops); calls from the
// game's other threads go on to the real function's hook. A fake vtable gives Compass_SetTarget its slot 43.
#include "check_util.h"

namespace zax {

int hooks_short_hooks(Hook *out);

namespace {

enum Kind { ANY, PTR0, PTR, VCALL };

struct ShortCase {
    DWORD address;
    void *replacement;
    Kind kind;
    DWORD target;   // the jump or call out (VCALL: the slot offset)
    DWORD pop;      // the bytes the stand-in pops
    DWORD global, global_size;
    int fpu, null_this;
    const char *name;
};

#include "short_hooks_cases.inc"

}  // namespace

// The runner's context: registers in, 8 stack arguments, the function; registers out and esp after the return
// relative to before the call (the bytes popped)
struct ShortRegs {
    DWORD in[7];   // eax ecx edx ebx esi edi ebp
    DWORD args[8];
    DWORD fn;
    DWORD out[7];
    DWORD popped;
};

extern "C" {
ShortRegs *g_short_ctx;
DWORD g_short_esp, g_short_esp_call, g_short_fn, g_short_tmp;
DWORD g_short_thread, g_short_rec_pop, g_short_rec_ret, g_short_rec_pass, g_short_rec_back;
DWORD g_short_rec[6], g_short_rec_count;
void zax_short_run(ShortRegs *r);
void zax_short_rec();
}

__asm__(R"(
    .intel_syntax noprefix
    .text
    .globl _zax_short_run
_zax_short_run:
    push ebp
    push ebx
    push esi
    push edi
    mov eax, dword ptr [esp + 20]
    mov dword ptr [_g_short_ctx], eax
    mov dword ptr [_g_short_esp], esp
    sub esp, 64
    push dword ptr [eax + 56]
    push dword ptr [eax + 52]
    push dword ptr [eax + 48]
    push dword ptr [eax + 44]
    push dword ptr [eax + 40]
    push dword ptr [eax + 36]
    push dword ptr [eax + 32]
    push dword ptr [eax + 28]
    mov ecx, dword ptr [eax + 60]
    mov dword ptr [_g_short_fn], ecx
    mov ecx, dword ptr [eax + 4]
    mov edx, dword ptr [eax + 8]
    mov ebx, dword ptr [eax + 12]
    mov esi, dword ptr [eax + 16]
    mov edi, dword ptr [eax + 20]
    mov ebp, dword ptr [eax + 24]
    mov eax, dword ptr [eax]
    mov dword ptr [_g_short_esp_call], esp
    call dword ptr [_g_short_fn]
    mov dword ptr [_g_short_tmp], eax
    mov eax, dword ptr [_g_short_ctx]
    mov dword ptr [eax + 68], ecx
    mov dword ptr [eax + 72], edx
    mov dword ptr [eax + 76], ebx
    mov dword ptr [eax + 80], esi
    mov dword ptr [eax + 84], edi
    mov dword ptr [eax + 88], ebp
    mov ecx, esp
    sub ecx, dword ptr [_g_short_esp_call]
    mov dword ptr [eax + 92], ecx
    mov ecx, dword ptr [_g_short_tmp]
    mov dword ptr [eax + 64], ecx
    mov esp, dword ptr [_g_short_esp]
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

    .globl _zax_short_rec
_zax_short_rec:
    push eax
    mov eax, dword ptr fs:[0x24]
    cmp eax, dword ptr [_g_short_thread]
    pop eax
    jne 1f
    mov dword ptr [_g_short_rec], eax
    mov dword ptr [_g_short_rec + 4], ecx
    mov dword ptr [_g_short_rec + 8], edx
    push eax
    mov eax, dword ptr [esp + 8]
    mov dword ptr [_g_short_rec + 12], eax
    mov eax, dword ptr [esp + 12]
    mov dword ptr [_g_short_rec + 16], eax
    mov eax, dword ptr [esp + 16]
    mov dword ptr [_g_short_rec + 20], eax
    pop eax
    inc dword ptr [_g_short_rec_count]
    pop dword ptr [_g_short_rec_back]
    add esp, dword ptr [_g_short_rec_pop]
    push dword ptr [_g_short_rec_back]
    mov eax, dword ptr [_g_short_rec_ret]
    ret
1:
    jmp dword ptr [_g_short_rec_pass]
    .att_syntax
)");

namespace {

constexpr DWORD ARENA = 0x1000;
alignas(16) BYTE g_arena[ARENA];
alignas(16) BYTE g_arena0[ARENA];

DWORD arena_ptr() { return (DWORD)(uintptr_t)g_arena + (random() % (ARENA - 0x40)) / 4 * 4; }

void fill(Kind kind)
{
    DWORD *d = reinterpret_cast<DWORD *>(g_arena);
    for (DWORD i = 0; i < ARENA / 4; i++) {
        const DWORD r = random() % 8;
        if (kind == ANY || kind == VCALL)
            d[i] = r < 2 ? arena_ptr() : r < 4 ? random() % 0x100 : r == 4 ? 0 : random();
        else if (kind == PTR0)
            d[i] = r < 2 ? 0 : arena_ptr();
        else
            d[i] = arena_ptr();
    }
}

// The state after one side
struct Side {
    ShortRegs r;
    DWORD rec[6], rec_count;
    DWORD global;
    WORD top;
    BYTE st0[10];
    unsigned arena;
};

void run_side(const ShortCase &c, const ShortRegs &in, DWORD global_value, Side &s)
{
    memcpy(g_arena, g_arena0, ARENA);
    if (c.global)
        memcpy(at<void *>(c.global), &global_value, c.global_size);
    memset(g_short_rec, 0, sizeof g_short_rec);
    g_short_rec_count = 0;
    s.r = in;
    BYTE fsave[108];
    __asm__ volatile("fnsave %0\n\tfninit" : "=m"(fsave));
    zax_short_run(&s.r);
    WORD sw;
    __asm__ volatile("fnstsw %0" : "=m"(sw));
    s.top = (sw >> 11) & 7;
    memset(s.st0, 0, sizeof s.st0);
    if (s.top)
        __asm__ volatile("fstpt %0" : "=m"(s.st0));
    __asm__ volatile("frstor %0" : : "m"(fsave));
    memcpy(s.rec, g_short_rec, sizeof s.rec);
    s.rec_count = g_short_rec_count;
    s.global = 0;
    if (c.global)
        memcpy(&s.global, at<void *>(c.global), c.global_size);
    s.arena = fnv(2166136261u, g_arena, ARENA);
}

bool installed(const ShortCase &c)
{
    const BYTE *site = at<const BYTE *>(c.address);
    return site[0] == 0xe9 && c.address + 5 + *reinterpret_cast<const DWORD *>(site + 1) == (DWORD)(uintptr_t)c.replacement;
}

}  // namespace

ZAX_CHECKS(short_hooks)
{
    (void)cls;
    (void)n;
    Tally hooked{"short hooks: hook sites installed (a jump to the replacement without a trampoline)"};
    Tally t{"short hooks: getters, ret stubs, jump thunks, the tile cache handles against the original"};
    Hook table[160];
    const int nh = hooks_short_hooks(nullptr);
    if (!hook_sites_fit("short_hooks", nh, 160)) {
        t.report();
        return;
    }
    hooks_short_hooks(table);
    g_short_thread = GetCurrentThreadId();
    for (const ShortCase &c : SHORT_CASES) {
        const Hook *h = nullptr;
        for (int i = 0; i < nh; i++)
            if (table[i].address == c.address)
                h = &table[i];
        char d[200];
        wsprintfA(d, "[%s %x]", c.name, c.address);
        hooked.check(h && installed(c), "", d);
        if (!h || !installed(c))
            continue;
        BYTE *site = at<BYTE *>(c.address);
        BYTE hook_bytes[5];
        memcpy(hook_bytes, site, 5);
        // the stand-in of the jump or call out; other threads go on through the target's own hook
        CodePatch target{};
        if (c.target && c.kind != VCALL) {
            const BYTE *ts = at<const BYTE *>(c.target);
            if (ts[0] != 0xe9) {
                wsprintfA(d, "[%s %x: its target %x is not hooked]", c.name, c.address, c.target);
                t.check(false, "", d);
                continue;
            }
            g_short_rec_pass = c.target + 5 + *reinterpret_cast<const DWORD *>(ts + 1);
            patch_jump(target, c.target, (void *)&zax_short_rec);
        }
        g_short_rec_pop = c.pop;
        DWORD saved_global = 0;
        if (c.global)
            memcpy(&saved_global, at<void *>(c.global), c.global_size);
        const int cases = scaled(24);
        for (int k = 0; k < cases; k++) {
            fill(c.kind);
            ShortRegs in{};
            for (DWORD &v : in.in)
                v = random();
            for (DWORD &v : in.args)
                v = random() % 4 ? random() : arena_ptr();
            in.in[1] = (DWORD)(uintptr_t)g_arena + random() % 0x400;   // this
            if (c.kind != ANY)
                in.in[1] &= ~3u;
            if (c.kind == VCALL) {
                const DWORD vt = (DWORD)(uintptr_t)g_arena + 0xc00;
                memcpy(at<void *>(in.in[1]), &vt, 4);
                const DWORD rec = (DWORD)(uintptr_t)&zax_short_rec;
                memcpy(g_arena + 0xc00 + c.target, &rec, 4);
            }
            if (c.kind == PTR && k % 4 == 1)
                memset(at<void *>(in.in[1]), 0, 4);
            if (c.null_this && k % 4 == 0)
                in.in[1] = 0;
            in.fn = c.address;
            memcpy(g_arena0, g_arena, ARENA);
            g_short_rec_ret = random();
            const DWORD global_value = random();
            Side s[2];
            write_code(site, h->expected);
            run_side(c, in, global_value, s[0]);
            write_code(site, hook_bytes);
            run_side(c, in, global_value, s[1]);
            const bool same = !memcmp(&s[0].r, &s[1].r, sizeof s[0].r) && !memcmp(s[0].rec, s[1].rec, sizeof s[0].rec) &&
                              s[0].rec_count == s[1].rec_count && s[0].global == s[1].global && s[0].top == s[1].top &&
                              !memcmp(s[0].st0, s[1].st0, 10) && s[0].arena == s[1].arena;
            wsprintfA(d, "[%s %x case %d: eax %x / %x, ecx %x / %x, edx %x / %x, popped %u / %u, calls %u / %u, arena %x / %x, top %u / %u]",
                      c.name, c.address, k, s[0].r.out[0], s[1].r.out[0], s[0].r.out[1], s[1].r.out[1], s[0].r.out[2],
                      s[1].r.out[2], s[0].r.popped, s[1].r.popped, s[0].rec_count, s[1].rec_count, s[0].arena, s[1].arena,
                      s[0].top, s[1].top);
            t.check(same, "", d);
        }
        if (c.global)
            memcpy(at<void *>(c.global), &saved_global, c.global_size);
        if (target.site)
            unpatch(target);
    }
    hooked.report();
    t.report();
}

}  // namespace zax
