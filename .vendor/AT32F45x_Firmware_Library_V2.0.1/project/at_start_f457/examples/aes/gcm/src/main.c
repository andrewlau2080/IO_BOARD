/**
  **************************************************************************
  * @file     main.c
  * @brief    main program
  **************************************************************************
  *
  * Copyright (c) 2025, Artery Technology, All rights reserved.
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */

#include "at32f45x_board.h"
#include "at32f45x_clock.h"
#include <string.h>

/** @addtogroup AT32F457_periph_examples
  * @{
  */

/** @addtogroup 457_AES_GCM
  * @{
  */

#define AAD_LENGTH                       28   /* 0 means no aad stage*/
#define DATA_LENGTH                      450  /* data length is limited by the buffer address range below, maximum 4096 */
#define AES_GCM_IV_COUNT0                (uint32_t)(0x00000010) /* gcm initial vector count */

#define ORIGINAL_BUF_ADDR                (uint32_t)(0x20001000)
#define ENCRYPT_BUF_ADDR                 (uint32_t)(0x20002000)
#define DECRYPT_BUF_ADDR                 (uint32_t)(0x20003000)
#define AAD_BUF_ADDR                     (uint32_t)(0x20004000)
static const uint8_t plaintext_data[] =
{
  0xc3,0x3d,0xd3,0x1c, 0xe3,0x7f,0xf3,0x5e, 0x12,0x90,0x22,0xf3, 0x32,0xd2,0x42,0x35, 0x52,0x14,0x62,0x77, 0x72,0x56,0xb5,0xea,
  0x4a,0x75,0x5a,0x54, 0x6a,0x37,0x7a,0x16, 0x0a,0xf1,0x1a,0xd0, 0x2a,0xb3,0x3a,0x92, 0xed,0x0f,0xdd,0x6c, 0xcd,0x4d,0xbd,0xaa,
  0xbb,0x3b,0xab,0x1a, 0x6c,0xa6,0x7c,0x87, 0x5c,0xc5,0x2c,0x22, 0x3c,0x03,0x0c,0x60, 0x1c,0x41,0xed,0xae, 0xfd,0x8f,0xcd,0xec,
  0xad,0x8b,0x9d,0xe8, 0x8d,0xc9,0x7c,0x26, 0x5c,0x64,0x4c,0x45, 0x3c,0xa2,0x2c,0x83, 0x1c,0xe0,0x0c,0xc1, 0xef,0x1f,0xff,0x3e,
  0x95,0xa8,0x85,0x89, 0xf5,0x6e,0xe5,0x4f, 0xd5,0x2c,0xc5,0x0d, 0x34,0xe2,0x24,0xc3, 0x04,0x81,0x74,0x66, 0x64,0x47,0x54,0x24,
  0x78,0x06,0x68,0x27, 0x18,0xc0,0x08,0xe1, 0x28,0xa3,0xcb,0x7d, 0xdb,0x5c,0xeb,0x3f, 0xfb,0x1e,0x8b,0xf9, 0x9b,0xd8,0xab,0xbb,
  0xdf,0x7c,0xaf,0x9b, 0xbf,0xba,0x8f,0xd9, 0x9f,0xf8,0x6e,0x17, 0x7e,0x36,0x4e,0x55, 0x2e,0x93,0x3e,0xb2, 0x0e,0xd1,0x1e,0xf0,
  0xa3,0x5a,0xd3,0xbd, 0xc3,0x9c,0xf3,0xff, 0xe3,0xde,0x24,0x62, 0x34,0x43,0x04,0x20, 0x64,0xe6,0x74,0xc7, 0x44,0xa4,0x54,0x85,
  0xad,0x2a,0xbd,0x0b, 0x8d,0x68,0x9d,0x49, 0x7e,0x97,0x6e,0xb6, 0x5e,0xd5,0x4e,0xf4, 0x2e,0x32,0x1e,0x51, 0x0e,0x70,0xff,0x9f,
  0xef,0xbe,0xdf,0xdd, 0xcf,0xfc,0xbf,0x1b, 0x9f,0x59,0x8f,0x78, 0x91,0x88,0x81,0xa9, 0xb1,0xca,0xa1,0xeb, 0xd1,0x0c,0xc1,0x2d,
  0xe1,0x6f,0x10,0x80, 0x00,0xa1,0x30,0xc2, 0x20,0xe3,0x50,0x04, 0x40,0x25,0x70,0x46, 0x83,0xb9,0x93,0x98, 0xa3,0xfb,0xb3,0xda,
  0x00,0x00,0x10,0x21, 0x20,0x42,0x30,0x63, 0x40,0x84,0x50,0xa5, 0x60,0xc6,0x70,0xe7, 0x91,0x29,0xa1,0x4a, 0xb1,0x6b,0xc1,0x8c,
  0x56,0x95,0x46,0xb4, 0xb7,0x5b,0xa7,0x7a, 0x97,0x19,0x87,0x38, 0xf7,0xdf,0xe7,0xfe, 0xc7,0xbc,0x48,0xc4, 0x58,0xe5,0x68,0x86,
  0x44,0x05,0xa7,0xdb, 0xb7,0xfa,0x87,0x99, 0xe7,0x5f,0xf7,0x7e, 0xc7,0x1d,0xd7,0x3c, 0x26,0xd3,0x36,0xf2, 0x06,0x91,0x16,0xb0,
  0x76,0x76,0x46,0x15, 0x56,0x34,0xd9,0x4c, 0xc9,0x6d,0xf9,0x0e, 0xe9,0x2f,0x99,0xc8, 0xb9,0x8a,0xa9,0xab, 0x58,0x44,0x48,0x65,
  0x78,0xa7,0x08,0x40, 0x18,0x61,0x28,0x02, 0xc9,0xcc,0xd9,0xed, 0xe9,0x8e,0xf9,0xaf, 0x89,0x48,0x99,0x69, 0xa9,0x0a,0xb9,0x2b,
  0xd1,0xad,0xe1,0xce, 0xf1,0xef,0x12,0x31, 0x32,0x73,0x22,0x52, 0x52,0xb5,0x42,0x94, 0x72,0xf7,0x62,0xd6, 0x93,0x39,0x83,0x18,
  0xa5,0x6a,0xb5,0x4b, 0x85,0x28,0x95,0x09, 0xf5,0xcf,0xc5,0xac, 0xd5,0x8d,0x36,0x53, 0x26,0x72,0x16,0x11, 0x06,0x30,0x76,0xd7,
  0x8d,0x68,0x9d,0x49, 0xf7,0xdf,0xe7,0xfe, 0xe9,0x8e,0xf9,0xaf, 0x06,0x30,0x76,0xd7, 0x93,0x39,0x83,0x18, 0xb9,0x8a,0xa9,0xab,
  0x4a,0xd4,0x7a,0xb7, 0x6a,0x96,0x1a,0x71, 0x0a,0x50,0x3a,0x33, 0x2a,0x12,0xdb,0xfd, 0xfb,0xbf,0xeb,0x9e, 0x9b,0x79,0x8b,0x58, 
};

