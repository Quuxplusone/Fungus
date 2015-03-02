
  .FILL 0
  .EQU #OUT 1
  .EQU #EXIT 2

  .ENTRY .+(0,1)         GOS       SMR.x $4, #OUT  NOP      GOW    {Got a character.}
  GOE   LW $3, [++$PC]   GOE       LW $4, [$3]     INC 3,3  DZ.Y $4 
  .ORG (10,10)           WORD data                          TRP #EXIT


  {The trap handler for TRP 0.}
  SMR $0, #EXIT
  .ORG (#EXIT,0)



  .EQU data .+(0,1)   .ORG (42d,42d)
  WORD 'H'  WORD 'e'  WORD 'l'  WORD 'l'  WORD 'o'  WORD ','  WORD ' '  WORD 'w'  WORD 'o'  WORD 'r'  WORD 'l'  WORD 'd'  WORD '!'  WORD '\n'  WORD '\0'
