# Phase 2 Transport Boundary Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the legacy protocol parser transport-independent while preserving the node 1 HC32 I2C1 RX/TX, timeout, recovery and wire behavior.

**Architecture:** Add a pure chunk-input parser API, move the I2C RX drain and critical section into the I2C BSP, and let `main.c` orchestrate transport read, parser feed, timeout and recovery. Keep outbound response reservation/publication I2C-specific until Phase 3.

**Tech Stack:** C11, CMake/Ninja, arm-none-eabi-gcc 14.3.1, CTest host executables, HC32F460 LL/BSP, SEGGER J-Link HIL.

**Spec:** `docs/superpowers/specs/2026-08-25-phase2-transport-boundary-design.md`

## Global Constraints

- Preserve the exact node 1 request/response bytes, separate STOP/write and delayed read behavior, TEI completion path and TX ownership rules.
- The parser library must not include BSP headers or reference I2C buffers, slave states or critical sections.
- Do not add a transport vtable, function-pointer registry, runtime transport selection, dynamic allocation or unused outbound abstraction.
- Keep `BootUpdateService` TX reserve/write behavior unchanged; outbound decoupling belongs to Phase 3.
- Keep the current HC32 ICG block at address `0x00000400`, size `0x20`, with the validated eight words.
- Prefix every shell command with `rtk`.
- Preserve local HIL failures and retries under ignored `debug_artifacts/`; do not convert failed attempts into passes.

---

### Task 1: Add pure parser chunk input

**Files:**
- Modify: `Protocol/Inc/boot_protocol_parser.h:60-70`
- Modify: `Protocol/Src/boot_protocol_parser.c:90-168`
- Modify: `Tests/boot_protocol_parser_tests.c:110-270`

**Interfaces:**
- Consumes: `bool BootProtocolParserPushByte(boot_protocol_parser_t*, uint8_t)`
- Produces: `size_t BootProtocolParserPushBytes(boot_protocol_parser_t*, const uint8_t*, size_t)` returning completed valid-frame count.

- [ ] **Step 1: Add a failing chunk-input test**

Add this test after `feed_bytes()` in `Tests/boot_protocol_parser_tests.c` and call it from `main()` before the legacy callback test:

```c
static void test_push_bytes(void) {
    boot_protocol_parser_t parser;
    uint8_t payload[BOOT_PROTOCOL_MIN_PAYLOAD_SIZE] = {PACKET_CMD_HANDSHAKE, PACKET_CMD_TYPE_DATA};
    uint8_t first[BOOT_PROTOCOL_MAX_FRAME_SIZE];
    uint8_t second[BOOT_PROTOCOL_MAX_FRAME_SIZE];
    uint8_t stream[BOOT_PROTOCOL_MAX_FRAME_SIZE * 2U + 3U];
    const size_t first_length = make_frame(first, 0x21U, payload, sizeof(payload));
    const size_t second_length = make_frame(second, 0x22U, payload, sizeof(payload));

    BootProtocolParserInit(&parser);
    BootProtocolParserRegisterCallback(&parser, capture_frame, NULL);
    callback_count = 0U;
    assert(BootProtocolParserPushBytes(NULL, first, first_length) == 0U);
    assert(BootProtocolParserPushBytes(&parser, NULL, first_length) == 0U);
    assert(BootProtocolParserPushBytes(&parser, first, 0U) == 0U);

    assert(BootProtocolParserPushBytes(&parser, first, 3U) == 0U);
    assert(BootProtocolParserHasPartialFrame(&parser));
    assert(BootProtocolParserPushBytes(&parser, &first[3U], first_length - 3U) == 1U);
    assert(callback_count == 1U);
    assert(callback_frame.frame_number == 0x21U);

    stream[0] = 0x55U;
    stream[1] = 0xAAU;
    stream[2] = 0x00U;
    memcpy(&stream[3U], first, first_length);
    memcpy(&stream[3U + first_length], second, second_length);
    assert(BootProtocolParserPushBytes(&parser, stream, 3U + first_length + second_length) == 2U);
    assert(callback_count == 3U);
    assert(callback_frame.frame_number == 0x22U);
}
```

