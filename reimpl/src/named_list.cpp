// ZNamedList, ZRecordList, ZEnumTable, ZListView reimplemented (see named_list.h).
#include "named_list.h"

namespace zax {

// The exe's functions that the lists call directly (not through the vtable); through their exe
// addresses so Disable= applies to them
static int find_file_name_direct(ZNamedList *l, const char *s)
{
    return fn<int(THISCALL *)(ZNamedList *, const char *)>(0x591ac0)(l, s);
}
static void zstring_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static const char *zstring_c_str(ZString *z) { return fn<const char *(THISCALL *)(ZString *)>(0x4e13a0)(z); }
static bool zstring_is_empty(ZString *z) { return fn<bool(THISCALL *)(ZString *)>(0x4e13d0)(z); }
static bool zstring_not_empty(ZString *z) { return fn<bool(THISCALL *)(ZString *)>(0x4e13e0)(z); }
static void zstring_append(ZString *z, const char *s) { fn<void(THISCALL *)(ZString *, const char *)>(0x4e1be0)(z, s); }
template <typename... A> static void zstring_format(ZString *z, const char *fmt, A... a)
{
    fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, fmt, a...);
}
template <typename... A> static void zstring_append_format(ZString *z, const char *fmt, A... a)
{
    fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e0a30)(z, fmt, a...);
}
// 0x4d24c0: message box with the arrow cursor (caption in ecx, text in edx); 0x5d0e36: exit
static void show_error(const char *caption, const char *text)
{
    fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(caption, text);
}
static void crt_exit(int code) { fn<void(__cdecl *)(int)>(0x5d0e36)(code); }

// ---- ZNamedList ------------------------------------------------------------------------------

// 0x415340
ZNamedList *THISCALL ZNamedList::DeletingDtor(DWORD flags)
{
    vftable = vt(NAMED_LIST_VTABLE);
    if (flags & 1)
        pool_free(this, sizeof(ZNamedList));
    return this;
}

// 0x591bd0: the hint (compared unsigned), then all entries in order, then the file-name search
int THISCALL ZNamedList::Find(const char *s, DWORD hint)
{
    DWORD n = count();
    if (hint < n && crt_stricmp(name(hint), s) == 0)
        return (int)hint;
    for (DWORD i = 0; i < n; i++)
        if (crt_stricmp(name(i), s) == 0)
            return (int)i;
    return find_file_name(s);
}

