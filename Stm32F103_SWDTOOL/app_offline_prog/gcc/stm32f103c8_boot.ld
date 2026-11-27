/* -----------------------------------------------------------------------------
 * STM32F103C8T6 (64KB Flash / 20KB RAM) — BOOT (bootloader) 链接脚本
 *
 * Flash 布局与 Keil 工程完全一致:
 *   0x08000000 - 0x080017FF  boot 6KB      <-- 本镜像
 *   0x08001800 - 0x08001FFF  升级标志区 2KB
 *   0x08002000 - 0x080077FF  应用 22KB
 *   0x08007800 - 0x0800FFFF  记录区 + 目标镜像区
 * -------------------------------------------------------------------------- */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);   /* 栈顶 = 0x20005000 */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 0x1800   /* 6 KB  */
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 0x5000   /* 20 KB */
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } > FLASH

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } > FLASH

  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT> FLASH

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
    . = ALIGN(8);
    end = .;
    _end = .;
  } > RAM

}
