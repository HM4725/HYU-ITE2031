        add     0   0   1       ; r-format
        add     0   0   2
        nor     0   0   3
        lw      0   1   data    ; i-format
        sw      0   1   data
        jalr    0   0   10      ; j-format
        noop    10  10  10      ; o-format
        halt    10  10  10
data    .fill   data
        .fill   0