- [ ] **Step 2: Run the focused host build and confirm red**

Run:

```sh
rtk cmake --preset HostTests
rtk cmake --build build/HostTests --target boot_protocol_tests --parallel
```

Expected: build or link failure because `BootProtocolParserPushBytes` is not declared/defined.

- [ ] **Step 3: Declare and implement the minimum chunk API**

Add to `Protocol/Inc/boot_protocol_parser.h` immediately after `BootProtocolParserPushByte()`:

```c
/** @brief Feeds an ordered byte chunk and returns the number of valid frames completed. */
size_t BootProtocolParserPushBytes(boot_protocol_parser_t* parser, const uint8_t* bytes, size_t length);
```

Add to `Protocol/Src/boot_protocol_parser.c` immediately after `BootProtocolParserPushByte()`:

```c
size_t BootProtocolParserPushBytes(boot_protocol_parser_t* parser, const uint8_t* bytes, size_t length) {
    size_t completed = 0U;
    if ((parser == NULL) || (bytes == NULL))
        return 0U;
    for (size_t index = 0U; index < length; ++index)
        completed += BootProtocolParserPushByte(parser, bytes[index]) ? 1U : 0U;
    return completed;
}
```

Replace the test-only `feed_bytes()` loop with:

```c
static uint32_t feed_bytes(boot_protocol_parser_t* parser, const uint8_t* bytes, size_t length) {
    return (uint32_t)BootProtocolParserPushBytes(parser, bytes, length);
}
```

- [ ] **Step 4: Run parser and full host tests and confirm green**

Run:

```sh
rtk cmake --build build/HostTests --target boot_protocol_tests --parallel
rtk build/HostTests/Tests/boot_protocol_tests
rtk ctest --test-dir build/HostTests --output-on-failure
```

Expected: `boot_protocol_tests: PASS`; CTest 4/4 passed.

- [ ] **Step 5: Review and commit the parser API**

Run:

```sh
rtk proxy git diff --check
rtk git diff -- Protocol/Inc/boot_protocol_parser.h Protocol/Src/boot_protocol_parser.c Tests/boot_protocol_parser_tests.c
rtk git add Protocol/Inc/boot_protocol_parser.h Protocol/Src/boot_protocol_parser.c Tests/boot_protocol_parser_tests.c
rtk git commit -m "feat(protocol): accept transport byte chunks"
```

Expected: one commit containing only the chunk API and its host tests.

---

### Task 2: Move I2C RX draining to the BSP and main loop

**Files:**
- Modify: `Drivers/BSP/Inc/bsp_i2c_slave.h:45-62`
- Modify: `Drivers/BSP/Src/bsp_i2c_slave.c:82-115`
- Modify: `Protocol/Inc/boot_protocol_parser.h:60-75`
- Modify: `Protocol/Src/boot_protocol_parser.c:1-180`
- Modify: `Core/Src/main.c:19-155`
- Modify: `Tests/boot_protocol_parser_tests.c:10-280`

**Interfaces:**
- Consumes: `BootProtocolParserPushBytes()` from Task 1 and the existing I2C RX ring/state.
- Produces: `size_t bsp_i2c_slave_read(uint8_t* buffer, size_t capacity)`; a parser library with no BSP dependency.

- [ ] **Step 1: Record the failing dependency gate**

Run:

```sh
rtk rg -n '#include "bsp_|rxBuffer|SLAVE_|BootProtocolParserProcess' Protocol
```

Expected: matches in `Protocol/Src/boot_protocol_parser.c` and `Protocol/Inc/boot_protocol_parser.h`. This is the red architectural test.

- [ ] **Step 2: Add the BSP batch-read interface**

Add to `Drivers/BSP/Inc/bsp_i2c_slave.h` next to `bsp_i2c_slave_poll()`:

```c
size_t bsp_i2c_slave_read(uint8_t* buffer, size_t capacity);
```

