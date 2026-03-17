# BlockLang Specification

Below is the formal specification for BlockLang, a tiny bytecode language with a grid-based VM.

## 1. Overview

- **Word size**: 8 bits (u8). Every value, register, stack entry and immediate constant is an unsigned byte (0-255).
- **Program unit**: A linear array of instructions stored in read-only memory.
- **Instruction length**: 1 byte for normal ops, 2 bytes when the target is ADJ (the second byte holds an immediate operand).
- **Execution model**: A grid of blocks. Each block runs its own program independently, stepping once per VM tick. Interaction between blocks and the outside world occurs via I/O slots attached to the four sides of the grid.

## 2. Types

| C typedef | Meaning |
|-----------|---------|
| u8 | unsigned 8-bit integer (0-255) |
| u16 | unsigned 16-bit integer (used only for slot offsets) |
| side | enumeration {up, right, down, left, any} - identifies a side of a block/grid |
| target_t | enumeration of all possible instruction operands (see §3) |
| operations | enumeration of all possible op-codes (see §4) |
| instruction | packed struct {operation:4, target:4} - occupies exactly one byte |

Both operations and targets are limited to 4 bits (<= 15) - enforced by static asserts.

## 3. Operand Set (targets)

| Value | Symbol | Description |
|-------|--------|-------------|
| 0 | STK | Current stack top value (read only) |
| 1 | ACC | Accumulator register |
| 2 | RG0 | General purpose register 0 |
| 3 | RG1 | General purpose register 1 |
| 4 | RG2 | General purpose register 2 |
| 5 | RG3 | General purpose register 3 |
| 6 | ADJ | Immediate byte following the instruction (operand) |
| 7 | UP | Neighboring block/slot above |
| 8 | RIGHT | Neighboring block/slot to the right |
| 9 | DOWN | Neighboring block/slot below |
| 10 | LEFT | Neighboring block/slot to the left |
| 11 | ANY | Any ready neighbour (inter-block transfer) |
| 12 | NIL | Constant zero - consumes input when used as a destination |
| 13 | SLN | Stack Length Number - number of elements on the stack |
| 14 | CUR | CURrent instruction pointer |
| 15 | REF | REFerence - bytecode byte at address in ACC (read) or RG3 (write) |

Only one target may be encoded per instruction.

## 4. Op-Codes (operations)

| Value | Mnemonic | Behaviour (operand taken from target) |
|-------|----------|---------------------------------------|
| 0 | NOP | Do nothing, consume one tick |
| 1 | WAIT | waiting_ticks <- operand (delay) |
| 2 | ADD | ACC <- ACC + operand (sets overflow flag) |
| 3 | SUB | ACC <- ACC - operand (sets underflow flag) |
| 4 | MLT | ACC <- ACC × operand (sets overflow flag) |
| 5 | DIV | ACC <- ACC ÷ operand (division-by-zero sets overflow) |
| 6 | MOD | ACC <- ACC mod operand (division-by-zero sets overflow) |
| 7 | GET | ACC <- operand |
| 8 | PUT | Write ACC to the destination (target can be side or local) |
| 9 | PUSH | Push operand onto the block's stack |
| 10 | POP | Pop top of stack into destination |
| 11 | JMP | Unconditional jump to address operand |
| 12 | JEZ | Jump if ACC == 0 |
| 13 | JNZ | Jump if ACC != 0 |
| 14 | JOF | Jump if the last arithmetic operation set the overflow flag |
| 15 | HALT | Stops execution of the block |

All arithmetic results are truncated to 8 bits; overflow/underflow is recorded in `last_caused_overflow`.

## 5. Instruction Encoding

```
byte 0 :  high-4 bits = operation
          low-4 bits = target
byte 1 :  present only when target == ADJ; contains the immediate operand.
```

Example (C pseudo-code):

```c
instruction i = {.operation = GET, .target = UP};   // 0x70
instruction j = {.operation = JOF, .target = ADJ};  // 0xE6
((u8 *)&j)[1] = 7;                                 // immediate 7
```

The VM fetches the extra byte after the current instruction pointer when target == ADJ. The instruction pointer is then advanced by 2 instead of 1.

## 6. Runtime State (per block)

| Field | Meaning |
|-------|---------|
| bytecode | Pointer to the program array |
| length | Program size in bytes (including immediates) |
| current_instruction | Index of the next byte to decode |
| registers[4] | RG0-RG3 |
| accumulator | ACC |
| stack[16] / stack_top | LIFO stack (max depth 16) |
| waiting_ticks | Countdown for WAIT |
| transfer_value | Temporary storage for transferred value |
| transfer_side | Which side (0-3) or ANY (4) for transfer |
| transfered | Flag indicating value was received |
| state_halted | Set by HALT |
| last_caused_overflow | Flag used by JOF |

