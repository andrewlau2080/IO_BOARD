/**
  **************************************************************************
  * @file     readme.txt
  * @brief    readme
  **************************************************************************
  */

  this demo is based on the at-start board, in this demo, shows how to use
  combine_mode(ordinary simultaneous mode only).
  the trigger source is: tmr1ch1,and use dma mode 3 transfer conversion data
  the convert data as follow:
  - adccom_ordinary_valuetab[0] ---> (adc2_channel_5<<8)  | adc1_channel_2
  - adccom_ordinary_valuetab[1] ---> (adc2_channel_6<<8)  | adc1_channel_3
  - adccom_ordinary_valuetab[2] ---> (adc2_channel_7<<8)  | adc1_channel_4

  for more detailed information. please refer to the application note document AN0242.
