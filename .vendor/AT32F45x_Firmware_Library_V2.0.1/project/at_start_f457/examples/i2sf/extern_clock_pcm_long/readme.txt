/**
  **************************************************************************
  * @file     readme.txt
  * @brief    readme
  **************************************************************************
  */

  this demo is based on the at-start board, in this demo, shows how to provide extern clock
  to i2sf module.
  the pins distribution as follow:
  - fullduplex i2s
  - i2sf5 master          spi i2s slaver
  - pb6(ws)      <--->     pb12(ws)
  - pb7(ck)      <--->     pb13(ck) 
  - pb9(sd)      <--->     pb15(sd)
  - pb8(sdext)   <--->     pb14(sdext)
  - pc8(mck) 
  - pa2(ckin)
    note: communication start when external clock is ready on pa2

  for more detailed information. please refer to the application note document AN0102.
