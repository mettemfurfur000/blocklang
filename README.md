Below is a concise, formal specification for BlockLang, the tiny byte‑code language.

1. Overview

    Word size: 8 bits (u8). Every value, register, stack entry and immediate constant is an unsigned byte (0‑255).
    Program unit: a linear array of instructions (instruction struct) stored in read‑only memory.
    Instruction length: 1 byte for normal ops, 2 bytes when the target is ADJ (the second byte holds an immediate operand).
    Execution model: a grid of blocks (cells). Each block runs its own program independently, stepping once per VM tick. Interaction between blocks and the outside world occurs via IO slots attached to the four sides of the grid.

2. Types
C typedef	Meaning
u8	unsigned 8‑bit integer
u16	unsigned 16‑bit integer (used only for slot offsets)
side	enumeration {up, right, down, left} – identifies a side of a block/grid
targets	enumeration of all possible instruction operands (see §3)
operations	enumeration of all possible op‑codes (see §4)
instruction	packed struct {operation:4, target:4} – occupies exactly one byte

Both operations and targets are limited to 4 bits (<= 15) – enforced by static asserts.
3. Operand Set (targets)
Value	Symbol	Description
0	STK	Current stack top (read only)
1	ACC	Accumulator register
2	RG0	General purpose register 0
3	RG1	General purpose register 1
4	RG2	General purpose register 2
5	RG3	General purpose register 3
6	ADJ	Immediate byte following the instruction
7	UP	Neighboring block/slot above
8	RIG	Neighboring block/slot to the right
9	DWN	Neighboring block/slot below
10	LFT	Neighboring block/slot to the left
11	ANY	Any ready neighbour (not used in current VM)
12	NIL	Constant zero – consumes input when used as a destination


Only one target may be encoded per instruction (the VM currently supports a single operand).
4. Op‑Codes (operations)
Value	Mnemonic	Behaviour (operand taken from target)
0	NOP	Do nothing, consume one tick
1	WAIT	waiting_ticks ← operand (delay)
2	ADD	ACC ← ACC + operand (sets overflow flag)
3	SUB	ACC ← ACC – operand (sets underflow flag)
4	MLT	ACC ← ACC × operand (sets overflow flag)
5	DIV	ACC ← ACC ÷ operand (division‑by‑zero sets overflow)
6	MOD	ACC ← ACC mod operand (division‑by‑zero sets overflow)
7	GET	ACC ← operand
8	PUT	Write ACC to the destination (target must be a side)
9	PUSH	Push operand onto the block’s stack
10	POP	Pop top of stack into destination (target must be a side)
11	JMP	Unconditional jump to address operand
12	JEZ	Jump if ACC == 0
13	JNZ	Jump if ACC != 0
14	JOF	Jump if the last arithmetic operation set the overflow flag
15	HALT Stops execution of the block

All arithmetic results are truncated to 8 bits; overflow/underflow is recorded in last_caused_overflow.
5. Instruction Encoding

byte 0 :  high‑4 bits = operation
          low‑4 bits = target
byte 1 :  present only when target == ADJ; contains the immediate operand.

Example (C pseudo‑code used in main.c):

instruction i = {.operation = GET, .target = UP};   // 0x70
instruction j = {.operation = JOF, .target = ADJ};  // 0xE6
((u8 *)&j)[1] = 7;                                 // immediate 7

The VM fetches the extra byte after the current instruction pointer when target == ADJ. The instruction pointer is then advanced by 2 instead of 1.
6. Runtime State (per block)
Field	Meaning
bytecode	Pointer to the program array
length	Program size in bytes (including immediates)
current_instruction	Index of the next byte to decode
registers[4]	RG0‑RG3
accumulator	ACC
stack[16] / stack_top	LIFO stack (max depth 16)
waiting_ticks	Countdown for WAIT
transfer_*	Temporary storage for inter‑block I/O
state_halted	Set by HALT
last_caused_overflow	Flag used by JOF

A block halts permanently when it executes HALT. All other blocks continue ticking.
7. Grid & I/O Model

    The grid is rectangular (width × height). Each cell holds a block.
    Four IO slots exist on each outer edge (top, bottom, left, right).
        Slots are read‑only when attached as inputs, write‑only when attached as outputs.
    attach_input(grid*, side, slot, src_ptr, len) registers a constant input buffer.
    attach_output(grid*, side, slot, dst_ptr, len) registers a destination buffer.
    Transfer semantics:
        Write (PUT, POP) to a side that touches the grid edge writes into the corresponding output slot.
        Read (GET, PUSH) from an edge reads the next byte from the matching input slot.
        Transfers between interior blocks are performed by matching a sender (waiting_transfer && waiting_for_io) with a receiver (!waiting_transfer && waiting_for_io). The sender’s transfer_value is copied to the receiver and both clear their waiting flags.

If a transfer cannot be satisfied (e.g., output slot full, input exhausted, neighbour halted), the block’s last_caused_overflow flag is set and execution proceeds to the next tick.
8. Execution Cycle (per tick)

For every block (order left‑to‑right, top‑to‑bottom):

    Pre‑move – decode current instruction, decide whether I/O is required, set waiting_transfer / waiting_for_io.
    Write stage – if the block is sending a value to an edge, attempt to store it in the appropriate output slot.
    Read stage – if the block needs a value from an edge or neighbour, attempt to fetch it.
    Execute – once waiting_for_io is cleared, perform the operation (arithmetic, jumps, stack manipulation, etc.).
    Advance PC – according to the operation semantics (normal increment or jump).

If no block performed any work during a tick (any_ticked == false), the VM terminates early.
9. Assembler Requirements

An assembler for BlockLang must:

    Parse mnemonic lines of the form
    OPCODE TARGET [, OPERAND]
    where OPERAND is either a numeric literal (0‑255) or a label.
    Validate that the target is legal for the opcode (e.g., PUT/POP require a side, JMP/JEZ/… require an immediate or a label that resolves to a byte offset).
    Encode each instruction into one byte, inserting a second byte when the target is ADJ.
    The assembler must compute the correct byte offset for labels, taking the extra byte of ADJ instructions into account.
    Emit a binary blob (uint8_t[]) that can be passed unchanged to load_program.
    Optionally generate a symbol table for debugging (mapping labels to byte offsets).

Error messages should be precise (e.g., “Label loop undefined”, “Operand required for ADJ target”, “Target UP invalid for opcode ADD”).
10. Formal Grammar (EBNF‑style)

program      ::= { line }
line         ::= [label ':'] instruction [comment] '\n'
label        ::= identifier
instruction  ::= opcode whitespace { target | number | label }
opcode       ::= 'NOP' | 'WAIT' | 'ADD' | 'SUB' | 'MLT' | 'DIV' | 'MOD'
               | 'GET' | 'PUT' | 'PUSH' | 'POP'
               | 'JMP' | 'JEZ' | 'JNZ' | 'JOF' | 'HALT'
target       ::= 'STK' | 'ACC' | 'RG0' | 'RG1' | 'RG2' | 'RG3'
               | 'ADJ' | 'UP' | 'RIG' | 'DWN' | 'LFT' | 'ANY'
               | 'NIL' 
number       ::= decimal_literal | hex_literal
identifier   ::= letter {letter | digit | '_' }
comment      ::= ';' {any_char}
whitespace   ::= space | tab