static const uint8_t aad_data[48] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
  0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
};
uint32_t aad_len = AAD_LENGTH;
uint8_t *aad_buf = (uint8_t *)AAD_BUF_ADDR;
uint8_t *original_buf = (uint8_t *)ORIGINAL_BUF_ADDR;
uint8_t *encrypt_buf = (uint8_t *)ENCRYPT_BUF_ADDR;
uint8_t *decrypt_buf = (uint8_t *)DECRYPT_BUF_ADDR;
uint32_t iv_buf[4] = {0};
uint32_t key_buf[4] = {0};
uint32_t encrypt_tag_buf[4];
uint32_t decrypt_tag_buf[4];

static void final_block_process(uint32_t aad_len, uint32_t data_len, uint8_t* final_buf);
void aes_encrypt_process(uint8_t* encrypt_buf, uint8_t* original_buf, uint32_t buf_len, uint8_t* tag_buf);
void aes_decrypt_process(uint8_t* decrypt_buf, uint8_t* encrypt_buf, uint32_t buf_len, uint8_t* tag_buf);

/**
  * @brief  process final block buf.
  * @param  aad_len: additional authenticated data length
  *         data_len: encrypt data length.
  *         final_buf: the processed final block buf.
  * @retval none
  */
static void final_block_process(uint32_t aad_len, uint32_t data_len, uint8_t* final_buf)
{
  uint64_t temp_val;
  uint8_t i;
  temp_val = aad_len * 8;
  for(i = 0; i < 8; i++)
  {
    final_buf[i] = (temp_val >> (8 * (7 - i))) & 0xFF;
  }
  temp_val = data_len * 8;
  final_buf += 8;
  for(i = 0; i < 8; i++)
  {
    final_buf[i] = (temp_val >> (8 * (7 - i))) & 0xFF;
  } 
}

