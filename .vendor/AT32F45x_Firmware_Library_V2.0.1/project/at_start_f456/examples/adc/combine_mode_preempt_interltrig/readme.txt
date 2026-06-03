/**
  **************************************************************************
  * @file     readme.txt
  * @brief    readme
  **************************************************************************
  */

  this demo is based on the at-start board, in this demo, shows how to use
  combine_mode(preempt interleaved trigger mode).
  the trigger source is software
  the convert data as follow:
  - adc1_preempt_valuetab[0] ---> adc1_channel_2
  - adc1_preempt_valuetab[1] ---> adc2_channel_3
  - adc1_preempt_valuetab[2] ---> adc2_channel_4
  
  - adc2_preempt_valuetab[0] ---> adc1_channel_5
  - adc2_preempt_valuetab[1] ---> adc2_channel_6
  - adc2_preempt_valuetab[2] ---> adc2_channel_7

  for more detailed information. please refer to the application note document AN0242.
