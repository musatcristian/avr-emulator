## Plan: Tiny Visual AVR Emulator Master Roadmap

The project will remain a headless, testable AVR-inspired emulator while it grows from the current Phase 4 GPIO core into a machine capable of running small interactive programs. The visual layer is intentionally near the end: first establish control flow, enough instruction coverage, stable memory/timing contracts, and an observation boundary. Add a TUI as the first visual frontend, then evaluate SDL as an optional graphical frontend once the same emulator-facing interface is sufficient.

**Steps**

### Phase 5: Control Flow and Looping

1. Add relative control flow beginning with `RJMP`, then conditional branches (`BRBS`/`BRBC` or a deliberately smaller `BRNE`/`BREQ` subset) using signed PC-relative word offsets.
2. Add the minimum loop-support instruction missing from the current core, preferably `DEC`, with correct SREG behavior; keep branch aliases/documentation consistent with the encoded operation model.
3. Define PC semantics precisely: a taken branch uses the current instruction address plus one plus the signed displacement; a not-taken branch advances by one; failures leave PC and all state unchanged.
4. Validate exact encodings, signed boundary offsets, forward and backward jumps, taken/not-taken branches, invalid operands, and an encoded decrement/branch loop that terminates.
5. Keep the existing fetch/decode/execute ownership in `avr_mcu_step` and `avr_execute_instruction`; do not introduce a UI or timing dependency here.

### Phase 6: Essential Tiny-Program Instruction Set

1. Add only instructions that materially improve small programs: `AND`, `OR`, `EOR`, `CPI`/`CP` as needed for decisions, and useful bit operations such as `SBI`/`CBI` if the encoding and I/O contract remain clear.
2. Centralize flag calculation by instruction family and test every affected and preserved SREG bit, including GPIO-oriented bit manipulation.
3. Prefer a coherent small subset over a long opcode list. Defer multiplication, obscure addressing forms, and instructions that do not support the target tiny-program examples.
4. Add integration programs for input testing, masking, conditional GPIO output, and repeated bit patterns.

### Phase 7: Stack and Subroutines (After the Small-Core Goal Is Proven)

1. Decide and document stack representation before implementation. Recommended direction: model a dedicated `sp` field with explicit bounds and APIs for clarity, while documenting that real AVR hardware uses a stack pointer register pair; avoid silently aliasing general registers unless AVR compatibility becomes a primary requirement.
2. Add `PUSH`, `POP`, `CALL`, and `RET` only after 32-bit instruction and PC-advance semantics are specified. Define return-address byte order, stack growth direction, reset value, overflow/underflow behavior, and failure atomicity.
3. Add nested call/return and encoded subroutine tests. Keep stack behavior independent from the future UI.

### Phase 8: Data Memory and Program Loading Foundations

1. Decide whether the project needs the full AVR data-space map for its stated tiny-program goal. Recommended first step: retain the current bounded SRAM and symbolic I/O APIs while adding Y/Z and post-increment/pre-decrement only if example programs require them.
2. If realistic AVR programs become a goal, introduce a unified address-space abstraction in a compatibility-preserving way, with explicit ranges and tests for register, I/O, and SRAM aliases. Do not make this refactor a prerequisite for the first visual milestone.
3. Add a practical program input path. Recommended order: keep C-defined instruction arrays as a test fixture, then add a small human-readable program format or Intel HEX loader; defer ELF/DWARF until the instruction set and address model are stable.
4. Add tests that prove loader output is identical to directly constructed machine-code programs and that malformed input fails without partial mutation.

### Phase 9: Deterministic Timing and Execution Control

