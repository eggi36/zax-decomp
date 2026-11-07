"""Build helpers of the standalone exe (reimpl/build.sh --exe; docs/DECOMPILATION.md 13, "Batch 6-1: the scaffold").

    standalone_pe.py resources <Zax.exe> <dir>   check the exe's SHA-256, write <dir>/standalone.rc and the icon
                                                 (Zax.exe's icon group 1, string table and version, as they are)
    standalone_pe.py header <Zax.exe> <our.exe> [base]  give our exe the original's optional header values that change
                                                 how Windows treats the process: no ASLR (it must stay at
                                                 0x400000) and no DEP opt-in (DllCharacteristics 0), OS and
                                                 subsystem version 4.0, stack and heap reserve and commit

What these write is derived from the user's own Zax.exe at build time and stays in the build's temporary folder
and out/; it is never committed (like the launcher's icon).
"""
import hashlib
import struct
import sys
from pathlib import Path

import pefile

SHA256 = '7d9efcae1ef09975b0826e7c5d164d97a490bda5fa826ddf906e2e4acea236e4'
RT_ICON, RT_STRING, RT_GROUP_ICON = 3, 6, 14


def load(path: str) -> pefile.PE:
    data = Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest() != SHA256:
        sys.exit(f'{path}: not the supported Zax.exe (SHA-256 {SHA256[:8]}...)')
    return pefile.PE(data=data)


def _data(pe, entry):
    d = entry.directory.entries[0].data.struct
    return pe.get_data(d.OffsetToData, d.Size)


def rc_string(b: bytes) -> str:
    return '"' + b.decode('cp1252').replace('"', '""').replace('\\', '\\\\') + '"'


def resources(exe: str, out: str) -> None:
    pe = load(exe)
    outdir = Path(out)
    res = {t.id: t for t in pe.DIRECTORY_ENTRY_RESOURCE.entries}
    lines = ['#pragma code_page(1252)']
    # the icon group as an .ico file: the group's entries with their RT_ICON images appended
    icons = {e.id: _data(pe, e) for e in res[RT_ICON].directory.entries}
    for group in res[RT_GROUP_ICON].directory.entries:
        g = _data(pe, group)
        count = struct.unpack_from('<H', g, 4)[0]
        head, blobs = struct.pack('<HHH', 0, 1, count), []
        offset = 6 + 16 * count
        for i in range(count):
            w, h, c, r, planes, bits, size, iid = struct.unpack_from('<BBBBHHIH', g, 6 + 14 * i)
            head += struct.pack('<BBBBHHII', w, h, c, r, planes, bits, len(icons[iid]), offset)
            offset += len(icons[iid])
            blobs.append(icons[iid])
        name = f'icon{group.id}.ico'
        (outdir / name).write_bytes(head + b''.join(blobs))
        lines.append(f'{group.id} ICON "{name}"')
    # the string table: block n holds the strings 16 (n - 1) ... 16 n - 1, each a length and UTF-16 text
    strings = []
    for block in res[RT_STRING].directory.entries:
        b, pos = _data(pe, block), 0
        for k in range(16):
            n = struct.unpack_from('<H', b, pos)[0]
            text = b[pos + 2:pos + 2 + 2 * n].decode('utf-16-le')
            pos += 2 + 2 * n
            if text:
                strings.append(f'    {16 * (block.id - 1) + k}, {rc_string(text.encode("cp1252"))}')
    lines += ['STRINGTABLE', 'BEGIN', *strings, 'END']
    # the version resource
    ffi = pe.VS_FIXEDFILEINFO[0]
    v = lambda ms, ls: f'{ms >> 16},{ms & 0xffff},{ls >> 16},{ls & 0xffff}'
    lines += ['1 VERSIONINFO', f'FILEVERSION {v(ffi.FileVersionMS, ffi.FileVersionLS)}',
              f'PRODUCTVERSION {v(ffi.ProductVersionMS, ffi.ProductVersionLS)}', f'FILEFLAGSMASK 0x{ffi.FileFlagsMask:x}',
              f'FILEFLAGS 0x{ffi.FileFlags:x}', f'FILEOS 0x{ffi.FileOS:x}', f'FILETYPE 0x{ffi.FileType:x}',
              f'FILESUBTYPE 0x{ffi.FileSubtype:x}', 'BEGIN']
    for fi in pe.FileInfo:
        for x in fi:
            if x.Key == b'StringFileInfo':
                lines.append('    BLOCK "StringFileInfo"\n    BEGIN')
                for t in x.StringTable:
                    lines.append(f'        BLOCK "{t.LangID.decode()}"\n        BEGIN')
                    for k, val in t.entries.items():
                        lines.append(f'            VALUE "{k.decode()}", {rc_string(val.decode("utf-8").encode("cp1252"))}')
                    lines.append('        END')
                lines.append('    END')
            elif x.Key == b'VarFileInfo':
                lines.append('    BLOCK "VarFileInfo"\n    BEGIN')
                for var in x.Var:
                    for k, val in var.entry.items():
                        lines.append(f'        VALUE "{k.decode()}", ' + ', '.join(val.split()))
                lines.append('    END')
    lines.append('END')
    (outdir / 'standalone.rc').write_bytes(('\r\n'.join(lines) + '\r\n').encode('cp1252'))


def header(exe: str, ours: str, base: str = '0x400000') -> None:
    o = load(exe).OPTIONAL_HEADER
    pe = pefile.PE(ours)
    h = pe.OPTIONAL_HEADER
    if h.ImageBase != int(base, 16):   # 0x400000, or the test build's second base (build_exe.sh ZAX_EXE_BASE, batch 6-10b)
        sys.exit(f'{ours}: image base {h.ImageBase:#x}, not {base}')
    for k in ('MajorOperatingSystemVersion', 'MinorOperatingSystemVersion', 'MajorSubsystemVersion', 'MinorSubsystemVersion',
              'SizeOfStackReserve', 'SizeOfStackCommit', 'SizeOfHeapReserve', 'SizeOfHeapCommit', 'DllCharacteristics'):
        setattr(h, k, getattr(o, k))
    h.CheckSum = 0
    data = bytearray(pe.write())
    pe.close()
    Path(ours).write_bytes(data)


if __name__ == '__main__':
    if len(sys.argv) not in (4, 5) or sys.argv[1] not in ('resources', 'header'):
        sys.exit(__doc__)
    {'resources': resources, 'header': header}[sys.argv[1]](*sys.argv[2:])
