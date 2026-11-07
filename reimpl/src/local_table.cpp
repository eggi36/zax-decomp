// The Local.txt table, its entries and the registered strings (see local_table.h).
//
// Return values are what the original leaves in eax. Where that comes from a function without a result
// (the ZString functions, qsort, operator delete, the file buffer's destructor), the reimplementation
// calls it through its exe address and passes its eax on.
#include "local_table.h"
#include "field_slots.h"
#include "file_buffer.h"
#include "ptr_array.h"

namespace zax {

static DWORD zs_ctor(ZString *z) { return (DWORD)(uintptr_t)fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static DWORD zs_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static void zs_ctor_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
static void zs_assign(ZString *z, const ZString *s) { fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(z, s); }
static int zs_compare(const ZString *z, const ZString *other) { return fn<int(THISCALL *)(const ZString *, const ZString *)>(0x4e1700)(z, other); }
static bool zs_not_empty(ZString *z) { return fn<bool(THISCALL *)(ZString *)>(0x4e13e0)(z); }
static DWORD entry_dtor(LocalEntry *e) { return fn<DWORD(THISCALL *)(LocalEntry *)>(0x4270f0)(e); }
static void table_add(ZLocalTable *t, const ZString *key, const ZString *text)
{
    fn<DWORD(THISCALL *)(ZLocalTable *, const ZString *, const ZString *)>(0x500900)(t, key, text);
}
static void local_translate(ZLocalTable *t, ZString *z, const char *context)
{
    fn<DWORD(THISCALL *)(ZLocalTable *, ZString *, const char *)>(0x500f50)(t, z, context);
}
static DWORD heap_free_eax(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }
static DWORD crt_qsort(void *base, DWORD n, DWORD size, DWORD compare)
{
    return fn<DWORD(__cdecl *)(void *, DWORD, DWORD, DWORD)>(0x5d097c)(base, n, size, fn_addr_dyn(compare));
}
static int crt_strncmp(const char *a, const char *b, DWORD n) { return fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5d17d0)(a, b, n); }
static char *crt_strstr(const char *s, const char *find) { return fn<char *(__cdecl *)(const char *, const char *)>(0x5d1030)(s, find); }
static void fb_ctor(CFileBuf *f) { fn<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(f); }
static DWORD fb_dtor(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(f); }
static bool fb_open(CFileBuf *f, const char *path, DWORD flags) { return fn<DWORD(THISCALL *)(CFileBuf *, const char *, DWORD)>(0x4a1840)(f, path, flags) & 0xff; }
static bool fb_read_line(CFileBuf *f, char *p, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, char *, DWORD)>(0x4a20a0)(f, p, n) & 0xff; }
static void fb_close(CFileBuf *f) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2100)(f); }

constexpr DWORD TEXT_ENGLISH = 0x6190a4, TEXT_LOCALIZED = 0x6190a8, TEXT_END = 0x6190ac, TEXT_CONTEXT = 0x6190b0;
constexpr DWORD TEXT_NEWLINE = 0x60a684;   // "\n"
constexpr DWORD TEXT_PERCENT_S = 0x6190dc; // "%s"
constexpr DWORD VT_PENDING_LIST = 0x5f99e0;
constexpr DWORD COMPARE_ENTRIES = 0x5008f0, TABLE_DTOR = 0x500890, PENDING_DTOR = 0x500a70;

static DWORD &dword_at(DWORD address) { return *at<DWORD *>(address); }
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
static void append_text(char *dst, const char *src) { copy_text(dst + text_length(dst), src); }

// The new[] block of n entries: {n, entries}; the inlined loop constructs both strings of each entry
static LocalEntry *new_entries(DWORD n)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc(n * 8 + 4));
    if (!block)
        return nullptr;
    block[0] = n;
    LocalEntry *e = reinterpret_cast<LocalEntry *>(block + 1);
    for (DWORD i = 0; i < n; i++) {
        zs_ctor(&e[i].key);
        zs_ctor(&e[i].text);
    }
    return e;
}

// ---- LocalEntry ---------------------------------------------------------------------------------------

