#include "device_config.h"

#include "at32f45x.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* AT32F455VET7 has 2 KiB Flash sectors.  The linker deliberately stops before
 * these two configuration sectors and the following learned-recipe sector:
 *
 *   0x0807E800  configuration copy A (+ WiFi MAC record at +0x100)
 *   0x0807F000  configuration copy B (+ WiFi MAC record at +0x100)
 *   0x0807F800  first-generation learned harness recipe
 *
 * Writing a new copy always erases/writes the inactive sector first.  A power
 * loss therefore leaves the former complete copy intact. */
#define DEVICE_CONFIG_SLOT_A_ADDR         0x0807E800U
#define DEVICE_CONFIG_SLOT_B_ADDR         0x0807F000U
#define DEVICE_CONFIG_MAC_RECORD_OFFSET   0x100U
#define DEVICE_CONFIG_MAGIC               0x31474643U /* "CFG1" */
#define DEVICE_CONFIG_VERSION             1U
#define DEVICE_CONFIG_COMMIT_MAGIC        0x434D4954U /* "CMIT" */
#define DEVICE_CONFIG_MAC_MAGIC           0x3143414DU /* "MAC1" */
#define DEVICE_CONFIG_MAC_VERSION         1U
#define DEVICE_CONFIG_MAC_COMMIT_MAGIC    0x434D4143U /* "CMAC" */

#define DEVICE_CONFIG_MCU_UID1_ADDR       0x1FFFF7E8U
#define DEVICE_CONFIG_MCU_UID2_ADDR       0x1FFFF7ECU
#define DEVICE_CONFIG_MCU_UID3_ADDR       0x1FFFF7F0U

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t sequence;
  uint32_t uid_words[3];
  device_config_t config;
  uint32_t crc32;
  uint32_t commit_magic;
} device_config_flash_image_t;

/* Keep the learned ESP-AT MAC independent from the editable configuration
 * image.  The record lives in the same configuration sector, but outside the
 * image above, and is copied to the new inactive sector on every config save.
 * This preserves both atomic config copies without changing version-1 layout. */
typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t sequence;
  uint32_t uid_words[3];
  char mac[DEVICE_CONFIG_WIFI_MAC_TEXT_MAX];
  uint8_t reserved[2];
  uint32_t crc32;
  uint32_t commit_magic;
} device_config_wifi_mac_flash_image_t;

_Static_assert((sizeof(device_config_flash_image_t) <=
                DEVICE_CONFIG_MAC_RECORD_OFFSET),
               "WiFi MAC record overlaps device config image");
_Static_assert((sizeof(device_config_wifi_mac_flash_image_t) %
                sizeof(uint32_t)) == 0U,
               "WiFi MAC record must be word aligned");

static device_config_t current_config;
static uint32_t current_sequence;
static uint32_t current_slot_addr;
static uint8_t current_stored;
static char current_wifi_mac[DEVICE_CONFIG_WIFI_MAC_TEXT_MAX];
static uint8_t current_wifi_mac_valid;