/**
  * @brief  aes process encrypt
  * @param  encrypt_buf: the result encrypted buffer.
  *         original_buf: the original buffer will be encrypt.
  *         buf_len: the buffer length will be encrypt.
  *         tag_buf: the tag buffer.
  * @retval none
  */
void aes_encrypt_process(uint8_t* encrypt_buf, uint8_t* original_buf, uint32_t buf_len, uint8_t* tag_buf)
{
  aes_init_type aes_init_struct; 
  uint32_t temp_block_buf[4];
  uint32_t i;
  
  aes_reset();
  /* prepare stage */
  aes_default_para_init(&aes_init_struct);
  aes_init_struct.operate_mode = AES_OPMODE_ENCRYPT;
  aes_init_struct.chaining_mode = AES_CHMODE_GCM;
  aes_init_struct.key_len = AES_KEY_LENGTH_128;
  aes_init_struct.key_buf = (uint32_t *)key_buf;
  aes_init_struct.data_swap_mode = AES_SWAP_TYPE_NONE;
  aes_init(&aes_init_struct);
  aes_suspend_info_init();
  iv_buf[3] = AES_GCM_IV_COUNT0;
  aes_iv_set(iv_buf);

  /* ghash init stage */
  aes_processing_stage_set(AES_PRC_INIT);
  aes_enable(TRUE);
  while(aes_flag_get(AES_PDFS_FLAG) == RESET);
  aes_flag_clear(AES_PDFS_FLAG);
  aes_enable(FALSE);
  
  /* assoc stage */
  if(aad_len)
  {
    aes_processing_stage_set(AES_PRC_ASSOC);
    aes_enable(TRUE);
    for(i = 0; i < aad_len / 16; i++)
    {
      aes_data_input((uint32_t *)aad_buf + i * 4);
      while(aes_flag_get(AES_PDFS_FLAG) == RESET);
      aes_flag_clear(AES_PDFS_FLAG);  
    }
    if(aad_len % 16 != 0)
    {
      aes_dummy_data_num_set(16 - aad_len % 16);
      aes_last_block_enable(TRUE);
      memcpy((uint8_t *)temp_block_buf, (uint8_t *)aad_buf + i * 16, (aad_len % 16));
      memset((uint8_t *)temp_block_buf + (aad_len % 16), 0, 16 - aad_len % 16);
      aes_data_input((uint32_t *)temp_block_buf);
      while(aes_flag_get(AES_PDFS_FLAG) == RESET);
      aes_flag_clear(AES_PDFS_FLAG);
      while(aes_flag_get(AES_NZDFS_FLAG) == RESET);
      aes_flag_clear(AES_NZDFS_FLAG);    
    }
    aes_enable(FALSE);
  }

  /* data stage */
  aes_processing_stage_set(AES_PRC_DATA);
  aes_enable(TRUE);
  for(i = 0; i < buf_len / 16; i++)
  {
    aes_data_input((uint32_t *)original_buf + i * 4);
    while(aes_flag_get(AES_PDFS_FLAG) == RESET);
    aes_flag_clear(AES_PDFS_FLAG); 
    aes_data_output((uint32_t *)encrypt_buf + i * 4); 
  }
  
  /* last block stage */
  if(buf_len % 16 != 0)
  {
    aes_dummy_data_num_set(16 - buf_len % 16);
    aes_last_block_enable(TRUE);   
    memcpy((uint8_t *)temp_block_buf, (uint8_t *)original_buf + i * 16, (buf_len % 16));
    memset((uint8_t *)temp_block_buf + (buf_len % 16), 0, 16 - buf_len % 16);   
    aes_data_input((uint32_t *)temp_block_buf);
    while(aes_flag_get(AES_PDFS_FLAG) == RESET);
    aes_flag_clear(AES_PDFS_FLAG); 
    while(aes_flag_get(AES_NZDFS_FLAG) == RESET);
    aes_flag_clear(AES_NZDFS_FLAG);
    aes_data_output((uint32_t *)encrypt_buf + i * 4);
  }
  aes_enable(FALSE);
  
  /* tag stage */
  aes_processing_stage_set(AES_PRC_TAG);
  aes_enable(TRUE);  
  final_block_process(aad_len, buf_len, (uint8_t *)temp_block_buf);
  aes_data_input((uint32_t *)temp_block_buf);
  while(aes_flag_get(AES_PDFS_FLAG) == RESET);
  aes_flag_clear(AES_PDFS_FLAG);
  aes_data_output((uint32_t *)tag_buf);
  aes_enable(FALSE);
}

