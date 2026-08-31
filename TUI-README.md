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
- **Current instruction:** shows the current line number (decimal, with the
  hex flash address in parentheses), the decoded instruction, a one-sentence
  plain-English explanation of what it does, and (de-emphasized) the raw
  16-bit machine word and breakpoint status.
- **Processor state:** shows the stack pointer and abstract cycle counter in
  decimal (hex in parentheses), and all 8 `SREG` flags spelled out by full
  name with an explicit `ON`/`OFF` state.
- **Registers:** shows all 32 eight-bit general-purpose registers (`R0` through
  `R31`) in hex.
- **Memory (SRAM):** shows bytes 0-63 (`0x00`-`0x3F`), the first 64-byte
  inspection window of the emulator's 256-byte SRAM, in hex.
- **GPIO:** a single table with one row per pin, `PB7` through `PB0`, showing
  each pin's Direction (`Input`/`Output`) and Level (`High`/`Low`) in words,
  plus a de-emphasized raw-register reference line and a legend.

Changed register and SRAM values are reverse-highlighted after an action; SRAM
bytes that are still zero are dimmed so nonzero bytes stand out. GPIO levels
that are high use the terminal's green color pair when available; pins
configured as outputs are bold. The current instruction line is always
reverse-highlighted so the next instruction is easy to locate.

The layout needs at least **100 columns by 40 rows**. If the terminal is
smaller, the TUI shows a `Terminal too small` message with the current and
required size instead of drawing a cramped or clipped screen, and resumes
normal rendering as soon as the terminal is large enough.

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
decoded instruction into the disassembly shown on the instruction line, and
`avr_debug_explain_instruction` turns the same decoded instruction into the
plain-English sentence shown beneath it. `avr_debug_flag_name` supplies the
full name for each `SREG` bit in the processor state panel. The TUI stores the
preceding snapshot and compares it with the new one to highlight changed
register and SRAM values.

Each frame, the TUI reads the current terminal size with `getmaxyx` and lays
out every panel relative to that size rather than fixed coordinates. Below the
100x40 minimum, it draws only the resize message described above.

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
- The external input state is supplied by the user with keys `0` through `7`.
- `PINB` is the actual observed pin value. Output-configured bits come from
  `PORTB`; input-configured bits come from the external input state.

Rather than showing those four registers as four separate rows of raw bits,
the GPIO table combines them into one row per pin: Direction reads `Output`
when the matching `DDRB` bit is set, else `Input`; Level reads `High` when the
computed `PINB` bit is set, else `Low`. A `Raw:` line below the table still
shows the underlying `DDRB`/`PORTB`/`PINB`/input hex values for reference.
Consequently, the demo's PB5 row shows `Output` throughout execution, while
PB0 shows `Input` and its Level tracks the externally toggled state.

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
line 0. Use `q` at any time to exit cleanly and restore the terminal.

### 2. Read the Initial State

At reset, the current instruction is `LDI R16, 32`. The next three instructions
configure the demo:

```text
0000  LDI R16, 32
0001  OUT DDRB, R16
0002  LDI R18, 1
```

After these run, `R16` holds `0x20`, so `OUT DDRB, R16` configures bit 5,
PB5, as an output. `R18` holds `0x01`, the mask used to isolate PB0. The GPIO
table's `PB5` row now shows Direction `Output`.

Press `s` three times to execute this setup. Watch the instruction line advance,
the cycle counter increase by one on every successful step, and the changed
register values appear highlighted.

### 3. Follow a Low Input Through the Program

By default, the external input is `00`, so PB0 is low. Continue stepping with `s`:

```text
Line 3  IN R17, PINB
Line 4  AND R17, R18
Line 5  BREQ +2 (0x0008)
```

`IN` reads the current pin state into `R17`; `AND` preserves only PB0. With PB0
low, `R17` becomes zero and sets the Zero flag. The plain-English sentence on
the `BREQ` at line 5 explains it is taken because the last result was zero, and
it jumps to line 8.

Step once more to execute `CBI PORTB, 5`. `PORTB` and `PINB` remain low, and the
PB5 row's Level stays `Low`. The following `RJMP` returns to line 3 to sample
the input again.

### 4. Turn PB5 On With PB0

Press `0` to toggle external input PB0. The GPIO table's `PB0` row changes
Level from `Low` to `High`. PB0's Direction stays `Input`, so it is not bold.

Now step through the loop again. At the `AND`, `R17` becomes `01`, so the Zero
flag is `OFF`. The `BREQ` is not taken; execution continues to:

```text
Line 6  SBI PORTB, 5
Line 7  RJMP +1 (0x0009)
```

After stepping `SBI`, the `PB5` row's Level becomes `High`, shown in green when
terminal colors are available, and its Direction remains `Output` (bold). It is
green because the program set its output latch high. Press `0` again and walk
through the next loop to see the `CBI` path turn PB5's Level back to `Low`.

### 5. Run Continuously

Press `r` to switch from `PAUSED` to `RUN`. The TUI executes batches of 64
instructions and redraws between batches. The demo loop repeats rapidly while
remaining deterministic inside the core. Press `r` again to pause it.

You can toggle PB0 with `0` while running. The program observes the new input on
its next pass through `IN R17, PINB`, then takes the set or clear path for PB5.

### 6. Stop on a Breakpoint

While paused, use `s` until the current line is `3`, the `IN R17, PINB`
instruction. Press `b` to set a breakpoint there. The de-emphasized line below
the instruction now reads "Breakpoint is set on this line" to confirm it.

Press `r`. The program runs forward through the loop. When it returns to
line 3, execution pauses and the status line reports `Stopped: breakpoint`.
Pressing `r` again resumes from that instruction, which lets you inspect or step
through the next loop iteration. Press `b` at the same address to remove the
breakpoint.

### 7. Reset and Exit

Press `x` to discard all current CPU state, GPIO input, breakpoints, SRAM, and
cycle count, then reload the bundled demo. The UI returns to paused line 0.

Press `q` to exit. The TUI calls `endwin`, restoring normal terminal input and
screen behavior.

## Key Reference

| Key        | Action                                                        |
| ---------- | ------------------------------------------------------------- |
| `s`        | Pause and execute one instruction.                            |
| `r`        | Toggle continuous run and pause.                              |
| `x`        | Reset the MCU and reload the bundled demo.                    |
| `b`        | Toggle a breakpoint at the current line.                      |
| `0` to `7` | Toggle the corresponding external input bit, PB0 through PB7. |
| `q`        | Quit and restore the terminal.                                |
| ---------- | ------------------------------------------------------------- |
