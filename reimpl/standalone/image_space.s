# The address range of Zax.exe's image inside Zax-Standalone.exe (docs/DECOMPILATION.md 13, "Batch 6-1", "Batch 6-10c").
#
# The standalone exe is linked at the original image base 0x400000; this object is the first one on the linker's
# command line, so its zero-filled .text chunk is the first one of the output's .text section (lld keeps the input
# order of chunks with the same section name) and covers 0x401000-0x75d000: the original's .text, .rdata, .data/.bss
# and the SecuROM, .idata and .rsrc sections behind them. The range is part of our own image, so the Windows loader
# reserves it before any DLL, heap or stack can take it. Until batch 6-10c loader.cpp copied the user's Zax.exe into
# it; since then nothing of Zax.exe is there and loader.cpp makes the range PAGE_NOACCESS, so a leftover use of an exe
# address faults. The test build at a second image base (build_exe.sh with ZAX_EXE_BASE) leaves this object out.
	.section .text,"xr"
	.p2align 12
	.globl _zax_image_space
_zax_image_space:
	.zero 0x35c000
	.globl _zax_image_space_end
_zax_image_space_end:
