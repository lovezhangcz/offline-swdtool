/* -----------------------------------------------------------------------------
 * STM32F103C8T6 (64KB Flash / 20KB RAM) — APP (应用) 链接脚本
 *
 * 与 Keil target "offline_prog_APP_CDC / APP_MSC" 的内存布局一致:
 *   Flash: 0x08002000, 22KB (0x5800)
 *   RAM  : 0x20000000, 20KB (0x5000)
 *
 * 特别注意: main.c 里
 *     const app_fw_info_t g_app_fw_info __attribute__((at(APP_FW_INFO_ADDR)))
 * 是 ARMCC 的绝对定位语法, 在 GCC 下由 gcc_compat.h 改写成
 *     __attribute__((section(".app_fw_info"), used))
 * 再由本脚本的 .app_fw_info 输出段固定在 APP_BASE + 0x100, 与 Keil 镜像
 * 同一地址, 上位机升级时按固定偏移读取识别码/版本号才不受影响。
 * -------------------------------------------------------------------------- */
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);   /* 栈顶 = 0x20005000 */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08002000, LENGTH = 0x5800   /* 22 KB */
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

  /* 固件识别码/版本号: 固定在 APP_BASE + 0x100 (= 0x08002100) */
  .app_fw_info ORIGIN(FLASH) + 0x100 :
  {
    KEEP(*(.app_fw_info))
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
