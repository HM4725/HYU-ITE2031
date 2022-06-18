        lw      0   1   ADDR
        nor     0   0   2
        nor     0   0   3
        nor     0   0   4
        noop
        add     1   2   7
        add     0   0   3
        add     0   0   4
        add     0   0   5
        sw      1   7   0
        halt
DATA    .fill   4200
ADDR    .fill   DATA
