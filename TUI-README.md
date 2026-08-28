# AVR Emulator TUI

## What It Does

The AVR Emulator TUI is an interactive terminal frontend for the project's
headless AVR-inspired CPU emulator. It lets you watch a small program execute
one instruction at a time or continuously, inspect the complete visible MCU
state, change external GPIO input, and pause at breakpoints.

Start it with:

```sh
make tui
```

The `tui` target builds `build/avr-tui` and requires the `ncurses` library.
Normal commands such as `make build` and `make test` do not compile, link, or
depend on `ncurses`.

When launched, the TUI loads its bundled ten-instruction demonstration program.
The program configures `PB5` as an output, reads virtual input `PB0`, and then
drives `PB5` high when `PB0` is high or low when `PB0` is low. It repeats this
loop forever. This gives the screen a small but complete input-to-program-to-
GPIO-output circuit to explore.

The screen contains these areas:

- **Status line:** indicates whether execution is `RUN` or `PAUSED` and shows
  the most recent action or stop reason.
- **Current instruction:** shows the word-addressed program counter (`PC`),
  current 16-bit machine word, decoded instruction, and whether a breakpoint
  is set at that `PC`.
- **CPU state:** shows the stack pointer (`SP`), abstract cycle counter, raw
  `SREG` value, and individual flags in AVR order: `I T H S V N Z C`.
- **Registers:** shows all 32 eight-bit general-purpose registers (`R0` through
  `R31`).
- **SRAM:** shows bytes `0x00` through `0x3F`, the first 64-byte inspection
  window of the emulator's 256-byte SRAM.
- **GPIO state:** shows `DDRB`, `PORTB`, `PINB`, external `INPUT`, and one
  indicator for every `PB7` through `PB0` pin.

Changed register and SRAM values are reverse-highlighted after an action.
GPIO bits that are high use the terminal's green color pair when available;
bits configured as outputs are bold. The current instruction line is always
reverse-highlighted so the next instruction is easy to locate.

## How It Works

The TUI is deliberately a separate executable. It does not implement
instructions, manipulate MCU fields directly, or change the core's timing
rules. Its code is in `src/ui/avr_tui.c`; the CPU, debugger, and tests remain
usable without a terminal UI.

At startup, the frontend constructs the demo instruction definitions, encodes
them with `avr_encode_instruction`, and loads the resulting machine words with
`avr_mcu_load_program`. Reset creates a fresh MCU through `avr_mcu_reset` and
then loads the same encoded demo again, because reset clears Flash along with
the rest of the simulated MCU state.

For rendering, the TUI asks `avr_debug_snapshot` for a read-only `AvrSnapshot`.
That snapshot includes the `PC`, fetched word, decoded instruction, registers,
SRAM, SREG, stack and cycle state, GPIO registers, computed pin state, external
input, and the breakpoint marker. `avr_debug_format_instruction` turns the
decoded instruction into the disassembly displayed on the instruction line.
The TUI stores the preceding snapshot and compares it with the new one to
highlight changed register and SRAM values.

Single-step execution calls `avr_debug_step_with_events`. That function performs
one regular MCU step and returns an event log describing the state changes; the
TUI currently reports the event count in its status line. Continuous execution
uses `avr_mcu_run` in batches of 64 abstract cycles. The core still advances by
one deterministic abstract cycle per successfully executed instruction. The TUI
does not add wall-clock time to the emulator; terminal polling merely decides
when another bounded batch is requested.

The GPIO display follows the emulator's symbolic Port B model:

- `DDRB` selects output bits. A set bit makes the matching PB pin an output.
- `PORTB` is the output latch written by the running program.
- `INPUT` is the external state supplied by the user with keys `0` through `7`.
- `PINB` is the actual observed pin value. Output-configured bits come from
  `PORTB`; input-configured bits come from `INPUT`.

For each pin indicator, `*` means the computed `PINB` bit is high and `.` means
it is low. The bold style shows that `DDRB` configured that pin as an output.
Consequently, the demo's PB5 indicator is bold throughout execution, while PB0
remains an externally controlled input.

