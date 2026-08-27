# AVR Emulator

This project is a phased, AVR-inspired emulator written in C.

The goal is to build a small but correct CPU model incrementally, validating
each layer with tests before adding more hardware behavior. The current
implementation focuses on instruction execution semantics, deterministic state,
and a clear fetch/decode/execute pipeline.

## Project Scope

- Language: C17
- Build system: Make
- Test style: assert-based unit and integration tests
- Emulation strategy: implement only a narrow subset per phase, then expand

Out of scope for current phases includes full peripheral emulation,
interrupt/timing fidelity, binary formats (ELF/HEX), and GUI tooling.

## Phase 1 Summary

Phase 1 establishes the minimal CPU core and instruction semantics.

Implemented in Phase 1:

- CPU state model with:
  - 32 general-purpose registers (`R0` to `R31`)
  - program counter (`PC`)
  - status register (`SREG`)
- Deterministic reset behavior for CPU state
- Basic CPU access APIs (registers, `PC`, `SREG`)
- Direct instruction execution support for:
  - `LDI`
  - `MOV`
  - `ADD`
  - `SUB`
  - `INC`
- Arithmetic flag behavior validation (`C`, `Z`, `N`, `V`, `S`, `H`) with
  preservation rules for unaffected bits

Key result:
Phase 1 proves that instruction logic and `SREG` side effects are correct even
before adding machine-code fetch and decode.

## Phase 2 Summary

Phase 2 moves execution from manually constructed instructions to encoded
machine code in simulated Flash.

Implemented in Phase 2:

- Simulated Flash memory of 16-bit instruction words
- Program loading API for encoded instruction arrays
- Instruction encoder for the Phase 1 subset
- Instruction decoder for the Phase 1 subset
- CPU step pipeline:
  - fetch word from `Flash[PC]`
  - decode into instruction form
  - execute instruction
  - advance `PC` only on success
- Failure behavior for unsupported/unknown instruction words

Key result:
Phase 2 proves the end-to-end path from AVR-style machine words to CPU state
mutation through fetch/decode/execute.

## Phase 3 Summary

Phase 3 adds a bounded data-memory path using SRAM and X-pointer indirect
access.

- Fixed SRAM size: `AVR_SRAM_SIZE` = 256 bytes.
- Addressing model (Phase 3): zero-based SRAM indexing only.
- X pointer: `X = (R27 << 8) | R26`.
- Supported memory instructions:
  - `LD Rd, X`
  - `ST X, Rr`
- Instruction width: 16-bit words only.

Current scope intentionally does not implement AVR I/O space or GPIO.
Memory addresses in this phase are interpreted only as indices into the
simulated SRAM array.

Not implemented yet:

- `IN`/`OUT`, memory-mapped I/O, GPIO peripherals
- Y/Z pointer modes and displacement forms
- `LDS`/`STS` and 32-bit instructions

## Phase 4 Summary

Phase 4 adds a small symbolic I/O boundary and an 8-bit GPIO port.

- `AVR_IO_PINB` is the read-only actual pin state register.
- `AVR_IO_DDRB` controls which port bits are outputs.
- `AVR_IO_PORTB` stores the output latch.
- `PINB` reads output-configured bits from `PORTB` and input-configured bits
  from the externally supplied input state.
- CPU writes to `PINB` are rejected; `DDRB` and `PORTB` are writable.
- `IN` and `OUT` use 16-bit instruction encodings and the symbolic I/O IDs.

The I/O IDs are compact emulator identifiers, not real ATmega328P addresses.
Phase 4 does not implement a complete AVR data-space map, PIN toggle writes,
timers, interrupts, or a visualizer.

## Phase 5 Summary

Phase 5 adds control flow and looping while keeping execution deterministic and
headless.

- `RJMP` with signed 12-bit word-relative offsets from -2048 to 2047.
- `BRNE` and `BREQ` with signed 7-bit word-relative offsets from -64 to 63.
- Taken branches use `PC + 1 + offset`; untaken branches use `PC + 1`.
- `DEC` with AVR-style `Z`, `N`, `V`, and `S` flag behavior while preserving
  `I`, `T`, `H`, and `C`.
- Control-flow failures leave the program counter and machine state unchanged.

All Phase 5 instructions remain 16-bit words. The conditional branch subset is
encoded using the AVR `BRBC`/`BRBS` Z-bit forms (`BRNE`/`BREQ` aliases).

## Phase 6 Summary

Phase 6 adds the logical, comparison, and bit operations needed by small
input-driven programs.

- Register operations: `AND`, `OR`, and `EOR`.
- Comparisons: `CP` for two registers and `CPI` for `R16` through `R31`.
- I/O bit operations: `SBI` and `CBI` for I/O IDs 0 through 31 and bits 0
  through 7.
- Logical operations update `Z`, `N`, and `S` and clear `V`; `CP` and `CPI`
  update the subtraction flags without changing registers; `SBI` and `CBI`
  preserve `SREG`.

`SBI` and `CBI` use the existing symbolic I/O API, so unsupported or read-only
registers fail without changing the MCU.

## Phase 7 Summary

Phase 7 adds a stack and subroutine calls on top of the existing SRAM model.

- A dedicated stack pointer (`sp`) tracks the address of the next free SRAM
  byte below the stack top; it resets to `AVR_SRAM_SIZE` (empty stack).
- `PUSH` pre-decrements `sp` then writes; `POP` reads then post-increments
  `sp`. Overflow (`sp == 0`) and underflow (`sp == AVR_SRAM_SIZE`) are
  detected before any mutation, so failures leave the MCU unchanged.
- `CALL` is a simplified single-word instruction with a 10-bit embedded
  absolute target (`AVR_FLASH_SIZE` fits in 10 bits), which keeps the
  existing one-word fetch/decode pipeline unchanged. This is a deliberate
  deviation from real AVR's two-word `CALL` encoding.
- `PUSH`, `POP`, and `RET` use the real AVR opcodes. The return address
  pushed by `CALL` is `pc + 1`, stored high byte first, and `RET` reconstructs
  it and advances `sp` back past both bytes.
- Nested `CALL`/`RET` sequences are covered by an encoded machine-code
  regression test, verifying `sp` returns to its initial value after matching
  returns.

## Phase 8 Summary

Phase 8 adds a practical program-loading path without changing the data-space
model.

- The addressing model is unchanged: registers, the symbolic I/O IDs, and
  SRAM remain separate namespaces, and there is still only an X pointer (no
  Y/Z or post-increment/pre-decrement). The unified AVR data-space map and
  extra pointer forms are deferred until a concrete example program needs
  them.
- `avr_mcu_load_intel_hex` loads a subset of the Intel HEX format (data
  records `00` and the end-of-file record `01`) directly into Flash.
  Extended segment/linear address records are rejected as unsupported,
  since Flash is small enough that every byte address fits in 16 bits.
- Every record's structure and checksum is validated against a staged copy
  of Flash before anything is written, so a malformed image (bad checksum,
  unsupported record type, truncated data, records after `EOF`, a missing
  `EOF` record, or an out-of-range address) leaves the MCU's real Flash
  completely unchanged.
- The loader only overwrites the bytes named by its data records, so it can
  be combined with `avr_mcu_load_program` or applied incrementally without
  clearing the rest of Flash.
- Loader output is tested for byte-for-byte equivalence with the existing
  C-array `avr_mcu_load_program` path.

## Build and Test

- Build emulator: `make build`
- Run test suite: `make test`
- Run emulator demo: `make run`