// 0x591fc0: Find (slot 2); if the name is missing, a message box "Fatal Not Found Error" with the
// list, up to 10 of its entries and the last file opened, then exit(-1). The message is never freed.
int THISCALL ZNamedList::FindOrFail(const char *s, int hint)
{
    int r = find(s, (DWORD)hint);
    if (r != -1)
        return r;
    ZString msg;
    zstring_ctor(&msg);
    zstring_format(&msg, glob<const char *>(0x623c58) /* "The game is looking for ... is \"%s\"" */, s);
    if (list_name()) {
        zstring_append_format(&msg, glob<const char *>(0x623c34) /* " which should be in the \"%s\" list" */,
                              list_name());
        if (folder() && *folder()) {
            const char *f = folder();
            if (crt_stricmp(list_name(), f) != 0)
                zstring_append_format(&msg, glob<const char *>(0x623c18) /* " located in the \"%s\" folder" */,
                                      folder());
        }
    } else if (folder() && *folder()) {
        zstring_append_format(&msg, glob<const char *>(0x623bf4) /* " which should be in the \"%s\" folder" */,
                              folder());
    }
    DWORD n = count();
    if (count() == 0) {
        zstring_append(&msg, glob<const char *>(0x623bd0) /* "***List contains no items***" */);
    } else {
        zstring_append_format(&msg, glob<const char *>(0x623b8c) /* "That list does contain ... \"%s\" is missing:" */,
                              s);
        for (DWORD i = 0; i < n; i++) {
            if (i >= 10) {
                zstring_append(&msg, glob<const char *>(0x623b78) /* "\r\n ..." */);
                break;
            }
            const char *e = name(i);
            zstring_append_format(&msg, glob<const char *>(0x623b80) /* "\r\n %d) %s" */, i, e);
        }
    }
    ZString *last = glob<ZString *>(LAST_FILE_OPENED);
    if (zstring_is_empty(last))
        zstring_append_format(&msg, glob<const char *>(0x623b44) /* "referenced by an unknown source" */,
                              zstring_c_str(last));
    if (zstring_not_empty(last))
        zstring_append_format(&msg, glob<const char *>(0x623b20) /* "referenced by \"%s\"" */, zstring_c_str(last));
    zstring_append(&msg, glob<const char *>(0x623b08) /* "This could mean:" */);
    const char *src = zstring_c_str(last);
    zstring_append_format(&msg, glob<const char *>(0x623974) /* "A) You recently got a new file ..." */, src, s,
                          zstring_c_str(last), s);
    zstring_append(&msg, glob<const char *>(0x62396c) /* " or" */);
    src = zstring_c_str(last);
    zstring_append_format(&msg, glob<const char *>(0x6238bc) /* "B) You (or someone else) deleted ..." */, s, src, s,
                          zstring_c_str(last), s);
    show_error(glob<const char *>(0x6238a4) /* "Fatal Not Found Error" */, zstring_c_str(&msg));
    crt_exit(-1);
    return -1;   // not reached
}

// 0x591a90: slot 21 of item i, if the list holds items (slot 6) and item i exists (slot 7)
DWORD THISCALL ZNamedList::ItemValue(DWORD i)
{
    if (!has_items())
        return 0;
    void *p = item(i);
    if (!p)
        return 0;
    return vcall<DWORD>(p, 21);
}

// 0x415250 (slot 14 of every named list)
const char *THISCALL ZNamedList::EmptyText() { return glob<const char *>(EMPTY_TEXT); }

// 0x5921f0
int THISCALL ZNamedList::NoIndex(DWORD) { return -1; }

// 0x591b20: binary search (stricmp order, signed indices). It starts at the hint (-1: the middle),
// a name starting with '!' at index 0. The hint is not checked against the count.
int THISCALL ZNamedList::BinarySearch(const char *s, int hint)
{
    int mid = s[0] == '!' ? 0 : hint;
    int lo = 0;
    int hi = (int)count() - 1;
    if (mid == -1)
        mid = hi / 2;
    if (hi < 0)
        return -1;
    do {
        int c = crt_stricmp(name((DWORD)mid), s);
        if (c == 0)
            return mid;
        if (c < 0)
            lo = mid + 1;
        else
            hi = mid - 1;
        mid = (hi + lo) / 2;
    } while (hi >= lo);
    return -1;
}

// 0x591ba0
int THISCALL ZNamedList::FindSorted(const char *s, int hint)
{
    int r = binary_search(s, hint);
    if (r == -1)
        r = find_file_name(s);
    return r;
}

// 0x591ac0: compares only the file names (behind the last '/' or '\\'). Calls slot 6 when nothing
// is found and ignores the result.
int THISCALL ZNamedList::FindFileName(const char *s)
{
    const char *wanted = path_name(s);
    DWORD n = count();
    for (DWORD i = 0; i < n; i++)
        if (crt_stricmp(path_name(name(i)), wanted) == 0)
            return (int)i;
    has_items();
    return -1;
}

// ---- ZRecordList -----------------------------------------------------------------------------

// 0x591c90
ZRecordList *THISCALL ZRecordList::Ctor(BYTE sorted_, DWORD stride_, DWORD num_, char *data_, DWORD unknown08_)
{
    sorted = sorted_;
    num = num_;
    data = data_;
    vftable = vt(0x6023dc);
    unknown08 = unknown08_;
    stride = stride_;
    unknown10 = 0;
    return this;
}