Breakpoints are managed using `avr_mcu_set_breakpoint`,
`avr_mcu_clear_breakpoint`, and `avr_mcu_has_breakpoint`. The core's run API
stops before executing a breakpoint encountered after the current run begins.
This allows a paused program to resume from a breakpoint at its current `PC`;
it will stop when it loops back to that address later.

## Tutorial

### 1. Build and Launch

From the repository root, run:

```sh
make tui
```

The terminal switches into the full-screen interface and begins paused at
`PC 0000`. Use `q` at any time to exit cleanly and restore the terminal.

### 2. Read the Initial State

At reset, the current instruction is `LDI R16, 32`. The next three instructions
configure the demo:

```text
0000  LDI R16, 32
0001  OUT DDRB, R16
0002  LDI R18, 1
```

After these run, `R16` holds `0x20`, so `OUT DDRB, R16` configures bit 5,
PB5, as an output. `R18` holds `0x01`, the mask used to isolate PB0. The `DDRB`
row displays `20 00100000`; the one at bit 5 means PB5 is the output pin.

Press `s` three times to execute this setup. Watch the instruction line advance,
the cycle counter increase by one on every successful step, and the changed
register values appear highlighted.

### 3. Follow a Low Input Through the Program

By default, external `INPUT` is `00`, so PB0 is low. Continue stepping with `s`:

```text
0003  IN R17, PINB
0004  AND R17, R18
0005  BREQ +2 (0x0008)
```

`IN` reads the current pin state into `R17`; `AND` preserves only PB0. With PB0
low, `R17` becomes zero and sets the Z flag. The `BREQ` at `PC 0005` is therefore
taken and lands at `PC 0008`.

Step once more to execute `CBI PORTB, 5`. `PORTB` and `PINB` remain low, and the
PB5 indicator stays `.`. The following `RJMP` returns to `PC 0003` to sample the
input again.

### 4. Turn PB5 On With PB0

Press `0` to toggle external input PB0. The `INPUT` row changes from `00` to
`01`, and the rightmost pin indicator, PB0, becomes `*`. PB0 is not an output,
so it is not bold.

Now step through the loop again. At the `AND`, `R17` becomes `01`, so Z is
clear. The `BREQ` is not taken; execution continues to:

```text
0006  SBI PORTB, 5
0007  RJMP +1 (0x0009)
```

After stepping `SBI`, `PORTB` displays `20` and PB5's indicator becomes a bold,
green `*` when terminal colors are available. It is bold because PB5 is an
output and lit because the program set its output latch. Press `0` again and
walk through the next loop to see the `CBI` path turn PB5 back off.

### 5. Run Continuously

Press `r` to switch from `PAUSED` to `RUN`. The TUI executes batches of 64
instructions and redraws between batches. The demo loop repeats rapidly while
remaining deterministic inside the core. Press `r` again to pause it.

You can toggle PB0 with `0` while running. The program observes the new input on
its next pass through `IN R17, PINB`, then takes the set or clear path for PB5.

### 6. Stop on a Breakpoint

While paused, use `s` until the current `PC` is `0003`, the `IN R17, PINB`
instruction. Press `b` to set a breakpoint there. The instruction line includes
`BREAKPOINT` to confirm it.

Press `r`. The program runs forward through the loop. When it returns to
`PC 0003`, execution pauses and the status line reports `Stopped: breakpoint`.
Pressing `r` again resumes from that instruction, which lets you inspect or step
through the next loop iteration. Press `b` at the same address to remove the
breakpoint.

### 7. Reset and Exit

Press `x` to discard all current CPU state, GPIO input, breakpoints, SRAM, and
cycle count, then reload the bundled demo. The UI returns to paused `PC 0000`.

Press `q` to exit. The TUI calls `endwin`, restoring normal terminal input and
screen behavior.

## Key Reference

| Key        | Action                                                        |
| ---------- | ------------------------------------------------------------- |
| `s`        | Pause and execute one instruction.                            |
| `r`        | Toggle continuous run and pause.                              |
| `x`        | Reset the MCU and reload the bundled demo.                    |
| `b`        | Toggle a breakpoint at the current `PC`.                      |
| `0` to `7` | Toggle the corresponding external input bit, PB0 through PB7. |
| `q`        | Quit and restore the terminal.                                |
| ---------- | ------------------------------------------------------------- |
