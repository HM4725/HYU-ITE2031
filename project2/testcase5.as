        lw   0   1   data
        lw   0   2   addr
        lw   0   3   one
        noop
        add  0   0   4
        noop
        add  1   3   5
        add  0   0   1
        add  0   0   3
        noop
        noop
        sw   2   5   1 
        noop
done    halt
data    .fill   5400
        .fill   0
addr    .fill   data
one     .fill   1
