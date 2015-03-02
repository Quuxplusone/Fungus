(* Usage:
 *     simfunge.exe printme.elf hello.txt
 * The program prints the contents of funge-space until
 * it encounters a blank line (two newlines in a row).
 * Files of more than 510 lines or containing lines
 * longer than 511 characters obviously won't work right.
 *)

  .EQU OUTPUT 001
  .EQU PRGMEXIT 002
  .ENTRY (0,0)
  .FILL '\n'

  .ORG (0,777)
  .SIZE (776,1)
  .FILL 160000    { GOE }


                      SMR.x $0, #PRGMEXIT
  GOS                 GOW
  LI $5, 1            SNZ $5  { GOW if we haven't seen two '\n's in a row }
  LV.y $PC, Skip      LV.x $7, #777
  NOP                 INV.y 7,7
  GOS                 GOW                          
  LI $5, #0           SZ $3   { GOW if $3 is not '\n' }  .EQU Skip ..y
  ADD 3,3,4           SUB 3,3,4
  SMR.x $3, #OUTPUT   LI $4, '\n'
  RET                 LW $3, [$7]
  WORD (1,0)          LW $6, [-PC] { go east when we return }
  GOE                 GON
                      .ORG (777,0)
