/**
  **************************************************************************
  * @file     readme.txt
  * @brief    readme
  **************************************************************************
  */

  this demo is based on the at-start board, in this demo, shows how to use
  combine_mode(ordinary shift mode only).
  the trigger source is: tmr1trgout,,and use dma mode 2 transfer conversion data
  the convert data as follow:
  - adccom_ordinary_valuetab[n] ---> (adc2_channel_5<<16) | adc1_channel_2

  for more detailed information. please refer to the application note document AN0242.