Replace the public RX read helpers in the header with only `bsp_i2c_slave_read()`. Keep TX APIs unchanged.

In `Drivers/BSP/Src/bsp_i2c_slave.c`, make the byte-copy loop private and add the safe wrapper:

```c
static size_t rx_buffer_read(uint8_t* buffer, size_t capacity) {
    size_t bytes_read = 0U;
    while ((bytes_read < capacity) && (rx_transactions._BufferHead != rx_transactions._BufferTail)) {
        buffer[bytes_read++] = rx_transactions.data[rx_transactions._BufferTail];
        rx_transactions._BufferTail = (uint16_t)(rx_transactions._BufferTail + 1U) % BSP_I2C_RX_CAPACITY;
    }
    return bytes_read;
}

size_t bsp_i2c_slave_read(uint8_t* buffer, size_t capacity) {
    size_t bytes_read = 0U;
    if ((buffer == NULL) || (capacity == 0U))
        return 0U;
    const bsp_irq_state_t irq_state = bsp_enter_critical();
    if (slave_state == SLAVE_RX_DONE)
        bytes_read = rx_buffer_read(buffer, capacity);
    bsp_exit_critical(irq_state);
    return bytes_read;
}
```

Make the ISR-only RX byte writer private and remove `rxBufferAvailable()`, `rxBufferRead()` and `rxBufferReadBytes()` once no caller remains.

- [ ] **Step 3: Move the transport pump into `main.c`**

Add beside `protocol_parser`:

```c
static uint8_t transport_rx_buffer[BOOT_I2C_BUFFER_CAPACITY];
```

Replace `BootProtocolParserProcess()` in the loop with:

```c
const size_t received = bsp_i2c_slave_read(transport_rx_buffer, sizeof(transport_rx_buffer));
if (received > 0U) {
    (void)BootProtocolParserPushBytes(&protocol_parser, transport_rx_buffer, received);
    protocol_last_byte_ms = now_ms;
}
```

Keep the partial-frame timeout and `bsp_i2c_slave_poll()` recovery blocks in their current order after this code.

- [ ] **Step 4: Remove parser-side transport code and obsolete host mocks**

In `Protocol/Src/boot_protocol_parser.c`:

- Remove `#include "bsp_critical.h"` and `#include "bsp_i2c_slave.h"`.
- Remove `uint8_t local_buffer[1024];`.
- Delete `BootProtocolParserProcess()`.

Remove the matching declaration from `Protocol/Inc/boot_protocol_parser.h`.

In `Tests/boot_protocol_parser_tests.c`:

- Remove `mock_rx`, `mock_rx_length`, `mock_rx_index`, `rxBufferAvailable()` and `rxBufferRead()`.
- Rename `test_process_and_legacy_callback()` to `test_chunk_and_legacy_callback()`.
- Replace its mock RX setup and `BootProtocolParserProcess()` call with:

```c
assert(BootProtocolParserPushBytes(&parser, request, sizeof(request)) == 1U);
```

- [ ] **Step 5: Run the dependency and software gates**

Run:

```sh
rtk rg -n '#include "bsp_|rxBuffer|SLAVE_|BootProtocolParserProcess|I2C' Protocol
rtk cmake --preset HostTests
rtk cmake --build build/HostTests --clean-first --parallel
rtk ctest --test-dir build/HostTests --output-on-failure
rtk cmake --preset Debug
rtk cmake --build build/Debug --clean-first --parallel
rtk arm-none-eabi-size build/Debug/hc32f460_boot.elf
```

Expected:

- The dependency search returns no matches.
- Host CTest passes 4/4.
- Debug links without warnings.
- Debug BSS is lower than node 1 because the 1024-byte parser buffer was replaced by a 544-byte transport buffer.

- [ ] **Step 6: Review and commit the inbound boundary**

Run:

```sh
rtk proxy git diff --check
rtk git diff -- Drivers/BSP/Inc/bsp_i2c_slave.h Drivers/BSP/Src/bsp_i2c_slave.c Protocol/Inc/boot_protocol_parser.h Protocol/Src/boot_protocol_parser.c Core/Src/main.c Tests/boot_protocol_parser_tests.c
rtk git add Drivers/BSP/Inc/bsp_i2c_slave.h Drivers/BSP/Src/bsp_i2c_slave.c Protocol/Inc/boot_protocol_parser.h Protocol/Src/boot_protocol_parser.c Core/Src/main.c Tests/boot_protocol_parser_tests.c
rtk git commit -m "refactor(transport): move I2C RX polling out of parser"
```

Expected: a commit that changes inbound ownership only; no `BootUpdateService` TX changes.

---

### Task 3: Synchronize Phase 2 documentation and complete software verification

**Files:**
- Modify: `docs/current_status.md`
- Modify: `docs/i2c_slave_contract.md`
- Modify: `docs/portability_plan.md`

**Interfaces:**
- Consumes: completed Task 2 dependency boundary and test evidence.
- Produces: documentation that marks Phase 2 implemented with target HIL pending.

- [ ] **Step 1: Update documentation with the implemented boundary**

Record these exact facts:

- `Protocol/` accepts bytes/chunks and has no BSP/I2C dependency.
- `main.c` owns the fixed 544-byte transport RX buffer and orchestration order.
- `bsp_i2c_slave_read()` owns RX critical-section handling.
- Outbound reserve/write remains I2C-specific Phase 3 scope.
- Phase 2 software gates are green; target HIL is pending until Task 4.

- [ ] **Step 2: Run all firmware and host builds from a clean target**

Run:

```sh
rtk cmake --preset Debug
rtk cmake --build build/Debug --clean-first --parallel
rtk cmake --preset Release
rtk cmake --build build/Release --clean-first --parallel
rtk cmake --preset ReleaseNoLog
rtk cmake --build build/ReleaseNoLog --clean-first --parallel
rtk cmake --preset HostTests
rtk cmake --build build/HostTests --clean-first --parallel
rtk ctest --test-dir build/HostTests --output-on-failure
```

Expected: all builds exit zero without warnings; CTest 4/4 passed.

- [ ] **Step 3: Verify ICG and artifact metadata**

Run:

```sh
rtk arm-none-eabi-objdump -h build/Debug/hc32f460_boot.elf build/Release/hc32f460_boot.elf build/ReleaseNoLog/hc32f460_boot.elf | rtk rg '\.icg_sec'
rtk arm-none-eabi-objdump -s -j .icg_sec build/Debug/hc32f460_boot.elf
rtk arm-none-eabi-objdump -s -j .icg_sec build/Release/hc32f460_boot.elf
rtk arm-none-eabi-objdump -s -j .icg_sec build/ReleaseNoLog/hc32f460_boot.elf
rtk sha256sum build/Debug/hc32f460_boot.bin build/Debug/hc32f460_boot.elf
```

Expected: all three sections are at `0x00000400`, size `0x20`, with words `FFDFFFBF FFFFFEFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF`. Record the new Debug hashes for Task 4.

- [ ] **Step 4: Perform the scoped semantic review**

Review:

- Chunk API null/zero/multiple-frame behavior.
- RX ring critical section and partial-read behavior.
- Main-loop timeout/recovery ordering.
- Absence of parser-to-BSP dependencies.
- No changes to TEI/NACK/STOP TX behavior or update-service ownership.

Run:

```sh
rtk proxy git diff --check
rtk git status --short
```

- [ ] **Step 5: Commit software documentation**

Run:

```sh
rtk git add docs/current_status.md docs/i2c_slave_contract.md docs/portability_plan.md
rtk git commit -m "docs: record phase2 transport boundary"
```

Expected: documentation states software implementation complete and HIL pending.

---

### Task 4: Repeat the node 1 400 kHz HIL and close Phase 2

**Files:**
- Local evidence only: `debug_artifacts/jlink/`
- Modify after pass: `docs/current_status.md`
- Modify after pass: `docs/portability_plan.md`

