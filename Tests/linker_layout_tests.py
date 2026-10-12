#!/usr/bin/env python3
"""Check Boot/App linker contracts with the real HC32 startup and GNU Arm tools."""
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


if __name__ == "__main__":
    main(sys.argv[1])
