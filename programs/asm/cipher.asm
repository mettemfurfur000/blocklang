; Simple XOR stream cipher test
; Encrypts plaintext with keystream: ciphertext = plaintext ^ keystream
; Decrypts by same operation: plaintext = ciphertext ^ keystream

; PRNG: x = (x * 97 + 43) mod 256 (simple LCG)
; Kept in RG0

main:
get 97        ; ACC = 97 (multiplier)
put RG1       ; RG1 = 97
get 43        ; ACC = 43 (increment)
put RG2       ; RG2 = 43
get 0         ; ACC = 0 (initial seed in RG0)

; Read plaintext byte from input (UP slot)
get UP        ; ACC = plaintext byte
put RG0       ; RG0 = plaintext (saved)

; Generate keystream: ACC = (RG0 * RG1 + RG2) mod 256
get RG0       ; ACC = seed
mlt RG1       ; ACC = seed * 97
put RG0       ; RG0 = seed * 97
get RG0       ; ACC = seed * 97
add RG2       ; ACC = seed * 97 + 43
mod RG2       ; ACC = (seed * 97 + 43) mod 256
xor RG0       ; ACC = plaintext ^ keystream (the encryption!)
put RIGHT     ; output ciphertext

jmp main      ; loop for next byte

halt
