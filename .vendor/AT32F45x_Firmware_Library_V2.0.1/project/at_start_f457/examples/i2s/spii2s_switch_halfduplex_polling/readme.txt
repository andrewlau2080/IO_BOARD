/**
  **************************************************************************
  * @file     readme.txt
  * @brief    readme
  **************************************************************************
  */

  this demo is based on the at-start board, in this demo, shows in halfduplex
  mode how to use polling transfer data. use the mode switch to realize spi and
  i2s communication.
  the pins connection as follow:
  - i2s2                      i2s1
    pb12(ws)        <--->     pa4(ws)
    pb13(sck)       <--->     pa5(sck)
    pb15(sd)        <--->     pa7(sd)
  for more detailed information. please refer to the application note document AN0102.
