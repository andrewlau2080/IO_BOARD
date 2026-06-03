/**
  **************************************************************************
  * @file     readme.txt
  * @brief    readme
  **************************************************************************
  */

  this demo is based on the at-start board, in this demo, shows in fullduplex
  mode how to use dma transfer data.
  the pins connection as follow:
  - pb6 as mck out
  - i2s2 master              i2s3 slaver
    pb12(ws)        <--->     pa4(ws)
    pb13(sck)       <--->     pc10(sck)
    pb15(sd)        <--->     pc12(sd)
    pb14(sdext)     <--->     pc11(sdext)

  for more detailed information. please refer to the application note document AN0102.
