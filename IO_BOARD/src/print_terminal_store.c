#include "print_terminal_store.h"

#include "at32f45x.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* These two sectors are reserved exclusively for the print host.  The next
 * three sectors remain the existing tester device/WiFi copies and learned
 * harness recipe:
 *
 *   0x0807D800 print-host copy A
 *   0x0807E000 print-host copy B
 *   0x0807E800 tester device/WiFi copy A
 *   0x0807F000 tester device/WiFi copy B
 *   0x0807F800 learned harness recipe
 */
#define PRINT_TERMINAL_STORE_SLOT_A_ADDR  0x0807D800U
#define PRINT_TERMINAL_STORE_SLOT_B_ADDR  0x0807E000U
#define PRINT_TERMINAL_STORE_MAGIC        0x31545350U /* "PST1" */
#define PRINT_TERMINAL_STORE_VERSION      2U
#define PRINT_TERMINAL_STORE_COMMIT       0x54494D43U /* "CMIT" */
#define PRINT_TERMINAL_STORE_UID1_ADDR    0x1FFFF7E8U
#define PRINT_TERMINAL_STORE_UID2_ADDR    0x1FFFF7ECU
#define PRINT_TERMINAL_STORE_UID3_ADDR    0x1FFFF7F0U
#define PRINT_TERMINAL_STORE_SECTOR_SIZE  2048U

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t sequence;
  uint32_t uid_words[3];
  print_terminal_store_config_t config;
  uint32_t crc32;
  uint32_t commit_magic;
} print_terminal_store_image_t;

_Static_assert(sizeof(print_terminal_store_image_t) <= PRINT_TERMINAL_STORE_SECTOR_SIZE,
               "print terminal store exceeds its reserved Flash sector");
_Static_assert((sizeof(print_terminal_store_image_t) % sizeof(uint32_t)) == 0U,
               "print terminal store must be word aligned");

volatile uint32_t g_print_terminal_store_save_count;
volatile uint32_t g_print_terminal_store_save_error_count;
volatile uint8_t g_print_terminal_store_loaded;

static print_terminal_store_config_t current_config;
static uint32_t current_sequence;
static uint32_t current_slot_addr;