1. Introduce a cycle/instruction counter and define whether each supported instruction consumes one abstract cycle or has a small documented latency table. Recommended first contract: deterministic abstract cycles, with realistic latencies deferred.
2. Add an explicit execution API suitable for both headless tools and future frontends: single-step, bounded run, stop condition, reset, and optional breakpoint/watchpoint callbacks or status results.
3. Keep wall-clock scheduling out of the emulator core. A frontend may map abstract cycles to real time; tests must be able to drive execution deterministically.
4. Test counter updates, run limits, breakpoints, invalid-instruction stops, and equivalence between repeated `step` calls and bounded execution.

### Phase 10: Emulator Observation and Debugging Boundary

1. Add a read-only snapshot or inspection API that exposes PC, current/fetched word, decoded instruction, registers, SREG bits, SRAM window, GPIO registers/pins, execution counters, and stop/error status without requiring the UI to reach into internal struct fields.
2. Add a small instruction formatting/disassembly helper for the supported subset, including register names, symbolic I/O names, immediates, and relative targets.
3. Add execution-event reporting for at least instruction start/completion, register or SREG changes, memory/I/O writes, and GPIO pin changes. Prefer a caller-owned event/snapshot structure over hard-wiring callbacks into instruction semantics.
4. Test that the observation layer is accurate, read-only, deterministic, and does not alter emulator results. This phase is the contract that lets TUI and SDL remain separate from the emulator.

### Phase 11: First Visual Frontend, TUI

1. Build a separate TUI target using `ncurses` or an equivalent terminal library, while preserving the normal headless build and test targets.
2. Implement the smallest complete workflow: load the demo or a supported program, reset, single-step, run/pause, stop on invalid instruction or breakpoint, and quit cleanly.
3. Render the current instruction and machine word, PC, selected/all registers, SREG flags, SRAM inspection, `DDRB`/`PORTB`/`PINB`, external input, and eight GPIO pin states. Highlight changed values and the currently executing instruction.
4. Add clickable-equivalent keyboard controls for external input bits and breakpoint management if the terminal environment supports them; otherwise provide explicit key controls.
5. Verify that running the same program headlessly and through the TUI produces identical state at every step. Keep UI tests focused on formatting/input behavior where possible and leave CPU correctness to existing unit/integration tests.

### Phase 12: Optional SDL Visualizer

1. Add SDL only after the TUI and observation boundary demonstrate a useful interactive workflow. Reuse the same emulator execution and snapshot/event APIs; do not fork CPU behavior for graphics.
2. Implement a focused graphical scene: CPU/register panel, current instruction, GPIO register bitfields, eight LEDs or pin indicators, and eight clickable external input switches.
3. Add run/pause/step/reset, speed control based on abstract cycles, breakpoint/stop indication, and visible event history. A program should be able to read a clicked input and drive a visible output through the real GPIO model.
4. Keep SDL optional in the Makefile so core builds remain dependency-light. Validate macOS build/link instructions separately from headless CI.
5. Treat richer animation, source mapping, multi-port boards, and polished educational diagrams as follow-on work, not prerequisites for the first usable visual emulator.

### Advanced Hardware, Deferred Beyond the First Visual Milestone

1. Add timers/PWM only if time-based behavior is needed for a concrete demonstration such as LED blinking; expose deterministic `tick` APIs before connecting them to wall-clock UI time.
2. Add interrupts only after stack, timing, and event semantics are stable. Begin with manually injected interrupts and simple vector dispatch; defer nested priorities and peripheral-generated interrupts.
3. Consider UART, additional GPIO ports, SPI, ADC, EEPROM, watchdogs, fuses, and full ATmega328P compatibility as separate optional milestones.

**Relevant files**

