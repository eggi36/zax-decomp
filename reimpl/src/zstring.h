// ZString: the string class of Zax.exe. Reference-counted, copy on write.
//
// A ZString is a single pointer to a buffer {reference count, capacity, text}. The text starts at
// byte 8 and has room for capacity + 1 bytes; the buffer comes from the pool allocator with size
// capacity + 9. All empty strings share one static buffer (0x6c0ef0, capacity 0), created on first
// use (guard bit 0 of 0x6c0f08) and also reference-counted.
#pragma once
#include "reimpl.h"

namespace zax {

struct ZStringBuffer {
    DWORD refs;       // 0x00
    DWORD capacity;   // 0x04 characters without the terminator
    char text[1];     // 0x08

    ZStringBuffer *THISCALL Init(DWORD refs);   // 0x4dffb0
};

struct ZString {
    ZStringBuffer *buffer;

    const char *c_str() const { return buffer->text; }
    bool empty() const { return buffer->text[0] == 0; }

    ZString *THISCALL Ctor();                             // 0x4dec90
    ZString *THISCALL CopyCtor(const ZString *other);     // 0x4dedf0
    void THISCALL Dtor();                                 // 0x4defd0
    const char *THISCALL CStr();                          // 0x4e13a0
    DWORD THISCALL Length();                              // 0x4e13b0
    bool THISCALL IsEmpty();                              // 0x4e13d0
    bool THISCALL NotEmpty();                             // 0x4e13e0
    ZString *THISCALL Assign(const ZString *other);       // 0x4e18f0
    ZString *THISCALL AssignText(const char *s);          // 0x4e1930
    void THISCALL AssignTextN(const char *s, DWORD n);    // 0x4df160 (eax undefined in the original)

    // Constructors from text (the empty one without the check hook of Ctor returns nothing)
    void THISCALL InitEmpty();                                                        // 0x4dece0
    ZString *THISCALL CtorText(const char *s);                                        // 0x4dee00
    ZString *THISCALL CtorTextN(const char *s, DWORD n);                              // 0x4deeb0
    ZString *THISCALL CtorConcat(const char *a, DWORD na, const char *b, DWORD nb);   // 0x4def50

    // Assignment variants that return nothing (eax undefined in the original)
    void THISCALL Empty();                                // 0x4ded30 (accepts the null buffer)
    void THISCALL SetText(const char *s);                 // 0x4df010, like AssignText
    void THISCALL SetString(const ZString *other);        // 0x4df270, like Assign
    void THISCALL Clear();                                // 0x4df2b0
    void THISCALL SetPath(const char *dir, const char *name);                       // 0x4dfa50
    void THISCALL SetPath3(const char *dir, const char *name, const char *suffix);  // 0x4e02c0
    void THISCALL Append(const char *s);                  // 0x4e1be0

    // Appending (all grow to exactly the needed size)
    void THISCALL AppendText(const char *s);              // 0x4df330 (no null check)
    void THISCALL AppendTextN(const char *s, DWORD n);    // 0x4df440 n bytes, no terminator check
    void THISCALL AppendRepeat(char c, DWORD n);          // 0x4df540
    void THISCALL AppendString(const ZString *other);     // 0x4e1ab0
    void THISCALL AppendChar(char c);                     // 0x4e1d10
    void THISCALL Insert(const char *s, DWORD pos);       // 0x4e0d30 (no check of pos)
    void THISCALL AppendPathName(const char *name);       // 0x4df650 null: only ensure a final '/'
    void THISCALL AppendPathN(const char *name, DWORD n); // 0x4df880
    void THISCALL GrowTo(DWORD n);                        // 0x4e0e60 like Reserve, but a shared buffer stays

    // dir + '/' + directory of file + '/' + '-' + name of file [+ '/' + sub] + suffix
    void THISCALL SetDashPath(const char *dir, const char *file, const char *suffix);                    // 0x4dffd0
    void THISCALL SetDashPathSub(const char *dir, const char *file, const char *sub, const char *suffix); // 0x4dfc70

