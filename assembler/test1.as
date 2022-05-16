        lw  0   6   hw   ; reg6: 'hello world' entry
        lw  0   1   one  ; reg1: 1
        add 0   6   2    ; reg2: ptr
loop    lw  2   3   0    ; reg3: *ptr
        beq 2   0   done ; while(*ptr != '\0')
        noop             ; something like print
        noop
        noop
        add 1   2   2    ; ptr++
        beq 0   0   loop ; jump to loop
        noop
done    halt
        noop
one     .fill     1
hw      .fill   hw0      ; address of 'hello world'
hw0     .fill   104      ; 'h'
        .fill   101      ; 'e'
        .fill   108      ; 'l'
        .fill   108      ; 'l'
        .fill   111      ; 'o'
        .fill    32      ; ' '
        .fill   119      ; 'w'
        .fill   111      ; 'o'
        .fill   114      ; 'r'
        .fill   108      ; 'l'
        .fill   108      ; 'd'
        .fill     0      ; '\0'
        .fill     0      ; '\0'
        .fill     0      ; '\0'
