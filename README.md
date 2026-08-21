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

## Build and Test

- Build emulator: `make build`
- Run test suite: `make test`
- Run emulator demo: `make run`