// 0x500740
LocalEntry *THISCALL LocalEntry::Ctor()
{
    zs_ctor(&key);
    zs_ctor(&text);
    return this;
}

// 0x500760
LocalEntry *THISCALL LocalEntry::CtorFrom(const ZString *k, const ZString *t)
{
    zs_ctor(&key);
    zs_ctor(&text);
    zs_assign(&key, k);
    zs_assign(&text, t);
    return this;
}

// 0x4270f0: the text first
DWORD THISCALL LocalEntry::Dtor()
{
    zs_dtor(&text);
    return zs_dtor(&key);
}

// 0x5008f0
int __cdecl LocalEntry::Compare(const LocalEntry *a, const LocalEntry *b) { return zs_compare(&a->key, &b->key); }

// ---- ZLocalTable --------------------------------------------------------------------------------------

// 0x500ab0
DWORD THISCALL ZLocalTable::HasData() { return entries != nullptr; }

// 0x500900: the entry is written at the count first; when the count then reaches the capacity, the
// entries move into a block with 10 more (assigned, the old ones destroyed from the end); then qsort
DWORD THISCALL ZLocalTable::Add(const ZString *key, const ZString *text)
{
    zs_assign(&entries[count].key, key);
    zs_assign(&entries[count].text, text);
    count++;
    if (count >= capacity) {
        capacity += 10;
        LocalEntry *e = new_entries(capacity);
        for (DWORD i = 0; i < count; i++) {
            zs_assign(&e[i].key, &entries[i].key);
            zs_assign(&e[i].text, &entries[i].text);
        }
        LocalEntry *old = entries;
        entries = e;
        if (old) {
            DWORD *block = reinterpret_cast<DWORD *>(old) - 1;
            for (DWORD i = block[0]; i-- > 0;) {
                zs_dtor(&old[i].text);
                zs_dtor(&old[i].key);
            }
            heap_free(block);
        }
    }
    // count is at least 1 here, so the original always sorts
    return crt_qsort(entries, count, 8, fn_addr(COMPARE_ENTRIES));
}

// Every registered string through the table (the list pointer is read again for every entry, the
// count once)
static void translate_pending(ZLocalTable *t)
{
    ZLocalPendingList *l = *glob<ZLocalPendingList **>(LOCAL_PENDING_PTR);
    if (!l)
        return;
    const DWORD n = l->count;
    for (DWORD i = 0; i < n; i++) {
        l = *glob<ZLocalPendingList **>(LOCAL_PENDING_PTR);
        local_translate(t, l->data[i].text, l->data[i].context);
    }
}

// A part of a pair: the rest of the line behind the prefix, joined with "\n" and the following lines
// until the line just read contains "[END]" (the result of a read is not looked at)
static void read_part(CFileBuf *f, char *line, char *out, DWORD prefix)
{
    const char *end = *glob<const char **>(TEXT_END);
    copy_text(out, line + prefix);
    while (!crt_strstr(line, end)) {
        fb_read_line(f, line, 0x400);
        append_text(out, glob<const char *>(TEXT_NEWLINE));
        append_text(out, line);
    }
}

