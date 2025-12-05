; Test LGet (l=:) instruction
; Store L register to memory and verify

    .org    0x1000

start:
    ; Set L register to a known value
    w1 := $0xDEADBEEF
    l := i1                 ; Copy I1 to L register

    ; Store L to memory at target address
    l=: $target             ; L -> target (this is the instruction under test)

    ; Halt or spin
    stop

    .org    0x2000
target:
    .word   0               ; Destination for l=: instruction