// 0x591cd0: the constructor without vtable and +0x10. Returns the stride (eax left over).
DWORD THISCALL ZRecordList::Set(BYTE sorted_, DWORD stride_, DWORD num_, char *data_, DWORD unknown08_)
{
    sorted = sorted_;
    num = num_;
    data = data_;
    unknown08 = unknown08_;
    stride = stride_;
    return stride_;
}

// 0x591d00: 0 for an empty name and beyond the count; the index equal to the count is still read
const char *THISCALL ZRecordList::Name(DWORD i)
{
    if (i > as_list()->count())
        return nullptr;
    char *p = data + stride * i;
    if (!data || !*p)
        return nullptr;
    return p;
}

// 0x591d30: sorted lists use slot 25; otherwise the hint, then the records up to the first empty
// name (not the count), then the file-name search
int THISCALL ZRecordList::Find(const char *s, int hint)
{
    if (!data)
        return -1;
    if (sorted)
        return vcall<int>(this, 25, s, hint);
    if (hint != -1 && crt_stricmp(data + stride * (DWORD)hint, s) == 0)
        return hint;
    int i = 0;
    for (char *p = data; *p; p += stride, i++)
        if (crt_stricmp(p, s) == 0)
            return i;
    return find_file_name_direct(as_list(), s);
}

// 0x591dd0
DWORD THISCALL ZRecordList::Count() { return num; }

// 0x591f90
ZRecordList *THISCALL ZRecordList::DeletingDtor(DWORD flags)
{
    vftable = vt(NAMED_LIST_VTABLE);
    if (flags & 1)
        pool_free(this, sizeof(ZRecordList));
    return this;
}

// ---- ZEnumTable ------------------------------------------------------------------------------

static DWORD count_enum_records(DWORD *table)
{
    DWORD n = 0;
    for (DWORD *r = table + 1; *r; r += 3)
        n++;
    return n;
}

// 0x591ee0 (writes the base vtable 0x60244c first)
ZEnumTable *THISCALL ZEnumTable::Ctor(DWORD *table_)
{
    table = table_;
    names = reinterpret_cast<const char **>(table_ + 1);
    alt_names = reinterpret_cast<const char **>(table_ + 2);
    stride = 3;
    num = count_enum_records(table_);
    vftable = vt(0x6024b8);
    return this;
}

// 0x591f40: without alternative names; the table is kept at +0x18
ZEnumTable *THISCALL ZEnumTable::CtorNoAlt(DWORD *table_)
{
    names = reinterpret_cast<const char **>(table_ + 1);
    table = nullptr;
    alt_names = nullptr;
    stride = 3;
    source = table_;
    num = count_enum_records(table_);
    vftable = vt(0x602524);
    return this;
}

// 0x591e10 (no check of i)
const char *THISCALL ZEnumTable::Name(DWORD i) { return names[stride * i]; }

// 0x591e30: the hint, then the names up to the null name, then the alternative names of those
// records, then the file-name search
int THISCALL ZEnumTable::Find(const char *s, int hint)
{
    if (hint != -1 && crt_stricmp(names[stride * (DWORD)hint], s) == 0)
        return hint;
    DWORD i = 0;
    for (const char **p = names; *p; p += stride, i++)
        if (crt_stricmp(*p, s) == 0)
            return (int)i;
    DWORD n = i;
    if (const char **a = alt_names)
        for (i = 0; i < n; i++, a += stride)
            if (*a && crt_stricmp(*a, s) == 0)
                return (int)i;
    return find_file_name_direct(as_list(), s);
}

// 0x591de0
ZEnumTable *THISCALL ZEnumTable::DeletingDtor(DWORD flags)
{
    vftable = vt(NAMED_LIST_VTABLE);
    if (flags & 1)
        pool_free(this, 0x18);
    return this;
}

