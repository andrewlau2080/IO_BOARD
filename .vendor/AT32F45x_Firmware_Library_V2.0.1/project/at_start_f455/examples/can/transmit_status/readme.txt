/**
  **************************************************************************
  * @file     readme.txt
  * @brief    readme
  **************************************************************************
  */

  this demo is based on the at-start board and at32-comm-ev, in this demo, 
  shows how to get the status of the frame transmissions. 
  at initialization, set re-transmission and re-arbitration times limit.
  every 1s transmit one message and print txbuf handle and the led4 blink, 
  when transmission of the transmit buffer be completed, 
  led3 blink(the transmission is ok) or led2 blink(the transmission is fail), 
  and print transmit status.
  set-up
  - can_tx(pd1)
  - can_rx(pd0)
  - can_stb(pb7)

  for more detailed information. please refer to the application note document AN0232.
