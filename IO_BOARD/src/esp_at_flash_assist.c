#include "esp_at_flash_assist.h"

#include "at32f45x_board.h"

#define ESP_AT_FLASH_ASSIST_EN_PIN    GPIO_PINS_3
#define ESP_AT_FLASH_ASSIST_BOOT_PIN  GPIO_PINS_9

#define ESP_AT_FLASH_ASSIST_RESET_HOLD_MS     150U
#define ESP_AT_FLASH_ASSIST_BOOT_SETTLE_MS    150U

static void esp_at_flash_assist_gpio_init(void)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);

  /* Load zero into both output latches before enabling the drivers.  With
   * open drain this first holds EN and BOOT low, safely forcing the ESP into
   * its ROM UART downloader on the following EN rising edge. */
  gpio_bits_reset(GPIOC, ESP_AT_FLASH_ASSIST_EN_PIN);
  gpio_bits_reset(GPIOB, ESP_AT_FLASH_ASSIST_BOOT_PIN);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_OPEN_DRAIN;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;

  gpio_init_struct.gpio_pins = ESP_AT_FLASH_ASSIST_EN_PIN;
  gpio_init(GPIOC, &gpio_init_struct);

  gpio_init_struct.gpio_pins = ESP_AT_FLASH_ASSIST_BOOT_PIN;
  gpio_init(GPIOB, &gpio_init_struct);
}

void esp_at_flash_assist_init(void)
{
  esp_at_flash_assist_gpio_init();

  /* Keep IO9 low throughout reset, then release EN.  ESP32-C3 samples this
   * strap on reset release and enters the built-in UART download protocol. */
  delay_ms(ESP_AT_FLASH_ASSIST_RESET_HOLD_MS);
  gpio_bits_set(GPIOC, ESP_AT_FLASH_ASSIST_EN_PIN);
  delay_ms(ESP_AT_FLASH_ASSIST_BOOT_SETTLE_MS);

  /* The ROM has sampled IO9 by now.  Release it to restore its normal
   * pull-up level while keeping the ESP alive in download mode. */
  gpio_bits_set(GPIOB, ESP_AT_FLASH_ASSIST_BOOT_PIN);
}
