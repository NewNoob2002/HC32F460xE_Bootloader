#!/usr/bin/env python3
"""Check Boot/App linker contracts with the real HC32 startup and GNU Arm tools."""
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPU_FLAGS = ["-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"]


def run(command):
    return subprocess.run(command, text=True, capture_output=True, check=False)


def require_success(command):
    result = run(command)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout


def main(compiler):
    compiler = Path(shutil.which(compiler) or compiler).resolve()
    nm = compiler.with_name(compiler.name.replace("gcc", "nm"))
    cxx = compiler.with_name(compiler.name.replace("gcc", "g++"))
    objdump = compiler.with_name(compiler.name.replace("gcc", "objdump"))
    objcopy = compiler.with_name(compiler.name.replace("gcc", "objcopy"))
    startup = ROOT / "Drivers/CMSIS/Device/HDSC/hc32f4xx/Source/GCC/startup_hc32f460.S"
    common = ROOT / "Platform/HC32F460/Linker"
    with tempfile.TemporaryDirectory(prefix="hc32-linker-test-") as temp:
        work = Path(temp)
        source = work / "fixture.c"
        source.write_text('''#include <stdint.h>
volatile uint32_t initialized = 0x12345678U;
volatile uint32_t zeroed;
__attribute__((section(".noinit.test"))) volatile uint32_t retained;
__attribute__((section(".ret_ram_data"))) volatile uint32_t ret_initialized = 5U;
__attribute__((section(".ret_ram_bss"))) volatile uint32_t ret_zeroed;
void SystemInit(void) {}
void __libc_init_array(void) {}
int main(void) { return (int)(initialized + zeroed + retained + ret_initialized + ret_zeroed); }
''')
        require_success([str(compiler), *CPU_FLAGS, "-c", str(startup), "-o", str(work / "startup.o")])
        require_success([str(compiler), *CPU_FLAGS, "-c", str(source), "-o", str(work / "fixture.o")])

        def extra_object(name, section, size):
            assembly = work / (name + ".S")
            assembly.write_text(f'.section {section}, "aw"\n.balign 4\n.space {size}\n')
            output = work / (name + ".o")
            require_success([str(compiler), *CPU_FLAGS, "-c", str(assembly), "-o", str(output)])
            return str(output)

        icg = extra_object("icg", ".icg_sec", 32)
        for image, origin, capacity in (("Boot", 0, 0x8000), ("App", 0x8000, 0x72000)):
            script = ROOT / image / "Linker" / f"HC32F460_{image.lower()}.ld"
            output = work / (image + ".elf")
            base = [str(compiler), *CPU_FLAGS, "-nostdlib", f"-Wl,-L,{common}", f"-T{script}",
                    str(work / "startup.o"), str(work / "fixture.o"), "-o", str(output)]
            objects = [icg] if image == "Boot" else []
            require_success(base + objects)
            symbols = {}
            for line in require_success([str(nm), str(output)]).splitlines():
                fields = line.split()
                if len(fields) == 3:
                    symbols[fields[2]] = int(fields[0], 16)
            assert symbols["FLASH_ORIGIN"] == origin
            assert symbols["FLASH_LENGTH"] == capacity
            assert symbols["__Vectors"] == origin
            assert origin <= symbols["__etext"] < origin + capacity
            assert symbols["__etext_ret_ram"] + 4 <= origin + capacity
            assert symbols["__bss_end__"] <= symbols["retained"] < symbols["__StackLimit"]
            assert 0x1FFF8000 <= symbols["__data_start__"] < symbols["__StackTop"] <= 0x20027000
            assert 0x200F0000 <= symbols["ret_initialized"] < 0x200F1000
            vectors = work / "vectors.bin"
            require_success([str(objcopy), "-O", "binary", "--only-section=.vectors", str(output), str(vectors)])
            msp, reset = struct.unpack_from("<II", vectors.read_bytes())
            assert msp == symbols["__StackTop"] and msp % 8 == 0
            assert reset & 1 and (reset & ~1) == (symbols["Reset_Handler"] & ~1)

            def reject(arguments, message, command=base):
                result = run(command + arguments)
                assert result.returncode != 0 and message in result.stderr, result.stdout + result.stderr

            if image == "Boot":
                reject([], "ICG must contain exactly eight")
                reject(objects + ["-Wl,--defsym=FLASH_LENGTH=0x10000"], "exactly 32 KiB")
            else:
                reject([icg], "ICG belongs to Boot")
                reject(["-Wl,--defsym=FLASH_LENGTH=0x74000"], "must end at 0x7A000")
            reject(objects + ["-Wl,--defsym=FLASH_ORIGIN=0x1000"], "must start")
            overflow = extra_object(image + "_overflow", ".rodata.overflow", capacity)
            reject(objects + [overflow], "Flash load image exceeds image region")
            # Code fits, but RAM initialization bytes would cross the image's Flash end.
            pad = extra_object(image + "_pad", ".rodata.pad", capacity - 0x800)
            data = extra_object(image + "_data", ".data.extra", 0x1000)
            reject(objects + [pad, data], "Flash load image exceeds image region")
            print(f"{image}: vectors/startup/data/noinit and ICG/range/overflow guards PASS")

        # 使用真实 G++/newlib，验证 C linkage 与静态构造函数，不复用上面的空 libc stub。
        c_source = work / "mixed.c"
        c_source.write_text('''#include "bsp_power.h"
bool bsp_power_init(void) { return true; }
''')
        cpp_source = work / "constructor.cpp"
        cpp_source.write_text('''#include <stdint.h>
#include "bsp_board_config.h"
#include "bsp_power.h"
#include "bsp_debug_port.h"
#include "bsp_external_watchdog.h"
#include "bsp_status_led.h"
#include "bsp_write_protection.h"
#include "hc32_ll.h"
volatile uint32_t constructed;
struct Probe { Probe() { constructed = 0x12345678U; } };
Probe probe;
extern "C" void SystemInit(void) {}
int main() { return bsp_power_init() && constructed == 0x12345678U ? 0 : 1; }
''')
        includes = [f"-I{ROOT / path}" for path in (
            "Drivers/BSP/Inc", "Config", "Drivers/CMSIS/Include",
            "Drivers/CMSIS/Device/HDSC/hc32f4xx/Include", "Drivers/hc32f4xx_ll_drivers/inc")]
        require_success([str(compiler), *CPU_FLAGS, *includes, "-std=c11", "-c", str(c_source),
                         "-o", str(work / "mixed.o")])
        require_success([str(cxx), *CPU_FLAGS, *includes, "-std=c++17", "-fno-exceptions", "-fno-rtti",
                         "-DHC32F460", "-D__PROGRAM_START=main", "-c", str(cpp_source),
                         "-o", str(work / "constructor.o")])
        output = work / "cpp_app.elf"
        require_success([str(cxx), *CPU_FLAGS, f"-Wl,-L,{common}",
                         f"-T{ROOT / 'App/Linker/HC32F460_app.ld'}", "-Wl,--gc-sections",
                         "--specs=nano.specs", "--specs=nosys.specs", str(work / "startup.o"),
                         str(work / "mixed.o"), str(work / "constructor.o"), "-o", str(output)])
        symbols = require_success([str(nm), str(output)])
        assert " T bsp_power_init" in symbols and " T __libc_init_array" in symbols
        assert "_GLOBAL__sub_I_" in symbols
        ctor_data = work / "constructors.bin"
        require_success([str(objcopy), "-O", "binary", "--only-section=.init_array", str(output), str(ctor_data)])
        assert len(ctor_data.read_bytes()) >= 4
        constructor_addresses = struct.unpack(f"<{len(ctor_data.read_bytes()) // 4}I", ctor_data.read_bytes())
        ctor_symbol = next(line for line in symbols.splitlines() if "_GLOBAL__sub_I_" in line)
        assert (int(ctor_symbol.split()[0], 16) | 1) in constructor_addresses
        startup_code = require_success([str(objdump), "-d", "--disassemble=Reset_Handler", str(output)])
        assert startup_code.index("<__libc_init_array>") < startup_code.index("<main>")
        print("App C++17: C linkage / real libc init / constructor table PASS")