A block halts permanently when it executes HALT. All other blocks continue ticking.

## 7. Grid & I/O Model

- The grid is rectangular (width × height). Each cell holds a block.
- Four I/O slots exist on each outer edge (top, bottom, left, right).
  - Slots are read-only when attached as inputs, write-only when attached as outputs.
- `attach_input(grid*, side, slot, src_ptr, len)` registers a constant input buffer.
- `attach_output(grid*, side, slot, dst_ptr, len)` registers a destination buffer.

### Transfer Semantics

**Edge transfers:**
- Write (PUT, POP) to a side that touches the grid edge writes into the corresponding output slot.
- Read (GET, PUSH) from an edge reads the next byte from the matching input slot.

**Direct block-to-block transfers (optimization):**
- When a writing block (PUT/POP) targets a side where the neighbor block has a reading operation (GET/PUSH/ADD/etc.) targeting the opposite side, the transfer happens directly in a single tick.
- Both blocks complete their operations without waiting.

**Failure handling:**
- If a transfer cannot be satisfied (output slot full, input exhausted, neighbour halted), the block's `last_caused_overflow` flag is set and execution proceeds with operand value 0.

## 8. Execution Cycle

The VM uses a single-pass execution model (`block_exec_instruction_mono`). For every block (order left-to-right, top-to-bottom):

1. **Check state** - If halted or waiting_ticks > 0, skip (decrement waiting_ticks if applicable)
2. **Wrap PC** - If current_instruction >= length, wrap to 0
3. **Halt check** - If operation is HALT, set state_halted and return
4. **Analyze instruction** - Determine transfer_side, whether I/O is needed, and if it's a writing operation
5. **Write phase** - If writing to a side, attempt direct block transfer first, then edge slot
6. **Read phase** - If reading from a side, try direct block transfer, then edge slot; if failed, set overflow and use 0
7. **Local operand** - If target doesn't require I/O, read operand directly from local target
8. **Execute operation** - Perform arithmetic, jumps, stack operations, etc.
9. **Advance PC** - Set current_instruction to advance_to (normal increment or jump target)

If no block performed any work during a tick (`any_ticked == false`), the VM terminates early.

## 9. Assembler Requirements

An assembler for BlockLang must:

- Parse mnemonic lines of the form: `OPCODE TARGET [, OPERAND]` where OPERAND is either a numeric literal (0-255), a character literal ('a'), or a label.
- Validate that the target is legal for the opcode.
- Encode each instruction into one byte, inserting a second byte when the target is ADJ.
- Compute the correct byte offset for labels, taking the extra byte of ADJ instructions into account.
- Emit a binary blob that can be passed to `load_program`.
- Generate a debug object file with embedded source and line table for debugging.

## 10. Formal Grammar (EBNF-style)

```
program      ::= { line }
line         ::= [label ':'] instruction [comment] '\n'
label        ::= identifier
instruction  ::= opcode whitespace { target | number | label | char_literal }
opcode       ::= 'NOP' | 'WAIT' | 'ADD' | 'SUB' | 'MLT' | 'DIV' | 'MOD'
               | 'GET' | 'PUT' | 'PUSH' | 'POP'
               | 'JMP' | 'JEZ' | 'JNZ' | 'JOF' | 'HALT'
target       ::= 'STK' | 'ACC' | 'RG0' | 'RG1' | 'RG2' | 'RG3'
               | 'ADJ' | 'UP' | 'RIGHT' | 'DOWN' | 'LEFT' | 'ANY'
               | 'NIL' | 'SLN' | 'CUR' | 'REF'
number       ::= decimal_literal | hex_literal
char_literal ::= "'" character "'"
identifier   ::= letter {letter | digit | '_' }
comment      ::= ';' {any_char}
whitespace   ::= space | tab
```

## 11. Building and Running

### Build
```bash
make          # Builds all targets: assembler, singleblock, blocklang
make clean    # Removes build/* and obj/*
```

### Running Tests
```bash
./build/test_app <input_file> <width> <height> <debug> <ticks_limit> <print_strings> [io_specifications...]
```

Example:
```bash
./build/test_app test.bl 2 2 true 128 true "in:up:0:1,2,3" "out:down:0:="
```

### Single Block Mode
```bash
./build/basm.exe -o program.b -f source.bl   # Assemble
./build/block.exe -r -f program.b              # Run with stdin/stdout
```