// 0x500ac0. The upper bytes of eax are the file buffer destructor's eax.
DWORD THISCALL ZLocalTable::Load(const char *file, DWORD)
{
    copy_text(path, file);
    CFileBuf f;
    fb_ctor(&f);
    from_archive = 0;
    bool opened = fb_open(&f, file, 0x224);
    if (!opened) {
        from_archive = 1;
        opened = fb_open(&f, file, 4);
    }
    if (!opened) {
        translate_pending(this);
        return fb_dtor(&f) & 0xffffff00;
    }
    if (entries) {
        DWORD *block = reinterpret_cast<DWORD *>(entries) - 1;
        for (DWORD i = block[0]; i-- > 0;)
            entry_dtor(&entries[i]);
        heap_free(block);
        entries = nullptr;
    }
    count = 0;
    entries = new_entries(10);
    capacity = 10;
    char line[0x400], english[0x400], localized[0x400];
    if (fb_read_line(&f, line, 0x400)) {
        do {
            if (crt_strncmp(line, *glob<const char **>(TEXT_ENGLISH), dword_at(glob_addr(LOCAL_LEN_ENGLISH))))
                continue;
            read_part(&f, line, english, dword_at(glob_addr(LOCAL_LEN_ENGLISH)));
            const char *end = *glob<const char **>(TEXT_END);
            char *p = crt_strstr(english, end);
            if (english + text_length(english) - dword_at(glob_addr(LOCAL_LEN_END)) == p)
                *p = 0;
            fb_read_line(&f, line, 0x400);
            read_part(&f, line, localized, dword_at(glob_addr(LOCAL_LEN_LOCALIZED)));
            *crt_strstr(localized, end) = 0;
            ZString key, text;
            zs_ctor_text(&key, english);
            zs_ctor_text(&text, localized);
            table_add(this, &key, &text);
            zs_dtor(&text);
            zs_dtor(&key);
        } while (fb_read_line(&f, line, 0x400));
    }
    fb_close(&f);
    if (count)
        crt_qsort(entries, count, 8, fn_addr(COMPARE_ENTRIES));
    translate_pending(this);
    *glob<BYTE *>(LOCAL_LOADED) = 1;
    return (fb_dtor(&f) & 0xffffff00) | 1;
}

// ---- Registered strings -------------------------------------------------------------------------------

// 0x501030
ZString *THISCALL LocalString_Ctor(ZString *self, const char *text, const char *context)
{
    zs_ctor_text(self, text);
    ZLocalPendingList *l = glob<ZLocalPendingList *>(LOCAL_PENDING);
    BYTE &guard = *glob<BYTE *>(LOCAL_PENDING_GUARD);
    if (!(guard & 1)) {
        guard |= 1;
        l->vftable = vt(VT_PENDING_LIST);
        l->data = nullptr;
        l->count = 0;
        l->capacity = 0;
        crt_atexit(code<void(__cdecl *)()>(PENDING_DTOR));
    }
    *glob<ZLocalPendingList **>(LOCAL_PENDING_PTR) = l;
    if (!zs_not_empty(self))
        return self;
    if (l->capacity < l->count + 1) {
        const DWORD n = array_grown_capacity(l->count, l->capacity);
        if (l->capacity < n) {
            l->capacity = n;
            LocalPending *p = static_cast<LocalPending *>(heap_alloc(n * 8));
            for (DWORD i = 0; i < l->count; i++)
                p[i] = l->data[i];
            if (l->data)
                heap_free(l->data);
            l->data = p;
        }
    }
    l->data[l->count++] = {self, context};
    return self;
}

// ---- Static initialisers and destructors ------------------------------------------------------------

// 0x5007e0, 0x500800, 0x500820: the lengths of the texts; eax is 0 (from the scan)
static DWORD __cdecl Local_InitEnglishLength()
{
    dword_at(glob_addr(LOCAL_LEN_ENGLISH)) = text_length(*glob<const char **>(TEXT_ENGLISH));
    return 0;
}
static DWORD __cdecl Local_InitLocalizedLength()
{
    dword_at(glob_addr(LOCAL_LEN_LOCALIZED)) = text_length(*glob<const char **>(TEXT_LOCALIZED));
    return 0;
}
static DWORD __cdecl Local_InitEndLength()
{
    dword_at(glob_addr(LOCAL_LEN_END)) = text_length(*glob<const char **>(TEXT_END));
    return 0;
}
// 0x500840: the length " (%s)" adds to the context
static DWORD __cdecl Local_InitContextLength()
{
    dword_at(glob_addr(LOCAL_LEN_CONTEXT)) = text_length(*glob<const char **>(TEXT_CONTEXT)) - text_length(glob<const char *>(TEXT_PERCENT_S));
    return 0;
}

// 0x500870: atexit's eax
static DWORD __cdecl LocalTable_StaticInit()
{
    glob<ZLocalTable *>(LOCAL_TABLE)->entries = nullptr;
    return (DWORD)crt_atexit(code<void(__cdecl *)()>(TABLE_DTOR));
}

