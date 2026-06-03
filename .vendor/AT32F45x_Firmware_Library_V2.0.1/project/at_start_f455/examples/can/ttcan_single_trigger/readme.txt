/**
  **************************************************************************
  * @file     readme.txt
  * @brief    readme
  **************************************************************************
  */

  this demo is based on the at-start board and at32-comm-ev, in this demo, 
  shows how to use single shot transmit trigger of the ttcan trigger mode. 
  each cycle trigger once and transmit one message.
  1. the single shot transmit trigger time(320ms) is set and the led4 toggle 
  when the watch trigger time(80ms) is up.
  2. the led2 toggle when the trigger time(320ms) is up and the transmission 
  had completed. 
  3. the led3 toggle when received reference message, and cycle time is reset.
  
  the cycle time(0 ~ 0xFFFF) is as follows:
  0 --> 80ms(the watch trigger time) --> 320ms(the trigger time) --> 
  524.28ms(overflow auto return 0)
  
  set-up
  - can_tx(pd1)
  - can_rx(pd0)
  - can_stb(pb7)

  for more detailed information. please refer to the application note document AN0232.