**Interfaces:**
- Consumes: Task 3 Debug BIN hash and `Tools/jlink/test_board_*` workflows.
- Produces: target evidence that the transport-boundary refactor preserved the exact wire behavior.

- [ ] **Step 1: Create state-change preflights**

Create local `safety_preflight` YAML records for:

1. Flashing the exact Debug BIN at `0x00000000`.
2. Fresh-session verification.
3. One reset/run.
4. One exact I2C write/read transaction.
5. One post-read halt/read/resume capture.

Record board, J-Link serial `20781318`, SWD 100 kHz, artifact SHA-256, BIN size/range, I2C address `0x11`, 400 kHz stimulus and recovery plan. Never unlock, recover or mass erase.

- [ ] **Step 2: Flash, independently verify and run**

Run from the repository root using separate J-Link sessions:

```sh
rtk JLinkExe -NoGui 1 -ExitOnError 1 -CommandFile Tools/jlink/test_board_flash_debug.jlink
rtk JLinkExe -NoGui 1 -ExitOnError 1 -CommandFile Tools/jlink/test_board_verify_debug.jlink
rtk JLinkExe -NoGui 1 -ExitOnError 1 -CommandFile Tools/jlink/test_board_reset_run.jlink
```

Expected: selected S/N `20781318`, VTref near 3.35 V, successful program and two successful BIN verifications, then target running.

- [ ] **Step 3: Execute the exact I2C transaction**

At 400 kHz, write to seven-bit address `0x11` with STOP:

```text
AA 44 18 01 FE 0C 00 20 11 00 00 00 00 00 00 00 00 00 00 D4 E3
```

Wait 5-20 ms, then perform a separate 21-byte read. Expected:

```text
AA 44 18 01 FE 0C 00 20 00 00 00 00 00 00 00 00 00 00 00 A0 6E
```

Stop without retry/reset if the write, read, length or payload differs. Preserve the failed attempt before diagnosis.

- [ ] **Step 4: Capture post-read state**

First confirm the script addresses against the current ELF:

```sh
rtk arm-none-eabi-nm -n -S build/Debug/hc32f460_boot.elf | rtk rg 'i2c_slave_counters_stats|tx_transaction_started_bytes|tx_transactions|m_apfnIrqHandler'
```

Update `Tools/jlink/test_board_capture_i2c1_postread.jlink` only if the symbols moved, then run it with an approved preflight. Expected counters: RX/TX address match `1/1`, RX/TX bytes `21/21`, completed reads `1`, response buffer released, no overflow/empty-read/busy/arbitration/recovery event, `PRIMASK=0`, no exception.

- [ ] **Step 5: Close Phase 2 documentation and commit**

After a passing HIL, change Phase 2 from implemented/HIL-pending to closed with target evidence in `docs/current_status.md` and `docs/portability_plan.md`. Record the Debug BIN SHA and exact response.

Run:

```sh
rtk proxy git diff --check
rtk git add docs/current_status.md docs/portability_plan.md
rtk git commit -m "docs: close phase2 transport boundary"
```

Expected: Phase 2 branch contains the design, parser chunk API, inbound transport boundary, tests, software evidence and target closure documentation.

---

### Task 5: Final branch verification

**Files:**
- Read-only verification of the complete branch.

**Interfaces:**
- Consumes: all Phase 2 commits and target evidence.
- Produces: a clean branch ready for review/integration decision.

- [ ] **Step 1: Run the final software gate**

Run:

```sh
rtk cmake --build build/Debug --parallel
rtk cmake --build build/Release --parallel
rtk cmake --build build/ReleaseNoLog --parallel
rtk ctest --test-dir build/HostTests --output-on-failure
rtk proxy git diff 16f72f5..HEAD --check
```

Expected: all commands exit zero and CTest reports 4/4 passed.

- [ ] **Step 2: Verify branch history and cleanliness**

Run:

```sh
rtk git status --short
rtk git log --oneline --decorate 16f72f5..HEAD
rtk git diff --stat 16f72f5..HEAD
```

Expected: clean worktree and a reviewable sequence of design, parser API, boundary, docs and HIL closure commits.
