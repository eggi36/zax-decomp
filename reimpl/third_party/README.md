# Third-party sources (phase 5b)

Zax.exe links zlib 1.1.3 and libogg/libvorbis/vorbisfile 1.0 beta 4 statically (`docs/DECOMPILATION.md` section 10). Phase 5b replaces those copies with the original releases built from source into `Zax-Reimpl.dll` and checks them against the exe's copies in the running game (`reimpl/src/checks_zlib.cpp`, `checks_vorbis.cpp`).

The files here are copied **unchanged** from the release archives below; nothing else of the archives is in the repository, and no script, makefile or build file of them was run. Only the `.c`/`.h` files the build needs and the licence files are here.

| Folder | Release | Archive (official) | SHA-256 of the archive | Verified against |
|---|---|---|---|---|
| `zlib-1.1.3/` | zlib 1.1.3 (1998-07-09) | https://zlib.net/fossils/zlib-1.1.3.tar.gz | `cae5847bc0e1cf113d3f70d037400da3e47c2e2b7b1c96b0b08447a5fbb906f4` | every file identical to the tree of the tag `v1.1.3` of https://github.com/madler/zlib (Mark Adler's history; `diff -r`, 2026-10-28) |
| `libogg-1.0beta4/` | libogg 1.0 beta 4 (2001-02-26) | https://downloads.xiph.org/releases/ogg/libogg-1.0beta4.tar.gz | `c8a4157b0194962aa885e2088cf8561c65ce2eee36a77ca6325c6c36c842b2a9` | https://downloads.xiph.org/releases/ogg/SHA256SUMS |
| `libvorbis-1.0beta4/` | libvorbis 1.0 beta 4 with vorbisfile (2001-02-26) | https://downloads.xiph.org/releases/vorbis/libvorbis-1.0beta4.tar.gz | `732c254d41c77f81b6c432a9592315b105beec6296cdeacc778800223df38202` | https://downloads.xiph.org/releases/vorbis/SHA256SUMS |

Downloaded on 2026-10-28. The versions match the exe: the strings and tables of zlib 1.1.3, `vorbisfile.c` 1.41, `mapping0.c` 1.27 and `registry.c` 1.6 of the beta 4 release (section 10).

## Licences

- **zlib**: the zlib licence ("Copyright notice" in `zlib-1.1.3/README`, also at the top of `zlib.h`): use for any purpose, including commercial, altering and redistributing freely, provided the origin is not misrepresented, altered versions are marked as such and the notice stays in the source distribution. (C) 1995-1998 Jean-loup Gailly and Mark Adler.
- **libogg, libvorbis**: BSD-style licence of the Xiphophorus Company (`COPYING` in both folders): redistribution in source and binary form with the copyright notice, the conditions and the disclaimer; the name may not be used to promote derived products. Copyright (c) 2001, Xiphophorus. `AUTHORS` as in the release.

Both allow building them into the DLL. A binary distribution of the DLL must carry the BSD notice of libogg and libvorbis (its documentation or other materials).

## Files

- `zlib-1.1.3/`: `adler32.c compress.c uncompr.c deflate.c deflate.h trees.c trees.h inflate.c infblock.c infblock.h infcodes.c infcodes.h inftrees.c inftrees.h inffixed.h infutil.c infutil.h inffast.c inffast.h zutil.c zutil.h zlib.h zconf.h README`. Not taken: `crc32.c`, `gzio.c`, the examples and the platform folders (the exe has none of it).
- `libogg-1.0beta4/`: `src/framing.c src/bitwise.c include/ogg/ogg.h include/ogg/os_types.h COPYING AUTHORS`.
- `libvorbis-1.0beta4/`: `include/vorbis/codec.h include/vorbis/vorbisfile.h` and of `lib/` the decoder (`block.c codebook.c floor0.c info.c mapping0.c mdct.c registry.c res0.c sharedbook.c synthesis.c time0.c window.c vorbisfile.c`), the parts the backends' export bundles pull in (`analysis.c envelope.c iir.c lpc.c lsp.c psy.c smallft.c`) and their headers, `COPYING AUTHORS`. `iir.c` is included by `envelope.c` itself. Not taken: `vorbisenc.c`, `bitbuffer.c`, the tools (`barkmel.c psytune.c tone.c lookups.pl`), `lookup.c` and `lookup_data.h` (only for `FLOAT_LOOKUP`/`INT_LOOKUP`, which the exe's build did not use: its `vorbis_lsp_to_curve` is the plain one), the `books/` and `modes/` folders (encoder settings).

## How they are built (`reimpl/build.sh`)

- One wrapper per source file in `reimpl/src/libs/` includes the file as it is and exports the addresses of the functions the exe has a copy of (also the static ones) for the hook tables `zlib_hooks.inc` and `vorbis_hooks.inc` (`reimpl/src/libs/libs.h`).
- As C (gnu89) with clang for `i386-pc-windows-msvc`, like the rest of the DLL, with three settings the exe's copies show:
  - `__fastcall` as the default convention (MSVC `/Gr`; section 10). clang applies that default only with SSE2 in the target, so the build enables SSE2 for these files and the wrappers turn it off for every function (`#pragma clang attribute push(__attribute__((target("no-sse"))), apply_to = function)`): the code is x87 only.
  - Structures packed to 4 bytes (`-fpack-struct=4`, MSVC `/Zp4`): the exe's `OggVorbis_File` has `bittrack` at +0x64 and `os` at +0x74 (8-byte alignment would give +0x68 and +0x78), its `vorbis_block` is 0x6c bytes (`vorbis_block_init`'s memset), the callbacks are at +0x2b0. With 4-byte packing every size and offset the exe's code uses agrees (`codec_setup_info` 0xe54, `deflate_state` 0x16b8); zlib has no 8-byte members, so it does not change there.
  - No changes to any source line.
- Their C runtime is `reimpl/src/libc/` (`zax_libc.h`): `malloc`, `calloc`, `realloc`, `free`, `memmove`, `memchr`, `qsort`, `strlen`, `strcpy`, `strcat`, `strncmp`, `_strdup`, `toupper`, `_errno`, `floor`, `ldexp`, `pow`, `acos` and the file functions of `ov_open` are the exe's own MSVC 6 runtime functions, called by address (so memory moves freely between the exe's copies and the source-built ones); `sin`, `cos`, `sqrt`, `fabs`, `log`, `exp` and `atan` are the x87 sequences MSVC 6 compiled inline for them in the exe's libraries (`fsin`, `fldln2 / fyl2x`, `fldl2e / f2xm1 / fscale`, `fld1 / fpatan`); `labs` inline; `memcpy`, `memset`, `memcmp` are the DLL's own (`runtime.cpp`). `alloca` and frames above 4 KB call the exe's `_chkstk` 0x5d2080 (`lib_vorbis.cpp`). `os.h` of libvorbis takes its MSVC branch (`_WIN32` without `__GNUC__`): `vorbis_ftoi` is the inline `fld`/`fistp` and `rint(x)` is `floor(x + 0.5f)`, as in the exe.