def check_actual_app(compiler, image):
    compiler = Path(shutil.which(compiler) or compiler).resolve()
    image = Path(image)
    nm = compiler.with_name(compiler.name.replace("gcc", "nm"))
    objdump = compiler.with_name(compiler.name.replace("gcc", "objdump"))
    objcopy = compiler.with_name(compiler.name.replace("gcc", "objcopy"))
    symbols = {}
    for line in require_success([str(nm), str(image)]).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    assert symbols["__Vectors"] == 0x8000
    assert symbols["FLASH_LENGTH"] == 0x72000
    assert not any(name.startswith(("boot_", "g_boot_")) for name in symbols)
    assert not any(name.startswith(("_ZTI", "_ZTV", "__cxa_throw")) for name in symbols)
    sections = require_success([str(objdump), "-h", str(image)]).splitlines()
    for index, line in enumerate(sections[:-1]):
        match = re.match(r"\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", line)
        if match:
            name, size, _, lma = match.groups()
            assert name != ".icg_sec" or int(size, 16) == 0
            if "LOAD" in sections[index + 1] and int(size, 16):
                assert 0x8000 <= int(lma, 16) < int(lma, 16) + int(size, 16) <= 0x7A000
    with tempfile.TemporaryDirectory(prefix="x1-image-test-") as temp:
        vectors = Path(temp) / "vectors.bin"
        require_success([str(objcopy), "-O", "binary", "--only-section=.vectors", str(image), str(vectors)])
        words = struct.unpack("<16I", vectors.read_bytes()[:64])
        assert words[0] == symbols["__StackTop"] and words[0] % 8 == 0
        for index, handler in ((1, "Reset_Handler"), (2, "NMI_Handler"), (3, "HardFault_Handler"),
                               (4, "MemManage_Handler"), (5, "BusFault_Handler"), (6, "UsageFault_Handler"),
                               (11, "SVC_Handler"), (12, "DebugMon_Handler"), (14, "PendSV_Handler"),
                               (15, "SysTick_Handler")):
            assert words[index] & 1 and (words[index] & ~1) == (symbols[handler] & ~1)
            assert symbols[handler] != symbols["Default_Handler"]
    print(f"{image}: actual X1 vectors / load ranges / no ICG or Boot dependency PASS")


if __name__ == "__main__":
    main(sys.argv[1])
    for app_image in sys.argv[2:]:
        check_actual_app(sys.argv[1], app_image)