// ---- ZListView -------------------------------------------------------------------------------

// 0x591c60
const char *THISCALL ZListView::Name(DWORD i)
{
    if (!indices || !list || i >= indices->count)
        return nullptr;
    return list->name((DWORD)(uintptr_t)indices->data[i]);
}

// 0x591c40
DWORD THISCALL ZListView::Count() { return indices && list ? indices->count : 0; }

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(named_list)
{
    const Hook list[] = {
        {0x415340, ptr(&ZNamedList::DeletingDtor), "ZNamedList::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0xa8}},
        {0x591bd0, ptr(&ZNamedList::Find), "ZNamedList::Find", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x06}},
        {0x591fc0, ptr(&ZNamedList::FindOrFail), "ZNamedList::FindOrFail", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x591a90, ptr(&ZNamedList::ItemValue), "ZNamedList::ItemValue", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x50, 0x18}},
        {0x5921f0, ptr(&ZNamedList::NoIndex), "ZNamedList::NoIndex", 6, {0x83, 0xc8, 0xff, 0xc2, 0x04, 0x00, 0x90, 0x90}},
        {0x591b20, ptr(&ZNamedList::BinarySearch), "ZNamedList::BinarySearch", 5, {0x8b, 0x44, 0x24, 0x04, 0x53, 0x8b, 0xd9, 0x55}},
        {0x591ba0, ptr(&ZNamedList::FindSorted), "ZNamedList::FindSorted", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x0c, 0x57}},
        {0x591ac0, ptr(&ZNamedList::FindFileName), "ZNamedList::FindFileName", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24}},
        {0x591c90, ptr(&ZRecordList::Ctor), "ZRecordList::Ctor", 6, {0x8b, 0x54, 0x24, 0x0c, 0x8b, 0xc1, 0x8a, 0x4c}},
        {0x591cd0, ptr(&ZRecordList::Set), "ZRecordList::Set", 8, {0x8a, 0x44, 0x24, 0x04, 0x8b, 0x54, 0x24, 0x0c}},
        {0x591d00, ptr(&ZRecordList::Name), "ZRecordList::Name", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x50, 0x58}},
        {0x591d30, ptr(&ZRecordList::Find), "ZRecordList::Find", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x47}},
        {0x591dd0, ptr(&ZRecordList::Count), "ZRecordList::Count", 5, {0x8b, 0x41, 0x14, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x591f90, ptr(&ZRecordList::DeletingDtor), "ZRecordList::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0xa8}},
        {0x591ee0, ptr(&ZEnumTable::Ctor), "ZEnumTable::Ctor", 6, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0xc1, 0x56, 0x57}},
        {0x591f40, ptr(&ZEnumTable::CtorNoAlt), "ZEnumTable::CtorNoAlt", 6, {0x53, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xc1}},
        {0x591e10, ptr(&ZEnumTable::Name), "ZEnumTable::Name", 6, {0x8b, 0x41, 0x10, 0x8b, 0x49, 0x04, 0x0f, 0xaf}},
        {0x591e30, ptr(&ZEnumTable::Find), "ZEnumTable::Find", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x56, 0x8b}},
        {0x591de0, ptr(&ZEnumTable::DeletingDtor), "ZEnumTable::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0xa8}},
        {0x591c60, ptr(&ZListView::Name), "ZListView::Name", 5, {0x8b, 0x51, 0x08, 0x33, 0xc0, 0x85, 0xd2, 0x56}},
        {0x591c40, ptr(&ZListView::Count), "ZListView::Count", 5, {0x8b, 0x51, 0x08, 0x33, 0xc0, 0x85, 0xd2, 0x74}},
        {0x415250, ptr(&ZNamedList::EmptyText), "ZNamedList::EmptyText", 5, {0xb8, 0x58, 0xe5, 0x62, 0x00, 0xc3, 0x90, 0x90}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
