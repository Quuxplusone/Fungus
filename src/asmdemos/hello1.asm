
  .ENTRY .+(0,1)    .ORG (2,0)
  GOE  LI $3, 'H'   SMR.x $3, #1  GOS
  GOS  NOP          NOP           GOW
  GOE  LI $3, 'e'   SMR.x $3, #1  GOS
  GOS  NOP          NOP           GOW
  GOE  LI $3, 'l'   SMR.x $3, #1  GOS
  GOS  NOP          NOP           GOW
  GOE  LI $3, 'l'   SMR.x $3, #1  GOS
  GOS  NOP          NOP           GOW
  GOE  LI $3, 'o'   SMR.x $3, #1  GOS
  GOS  NOP          NOP           GOW
  GOE  LI $3, ' '   SMR.x $3, #1  GOS
  GOS  NOP          NOP           GOW
  GOE  LI $3, 'w'   SMR.x $3, #1  GOS
  GOS  NOP          NOP           GOW
  GOE  LI $3, 'o'   SMR.x $3, #1  GOS
  GOS  NOP          NOP           GOW
  GOE  LI $3, 'r'   SMR.x $3, #1  GOS
  GOS  NOP          NOP           GOW
  GOE  LI $3, 'l'   SMR.x $3, #1  GOS
  GOS  NOP          NOP           GOW
  GOE  LI $3, 'd'   SMR.x $3, #1  GOS
  GOS  NOP          NOP           GOW
  GOE  LI $3, '\n'  SMR.x $3, #1  SMR $0, #2
