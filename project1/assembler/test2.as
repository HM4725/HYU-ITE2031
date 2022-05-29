start   add     0   0   1
        add     0   0   2
loop    nor     0   1   1
        nor     0   2   2
        beq     0   0   loop    ; inifinite loop
        noop
        noop
        halt
