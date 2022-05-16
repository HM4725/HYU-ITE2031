        lw  0   1   addr
        jalr 1  2
        noop
        noop
        beq 0   0   done
start   lw  0   7   arr
        lw  0   1   one
        add 0   7   2
        sw  2   0   0   ; three: 0
        add 2   1   2
        sw  2   0   0   ; two: 0
        add 2   1   2
        sw  2   0   0   ; one: 0
        add 2   1   2
        sw  2   0   0   ; neg1: 0
        add 2   1   2
        sw  2   0   0   ; neg2: 0
        add 2   1   2
        sw  2   0   0   ; neg3: 0
        add 0   0   1 
        add 0   0   2 
        add 0   0   3 
        add 0   0   4 
        add 0   0   5 
        add 0   0   6 
        add 0   0   7 
        beq 0   0   done
        beq 0   0   start
        noop
        noop
        noop
done    halt
addr    .fill   start
arr     .fill   three
three   .fill   3
two     .fill   2
one     .fill   1
neg1    .fill   -1
neg2    .fill   -1
neg3    .fill   -3
