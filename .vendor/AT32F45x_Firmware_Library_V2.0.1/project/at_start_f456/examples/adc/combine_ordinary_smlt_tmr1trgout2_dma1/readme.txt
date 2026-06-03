/**
  **************************************************************************
  * @file     readme.txt
  * @brief    readme
  **************************************************************************
  */

  this demo is based on the at-start board, in this demo, shows how to use
  combine_mode(ordinary simultaneous mode only).
  the trigger source is: tmr1trgout2,,and use dma mode 1 transfer conversion data
  the convert data as follow:
  - adccom_ordinary_valuetab[0] ---> adc1_channel_2
  - adccom_ordinary_valuetab[1] ---> adc2_channel_5
  
  - adccom_ordinary_valuetab[2] ---> adc1_channel_3
  - adccom_ordinary_valuetab[3] ---> adc2_channel_6

  - adccom_ordinary_valuetab[4] ---> adc1_channel_4
  - adccom_ordinary_valuetab[5] ---> adc2_channel_7

  for more detailed information. please refer to the application note document AN0242.
