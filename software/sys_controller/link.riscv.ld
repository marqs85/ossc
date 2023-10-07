/* not needed, but we need separate linker scripts anyway */
OUTPUT_ARCH(riscv)

/* required to correctly link newlib */
GROUP( -lc -lgcc )

INCLUDE link.common.ld
