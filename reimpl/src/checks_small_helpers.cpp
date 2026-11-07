// Checks of the small helpers (small_helpers.h) against the original (trampolines), in the game thread:
//  - DPlay_ErrorText on every known code, its neighbours, the whole range of the jump table and random
//    codes: the returned pointer and, for unknown codes, the text in the static buffer;
//  - the player slot clearing on objects filled with random bytes (the critical section 0x6bdbf0 is
//    initialised for the test if the game has not done so, then deleted and cleared again);
//  - RemoveAndDeleteAt on pointer arrays of fake elements (null ones too) at every index and behind the
//    count: deletions, data, count, eax;
//  - the 12-byte assignment and the segment array's Reserve (capacities 0-12, counts up to the capacity,
//    targets below, at and above, old block or none): the copied members of every element, the vtables,
//    capacity, count, eax where it is not operator delete's.
#include "check_util.h"
#include "init_list.h"
#include "small_helpers.h"

WINIMPORT void WINAPI InitializeCriticalSection(void *cs);
WINIMPORT void WINAPI DeleteCriticalSection(void *cs);
WINIMPORT char *WINAPI lstrcpynA(char *dst, const char *src, int n);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

namespace zax {

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

void check_error_text(Tally &t, DWORD code)
{
    using Fn = const char *(__fastcall *)(DWORD);
    char text[2][64];
    const char *r[2];
    for (int s = 0; s < 2; s++) {
        memset(at<char *>(DPLAY_ERROR_BUFFER), 0x55, 40);
        r[s] = s ? at<Fn>(0x47f350)(code) : original<Fn>(0x47f350)(code);
        lstrcpynA(text[s], r[s], sizeof text[s]);
    }
    char d[200];
    wsprintfA(d, "[0x%08x: %x %x '%.40s' '%.40s']", code, ptrv(r[0]), ptrv(r[1]), text[0], text[1]);
    t.check(r[0] == r[1] && lstrcmpA(text[0], text[1]) == 0, "", d);
}

void check_clear_slots(Tally &t)
{
    BYTE *cs = at<BYTE *>(DPLAY_LOCK);
    BYTE saved[24];
    memcpy(saved, cs, 24);
    bool zero = true;
    for (BYTE b : saved)
        zero = zero && !b;
    if (zero)
        InitializeCriticalSection(cs);
    static BYTE obj[2][0x900];
    for (int k = 0; k < 20; k++) {
        for (int s = 0; s < 2; s++) {
            g_seed = 0x480c80u + (unsigned)k;
            for (BYTE &b : obj[s])
                b = (BYTE)(random() >> 16);
            if (s)
                at<DWORD(THISCALL *)(BYTE *)>(0x480c80)(obj[s]);
            else
                original<DWORD(THISCALL *)(BYTE *)>(0x480c80)(obj[s]);
        }
        char d[64];
        wsprintfA(d, "[clear slots %d]", k);
        t.check(memcmp(obj[0], obj[1], sizeof obj[0]) == 0, "", d);
    }
    if (zero) {
        DeleteCriticalSection(cs);
        memcpy(cs, saved, 24);
    }
}

void check_remove_delete(Tally &t)
{
    init_fake_entries();
    void *data[2][16];
    for (DWORD count = 0; count <= 12; count++)
        for (DWORD index = 0; index <= count + 1 && index < 16; index++)
            for (int nulls = 0; nulls < 2; nulls++) {
                unsigned h[2];
                for (int s = 0; s < 2; s++) {
                    g_num_deleted = 0;
                    for (DWORD i = 0; i < 16; i++)
                        data[s][i] = nulls && (i % 3 == 1) ? nullptr : &g_fake_entries[i];
                    ZOwningPtrArray a{0x5f1618, data[s], count};
                    using Fn = DWORD(THISCALL *)(ZOwningPtrArray *, DWORD);
                    DWORD r = s ? at<Fn>(0x480cc0)(&a, index) : original<Fn>(0x480cc0)(&a, index);
                    // eax relative to the side's data where it points there
                    if (r >= ptrv(data[s]) && r <= ptrv(data[s] + 16))
                        r = r - ptrv(data[s]) + 0x10000000;
                    DWORD rel[16];
                    for (DWORD i = 0; i < 16; i++)
                        rel[i] = data[s][i] ? ptrv(data[s][i]) - ptrv(g_fake_entries) : 0xffffffff;
                    h[s] = fnv(fnv(fnv(deleted_hash(), &r, 4), rel, sizeof rel), &a.count, 4);
                    h[s] = fnv(h[s], &a.vftable, 4);
                    if (a.data != data[s])
                        h[s] ^= 0x1234567;
                }
                char d[80];
                wsprintfA(d, "[remove at %u of %u, nulls %d]", index, count, nulls);
                t.check(h[0] == h[1], "", d);
            }
}

void check_assign(Tally &t)
{
    for (int k = 0; k < 50; k++) {
        ZVec3 a[2], b;
        DWORD r[2];
        g_seed = 0x5003e0u + (unsigned)k;
        for (DWORD &v : b.v)
            v = random();
        for (int s = 0; s < 2; s++) {
            memset(&a[s], 0xcc, sizeof a[s]);
            using Fn = ZVec3 *(THISCALL *)(ZVec3 *, const ZVec3 *);
            r[s] = ptrv(s ? at<Fn>(0x5003e0)(&a[s], &b) : original<Fn>(0x5003e0)(&a[s], &b)) - ptrv(&a[s]);
        }
        t.check(r[0] == r[1] && memcmp(&a[0], &a[1], sizeof a[0]) == 0 && memcmp(&a[0], &b, sizeof b) == 0, "", "[vec3 assign]");
    }
}

// The members Reserve copies (everything but the vtable and the bytes 0x0d-0x0f, 0x27)
bool copied_byte(DWORD i) { return i >= 4 && !(i >= 0xd && i <= 0xf) && i != 0x27; }

void check_reserve(Tally &t)
{
    const DWORD targets[] = {0, 1, 2, 3, 5, 8, 11, 12, 13, 20, 40};
    for (DWORD cap = 0; cap <= 12; cap++)
        for (DWORD count = 0; count <= cap; count += cap > 6 ? 3 : 1)
            for (DWORD n : targets)
                for (int with_data = 0; with_data < 2; with_data++) {
                    if (count && !with_data)
                        continue;
                    unsigned h[2];
                    DWORD eaxv[2] = {0, 0};
                    bool eax_comparable = false;
                    for (int s = 0; s < 2; s++) {
                        g_seed = 0x500570u + cap * 977 + count * 31 + n * 7 + (unsigned)with_data;
                        LightningSegment *old = nullptr;
                        if (with_data && cap) {
                            old = static_cast<LightningSegment *>(heap_alloc(cap * sizeof(LightningSegment)));
                            for (DWORD i = 0; i < cap; i++) {
                                BYTE *p = reinterpret_cast<BYTE *>(&old[i]);
                                for (DWORD j = 0; j < sizeof(LightningSegment); j++)
                                    p[j] = (BYTE)(random() >> 16);
                            }
                        }
                        ZSegmentArray a{0x5f99d4, old, count, cap};
                        using Fn = DWORD(THISCALL *)(ZSegmentArray *, DWORD);
                        const DWORD r = s ? at<Fn>(0x500570)(&a, n) : original<Fn>(0x500570)(&a, n);
                        const bool grew = a.data != old;
                        eax_comparable = !grew || !old;
                        eaxv[s] = r;
                        h[s] = fnv(fnv(fnv(2166136261u, &a.count, 4), &a.capacity, 4), &a.vftable, 4);
                        h[s] = fnv(h[s], &grew, 1);
                        if (grew && a.data) {
                            g_seed = 0x500570u + cap * 977 + count * 31 + n * 7 + (unsigned)with_data;
                            for (DWORD i = 0; i < a.capacity; i++) {
                                const BYTE *p = reinterpret_cast<const BYTE *>(&a.data[i]);
                                h[s] = fnv(h[s], p, 4);   // vtable
                                if (i < count)
                                    for (DWORD j = 4; j < sizeof(LightningSegment); j++)
                                        if (copied_byte(j))
                                            h[s] = fnv(h[s], p + j, 1);
                            }
                            heap_free(a.data);
                        } else if (old) {
                            heap_free(old);
                        }
                    }
                    char d[120];
                    wsprintfA(d, "[reserve cap %u count %u n %u data %d: eax %x %x]", cap, count, n, with_data, eaxv[0], eaxv[1]);
                    t.check(h[0] == h[1] && (!eax_comparable || eaxv[0] == eaxv[1]), "", d);
                }
}

}  // namespace

