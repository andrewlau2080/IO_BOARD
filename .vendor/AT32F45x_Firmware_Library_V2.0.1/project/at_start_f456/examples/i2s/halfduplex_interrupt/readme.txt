/**
  **************************************************************************
  * @file     readme.txt
  * @brief    readme
  **************************************************************************
  */

  this demo is based on the at-start board, in this demo, shows in halfduplex
  mode how to use interrupt transfer data.
  the pins connection as follow:
  - pb6 as mck out
  - i2s2 slaver              i2s1 master
    pb12(ws)        <--->     pa4(ws)
    pb13(sck)       <--->     pa5(sck)
    pb15(sd)        <--->     pa7(sd)

  for more detailed information. please refer to the application note document AN0102.