    // Directory (without the final separator) and name of a path; '/' and '\\' both separate
    static void __fastcall SplitPath(const ZString *path, ZString *dir, ZString *name);   // 0x4e0570
    static void __fastcall SplitPathText(const char *path, ZString *dir, ZString *name);  // 0x4e0770

    // In-place editing through the text functions of the exe, after making the buffer its own
    void THISCALL TrimRight();                            // 0x4e0f20 tab, space, CR, LF
    void THISCALL TrimLeft();                             // 0x4e10f0
    void THISCALL KeepChars(const char *chars);           // 0x4e1000 removes all characters not in chars
    void THISCALL MakeUpper();                            // 0x4e11d0 a-z only
    void THISCALL ReplaceChar(char from, char to);        // 0x4e12b0
    void THISCALL SetHex(const void *data, DWORD n);      // 0x4e1880 two hex digits per byte

    // printf into the string; the size comes from FormatLength (an upper bound)
    static void __cdecl Format(ZString *z, const char *fmt, ...);         // 0x4e09b0
    static void __cdecl AppendFormat(ZString *z, const char *fmt, ...);   // 0x4e0a30

    // Times in seconds with the localized separators and suffixes (globals below)
    void THISCALL SetTimeHMS(float seconds);              // 0x4e0b30 "%02d:%02d:%02d"
    void THISCALL SetDuration(float seconds);             // 0x4e0bc0 days, hours or minutes first

    // Parts, buffer access, comparison
    ZString *THISCALL Mid(ZString *out, DWORD start);                 // 0x4e1400
    ZString *THISCALL MidN(ZString *out, DWORD start, DWORD count);   // 0x4e14c0
    char *THISCALL GetBuffer(DWORD n);                                // 0x4e1590
    char *THISCALL GetBufferDiscard(DWORD n);                         // 0x4e1660
    void THISCALL ReleaseBuffer(int n);                               // 0x4e16d0
    int THISCALL Compare(const ZString *other);                       // 0x4e1700
    int THISCALL CompareText(const char *s);                          // 0x4e1770
    int THISCALL CompareNoCase(const ZString *other);                 // 0x4e17e0
    int THISCALL CompareNoCaseText(const char *s);                    // 0x4e1830
    char THISCALL GetAt(DWORD i);                                     // 0x4e1aa0 (upper bytes of eax undefined)

    // Buffer management, called by the inlined code of other functions as well
    void THISCALL Reserve(DWORD n);                       // 0x4e1e00 own buffer of capacity >= n, text kept
    void THISCALL Release();                              // 0x4e1ec0
    void THISCALL AllocBuffer(DWORD n);                   // 0x4e1ef0 (old buffer not released)
    void THISCALL AllocCopy(const char *s, DWORD n);      // 0x4e1f20 (old buffer not released)

private:
    void release();
    void alloc(DWORD n);
    void grow_copy(DWORD n);
    void set_empty();
    void set_text(const char *s, DWORD n);
    void join_path(const char *name, DWORD nname);
    void make_own();
    void append_dash_name(const ZString &dirpart, const ZString &name);
    static void split(const char *path, ZString *dir, ZString *name);
};
static_assert(sizeof(ZString) == 4);

ZStringBuffer *__cdecl ZString_EmptyBuffer();   // 0x4dec40
DWORD __fastcall FormatLength(const char *fmt, const DWORD *args);   // 0x585490

constexpr DWORD ZSTRING_EMPTY = 0x6c0ef0;
constexpr DWORD ZSTRING_EMPTY_GUARD = 0x6c0f08;

// Localized ZStrings for times, set by static initializers (text from the language pack)
constexpr DWORD TIME_SUFFIX_MINUTES = 0x6c0ee8;   // "suffix indicating minutes"
constexpr DWORD TIME_SEP_HOURS = 0x6c0efc;        // "Hours:Minutes seperator"
constexpr DWORD TIME_SEP_MINUTES = 0x6c0f00;      // "Minutes:Seconds seperator"
constexpr DWORD TIME_SUFFIX_SECONDS = 0x6c0f04;   // "suffix indicating seconds"
constexpr DWORD TIME_SUFFIX_DAYS = 0x6c0f0c;      // "\"d\" is short for the days"

}  // namespace zax
