
  .ENTRY (0,0)
  .EQU _NOP 607774   { the assembled value of NOP }
  .EQU _NOP2 400000  { Another kind of NOP; see below. }
  .EQU INPUT 000
  .EQU OUTPUT 001
  .EQU PRGMEXIT 002
  .EQU HCON 040
  .EQU HCAND 041
  .EQU HCOR 042
  .MACRO CALLV LV $PC,
  .FILL 000777 { TRP 777 }


    (***************** Barricade ******************)

  { We have to set up a barricade so the very first trap of
    the program will set up the usermode context. }

  .EQU Baseline 776  { the Y-coord immediately below the first
                      real instruction of all the handlers }

  .ORG (0,Baseline-1)
  .SIZE (256d,1)
  .FILL 070000   { RET, the way to handle most traps }

  .ORG (' ',Baseline)
  .SIZE (256d-32d,1)
  .FILL 160000   { GOW }

  .ORG (0,Baseline)
  .SIZE (32d,1)
  .FILL 150000   { GOE }

  .ORG (0,Baseline+1)   .ORG (1,777)
  .SIZE (256d,1)
  .FILL 046000   { SNZ $6 }

  (* We fill the Befunge page with _NOP2 before putting
   * the client's Befunge code there. Then, assuming that
   * the loader fills the 80x25 rectangle with ASCII 32d,
   * we can quickly test whether a coordinate pair $X is
   * within 80x25 by testing whether [$X] equals _NOP2,
   * since _NOP2 is neither NOP nor an ASCII character.
   * (Note that V == _NOP2 if and only if V is non-zero
   * and V<<1 is zero.) We use this quick test in the
   * trap handler for '"'. *)

  .ORG (0,0)
  .SIZE (128d,32d)
  .FILL _NOP2

  (* This page is a "better than exact replica" of the
   * Befunge page. We execute code out of the Befunge page,
   * but we 'p'ut and 'g'et data from this mirror page.
   * Therefore, this page stores everything exactly as
   * it would appear in a normal Befunge interpreter;
   * the Befunge page at (0,0) stores the same thing but
   * with all non-ASCII characters replaced with NOP,
   * and all cells outside 80x25 set to _NOP2. *)

  .ORG (128d,0)
  .SIZE (128d,32d)
  .FILL ' '


  (* Most of Funge-space is filled with TRP 777,
   * to make debugging easier. If the PC strays
   * from its intended course, it'll probably wind
   * up here eventually. *)
  TRP 777
  .ORG (777,0)


   (************ Hardware Context Setup Code *************)

      RET
      SUB $TPC,$TPC,$TDPC
      LI $TDPC, (1,0)
      LV.y $PC, InitL2
      SZ.Y $4
      LV.y $PC, InitL1            (* Now make a perfect copy of the
      SZ.X $4                      * userspace to its own east, and
      SW 5,[4]                     * replace any Befunge no-ops with
      SNZ.Y $3                     * Fungus NOP instructions (just to
      LW.V $5, [+$PC]  NOP         * speed up execution a little bit).
      SUB.Y 3,5,3                  * The copy to the east will be used
      LW.S $5, [+$PC]  RET         * by the 'g' and 'p' instructions,
      LW $3, [$3]                  * so the user can't break out of his
      LV.Y $3, Baseline-1          * sandbox by 'p'utting a sequence
      SW.S $3, [$4+$5]             * of Fungus instructions in the
      LI $5, 128d                  * path of his IP. *)
      LW $3, [$4]                                          LV.Y $PC, QuoL2 (* Yes, $5 == _NOP2. Ignore it. *)
      DEV.X $4, $4                          GOS            GOW
      LV.X $4, 80d     .EQU InitL1 .-(1,0)  LI $5, 128d    SZ $5           .EQU QuoL1 .-(1,0)
      DEV.Y $4, $4                          LW $5, [7|5]   SHL.s $5,$5
      LV.Y $4, 25d     .EQU InitL2 .-(1,0)  SUB.X 5,5,$PC  LV.Y $PC, QuoL1  (* Is $5 == #400000? *)    
      NOP                                   SNZ.X $5       SNZ $5          
      NOP                      SW 3,[4]     DEV $1,$1      LW $5, [7]      { $3=stack index }
      NOP                      RET          ADD.X 5,5,$PC  AND 7,7,5       { $4=stack base }
      NOP                      CALLV Push4  INC.s $3, $3   LMR $5, #HCAND  { $5=string char }
      SMR $0, #HCOR            INC.s 4,4    SW.s 5,[3+4]   ADD.v 7,7,6
      SMR $4, #HCAND           LV $4, -1    GOE            GON             .EQU QuoL2 .-(1,0)
      LV.Y $4, 037             SZ $4        LW 3,[4]       RET
      LV.X $4, 177     RET     CALLV Pop4   LV $4, Stack   ADD.v 7,7,6
      GON              GOW     GOW          GOW            GOW
      RET              SNZ $6  SNZ $6       SNZ $6         SNZ $6
                       .ORG (' ',0)         .ORG ('"',0)   .ORG ('#',0)


   (************ Stack Manipulation Routines *************)

    (* The Pop4 subroutine pops a value from the stack into $4,
       and "returns" to ([$7].x, Baseline-2). It trashes $3. *)
    (* The Pop45 subroutine pops $5, then pops $4. It trashes $3. *)

    .EQU Pop4 39d
    .EQU Pop45 41d

    .ORG (Pop4,31d)         .FILL _NOP
    GOE        GOS          MR $PC, $3         CALLV Pop4
    SW 4,[3]   ADD.s 4,4,3  LV.Y 3,Baseline-1  LW.s 5,[+5]
    DEC.s 4,4  LW.s 4,[+4]  LX 3,[7]           ADD.s 5,5,3
    GOE        GOE          GON                SW 5,[3]
    SNZ $4                  LI $4, 0           DEC.s 5,5
    LW 4,[3]                INC $1,$1
    LV $3, Stack            SZ $5
    .ORG (Pop4,Pop4)        LW 5,[3]
                            LV $3, Stack
                            .ORG (Pop45,Pop45)

    (* The Push4 subroutine pushes a value from $4 onto the stack and RETs. *)
    (* The Push45 subroutine pushes $4, then pushes $5, then RETs. *)

    .EQU Push4 42d
    .EQU Push45 44d

    GOE                 SW 5,[3]       SW.s 4,[3+5]     RET
    INC.s 5,5           GOS            GOW              .FILL _NOP
    LW 5,[3]            SW.s 4,[3+5]   INC.s 5,5
    LV $3, Stack        INC.s 5,5      LW 5,[3]
    .ORG (Push4,Push4)  SW 5,[3]       LV $3, Stack
                        LW 4, [++$PC]  SW $5, [$PC-$DPC]
                        SW.s 4, [3+5]  .ORG (Push45,Push45)
                        RET

  (* The stack goes as close to the top of RAM as we can manage,
   * because we're supposed to have an "infinite" stack. In reality,
   * our stack goes from (46d,46d) linearly through memory (INC.s
   * and DEC.s) until it hits the kernel code at the bottom of RAM.
   *)
  .EQU Stack (Push45+1,Push45+1)
  WORD 0        .ORG Stack+(1,0)

  (****** Handler for '>' ******)
    RET
    LI $TDPC, (1,0)
    .ORG ('>',Baseline)

  (****** Handler for '<' ******)
    RET
    LI $TDPC, (-1,0)
    .ORG ('<',Baseline)

  (****** Handler for '^' ******)
    RET
    MR $TDPC, $DPC
    .ORG ('^',Baseline)

  (****** Handler for 'v' ******)
    RET
    SUB.V $TDPC, $0, $DPC
    .ORG ('v',Baseline)

  (****** Handlers for '?' and '@' ******)

    (* This random number generator is a linear feedback shift
      register (LFSR) with a length of 9 bits. On each cycle,
      the register is shifted right, and the bit inserted on the
      left end is the XOR of bit 0 (the bit shifted out) and
      bit 4. Two cycles of the LFSR produce two bits, which we
      convert into one of the four cardinal directions.
      This LFSR is maximal, with a period of 511: its output
      repeats after every 511 calls to '?'.
       The output in each cycle is evenly distributed among the four
      possibilities, except that '<' appears only 127 times:
       <v^>>>v><>^><><><<<^<<><v<<>^v<>>>v<<v^v>>^>v>^<^^<<>>v^<vvv^>^
      <>^^>vvv<>^>>>^>^><<<v<^>^^<^>>^^vvv^<^^><v>>v<>v<vv<^vv^v^<<^>>
      v^>v>v<<^^>>vv>>>>><<^^<>>vv<>v>v><<^v<v>>^<vv^^v^^<^<><^>^v<<>v
      v^>v<v^<<v>vv<<vvvv><<vv^vv<^^v<v<><<>^^<v>v^<^v>^vv<v^><>>>^<^v
      <^>v^^^v<^<v<^<^<<<<><^<^><^<v>^^^^><^>vv^<v^v<><>><^^^>>^vv>v<>
      ^<>v^vv>^<v^^<v<v<<<^><v<>><v^<>vvv>><>v<^v^^>^^>^v><vv>v>><<v^^
      >v^v^><<>v^^vv^^^^<<<>>^^^v>^^v^v<<<v>^v^<>^v>>v>>><>^<<v<v><^^v
      >v^>^>><^v^v><^vvvv<<^v>vv><>v>^>^<<^<>>^v^>><vv<vv>^>v<^^^<><v> *)

    WORD (0,1)
    WORD (0,-1)
    WORD (1,0)
    WORD (-1,0)
    RET
    LW.y $TDPC, [$PC-$4]
    INV.y $4, $4
    INV.y $4, $4
    LV.Y $4, 000
    SX $4, [1+2]  (* At this point $4 is in [0..3]. Convert it to ><^v. *)
    AND $4, $3, $4
    LI $3, 0x3    (* $4 := low-two-bits-of( lfsr ) *)
    SX 4,[+3]     { store lfsr back in memory }
    SHR.S 4,4     { update lfsr a second time }
    SHR.S 4,4     { lfsr now has its new value }
    LV.Y $4, 000  { put $5 in $4.y }
    SX.v $5, [1+2]
    XOR 5,4,5     { $5 := lfsr ^ (lfsr >> 4) }
    SHR 5,5
    SHR 5,5
    SHR 5,5
    SHR 5,4               .EQU RndLfsr .+(0,1)
    LW 4,[+3]             WORD 000661
    ADD $3,PC,DPC         SMR $0, #PRGMEXIT
    .ORG ('?',Baseline)   .ORG ('@',Baseline)


  (******* Handlers for '/' and '0' through '9' ********)

LV.Y $PC, DivLoop+1
ADD.S 6,6,7
SUB.S 4,4,5
LV.Y $PC, DivLoop+1    (* dividend in 4, divisor in 5 *)
SZ $3
SUB.S 3,3,4
ADD.S 3,3,5          .FILL _NOP
SHR.S 3,3
SHL.S 3,3
SUB.S 3,4,5
GOE                  GOS
SNZ $7               LV.Y $DPC, (DivExit-.)
SHR.S 7,7
SHR.S 5,5                         .EQU DivLoop ..y
GOE                  GOS
SNZ $3               LV.Y $DPC, 2d
SUB.S 3,3,4
ADD.S 3,3,5          SHL.S 5,5
SHR.S 3,3
SHL.S 3,3            SHL.S 7,7
SUB.S 3,4,5
GON                  GOW
LI $7, 1
LI $6, 0
SX 3,[7]
LW $7, [+PC]         WORD DivMinus
                     DEC $1,$1
LI $3, 160  {4,6,0}  LI $3, 106  { SUB.s 4,0,6 }
INC $1,$1            DEC $1,$1
SNZ $6               SNZ $6
INV $1,$1            NEG.s 5,5
SZ.Y $3
AND.Y 3,5,3
LI $6, 1             DEC $1,$1    CALLV Push4
LV.Y $3, 400         LI $6, 0     LW.Y $7, [++$3]
INV $1,$1            NEG.s 4,4    LW $6, [$3]
SZ.Y $3              INV $1,$1    LW 3, [+PC]  WORD DivSave6
AND.Y 3,4,3          SUB.s 4,0,5  .EQU DivMinus .-(1,0)
GOSE                 GON          LI $4,0      .EQU DivExit DivMinus+(0,1)
SNZ $5               GOE          GON
SZ $4
SW 6,[+PC]           .EQU DivSave6 .
SW 7,[+PC]           WORD 0
LV.Y $3, 400         CALLV Push4
CALLV Pop45          LI.S $4, 0
.ORG ('/',Baseline)  .ORG ('0',Baseline)

       .MACRO CV4 CALLV Push4
       .MACRO L LI.S $4,
  CV4  CV4  CV4  CV4  CV4  CV4  CV4  CV4   CV4
  L 1  L 2  L 3  L 4  L 5  L 6  L 7  L 8d  L 9d
                      .ORG ('5',Baseline)


  (****** Handler for ':' ******)
    CALLV Push45
    MR $5, $4
    CALLV Pop4
    .ORG (':', Baseline)

  (****** Handler for '\' ******)
    CALLV Push45
    MR 5,3
    MR 4,5
    MR 3,4
    CALLV Pop45
    .ORG ('\\', Baseline)

  (****** Handler for '+' ******)
    CALLV Push4
    ADD.s 4,4,5
    CALLV Pop45
    .ORG ('+', Baseline)

  (****** Handler for '_' ******)
    RET
    LI $TDPC, -1
    SZ $4
    LI $TDPC, 1
    CALLV Pop4
    .ORG ('_', Baseline)

  (****** Handler for '|' ******)
    RET
    LV.Y $TDPC, 1
    SNZ $4
    MR $TDPC, $DPC
    CALLV Pop4
    .ORG ('|', Baseline)

  (****** Handler for 'g' ******)
    CALLV Push4
    LW.s $4, [4+5]        CALLV Push4
    LI $5, 128d           LI $4, 32d
    INC $1,$1
    SZ $5
    AND 5,4,5
    LW $5, [+$PC]         WORD (700,740)  { bitwise NOT of (127d,31d) }
    LV.Y $4, 0     (* load $5.x into $4.y *)
    SX $5, [1+2]
    CALLV Pop45           RET
    .ORG ('g', Baseline)  .ORG ('h',Baseline)

  (****** Handler for 'p' ******)
    .FILL _NOP            GOE           GOS
                          SW 5,[3]      INC.s 5,5      (* Get the character
                          DEC.s 5,5     LW.s 5,[3+5]     to 'p'ut, in $5.
                          GOE           GOS          The index is still in $4. *)
    RET                   SNZ $5        LI $3, 128d
    SW 5,[3]              LW 5,[3]      SW 5,[4|3]   (* change the perfect copy *)
    DEC.s 5,5  (* Decrement the stack   NEG.s 3,3
    SNZ $5      top to get rid of the   AND 3,5,3   (* if $5 is not an ASCII char...
    LW 5,[3]    character to 'p'ut. *)  SZ $3           use a NOP instead of $5
    INC $1,$1             NOP           LW $5, [-PC]  when changing the real copy *)
    SNZ $5                              SW 5,[4]      (* change the real copy *)
    LV $3, Stack                        RET
    AND 5,3,5     (* nonzero if index was outside Befunge grid *)
    LV $3, 400
    SUB.v 5,5,4   (* expect a strictly positive result *)
    LW $5, [+$PC]         WORD (80d,25d)  { size of Befunge grid }
    LV.Y $4, 0     (* load $5.x into $4.y *)
    SX $5, [1+2]
    CALLV Pop45           RET           RET
    .ORG ('p', Baseline)  .ORG ('q',Baseline)


  (****** Handler for '*' ******)
    GOS                   GOW
    LV.Y $4, #8d          DNZ.Y $4  (* <-------.  .EQU MulLoop .
    LW.Y 7, [$PC+$4]      LI $7, 1             |
    MR $4, $3             AND $7, $4, $7       |
    SZ $0                 SZ $7                |
    CALLV Push4           ADD.s $3, $3, $5     |
    GON                   SHL.s $5, $5         |
                          SHR.s $4, $4         / *)
                          LV.Y $DPC, MulLoop-.
                          LI $3, 0     (* $3 holds the result *)
    WORD 0                SW $7, [-$PC]
    RET                   CALLV Pop45
    .ORG (')', Baseline)  .ORG ('*', Baseline)

  (****** Handlers for '$' and '%' ******)


LV.Y $PC, RemLoop+1
SUB.S 4,4,5
LV.Y $PC, RemLoop+1    (* dividend in 4, divisor in 5 *)
SZ $3
SUB.S 3,3,4
ADD.S 3,3,5          .FILL _NOP
SHR.S 3,3            WORD RemSave6
SHL.S 3,3            LW $PC, [1+2]
SUB.S 3,4,5          LV.Y $3, (0,3)
GOE                  GON
SNZ $7
SHR.S 7,7
SHR.S 5,5                                 .EQU RemLoop ..y
GOE                  GOS
SNZ $3               LV.Y $DPC, 2d
SUB.S 3,3,4
ADD.S 3,3,5          SHL.S 5,5
SHR.S 3,3
SHL.S 3,3            SHL.S 7,7
SUB.S 3,4,5
GON                  GOW
LI $7, 1   (* Now dividend=$4 and divisor=$5 are both positive. *)
NEG.s 5,5
SZ.Y $3
AND.Y 3,5,3          LW $3, [+PC]
SX 7,[6]             GON                  .EQU RemExit .-(1,0)
LW $6, [+PC]         WORD RemMinus
LV.Y $3, 400         DEC $1,$1
LI $7, 140  {4,4,0}  NEG.s 4,4
INV $1,$1            LI $7, 104  {4,0,4}
SZ.Y $3
AND.Y 3,4,3
CALLV Push4          DEC $1,$1
LW.Y 6,[PC+$3]       CALLV Push4
LW.Y 7,[PC+$3]       SNZ $5
SUB.s 4,4,0          SZ $4                .EQU RemMinus .-(2,0)
.EQU RemSave6 .      SW 6,[-PC]
WORD 0               SW 7,[-PC]
RET                  LV.Y $3, 400
CALLV Pop4           CALLV Pop45          { We can't go over here. }
.ORG ('$',Baseline)  .ORG ('%',Baseline)  .ORG ('&',Baseline)


  (****** Handler for '`' ******)
    CALLV Push4
    LI $4, 1
    SZ.y $3
    LI $4, 0
    AND.y 3,3,5     .EQU SignBitsDiffer ..y
    LV.y $3, 400
    SUB.s 5,5,4
    LV.y $PC, SignBitsDiffer+1
    SZ.y $3
    XOR 5,5,4
    AND.y 3,3,5
    LV.y $3, 400
    XOR 5,5,4
    CALLV Pop45   (* $4=b $5=a *)
    .ORG ('`',Baseline)

  (****** Handler for '~' ******)
     (* "InBuffer" is shared by '~' and '&'; it's the
    one character of lookahead that we need in order to
   parse numbers properly. It has the following semantics:
        ib.y .ib.x . meaning ......... ib.y .ib.x after consumption
        -----------------------------------------------------------
          0 .. ?? .. no lookahead ...... 0 .. ??
         -1 .. ?? .. next char is ib.x . 0 .. ??
         -1 .. -1 .. end-of-file ...... -1 .. -1
        -----------------------------------------------------------
                    *)

   .EQU InBuffer .+(0,1)
   WORD 0
   CALLV Push4
   SW $4, [$3]          CALLV Push4
   SNZ $5               SY $0, [$3]
   INV $5, $4           SZ $5
   LMR.s $4, #INPUT     INV $5, $4
   GON                  GOW
                        SNZ.y $4
                        LW 4,[3]
                        LV.Y $3, InBuffer
   RET                  LV.X $3, InBuffer
   .ORG ('}',Baseline)  .ORG ('~',Baseline)

  (****** Handler for '&' ******)

      (* Read characters until you get one in [0..9]. Then, as long
      as the character is a digit, let r4 := 10*r4 + (ch-'0'). The first
      non-digit character goes in "inbuffer", and we return r4. If we
      read EOF before any digits, we return zero. If overflow occurs,
      we put the offending last character in "InBuffer" and return. *)
      (* Notice that we do not treat "-" specially. This is correct
      according to the Funge-98 spec, although most Befunge-93 interpreters
      use behavior equivalent to scanf() for '&'. *)

    INV $1,$1              GOS
    LV.y $PC, AmpLoop2     LI $3, '0'
    SNZ.y $3               ADD.s Ch5,5,3      CALLV Push4
    SUB.s 3,3,Ch5          LV.y $PC, AmpExit  SW Ch5,[3]
    LI $3, 9d              INV $1,$1          LW $3, [1-2]
    INV $1,$1              ADD.s Ch5,5,3      WORD InBuffer
    SZ.y Ch5               .EQU AmpExit .
    SUB.s Ch5,5,3          LW $3, [+PC]       WORD InBuffer
    LI $3, '0'             SW Ch5,[3]
    LMR.s Ch5, #INPUT      SZ $0              LV.y $PC, .-(0,4)
    ADD.s Res4, Res4, $3   CALLV Push4        LV Res4, -1
    ADD.s Res4, Res4, $3   GON                LV Ch5, -1
    ADD.s Res4, Res4, Ch5                     WORD AmpLoop2
    SHL.s $3, Res4         LV.y $PC, AmpLoop  LW $PC, [1+2]
    SHL.s Res4, Res4       LMR.s Ch5, #INPUT  LI $4, 0
    .EQU AmpLoop2 .        INC $1,$1          DEC $1,$1
                           SNZ.y $4           LV.y $PC, .-(0,4)
                           SUB.s 4,4,Ch5      SNZ $5
                           LI $4, 9d          INC.s 5,5
                           INV $1,$1          ADD.s 5,5,3
    CALLV Push4  { =0 }    SZ.y Ch5
    SW Ch5,[3]             SUB.s Ch5,Ch5,$3
    INV $1,$1              LI $3, '0'         .EQU AmpLoop .-(1,0)
    SZ $4  { ch != EOF }   
    INC.s $4, Ch5
    LMR.s Ch5, #INPUT
    SNZ.y Ch5
    LW Ch5,[3]
    LV.Y $3, InBuffer
    LV.X $3, InBuffer      RET                RET
    .ORG ('&',Baseline)    {'}                {(}  { ) is taken by some of *'s code }
    .MACRO Ch5 5
    .MACRO Res4 4

  (****** Handlers for ',' and '-' and '.' ******)

(* The '.' handler uses a variant of the "double-dabble" algorithm to convert
 * from binary to decimal. The buffer "PrtBuf[5]" contains the digits of the
 * output number, with PrtBuf[0] being the low-order digit. PrtBuf[4] may end
 * up being 10, 11, 12, or 13; we don't bother making a sixth array element
 * for a digit that's only ever 0 or 1.
 * . First, output "-" if the number is negative. If its negation is still
 * negative, it's INT_MIN (-131072); print that magic number separately. If
 * the input's not negative, it might be zero; print that magic number separately.
 * Otherwise, go to "PrtMain". There, initialize PrtBuf, and repeat the
 * "double-dabble" step 17 times. Each "double" pulls the leftmost bit from
 * the input into the rightmost bit of the input, and then propagates a base-10
 * carry bit ("dabbling") by incrementing PrtBuf[i+1] if PrtBuf[i] >= 10d.
 * That all takes place in the main column, org '-'.
 * . When that's done 17 times, "PrtBuf" will contain the correct decimal output.
 * In the left column --- org ',' --- first print a "1" if PrtBuf[5] >= 10d;
 * otherwise, iterate downward until you find PrtBuf[i] != 0. (You must find
 * such an index, because if the input was zero, it was handled already.)
 * Then dump the rest of the digits from PrtBuf[i] down to PrtBuf[0].
 * . Finally, print a single space after the number.
 *)

GOS                   TRP 0
LI $3, #10d           DEC $1,$1
SUB.s 3,7,3           LV.y $PC, PrtComp
SZ.y $3               SZ $5
LV.y $PC, .+(0,6)     DEC.s $5,$5
LI $4, '1'            SW.y $7, [--$6]
SMR.x $4, #OUTPUT     INC.s 7,7
MR 7,3                SNZ.y $3
LV.y $PC, .+(0,4)     SHL.s 7,7
LW.y $7, [$6]         LW.y $7, [--$6]   {4}
INV.y 6,6             SW $7, [$6]
SNZ $7                MR 7,3
LV.y $PC, .-(0,4)     SNZ.y $3
LI $5, '0'            SUB.s 3,7,3
ADD.x 7,5,7           LI $3, #10d
SMR.x $7, #OUTPUT     INC.s 7,7
SZ $0                 SNZ.y $3
WORD PrtBuf+(0,1)     SHL.s 7,7
LW $3, [PC-DPC]       LW $7, [$6]
XOR 3,3,6             DEV.y 6,6         {3}
SNZ $3                SW $7, [$6]
LV.y $PC, ..y+27d     MR 7,3
LW.y $7, [$6]         SNZ.y $3
INV.y 6,6             SUB.s 3,7,3
LV.y $PC, ..y-11d     LI $3, #10d 
TRP 0                 INC.s 7,7
                      SNZ.y $3
                      SHL.s 7,7
                      LW $7, [$6]
                      DEV.y 6,6         {2}
                      SW $7, [$6]
                      MR 7,3
                      SNZ.y $3
                      SUB.s 3,7,3
                      LI $3, #10d   
                      INC.s 7,7
                      SNZ.y $3
                      SHL.s 7,7
                      LW $7, [$6]
                      DEV.y 6,6         {1}
                      SW $7, [$6]
                      MR 7,3
                      SNZ.y $3
                      SUB.s 3,7,3
                      LI $3, #10d 
                      INC.s 7,7
                      SZ.y $3
                      AND 3,3,4
TRP 0                 SHL.s 7,7
LV.y $PC, ..y+2       LW $7, [$6]       {0}
WORD 400000           LW $3, [-$PC]      
WORD PrtBuf           LW $6, [-$PC]      
LW $7, [++$PC]        SHL.s 4,4            TRP 0
LW $6, [++$PC]        WORD 7               DEC $1,$1            .EQU PrtComp ..y
LI $3, ' '            WORD 6               SW $7, [--$PC]
SMR.x $3, #OUTPUT     WORD 0               SW $6, [--$PC]
RET                   WORD 0               SW $0, [--$PC]
                      WORD 0               SW $0, [--$PC]
GOS                   WORD 0               SW $0, [--$PC]
LV.Y $4, '13'         .EQU PrtBuf .        SW $0, [--$PC]
SMR.v $4, #OUTPUT     DEV $1,$1            SW $0, [--$PC]
LV.X $4, '0'          LV.X $4, '13'        LI $5, #17d
SMR.v $4, #OUTPUT     INC $1,$1            RET                  .EQU PrtMain ..y
LI $4, '7'            SNZ.Y $3             SMR.V $5, #OUTPUT
SMR.x $4, #OUTPUT     AND 3,4,3            LV.Y $5, '0 '
LV.X $4, '2 '         NEG.s 4,4            LV.X $5, '0 '
LV.Y $4, '2 '         SMR.X $5, #OUTPUT    LV.Y $PC, PrtMain
SMR.v $4, #OUTPUT     LI $5, '-'           SZ $4
RET                                        DEC $1,$1
SMR.x $4, #OUTPUT                          SZ.Y $3
AND 4,4,5             CALLV Push4          AND 3,4,3
LI $5, 0xff           SUB.s 4,4,5          LV $3, 400
CALLV Pop4            CALLV Pop45          CALLV Pop4
.ORG (',', Baseline)  .ORG ('-',Baseline)  .ORG ('.',Baseline)  .ORG ('/',Baseline)


