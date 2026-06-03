/**
  **************************************************************************
  * @file     readme.txt
  * @brief    readme
  **************************************************************************
  */

  this demo is based on the at-start board and at32-comm-ev, in this demo, 
  shows how to use the can filter funciton. the can tool transmit 2 group specified 
  messages in total (1 group extended-id messages and 1 group standard-id messages), 
  when mcu receive 1 group expect extended-id message, led2 will toggle. 
  when mcu receive 1 group expect standard-id message, led3 will toggle.
  set-up
  - can_tx(pd1)
  - can_rx(pd0)
  - can_stb(pb7)

  for more detailed information. please refer to the application note document AN0232.

