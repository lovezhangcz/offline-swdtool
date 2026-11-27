# =============================================================================
#  STM32F103 SWD 离线编程器 — GNU Arm Embedded (arm-none-eabi-gcc) 构建脚本
#
#  与 Keil 工程 app_offline_prog/project/offline_prog.uvproj 的 5 个 target 一一对应:
#      boot             <-> offline_prog_BOOT            0x08000000 / 6KB
#      app-cdc          <-> offline_prog_APP_CDC         0x08002000 / 22KB  USB_MODE=1
#      app-msc          <-> offline_prog_APP_MSC         0x08002000 / 22KB  USB_MODE=3
#      app-cdc-256k     <-> offline_prog_APP_CDC_256K    0x08002000 / 22KB  +FLASH_256K
#      app-msc-256k     <-> offline_prog_APP_MSC_256K    0x08002000 / 22KB  +FLASH_256K
#
#  用法 (在 gcc 目录下):
#      make                 编译全部 5 个 target
#      make boot            只编译 boot
#      make app-cdc         只编译 CDC 应用
#      make clean           清理 build/
#      make flash           用 J-Link 烧录 boot + app-cdc + 升级标志
#
#  Windows 下用 build.bat 更省事 (自动把工具链加入 PATH)。
# =============================================================================

VARIANTS := boot app-cdc app-msc app-cdc-256k app-msc-256k

# ---------------------------------------------------------------- 顶层入口
ifeq ($(BUILD_VARIANT),)

OBJDIRS := $(foreach v,$(VARIANTS),build/$(v)/obj)
$(shell powershell -NoProfile -ExecutionPolicy Bypass -File tools/mkdirs.ps1)

.PHONY: all clean $(VARIANTS) flash

all: $(VARIANTS)

$(VARIANTS):
	@$(MAKE) --no-print-directory BUILD_VARIANT=$@ _build

clean:
	powershell -NoProfile -Command "Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue"

flash: boot app-cdc
	@cmd /c tools\program.bat

# ----------------------------------------------------------------- 单个变体
else

VARIANT  := $(BUILD_VARIANT)
OBJDIR   := build/$(VARIANT)/obj
NAME     := offline_prog_$(subst -,_,$(VARIANT))
ELF      := build/$(VARIANT)/$(NAME).elf
BIN      := build/$(VARIANT)/$(NAME).bin
HEX      := build/$(VARIANT)/$(NAME).hex
MAP      := build/$(VARIANT)/$(NAME).map

CROSS    ?= arm-none-eabi-
CC       := $(CROSS)gcc
OBJCOPY  := $(CROSS)objcopy
SIZE     := $(CROSS)size

# ------------------------------------------------------------ 头文件路径
INCLUDES := \
  -I. \
  -I../source \
  -I../boot \
  -I../../fw_lib/CMSIS/CM3/CoreSupport \
  -I../../fw_lib/CMSIS/CM3/DeviceSupport/ST/STM32F10x

CMSIS_DEV := ../../fw_lib/CMSIS/CM3/DeviceSupport/ST/STM32F10x

# ---------------------------------------------------------- 每个变体的差异
DEFS     := -DSTM32F10X_MD
LDSCRIPT := stm32f103c8_app.ld

ifeq ($(VARIANT),boot)
  DEFS     := -DSTM32F10X_MD -DBUILD_BOOT -DUSB_MODE=1
  LDSCRIPT := stm32f103c8_boot.ld
endif
ifeq ($(VARIANT),app-cdc)
  DEFS     := -DSTM32F10X_MD -DBUILD_APP -DUSB_MODE=1
endif
ifeq ($(VARIANT),app-msc)
  DEFS     := -DSTM32F10X_MD -DBUILD_APP -DUSB_MODE=3
endif
ifeq ($(VARIANT),app-cdc-256k)
  DEFS     := -DSTM32F10X_MD -DBUILD_APP -DUSB_MODE=1 -DFLASH_256K
endif
ifeq ($(VARIANT),app-msc-256k)
  DEFS     := -DSTM32F10X_MD -DBUILD_APP -DUSB_MODE=3 -DFLASH_256K
endif

# ---------------------------------------------------------------- 编译选项
CFLAGS := \
  -mcpu=cortex-m3 -mthumb -mfloat-abi=soft \
  -std=gnu11 -Oz -g3 -gdwarf-4 \
  -Wall -Wextra -Wno-unused-parameter \
  -ffunction-sections -fdata-sections -fno-common \
  -fno-unwind-tables -fno-asynchronous-unwind-tables \
  -flto \
  $(DEFS) $(INCLUDES) \
  -include gcc_compat.h \
  -MMD -MP \
  $(EXTRA_CFLAGS)

LDFLAGS := \
  -mcpu=cortex-m3 -mthumb -mfloat-abi=soft \
  -nostartfiles -flto \
  -T$(LDSCRIPT) \
  -Wl,--gc-sections \
  -Wl,-Map=$(MAP),--print-memory-usage \
  --specs=nano.specs --specs=nosys.specs \
  $(EXTRA_LDFLAGS)

# ---------------------------------------------------------------- 源文件
APP_C  := main.c bsp.c crc.c storage.c proto.c target_prog.c flash_algo.c \
          flash_algo_gd32f1x0.c flash_algo_gd32f303.c flash_algo_apm32f40x.c \
          flash_algo_stm32f1.c flash_algo_stm32f0.c swd_host.c usb_hw.c \
          usb_cdc.c usb_msc.c usb_comp.c vfs.c log.c
BOOT_C := boot_main.c boot_flash.c boot_proto.c

OBJS := $(addprefix $(OBJDIR)/,$(APP_C:.c=.o) $(BOOT_C:.c=.o)) \
        $(OBJDIR)/system_stm32f10x.o \
        $(OBJDIR)/startup_stm32f10x_md.o

# ---------------------------------------------------------------- 构建规则
.PHONY: _build

_build: $(ELF) $(BIN) $(HEX)
	@echo [$(VARIANT)] build done: $(ELF)
	@$(SIZE) $(ELF)

$(ELF): $(OBJS) $(LDSCRIPT)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary --gap-fill=255 $< $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(OBJDIR)/%.o: ../source/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: ../boot/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/system_stm32f10x.o: $(CMSIS_DEV)/system_stm32f10x.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/startup_stm32f10x_md.o: startup_stm32f10x_md.s
	$(CC) $(CFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

endif
