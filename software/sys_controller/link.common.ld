SEARCH_DIR(.)
__DYNAMIC  =  0;

/* First 16 flash sectors reserved for 2 firmware images (2x 0.5MB).
   In typical configuration a firmware image consists of
   * compressed bitstream (5 sectors / 0.3MB)
   * flash_imem (3 sectors / 0.2MB)
   Last 16 flash sectors reserved for userdata (16x 64KB). */
MEMORY
{
    flash_imem : ORIGIN = 0x02050000, LENGTH = 196608
    dataram    : ORIGIN = 0x00010000, LENGTH = 16384
}

/* Stack information variables */
_min_stack      = 0x1000;   /* 4KB - minimum stack space to reserve */
_stack_start   = ORIGIN(dataram) + LENGTH(dataram);

/* We have to align each sector to word boundaries as our current s19->slm
 * conversion scripts are not able to handle non-word aligned sections. */

SECTIONS
{
    .vectors :
    {
        . = ALIGN(4);
        KEEP(*(.vectors))
    } > flash_imem

    .text : {
        . = ALIGN(4);
        _stext = .;
        *(.text)
        *(.text.*)
        _etext  =  .;
        __CTOR_LIST__ = .;
        LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)
        *(.ctors)
        LONG(0)
        __CTOR_END__ = .;
        __DTOR_LIST__ = .;
        LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)
        *(.dtors)
        LONG(0)
        __DTOR_END__ = .;
        *(.lit)
        *(.shdata)
        _endtext = .;
    }  > flash_imem

    .rodata : {
        . = ALIGN(4);
        *(.rodata);
        *(.rodata.*)
        *(.srodata);
        *(.srodata.*)
        . = ALIGN(4);
    } > flash_imem

    /*--------------------------------------------------------------------*/
    /* Global constructor/destructor segement                             */
    /*--------------------------------------------------------------------*/

    .preinit_array     :
    {
      PROVIDE_HIDDEN (__preinit_array_start = .);
      KEEP (*(.preinit_array))
      PROVIDE_HIDDEN (__preinit_array_end = .);
    } > dataram

    .init_array     :
    {
      PROVIDE_HIDDEN (__init_array_start = .);
      KEEP (*(SORT(.init_array.*)))
      KEEP (*(.init_array ))
      PROVIDE_HIDDEN (__init_array_end = .);
    } > dataram

    .fini_array     :
    {
      PROVIDE_HIDDEN (__fini_array_start = .);
      KEEP (*(SORT(.fini_array.*)))
      KEEP (*(.fini_array ))
      PROVIDE_HIDDEN (__fini_array_end = .);
    } > dataram

    .text_bram : AT ( LOADADDR (.rodata) + SIZEOF (.rodata) ) {
        . = ALIGN(4);
        PROVIDE (__ram_text_start = ABSOLUTE(.));
        *(.text_bram);
        . = ALIGN(4);
        PROVIDE (__ram_text_end = ABSOLUTE(.));
    } > dataram

    PROVIDE (__flash_text_bram_start = LOADADDR(.text_bram));

    .data : AT ( LOADADDR (.text_bram) + SIZEOF (.text_bram) ) {
        . = ALIGN(4);
        PROVIDE (__ram_rwdata_start = ABSOLUTE(.));
        sdata  =  .;
        _sdata  =  .;
        *(.data);
        *(.data.*)
        *(.sdata);
        *(.sdata.*)
        *(.sdata2);
        *(.sdata2.*)
        edata  =  .;
        _edata  =  .;
        . = ALIGN(4);
        PROVIDE (__ram_rwdata_end = ABSOLUTE(.));
    } > dataram

    PROVIDE (__flash_rwdata_start = LOADADDR(.data));

    .shbss :
    {
        . = ALIGN(4);
        *(.shbss)
    } > dataram

    .bss :
    {
        . = ALIGN(4);
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
        *(COMMON)
        _bss_end = .;
    } > dataram

    /* ensure there is enough room for stack */
    .stack (NOLOAD): {
        . = ALIGN(4);
        . = . + _min_stack ;
        . = ALIGN(4);
        stack = . ;
        _stack = . ;
    } > dataram

    .stab  0 (NOLOAD) :
    {
        [ .stab ]
    }

    .stabstr  0 (NOLOAD) :
    {
        [ .stabstr ]
    }

    .bss :
    {
        . = ALIGN(4);
        _end = .;
    } > dataram
}