/**
  * @brief  aes process decrypt
  * @param  decrypt_buf: the result decrypted buffer.
  *         encrypt_buf: the encrypted buffer will be decrypt.
  *         buf_len: the buffer length will be decrypt.
  *         tag_buf: the tag buffer.
  * @retval none
  */
void aes_decrypt_process(uint8_t* decrypt_buf, uint8_t* encrypt_buf, uint32_t buf_len, uint8_t* tag_buf)
{
  aes_init_type aes_init_struct; 
  uint32_t temp_block_buf[4];
  uint32_t i;
 
  aes_reset();
  /* prepare stage */
  aes_default_para_init(&aes_init_struct);
  aes_init_struct.operate_mode = AES_OPMODE_DECRYPT;
  aes_init_struct.chaining_mode = AES_CHMODE_GCM;
  aes_init_struct.key_len = AES_KEY_LENGTH_128;
  aes_init_struct.key_buf = (uint32_t *)key_buf;
  aes_init_struct.data_swap_mode = AES_SWAP_TYPE_NONE;
  aes_init(&aes_init_struct);
  aes_suspend_info_init();
  iv_buf[3] = AES_GCM_IV_COUNT0;
  aes_iv_set(iv_buf);
 
  /* ghash init stage */
  aes_processing_stage_set(AES_PRC_INIT);
  aes_enable(TRUE);
  while(aes_flag_get(AES_PDFS_FLAG) == RESET);
  aes_flag_clear(AES_PDFS_FLAG);
  aes_enable(FALSE);

  /* assoc stage */
  if(aad_len)
  {
    aes_processing_stage_set(AES_PRC_ASSOC);
    aes_enable(TRUE);
    for(i = 0; i < aad_len / 16; i++)
    {
      aes_data_input((uint32_t *)aad_buf + i * 4);
      while(aes_flag_get(AES_PDFS_FLAG) == RESET);
      aes_flag_clear(AES_PDFS_FLAG);  
    }
    if(aad_len % 16 != 0)
    {
      aes_dummy_data_num_set(16 - aad_len % 16);
      aes_last_block_enable(TRUE);
      memcpy((uint8_t *)temp_block_buf, (uint8_t *)aad_buf + i * 16, (aad_len % 16));
      memset((uint8_t *)temp_block_buf + (aad_len % 16), 0, 16 - aad_len % 16);
      aes_data_input((uint32_t *)temp_block_buf);
      while(aes_flag_get(AES_PDFS_FLAG) == RESET);
      aes_flag_clear(AES_PDFS_FLAG);
      while(aes_flag_get(AES_NZDFS_FLAG) == RESET);
      aes_flag_clear(AES_NZDFS_FLAG);    
    }
    aes_enable(FALSE);
  }
  
  /* data stage */
  aes_processing_stage_set(AES_PRC_DATA);
  aes_enable(TRUE);
  for(i = 0; i < buf_len / 16; i++)
  {
    aes_data_input((uint32_t *)encrypt_buf + i * 4);
    while(aes_flag_get(AES_PDFS_FLAG) == RESET);
    aes_flag_clear(AES_PDFS_FLAG); 
    aes_data_output((uint32_t *)decrypt_buf + i * 4); 
  }
  
  /* last block stage */
  if(buf_len % 16 != 0)
  {
    aes_dummy_data_num_set(16 - buf_len % 16);
    aes_last_block_enable(TRUE);   
    memcpy((uint8_t *)temp_block_buf, (uint8_t *)encrypt_buf + i * 16, (buf_len % 16));
    memset((uint8_t *)temp_block_buf + (buf_len % 16), 0, 16 - buf_len % 16);   
    aes_data_input((uint32_t *)temp_block_buf);
    while(aes_flag_get(AES_PDFS_FLAG) == RESET);
    aes_flag_clear(AES_PDFS_FLAG); 
    while(aes_flag_get(AES_NZDFS_FLAG) == RESET);
    aes_flag_clear(AES_NZDFS_FLAG);
    aes_data_output((uint32_t *)decrypt_buf + i * 4);
  }
  aes_enable(FALSE);

  /* tag stage */
  aes_processing_stage_set(AES_PRC_TAG);
  aes_enable(TRUE);
  final_block_process(aad_len, buf_len, (uint8_t *)temp_block_buf);
  aes_data_input((uint32_t *)temp_block_buf);
  while(aes_flag_get(AES_PDFS_FLAG) == RESET);
  aes_flag_clear(AES_PDFS_FLAG);
  aes_data_output((uint32_t *)tag_buf);
  aes_enable(FALSE);
}

