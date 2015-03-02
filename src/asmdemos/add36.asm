    (* Perform the 36-bit addition of (r3,r4) into (r5,r6),
     * preserving the values of (r3,r4,r7).
     * r3 holds the high bits and r4 holds the low bits.
     * r5 holds the high bits and r6 holds the low bits.
     *)

    ADD.s 6,6,4
    ADD.s 5,5,3
    SW $5, [+PC]    .EQU Result5 .
    SW $7, [+PC]    .EQU Result7 .
    XOR 5,6,4  
    LW $7, [+PC]    WORD 400000
    AND 7,7,5
    XOR 5,5,4
    SNZ $7
    SUB.s 5,5,4
    XOR 5,5,6
    XOR 5,5,4
    LW $7, [+PC]    WORD 400000
    AND 7,7,5
    LW $5, [+PC]    WORD Result5
    LW $5, [$5]
    SZ $7
    INC.s 5,5
    LW $7, [+PC]    WORD Result7
    LW $7, [$7]