static uint32_t device_config_crc32(const void *data, size_t data_size)
{
  const uint8_t *bytes = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFU;
  size_t index;
  uint8_t bit;

  if(data == 0) {
    return 0U;
  }

  for(index = 0U; index < data_size; index++) {
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

static void device_config_read_uid(uint32_t uid_words[3])
{
  if(uid_words == 0) {
    return;
  }

  uid_words[0] = *(const volatile uint32_t *)DEVICE_CONFIG_MCU_UID1_ADDR;
  uid_words[1] = *(const volatile uint32_t *)DEVICE_CONFIG_MCU_UID2_ADDR;
  uid_words[2] = *(const volatile uint32_t *)DEVICE_CONFIG_MCU_UID3_ADDR;
}

static uint8_t device_config_is_hex_digit(char value)
{
  return (((value >= '0' && value <= '9') ||
           (value >= 'A' && value <= 'F') ||
           (value >= 'a' && value <= 'f')) ? 1U : 0U);
}

static uint8_t device_config_wifi_mac_is_valid(const char *mac)
{
  uint8_t index;

  if(mac == 0) {
    return 0U;
  }

  /* ESP-AT returns the conventional six-byte colon form.  Validate it before
   * committing anything so a partial UART line can never become persistent
   * identity data. */
  for(index = 0U; index < (DEVICE_CONFIG_WIFI_MAC_TEXT_MAX - 1U); index++) {
    if(mac[index] == '\0') {
      return 0U;
    }
    if((index == 2U || index == 5U || index == 8U ||
        index == 11U || index == 14U)) {
      if(mac[index] != ':') {
        return 0U;
      }
    } else if(device_config_is_hex_digit(mac[index]) == 0U) {
      return 0U;
    }
  }

  return (mac[DEVICE_CONFIG_WIFI_MAC_TEXT_MAX - 1U] == '\0') ? 1U : 0U;
}

static const device_config_wifi_mac_flash_image_t *
device_config_wifi_mac_image_at(uint32_t slot_addr)
{
  return (const device_config_wifi_mac_flash_image_t *)
         (slot_addr + DEVICE_CONFIG_MAC_RECORD_OFFSET);
}

static uint8_t device_config_wifi_mac_image_is_valid(
  const device_config_wifi_mac_flash_image_t *image)
{
  uint32_t uid_words[3];
  uint32_t crc;

  if(image == 0 ||
     image->magic != DEVICE_CONFIG_MAC_MAGIC ||
     image->version != DEVICE_CONFIG_MAC_VERSION ||
     image->commit_magic != DEVICE_CONFIG_MAC_COMMIT_MAGIC ||
     device_config_wifi_mac_is_valid(image->mac) == 0U) {
    return 0U;
  }

  crc = device_config_crc32(image,
                            offsetof(device_config_wifi_mac_flash_image_t,
                                     crc32));
  if(crc != image->crc32) {
    return 0U;
  }

  device_config_read_uid(uid_words);
  if(image->uid_words[0] != uid_words[0] ||
     image->uid_words[1] != uid_words[1] ||
     image->uid_words[2] != uid_words[2]) {
    return 0U;
  }

  return 1U;
}

static uint8_t device_config_text_is_valid(const char *text, uint8_t text_size)
{
  uint8_t index;

  if(text == 0 || text_size == 0U) {
    return 0U;
  }

  for(index = 0U; index < text_size; index++) {
    unsigned char value = (unsigned char)text[index];

    if(value == '\0') {
      return 1U;
    }
    /* Keep UTF-8/GBK bytes intact for a factory AP name while rejecting
     * control delimiters that would break LCDM or ESP-AT command framing. */
    if(value < 0x20U || value == 0x7FU) {
      return 0U;
    }
  }

  return 0U;
}

static uint8_t device_config_service_host_is_valid(const char *host, uint8_t host_size)
{
  uint8_t index;

  if(host == 0 || host_size == 0U) {
    return 0U;
  }
  if(host[0] == '\0') {
    return 1U;
  }

  /* ESP-AT TCP client accepts a normal IPv4 address or DNS host name.  Keep
   * the stored endpoint deliberately narrow: spaces, quotes, slashes and a
   * port suffix would either make AT+CIPSTART ambiguous or hide a bad setup.
   */
  for(index = 0U; index < host_size; index++) {
    unsigned char value = (unsigned char)host[index];

    if(value == '\0') {
      return 1U;
    }
    if(!((value >= '0' && value <= '9') ||
         (value >= 'A' && value <= 'Z') ||
         (value >= 'a' && value <= 'z') ||
         value == '.' || value == '-')) {
      return 0U;
    }
  }

  return 0U;
}

static void device_config_make_default(device_config_t *config)
{
  uint32_t uid_words[3];

  if(config == 0) {
    return;
  }

  memset(config, 0, sizeof(*config));
  device_config_read_uid(uid_words);
  (void)snprintf(config->machine_id,
                 sizeof(config->machine_id),
                 "IOB-%04lX",
                 (unsigned long)(uid_words[2] & 0xFFFFU));
  config->wifi_dhcp = 1U;
}

uint8_t device_config_validate(const device_config_t *config)
{
  if(config == 0) {
    return 0U;
  }

  if(device_config_text_is_valid(config->machine_id,
                                 sizeof(config->machine_id)) == 0U ||
     config->machine_id[0] == '\0' ||
     device_config_text_is_valid(config->station_id,
                                 sizeof(config->station_id)) == 0U ||
     device_config_text_is_valid(config->line_id,
                                 sizeof(config->line_id)) == 0U ||
     device_config_text_is_valid(config->wifi_ssid,
                                 sizeof(config->wifi_ssid)) == 0U ||
     device_config_text_is_valid(config->wifi_password,
                                 sizeof(config->wifi_password)) == 0U ||
     device_config_text_is_valid(config->service_host,
                                 sizeof(config->service_host)) == 0U) {
    return 0U;
  }

  if(config->wifi_dhcp > 1U) {
    return 0U;
  }

  if(device_config_service_host_is_valid(config->service_host,
                                         sizeof(config->service_host)) == 0U ||
     ((config->service_host[0] == '\0' && config->service_port != 0U) ||
      (config->service_host[0] != '\0' && config->service_port == 0U))) {
    return 0U;
  }

  return 1U;
}

static uint8_t device_config_image_is_valid(const device_config_flash_image_t *image)
{
  uint32_t uid_words[3];
  uint32_t crc;

  if(image == 0 ||
     image->magic != DEVICE_CONFIG_MAGIC ||
     image->version != DEVICE_CONFIG_VERSION ||
     image->commit_magic != DEVICE_CONFIG_COMMIT_MAGIC) {
    return 0U;
  }

  crc = device_config_crc32(image, offsetof(device_config_flash_image_t, crc32));
  if(crc != image->crc32 || device_config_validate(&image->config) == 0U) {
    return 0U;
  }

  device_config_read_uid(uid_words);
  if(image->uid_words[0] != uid_words[0] ||
     image->uid_words[1] != uid_words[1] ||
     image->uid_words[2] != uid_words[2]) {
    return 0U;
  }

  return 1U;
}

static uint8_t device_config_sequence_is_newer(uint32_t candidate, uint32_t reference)
{
  return ((int32_t)(candidate - reference) > 0) ? 1U : 0U;
}

static void device_config_load_wifi_mac(void)
{
  const device_config_wifi_mac_flash_image_t *image_a =
    device_config_wifi_mac_image_at(DEVICE_CONFIG_SLOT_A_ADDR);
  const device_config_wifi_mac_flash_image_t *image_b =
    device_config_wifi_mac_image_at(DEVICE_CONFIG_SLOT_B_ADDR);
  const device_config_wifi_mac_flash_image_t *selected = 0;

  current_wifi_mac[0] = '\0';
  current_wifi_mac_valid = 0U;

  if(device_config_wifi_mac_image_is_valid(image_a) != 0U) {
    selected = image_a;
  }
  if(device_config_wifi_mac_image_is_valid(image_b) != 0U &&
     (selected == 0 ||
      device_config_sequence_is_newer(image_b->sequence,
                                      selected->sequence) != 0U)) {
    selected = image_b;
  }

  if(selected != 0) {
    memcpy(current_wifi_mac, selected->mac, sizeof(current_wifi_mac));
    current_wifi_mac[DEVICE_CONFIG_WIFI_MAC_TEXT_MAX - 1U] = '\0';
    current_wifi_mac_valid = 1U;
  }
}

void device_config_init(void)
{
  const device_config_flash_image_t *image_a =
    (const device_config_flash_image_t *)DEVICE_CONFIG_SLOT_A_ADDR;
  const device_config_flash_image_t *image_b =
    (const device_config_flash_image_t *)DEVICE_CONFIG_SLOT_B_ADDR;
  const device_config_flash_image_t *selected = 0;
  uint32_t selected_addr = 0U;

  current_sequence = 0U;
  current_slot_addr = 0U;
  current_stored = 0U;
  current_wifi_mac[0] = '\0';
  current_wifi_mac_valid = 0U;
  device_config_make_default(&current_config);

  if(device_config_image_is_valid(image_a) != 0U) {
    selected = image_a;
    selected_addr = DEVICE_CONFIG_SLOT_A_ADDR;
  }
  if(device_config_image_is_valid(image_b) != 0U &&
     (selected == 0 || device_config_sequence_is_newer(image_b->sequence,
                                                        selected->sequence) != 0U)) {
    selected = image_b;
    selected_addr = DEVICE_CONFIG_SLOT_B_ADDR;
  }

  if(selected != 0) {
    memcpy(&current_config, &selected->config, sizeof(current_config));
    current_sequence = selected->sequence;
    current_slot_addr = selected_addr;
    current_stored = 1U;
  }

  /* The MAC cache is deliberately loaded independently of the editable
   * configuration record.  An old version-1 config remains fully usable even
   * when no MAC has been learned yet. */
  device_config_load_wifi_mac();
}

const device_config_t *device_config_get(void)
{
  return &current_config;
}

uint8_t device_config_is_stored(void)
{
  return current_stored;
}

void device_config_copy(device_config_t *out_config)
{
  if(out_config != 0) {
    memcpy(out_config, &current_config, sizeof(*out_config));
  }
}

void device_config_get_uid_words(uint32_t uid_words[3])
{
  device_config_read_uid(uid_words);
}

void device_config_format_uid(char *text, uint8_t text_size)
{
  uint32_t uid_words[3];

  if(text == 0 || text_size == 0U) {
    return;
  }

  device_config_read_uid(uid_words);
  (void)snprintf(text,
                 text_size,
                 "%08lX%08lX%08lX",
                 (unsigned long)uid_words[0],
                 (unsigned long)uid_words[1],
                 (unsigned long)uid_words[2]);
}

void device_config_format_short_code(char *text, uint8_t text_size)
{
  uint32_t uid_words[3];

  if(text == 0 || text_size == 0U) {
    return;
  }

  device_config_read_uid(uid_words);
  (void)snprintf(text,
                 text_size,
                 "%04lX",
                 (unsigned long)(uid_words[2] & 0xFFFFU));
}

uint8_t device_config_station_number(void)
{
  const char *text = current_config.station_id;
  uint16_t value = 0U;
  uint8_t digits = 0U;

  /* Production labels use both ST-01 and WT-01 conventions.  Keep the
   * compact numeric station value independent of the human-facing prefix. */
  if((text[0] == 'S' || text[0] == 'W') && text[1] == 'T') {
    text += 2;
    if(*text == '-') {
      text++;
    }
  }
  while(*text >= '0' && *text <= '9') {
    value = (uint16_t)((value * 10U) + (uint16_t)(*text - '0'));
    if(value > 255U) {
      return 0U;
    }
    text++;
    digits++;
  }

  if(digits == 0U || *text != '\0' || value == 0U) {
    return 0U;
  }
  return (uint8_t)value;
}

static uint8_t device_config_save_internal(const device_config_t *config,
                                           const char *wifi_mac,
                                           uint8_t wifi_mac_valid)
{
  device_config_flash_image_t image;
  device_config_wifi_mac_flash_image_t mac_image;
  uint32_t target_addr;
  const uint32_t *words;
  uint32_t word_index;
  uint32_t word_count;
  flash_status_type status;
  uint32_t primask;

  if(device_config_validate(config) == 0U ||
     (wifi_mac_valid != 0U &&
      device_config_wifi_mac_is_valid(wifi_mac) == 0U)) {
    return 0U;
  }

  memset(&image, 0, sizeof(image));
  image.magic = DEVICE_CONFIG_MAGIC;
  image.version = DEVICE_CONFIG_VERSION;
  image.sequence = current_sequence + 1U;
  if(image.sequence == 0U) {
    image.sequence = 1U;
  }
  device_config_read_uid(image.uid_words);
  memcpy(&image.config, config, sizeof(image.config));
  image.crc32 = device_config_crc32(&image,
                                    offsetof(device_config_flash_image_t, crc32));
  image.commit_magic = DEVICE_CONFIG_COMMIT_MAGIC;

  memset(&mac_image, 0, sizeof(mac_image));
  if(wifi_mac_valid != 0U) {
    mac_image.magic = DEVICE_CONFIG_MAC_MAGIC;
    mac_image.version = DEVICE_CONFIG_MAC_VERSION;
    mac_image.sequence = image.sequence;
    device_config_read_uid(mac_image.uid_words);
    memcpy(mac_image.mac, wifi_mac, sizeof(mac_image.mac));
    mac_image.crc32 = device_config_crc32(
      &mac_image,
      offsetof(device_config_wifi_mac_flash_image_t, crc32));
    mac_image.commit_magic = DEVICE_CONFIG_MAC_COMMIT_MAGIC;
  }

  target_addr = (current_slot_addr == DEVICE_CONFIG_SLOT_A_ADDR) ?
                DEVICE_CONFIG_SLOT_B_ADDR : DEVICE_CONFIG_SLOT_A_ADDR;
  words = (const uint32_t *)&image;
  word_count = (uint32_t)(offsetof(device_config_flash_image_t, commit_magic) /
                          sizeof(uint32_t));

  primask = __get_PRIMASK();
  __disable_irq();
  flash_unlock();
  status = flash_sector_erase(target_addr);
  if(status == FLASH_OPERATE_DONE) {
    for(word_index = 0U; word_index < word_count; word_index++) {
      status = flash_word_program(target_addr + (word_index * sizeof(uint32_t)),
                                  words[word_index]);
      if(status != FLASH_OPERATE_DONE) {
        break;
      }
    }
  }
  if(status == FLASH_OPERATE_DONE) {
    status = flash_word_program(target_addr + (word_count * sizeof(uint32_t)),
                                image.commit_magic);
  }

  /* Commit the MAC companion record only after the configuration image is
   * complete.  If power is lost earlier, the old config/MAC slot remains the
   * last fully committed pair. */
  if(status == FLASH_OPERATE_DONE && wifi_mac_valid != 0U) {
    words = (const uint32_t *)&mac_image;
    word_count = (uint32_t)(offsetof(device_config_wifi_mac_flash_image_t,
                                     commit_magic) / sizeof(uint32_t));
    for(word_index = 0U; word_index < word_count; word_index++) {
      status = flash_word_program(target_addr + DEVICE_CONFIG_MAC_RECORD_OFFSET +
                                  (word_index * sizeof(uint32_t)),
                                  words[word_index]);
      if(status != FLASH_OPERATE_DONE) {
        break;
      }
    }
    if(status == FLASH_OPERATE_DONE) {
      status = flash_word_program(target_addr + DEVICE_CONFIG_MAC_RECORD_OFFSET +
                                  (word_count * sizeof(uint32_t)),
                                  mac_image.commit_magic);
    }
  }
  flash_lock();
  if(primask == 0U) {
    __enable_irq();
  }

  if(status != FLASH_OPERATE_DONE ||
     device_config_image_is_valid((const device_config_flash_image_t *)target_addr) == 0U) {
    return 0U;
  }
  if(wifi_mac_valid != 0U &&
     device_config_wifi_mac_image_is_valid(device_config_wifi_mac_image_at(target_addr)) == 0U) {
    return 0U;
  }

  memcpy(&current_config, config, sizeof(current_config));
  current_sequence = image.sequence;
  current_slot_addr = target_addr;
  current_stored = 1U;
  if(wifi_mac_valid != 0U) {
    memcpy(current_wifi_mac, wifi_mac, sizeof(current_wifi_mac));
    current_wifi_mac[DEVICE_CONFIG_WIFI_MAC_TEXT_MAX - 1U] = '\0';
    current_wifi_mac_valid = 1U;
  } else {
    current_wifi_mac[0] = '\0';
    current_wifi_mac_valid = 0U;
  }
  return 1U;
}

uint8_t device_config_save(const device_config_t *config)
{
  return device_config_save_internal(config,
                                     current_wifi_mac_valid != 0U ?
                                       current_wifi_mac : 0,
                                     current_wifi_mac_valid);
}

uint8_t device_config_get_wifi_mac(char *text, uint8_t text_size)
{
  if(text == 0 || text_size == 0U) {
    return 0U;
  }

  text[0] = '\0';
  if(current_wifi_mac_valid == 0U) {
    return 0U;
  }

  (void)snprintf(text, text_size, "%s", current_wifi_mac);
  return 1U;
}

uint8_t device_config_save_wifi_mac(const char *mac)
{
  if(device_config_wifi_mac_is_valid(mac) == 0U) {
    return 0U;
  }

  return device_config_save_internal(&current_config, mac, 1U);
}