// 0x500890: the entries of the table destroyed from the end (text, then key, inlined), count 0
static DWORD __cdecl LocalTable_StaticDtor()
{
    ZLocalTable *t = glob<ZLocalTable *>(LOCAL_TABLE);
    if (t->entries) {
        DWORD *block = reinterpret_cast<DWORD *>(t->entries) - 1;
        for (DWORD i = block[0]; i-- > 0;) {
            zs_dtor(&t->entries[i].text);
            zs_dtor(&t->entries[i].key);
        }
        heap_free(block);
        t->entries = nullptr;
    }
    t->count = 0;
    return 0;
}

// 0x500a70: eax is operator delete's, or 0 without data
static DWORD __cdecl LocalPending_StaticDtor()
{
    ZLocalPendingList *l = glob<ZLocalPendingList *>(LOCAL_PENDING);
    DWORD r = (DWORD)(uintptr_t)l->data;
    l->vftable = vt(VT_PENDING_LIST);
    if (l->data) {
        r = heap_free_eax(l->data);
        l->data = nullptr;
        l->capacity = 0;
    }
    l->count = 0;
    return r;
}

ZAX_HOOKS(local_table)
{
    const Hook list[] = {
        {0x4270f0, ptr(&LocalEntry::Dtor), "LocalEntry::Dtor", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x04, 0xe8, 0xd5}},
        {0x500740, ptr(&LocalEntry::Ctor), "LocalEntry::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x48, 0xe5, 0xfd, 0xff}, 4},
        {0x500760, ptr(&LocalEntry::CtorFrom), "LocalEntry::CtorFrom", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x27, 0xe5, 0xfd}, 5},
        {0x5007e0, (void *)&Local_InitEnglishLength, "Local_InitEnglishLength", 7, {0x57, 0x8b, 0x3d, 0xa4, 0x90, 0x61, 0x00, 0x83}},
        {0x500800, (void *)&Local_InitLocalizedLength, "Local_InitLocalizedLength", 7, {0x57, 0x8b, 0x3d, 0xa8, 0x90, 0x61, 0x00, 0x83}},
        {0x500820, (void *)&Local_InitEndLength, "Local_InitEndLength", 7, {0x57, 0x8b, 0x3d, 0xac, 0x90, 0x61, 0x00, 0x83}},
        {0x500840, (void *)&Local_InitContextLength, "Local_InitContextLength", 7, {0x57, 0x8b, 0x3d, 0xb0, 0x90, 0x61, 0x00, 0x83}},
        {0x500870, (void *)&LocalTable_StaticInit, "LocalTable_StaticInit", 5, {0x68, 0x90, 0x08, 0x50, 0x00, 0xc7, 0x05, 0x9c}},
        {0x500890, (void *)&LocalTable_StaticDtor, "LocalTable_StaticDtor", 6, {0x8b, 0x0d, 0x9c, 0x2c, 0x6c, 0x00, 0x33, 0xc0}},
        {0x5008f0, (void *)&LocalEntry::Compare, "LocalEntry::Compare", 8, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x4c, 0x24, 0x04}},
        {0x500900, ptr(&ZLocalTable::Add), "ZLocalTable::Add", 5, {0x8b, 0x44, 0x24, 0x04, 0x53, 0x55, 0x56, 0x8b}},
        {0x500a70, (void *)&LocalPending_StaticDtor, "LocalPending_StaticDtor", 5, {0xa1, 0xac, 0x2c, 0x6c, 0x00, 0x56, 0x33, 0xf6}},
        {0x500ab0, ptr(&ZLocalTable::HasData), "ZLocalTable::HasData", 6, {0x8b, 0x91, 0x0c, 0x01, 0x00, 0x00, 0x33, 0xc0}},
        {0x500ac0, ptr(&ZLocalTable::Load), "ZLocalTable::Load", 6, {0x81, 0xec, 0x24, 0x0c, 0x00, 0x00, 0x33, 0xc0}},
        {0x501030, (void *)&LocalString_Ctor, "LocalString_Ctor", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xec, 0x08, 0x55}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
