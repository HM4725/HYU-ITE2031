        nor     0   0   1       ; r-format
        add     0   0   2
        nor     0   0   3
        add     0   0   4
        sw      0   1   data    ; i-format
        noop    10  10  10      ; o-format
        halt    10  10  10
data    .fill   data
        .fill   0