/**
  * @brief  main function.
  * @param  none
  * @retval none
  */
int main(void)
{
  uint8_t i = 0, err = 0;

  system_clock_config();

  at32_board_init();

  /* enable aes/trng clock */
  crm_periph_clock_enable(CRM_AES_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_TRNG_PERIPH_CLOCK, TRUE);

  /* enable trng noise source */
  trng_enable(TRUE);

  /* get the iv_buf */
  while(trng_flag_get(TRNG_DTRDY_FLAG) == RESET);
  for(i = 0; i < 3; i++)
  {
    iv_buf[i] = trng_value_get();
  }

  /* get the key_buf */
  while(trng_flag_get(TRNG_DTRDY_FLAG) == RESET);
  for(i = 0; i < 4; i++)
  {
    key_buf[i] = trng_value_get();
  }

  /* init aad buf and data buf */
  memcpy(aad_buf, aad_data ,AAD_LENGTH);
  memcpy(original_buf, plaintext_data ,DATA_LENGTH);
  memset(encrypt_buf, 0 ,DATA_LENGTH);
  memset(decrypt_buf, 0 ,DATA_LENGTH);

  /* data encrypt */
  aes_encrypt_process((uint8_t *)encrypt_buf, (uint8_t *)original_buf, DATA_LENGTH, (uint8_t *)encrypt_tag_buf);

  /* data decrypt */
  aes_decrypt_process((uint8_t *)decrypt_buf, (uint8_t *)encrypt_buf, DATA_LENGTH, (uint8_t *)decrypt_tag_buf);

  if(memcmp(decrypt_buf, original_buf, DATA_LENGTH) != RESET)
  {
    err = 1;
  }
 
  if(memcmp(encrypt_tag_buf, decrypt_tag_buf, 16) != RESET)
  {
    err = 1;
  }

  while(1)
  {
    if(err)
    {
      /* result error toggle led2 */
      at32_led_toggle(LED2);
      delay_ms(50);
    }
    else
    {
      /* result correct toggle led4 */
      at32_led_toggle(LED4);
      delay_ms(200);
    }
  }
}

/**
  * @}
  */

/**
  * @}
  */
