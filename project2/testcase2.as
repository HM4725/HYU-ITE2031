L1      beq  0   0  L2
        halt            ; <control hazard>
L2      beq  0   0  L3
        halt            ; <control hazard>
L3      beq  0   0  L4
        halt            ; <control hazard>
L4      lw   0   1  DATA
        noop
        halt
DATA    .fill   -1
