        lw      0   1   one     ; reg1: 1
        nor     0   0   2       ; reg2: -1
        lw      0   3   addr    ; reg3: data
        lw      3   4   0       ; reg4: 100
        lw      3   5   1       ; reg5: 200
        add     1   3   6       ; reg6: data + 1 
        add     1   6   6       ; reg6: data + 2 
        add     2   4   7       ; reg7: 99
        sw      6   7   0
        add     1   4   7       ; reg7: 101
        sw      6   7   1       
        add     2   5   7       ; reg7: 199
        sw      6   7   2       
        add     1   5   7       ; reg7: 201
        sw      6   7   3       
        noop
        halt


data    .fill       100
        .fill       200
        .fill       0
        .fill       0
        .fill       0
        .fill       0
one     .fill       1
addr    .fill       data