- `/Users/2305922/Code/C/avr-emulator/include/avr_instruction.h` — extend the operation and operand model per phase; avoid overloading fields ambiguously as 32-bit instructions arrive.
- `/Users/2305922/Code/C/avr-emulator/src/instructions/avr_instruction.c` — encoder/decoder/executor ownership for control flow, essential instructions, stack behavior, and flag semantics.
- `/Users/2305922/Code/C/avr-emulator/include/avr_mcu.h` — MCU state, memory/timing APIs, execution status, and eventually read-only inspection/snapshot types.
- `/Users/2305922/Code/C/avr-emulator/src/mcu/avr_mcu.c` — reset, memory, PC/stack behavior, bounded execution, timing, and event coordination.
- `/Users/2305922/Code/C/avr-emulator/src/main/main.c` — retain as a headless demo/reference runner; later add explicit mode selection without embedding UI logic in the core.
- `/Users/2305922/Code/C/avr-emulator/tests/test_phase5.c` through future `tests/test_phaseN.c` — phase-scoped assert tests following the existing organization.
- `/Users/2305922/Code/C/avr-emulator/tests/test_runner.c` — register each phase's tests while keeping them UI-independent.
- `/Users/2305922/Code/C/avr-emulator/Makefile` — add phase targets and optional TUI/SDL dependency/source selection without making graphical libraries mandatory for `make test`.
- `/Users/2305922/Code/C/avr-emulator/README.md` — document the supported subset, address/timing assumptions, program format, frontend commands, and explicit non-goals.
- New `include/avr_debug.h` and `src/debug/` or an equivalent local module — recommended location for snapshots, disassembly, and execution events once Phase 10 begins.
- New `src/ui/` and frontend headers — TUI first, SDL second; these consume public emulator/debug APIs and do not implement hardware semantics.

**Verification**

1. At every phase, run `make test` and `make build` with the existing C17 warning flags; add the new phase test file to the runner before considering the phase complete.
2. For each instruction, test direct execution, exact machine-word encoding, decoding, encode/decode round trips, invalid operands, and failure atomicity where applicable.
3. For control flow, run a real encoded loop with a finite termination condition and test both forward and backward PC-relative offsets.
4. For timing and execution control, compare bounded execution against repeated single-step execution and verify deterministic stop reasons.
5. Before UI work, run a headless observation/snapshot test that proves the frontend can reconstruct the displayed state without reading private implementation details.
6. For the TUI, manually verify reset, step, run/pause, breakpoint, input toggling, GPIO output changes, invalid-program handling, and clean terminal restoration; keep the headless test suite passing.
7. For SDL, manually verify that clicking an input changes the emulator's external input, that the program can observe it through `PINB`, and that `PORTB`/`DDRB` changes drive visible output indicators. Verify optional builds on macOS without changing the core build.
8. Maintain at least one end-to-end tiny program as a regression fixture: configure PB5 as output, read a virtual input, conditionally drive PB5, loop, and expose the same behavior in headless, TUI, and SDL modes.

**Decisions**

- The primary goal is a small AVR-inspired emulator that runs tiny programs, not immediate full ATmega328P compatibility.
- Phase 5 starts with control flow because loops and decisions are the largest current blocker to meaningful programs.
- The first visual milestone is a TUI in Phase 11; SDL is optional Phase 12 and reuses the same frontend-neutral APIs.
- The emulator core remains usable without any visual dependency. UI timing is external; emulator execution is deterministic.
- A read-only snapshot/event boundary precedes visual work so frontends do not depend on mutable internal fields.
- TUI/SDL are intentionally deferred until the core can show loops, input/output reactions, and stable execution state; timers and interrupts are not prerequisites for the first visual milestone.
- Advanced AVR fidelity is opt-in and sequenced by concrete educational value rather than completeness.

**Scope boundaries**

- Included through the first visual milestone: control flow, essential instruction support, optional stack/subroutines, practical program loading, deterministic execution control, observation APIs, TUI, and an optional SDL frontend.
- Explicitly excluded from the initial master goal: full AVR instruction coverage, exact ATmega328P memory/peripheral compatibility, ELF/DWARF, cycle-perfect timing, timers, interrupts, UART, SPI, ADC, EEPROM, watchdogs, fuses, and a polished multi-peripheral simulator.
- The repository's existing Phase 1-4 behavior and assert-based test style remain regression constraints.
