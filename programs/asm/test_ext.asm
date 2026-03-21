; Test program for extended opcodes
; Tests: xor, and, or, not, shl, shr, rol, ror
; Each test: set ACC via 'get', set target via 'put target' then 'get target',
; then ext_op target (ACC <op> target -> ACC)

; Test 1: XOR - 0x5A ^ 0xF0 = 0xAA
get 0x5A
xor 0xF0
put DOWN

; Test 2: AND - 0xFF & 0x0F = 0x0F
get 0x0F      ; ACC = 0x0F
and 0xFF       ; ACC = 0x0F & 0xFF = 0x0F
put DOWN     ; output: 0x0F

; Test 3: OR - 0x0F | 0xF0 = 0xFF
get 0x0F      ; ACC = 0x0F
or 0xF0        ; ACC = 0xF0 | 0x0F = 0xFF
put DOWN     ; output: 0xFF

; Test 4: NOT - ~0xAA = 0x55
not 0xAA       ; ACC = ~0xAA = 0x55
put DOWN     ; output: 0x55

; Test 5: SHL - 0x01 << 3 = 0x08
get 3         ; ACC = 3 (shift count)
shl 0x01       ; ACC = 0x01 << 3 = 0x08
put DOWN     ; output: 0x08

; Test 6: SHR - 0x80 >> 3 = 0x10
get 3         ; ACC = 3 (shift count)
shr 0x80       ; ACC = 0x80 >> 3 = 0x10
put DOWN     ; output: 0x10

; Test 7: ROL - rotate 0xF0 left by 4 = 0x0F
get 4         ; ACC = 4 (rotate count)
rol 0xF0       ; ACC = rotate 0xF0 left by 4 = 0x0F
put DOWN     ; output: 0x0F

; Test 8: ROR - rotate 0x0F right by 4 = 0xF0
get 4         ; ACC = 4 (rotate count)
ror 0x0F       ; ACC = rotate 0x0F right by 4 = 0xF0
put DOWN     ; output: 0xF0

halt