static uint32_t store_crc32(const void *data, size_t size)
{
  const uint8_t *bytes = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFU;
  size_t index;
  uint8_t bit;

  if(data == 0) {
    return 0U;
  }

  for(index = 0U; index < size; index++) {
    crc ^= bytes[index];
    for(bit = 0U; bit < 8U; bit++) {
      if((crc & 1U) != 0U) {
        crc = (crc >> 1U) ^ 0xEDB88320U;
      } else {
        crc >>= 1U;
      }
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

static void store_read_uid(uint32_t uid[3])
{
  if(uid == 0) {
    return;
  }
  uid[0] = *(const volatile uint32_t *)PRINT_TERMINAL_STORE_UID1_ADDR;
  uid[1] = *(const volatile uint32_t *)PRINT_TERMINAL_STORE_UID2_ADDR;
  uid[2] = *(const volatile uint32_t *)PRINT_TERMINAL_STORE_UID3_ADDR;
}

static uint8_t store_text_valid(const char *text, uint16_t capacity,
                                uint8_t allow_empty)
{
  uint16_t index;

  if(text == 0 || capacity == 0U) {
    return 0U;
  }
  for(index = 0U; index < capacity; index++) {
    unsigned char value = (unsigned char)text[index];

    if(value == '\0') {
      return (allow_empty != 0U || index != 0U) ? 1U : 0U;
    }
    if(value < 0x20U || value == 0x7FU) {
      return 0U;
    }
  }
  return 0U;
}

static uint8_t store_wifi_mac_valid(const char *mac)
{
  uint8_t index;

  if(mac == 0) {
    return 0U;
  }
  if(mac[0] == '\0') {
    return 1U;
  }
  for(index = 0U; index < PRINT_TERMINAL_WIFI_MAC_MAX - 1U; index++) {
    char value = mac[index];
    if(value == '\0') return 0U;
    if(index == 2U || index == 5U || index == 8U || index == 11U || index == 14U) {
      if(value != ':') return 0U;
    } else if(!((value >= '0' && value <= '9') || (value >= 'A' && value <= 'F') ||
                (value >= 'a' && value <= 'f'))) {
      return 0U;
    }
  }
  return (mac[PRINT_TERMINAL_WIFI_MAC_MAX - 1U] == '\0') ? 1U : 0U;
}

static uint8_t store_printer_config_valid(const print_driver_config_t *config)
{
  if(config == 0 || config->label_width_dot < 200U ||
     config->label_width_dot > 2400U || config->label_length_dot < 120U ||
     config->label_length_dot > 2400U || config->origin_x_dot > 1200U ||
     config->origin_y_dot > 1200U || config->rotation > 3U ||
     config->print_speed_ips < 1U || config->print_speed_ips > 14U ||
     config->print_darkness > 30U || config->title_font_dot < 12U ||
     config->title_font_dot > 120U || config->body_font_dot < 10U ||
     config->body_font_dot > 100U || config->footer_font_dot < 10U ||
     config->footer_font_dot > 100U || config->barcode_module_width < 1U ||
     config->barcode_module_width > 10U || config->barcode_ratio < 2U ||
     config->barcode_ratio > 3U || config->barcode_height_dot < 20U ||
     config->barcode_height_dot > 300U) {
    return 0U;
  }
  return 1U;
}

static uint8_t store_serial_config_valid(const print_rs485_config_t *config)
{
  if(config == 0 || config->data_bits != 8U || config->stop_bits != 1U ||
     config->parity != 0U || config->direction_enabled > 1U ||
     config->direction_active_high > 1U) {
    return 0U;
  }
  switch(config->baudrate) {
  case 1200UL:
  case 2400UL:
  case 4800UL:
  case 9600UL:
  case 19200UL:
  case 38400UL:
  case 57600UL:
  case 115200UL:
    return 1U;
  default:
    return 0U;
  }
}

static uint8_t store_job_valid(const print_job_t *job)
{
  if(job == 0 || job->station_id == 0U || job->station_id > 10U ||
     job->quantity == 0U || job->copies == 0U || job->pass > 1U) {
    return 0U;
  }
  return (store_text_valid(job->title, sizeof(job->title), 1U) != 0U &&
          store_text_valid(job->item, sizeof(job->item), 1U) != 0U &&
          store_text_valid(job->content, sizeof(job->content), 1U) != 0U &&
          store_text_valid(job->code, sizeof(job->code), 1U) != 0U) ? 1U : 0U;
}

static uint8_t store_sequence_is_newer(uint32_t candidate, uint32_t reference)
{
  return ((int32_t)(candidate - reference) > 0) ? 1U : 0U;
}

uint8_t print_terminal_store_validate(const print_terminal_store_config_t *config)
{
  uint8_t index;

  if(config == 0 || config->active_template >= PRINT_TERMINAL_TEMPLATE_COUNT ||
     config->wifi_listen_port == 0U || config->ir_fallback_enabled > 1U ||
     store_text_valid(config->controller_name,
                      sizeof(config->controller_name), 0U) == 0U ||
     store_text_valid(config->line_id, sizeof(config->line_id), 1U) == 0U ||
     store_text_valid(config->wifi_ssid, sizeof(config->wifi_ssid), 1U) == 0U ||
     store_text_valid(config->wifi_password,
                      sizeof(config->wifi_password), 1U) == 0U ||
     store_wifi_mac_valid(config->wifi_mac) == 0U ||
     store_printer_config_valid(&config->driver_config) == 0U ||
     store_serial_config_valid(&config->printer_config) == 0U) {
    return 0U;
  }

  for(index = 0U; index < PRINT_TERMINAL_TEMPLATE_COUNT; index++) {
    if(store_text_valid(config->templates[index].name,
                        sizeof(config->templates[index].name), 0U) == 0U ||
       store_job_valid(&config->templates[index].job) == 0U) {
      return 0U;
    }
  }
  return 1U;
}

static void store_config_default(print_terminal_store_config_t *config)
{
  uint8_t index;

  if(config == 0) {
    return;
  }
  memset(config, 0, sizeof(*config));
  (void)snprintf(config->controller_name,
                 sizeof(config->controller_name), "PRINT-HOST");
  (void)snprintf(config->line_id, sizeof(config->line_id), "L01");
  config->wifi_listen_port = 5001U;
  config->active_template = 0U;
  config->ir_fallback_enabled = 1U;

  config->driver_config.label_width_dot = 600U;
  config->driver_config.label_length_dot = 360U;
  config->driver_config.origin_x_dot = 0U;
  config->driver_config.origin_y_dot = 0U;
  config->driver_config.rotation = 0U;
  config->driver_config.dark_mode = 0U;
  config->driver_config.print_speed_ips = 4U;
  config->driver_config.print_darkness = 15U;
  config->driver_config.title_font_dot = 34U;
  config->driver_config.body_font_dot = 28U;
  config->driver_config.footer_font_dot = 26U;
  config->driver_config.barcode_module_width = 2U;
  config->driver_config.barcode_ratio = 2U;
  config->driver_config.barcode_height_dot = 70U;
  config->driver_config.barcode_human_readable = 1U;

  config->printer_config.baudrate = PRINT_RS485_BAUDRATE;
  config->printer_config.data_bits = 8U;
  config->printer_config.stop_bits = 1U;
  config->printer_config.parity = 0U;
  config->printer_config.direction_enabled = 0U;
  config->printer_config.direction_active_high = 1U;

  for(index = 0U; index < PRINT_TERMINAL_TEMPLATE_COUNT; index++) {
    (void)snprintf(config->templates[index].name,
                   sizeof(config->templates[index].name), "T%u",
                   (unsigned int)(index + 1U));
    print_job_init_default(&config->templates[index].job);
    (void)snprintf(config->templates[index].job.code,
                   sizeof(config->templates[index].job.code),
                   "CODE%06u", (unsigned int)(index + 1U));
  }
}

static uint8_t store_image_valid(const print_terminal_store_image_t *image)
{
  uint32_t uid[3];
  uint32_t crc;

  if(image == 0 || image->magic != PRINT_TERMINAL_STORE_MAGIC ||
     image->version != PRINT_TERMINAL_STORE_VERSION ||
     image->commit_magic != PRINT_TERMINAL_STORE_COMMIT ||
     print_terminal_store_validate(&image->config) == 0U) {
    return 0U;
  }
  crc = store_crc32(image, offsetof(print_terminal_store_image_t, crc32));
  if(crc != image->crc32) {
    return 0U;
  }
  store_read_uid(uid);
  return (image->uid_words[0] == uid[0] && image->uid_words[1] == uid[1] &&
          image->uid_words[2] == uid[2]) ? 1U : 0U;
}

void print_terminal_store_init(void)
{
  const print_terminal_store_image_t *image_a =
    (const print_terminal_store_image_t *)PRINT_TERMINAL_STORE_SLOT_A_ADDR;
  const print_terminal_store_image_t *image_b =
    (const print_terminal_store_image_t *)PRINT_TERMINAL_STORE_SLOT_B_ADDR;
  const print_terminal_store_image_t *selected = 0;
  uint32_t selected_addr = 0U;

  store_config_default(&current_config);
  current_sequence = 0U;
  current_slot_addr = 0U;
  g_print_terminal_store_loaded = 0U;
  g_print_terminal_store_save_count = 0U;
  g_print_terminal_store_save_error_count = 0U;

  if(store_image_valid(image_a) != 0U) {
    selected = image_a;
    selected_addr = PRINT_TERMINAL_STORE_SLOT_A_ADDR;
  }
  if(store_image_valid(image_b) != 0U &&
     (selected == 0 || store_sequence_is_newer(image_b->sequence,
                                                selected->sequence) != 0U)) {
    selected = image_b;
    selected_addr = PRINT_TERMINAL_STORE_SLOT_B_ADDR;
  }
  if(selected != 0) {
    memcpy(&current_config, &selected->config, sizeof(current_config));
    current_sequence = selected->sequence;
    current_slot_addr = selected_addr;
    g_print_terminal_store_loaded = 1U;
  }
}

const print_terminal_store_config_t *print_terminal_store_get(void)
{
  return &current_config;
}

void print_terminal_store_copy(print_terminal_store_config_t *out_config)
{
  if(out_config != 0) {
    memcpy(out_config, &current_config, sizeof(*out_config));
  }
}

uint8_t print_terminal_store_is_stored(void)
{
  return g_print_terminal_store_loaded;
}

uint8_t print_terminal_store_save(const print_terminal_store_config_t *config)
{
  print_terminal_store_image_t image;
  const uint32_t *words;
  uint32_t target_addr;
  uint32_t word_count;
  uint32_t index;
  uint32_t primask;
  flash_status_type status;

  if(print_terminal_store_validate(config) == 0U) {
    g_print_terminal_store_save_error_count++;
    return 0U;
  }

  memset(&image, 0, sizeof(image));
  image.magic = PRINT_TERMINAL_STORE_MAGIC;
  image.version = PRINT_TERMINAL_STORE_VERSION;
  image.sequence = current_sequence + 1U;
  if(image.sequence == 0U) {
    image.sequence = 1U;
  }
  store_read_uid(image.uid_words);
  memcpy(&image.config, config, sizeof(image.config));
  image.crc32 = store_crc32(&image,
                            offsetof(print_terminal_store_image_t, crc32));
  image.commit_magic = PRINT_TERMINAL_STORE_COMMIT;

  target_addr = (current_slot_addr == PRINT_TERMINAL_STORE_SLOT_A_ADDR) ?
                PRINT_TERMINAL_STORE_SLOT_B_ADDR : PRINT_TERMINAL_STORE_SLOT_A_ADDR;
  words = (const uint32_t *)&image;
  word_count = (uint32_t)(offsetof(print_terminal_store_image_t, commit_magic) /
                          sizeof(uint32_t));

  primask = __get_PRIMASK();
  __disable_irq();
  flash_unlock();
  status = flash_sector_erase(target_addr);
  if(status == FLASH_OPERATE_DONE) {
    for(index = 0U; index < word_count; index++) {
      status = flash_word_program(target_addr + (index * sizeof(uint32_t)),
                                  words[index]);
      if(status != FLASH_OPERATE_DONE) {
        break;
      }
    }
  }
  if(status == FLASH_OPERATE_DONE) {
    status = flash_word_program(target_addr + (word_count * sizeof(uint32_t)),
                                image.commit_magic);
  }
  flash_lock();
  if(primask == 0U) {
    __enable_irq();
  }

  if(status != FLASH_OPERATE_DONE ||
     store_image_valid((const print_terminal_store_image_t *)target_addr) == 0U) {
    g_print_terminal_store_save_error_count++;
    return 0U;
  }

  memcpy(&current_config, config, sizeof(current_config));
  current_sequence = image.sequence;
  current_slot_addr = target_addr;
  g_print_terminal_store_loaded = 1U;
  g_print_terminal_store_save_count++;
  return 1U;
}

void print_terminal_store_apply_runtime(void)
{
  (void)print_driver_config_set(&current_config.driver_config);
  print_rs485_config_set(&current_config.printer_config);
}

uint8_t print_terminal_store_save_runtime_config(
  const print_driver_config_t *driver_config,
  const print_rs485_config_t *printer_config)
{
  print_terminal_store_config_t updated;

  if(driver_config == 0 || printer_config == 0) {
    return 0U;
  }
  print_terminal_store_copy(&updated);
  updated.driver_config = *driver_config;
  updated.printer_config = *printer_config;
  return print_terminal_store_save(&updated);
}

uint8_t print_terminal_store_update_wifi_mac(const char *mac)
{
  print_terminal_store_config_t updated;

  if(store_wifi_mac_valid(mac) == 0U) {
    return 0U;
  }
  if(strcmp(current_config.wifi_mac, mac) == 0) {
    return 1U;
  }
  print_terminal_store_copy(&updated);
  (void)snprintf(updated.wifi_mac, sizeof(updated.wifi_mac), "%s", mac);
  return print_terminal_store_save(&updated);
}

uint8_t print_terminal_store_load_template(uint8_t index, print_job_t *out_job)
{
  if(out_job == 0 || index >= PRINT_TERMINAL_TEMPLATE_COUNT) {
    return 0U;
  }
  memcpy(out_job, &current_config.templates[index].job, sizeof(*out_job));
  return 1U;
}