ZAX_CHECKS(small_helpers)
{
    (void)cls;
    (void)n;
    {
        Tally t{"small helpers: DPlay_ErrorText"};
        const DWORD known[] = {0x0015800e, 0x80004001, 0x80004002, 0x80004005, 0x8007000e, 0x80070057, 0x80158080, 0x80158100,
                               0x80158120, 0x80158130, 0x80158150, 0x80158230, 0x80158340, 0x80158350, 0x80158390, 0x80158400,
                               0x80158410, 0x80158420, 0x80158470, 0x80158480, 0x80158570, 0x80158630, 0x80158640, 0x80158650};
        for (DWORD c : known)
            for (int d = -16; d <= 16; d++)
                check_error_text(t, c + (DWORD)d);
        for (DWORD c = 0x80158000; c <= 0x80158700; c += 0x10)
            check_error_text(t, c);
        for (DWORD c = 0x80158380; c <= 0x80158480; c++)
            check_error_text(t, c);
        for (DWORD c : {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu, 0x8015ffffu})
            check_error_text(t, c);
        g_seed = 0x47f350;
        for (int k = 0; k < 200; k++)
            check_error_text(t, random());
        t.report();
    }
    {
        Tally t{"small helpers: DirectPlay slots, RemoveAndDeleteAt"};
        check_clear_slots(t);
        check_remove_delete(t);
        t.report();
    }
    {
        Tally t{"small helpers: 12-byte assignment, segment array Reserve"};
        check_assign(t);
        check_reserve(t);
        t.report();
    }
}

}  // namespace zax
