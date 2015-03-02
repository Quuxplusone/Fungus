
(* This is an implementation of the "double-dabble" algorithm for
 * converting from base 2 to base 10 (binary-coded decimal). By
 * an amazing coincidence, 18 bits is just enough space to convert
 * a nine-bit binary number into a three-digit decimal number: the
 * input comes in on bits 0-8, and we produce the output on bits
 * 6-17 (twelve bits, or three BCD digits).
 *
 * Fungus' lack of a multi-bit shift operation really shines here.
 * Its inability to swap the half-words of a register means that
 * we don't even have a 9-bit right shift. (We do have a 9-bit
 * left shift, but this algorithm doesn't care.)
 *
 * Extending the program to deal with full 18-bit words, requiring
 * six BCD digits (24 bits), is left as an exercise for the reader.
 *)

  .MACRO OUT 1
  .MACRO EXIT 2


  (* This is the main program. Replace "123d" with your number. *)
  .ORG (0,0)     GOE    LI $4, 123d   TRP 'D'   SMR $0, #EXIT
  .ENTRY (1,0)   {^- entry point}


 (*********************************************************************
   Step 7 times. At each step, add 00600 if ((S >> 7) & 0xF) .ge. 5,
                               add 14000 if ((S >> 11) & 0xF) .ge. 5,
                               and then shift S := S << 1.
   Input: ........... 11 1111111
   Step 1: ......... 111 1111110
   Step 2: ...... 1 0101 1111100
   Step 3: ..... 11 0001 1111000
   Step 4: .... 110 0011 1110000
   Step 5: . 1 0010 0111 1100000
   Step 6: .10 0101 0101 1000000
   Output: 101 0001 0001 0000000
   After the 7th step, print (S >> 15), (S >> 11), and (S >> 7).
 *********************************************************************)

                (* The double-dabble loop. *)
  GOE  GOS
  NOP  SHR.s $5, $4
  NOP  GOE  SHR.s 5,5    LV.Y $5, #0     SHR.s 5,5      SHR.s 5,5     NOP        GOS
  NOP  GOS  ADD.s 4,4,5  LW.Y $5, [--5]  SHR.s 5,5      SHR.s 5,5     SHR.s 5,5  GOW
  NOP  SHL.s $5, $4
  NOP  GOE  SHL.s 5,5    SHL.s 5,5       SHR.s 5,5      SHR.s 5,5     SHR.s 5,5  GOS
  NOP  GOS  ADD.s 4,4,5  LW $5, [~$5]    LV.X $5, -1    SHR.s 5,5     SHR.s 5,5  GOW
  NOP  SHL.s $4, $4
  NOP  DEC.s $Count, $Count
  NOP  SZ $Count
  GON  GOW
  NOP  NOP      (* Now it's time to print the digits. *)
  NOP  NOP
  NOP  GOE  SHR.s 5,4    SHR.s 5,5       SHR.s 5,5      SHR.s 5,5     SHR.s 5,5  GOS
  NOP  GOS  SHR.s 5,4    SMR.Y 5, #OUT   ADD.Y 5,3,5    LV.Y $3, '0'  SHR.s 5,5  GOW
  NOP  GOE  SHR.s 5,5    LV.Y $3, 0xF    AND.Y 5,3,5    LV.Y $3, '0'  GOS
  NOP  GOS  SHL.s 5,5    SHL.s 5,4       SMR.Y 5, #OUT  ADD.Y 5,3,5   GOW
  NOP  GOE  LV.Y $3, 17  AND.Y 5,3,5     LV.Y $3, '0'   NOP           GOS
  NOP  GOS  RET          SMR.v 5, #OUT   LV.X 5, '\n'   ADD.Y 5,3,5   GOW
  NOP
  LI.s $Count, 7         .MACRO Count 3
  .ORG ('D', 0)


  WORD 14000
  WORD 14000  (* The (vertical) lookup table for adding 014000 if ((S >> 11) & 0xF) .ge. 5 *)
  WORD 14000
  WORD 14000
  WORD 14000      { These tables are in the lower left corner of RAM.
  WORD 0            The entry point is in the upper left, and the
  WORD 0             TRP 'D' subroutine is over to our right. }
  WORD 0
  WORD 0  (* The lookup table for adding 0600 if ((S >> 7) & 0xF) .ge. 5. *)
  WORD 0  WORD 0  WORD 0  WORD 0  WORD 0  WORD 600  WORD 600  WORD 600  WORD 600  WORD 600
  .ORG (0,0)



