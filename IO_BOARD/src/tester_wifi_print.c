#include "tester_wifi_print.h"

#include "at32f45x.h"
#include "at32f45x_exint.h"
#include "at32f45x_scfg.h"
#include "device_config.h"

#include <stdio.h>
#include <string.h>

/* The 2026-07-07-N board connects the ESP32-C3 as a network coprocessor:
 *   PC3 / WIFI_TX -> ESP32 RXD0
 *   PB9 / WIFI_RX <- ESP32 TXD0
 * These pins deliberately remain GPIO software UART pins.  They are not
 * shared with the LCDM USART or with the legacy IR transceiver. */
#define TESTER_WIFI_TX_GPIO              GPIOC
#define TESTER_WIFI_TX_GPIO_CLOCK        CRM_GPIOC_PERIPH_CLOCK
#define TESTER_WIFI_TX_PIN               GPIO_PINS_3
#define TESTER_WIFI_RX_GPIO              GPIOB
#define TESTER_WIFI_RX_GPIO_CLOCK        CRM_GPIOB_PERIPH_CLOCK
#define TESTER_WIFI_RX_PIN               GPIO_PINS_9
#define TESTER_WIFI_RX_PORT_SOURCE       SCFG_PORT_SOURCE_GPIOB
#define TESTER_WIFI_RX_PIN_SOURCE        SCFG_PINS_SOURCE9
#define TESTER_WIFI_RX_EXINT_LINE        EXINT_LINE_9

/* Production reply frames are newline-delimited JSON carried inside ESP-AT
 * +IPD indications.  The larger line and edge rings leave enough space for a
 * full ACK/DONE message while the foreground 4051 scan continues. */
#define TESTER_WIFI_RX_EDGE_MAX          4096U
#define TESTER_WIFI_RX_EDGE_MASK         (TESTER_WIFI_RX_EDGE_MAX - 1U)
#define TESTER_WIFI_RX_LINE_MAX          256U
#define TESTER_WIFI_EVENT_QUEUE_MAX      8U
#define TESTER_WIFI_AT_LINE_QUEUE_MAX    16U
#define TESTER_WIFI_AT_COMMAND_MAX       192U
#define TESTER_WIFI_TX_FRAME_MAX         320U

#define TESTER_WIFI_AT_TIMEOUT_MS        2500UL
#define TESTER_WIFI_JOIN_TIMEOUT_MS     20000UL
#define TESTER_WIFI_CONNECT_TIMEOUT_MS   7000UL
#define TESTER_WIFI_SEND_TIMEOUT_MS      4000UL
#define TESTER_WIFI_BACKOFF_MS           1000UL
#define TESTER_WIFI_ACK_TIMEOUT_MS      10000UL
#define TESTER_WIFI_DONE_TIMEOUT_MS     60000UL
#define TESTER_WIFI_JOB_MAX_RETRIES         3U

typedef struct {
  tester_wifi_print_event_t type;
  uint32_t event_id;
} tester_wifi_print_event_slot_t;

typedef enum {
  WIFI_ENGINE_STOPPED = 0,
  WIFI_ENGINE_CONFIG_REQUIRED,
  WIFI_ENGINE_BACKOFF,
  WIFI_ENGINE_AT,
  WIFI_ENGINE_SET_STA_MODE,
  WIFI_ENGINE_JOIN_AP,
  WIFI_ENGINE_READ_IP,
  WIFI_ENGINE_SET_MUX,
  WIFI_ENGINE_SET_MODE,
  WIFI_ENGINE_CLOSE_OLD,
  WIFI_ENGINE_CONNECT_HOST,
  WIFI_ENGINE_AP_ONLINE,
  WIFI_ENGINE_ONLINE,
  WIFI_ENGINE_SEND_PROMPT,
  WIFI_ENGINE_SEND_RESULT
} wifi_engine_state_t;

volatile uint32_t g_tester_wifi_print_tx_request_count;
volatile uint32_t g_tester_wifi_print_rx_frame_count;
volatile uint32_t g_tester_wifi_print_rx_error_count;
volatile uint32_t g_tester_wifi_print_rx_overflow_count;
volatile uint8_t g_tester_wifi_print_ready;
volatile uint8_t g_tester_wifi_print_network_state;
volatile uint8_t g_tester_wifi_print_online;
volatile uint8_t g_tester_wifi_print_ap_connected;
volatile uint32_t g_tester_wifi_print_connect_count;
volatile uint32_t g_tester_wifi_print_reconnect_count;
volatile uint32_t g_tester_wifi_print_tx_payload_count;
volatile uint32_t g_tester_wifi_print_tx_retry_count;
volatile uint32_t g_tester_wifi_print_network_error_count;

static uint32_t wifi_bit_cycles;
static uint32_t wifi_cycles_per_ms;
static uint32_t wifi_time_last_cycles;
static uint32_t wifi_time_remainder_cycles;
static uint32_t wifi_time_ms;
static volatile uint32_t wifi_rx_edge_cycles[TESTER_WIFI_RX_EDGE_MAX];
static volatile uint8_t wifi_rx_edge_levels[TESTER_WIFI_RX_EDGE_MAX];
static volatile uint32_t wifi_rx_edge_head;
static uint32_t wifi_rx_edge_tail;
static char wifi_rx_line[TESTER_WIFI_RX_LINE_MAX];
static uint16_t wifi_rx_line_len;
static tester_wifi_print_event_slot_t wifi_event_queue[TESTER_WIFI_EVENT_QUEUE_MAX];
static uint8_t wifi_event_head;
static uint8_t wifi_event_tail;
static char wifi_at_line_queue[TESTER_WIFI_AT_LINE_QUEUE_MAX][TESTER_WIFI_RX_LINE_MAX];
static uint8_t wifi_at_line_head;
static uint8_t wifi_at_line_tail;
static uint8_t wifi_at_capture_enabled;
/* Only WIFI_LINK_DIAG sets this token.  The normal print protocol never
 * treats a bare ESP-AT OK/ERROR text as a print event. */
static uint32_t wifi_link_test_sequence;

static wifi_engine_state_t wifi_engine_state;
static uint8_t wifi_auto_start_enabled;
static uint8_t wifi_restart_after_raw;
static uint8_t wifi_ap_ip_seen;
static uint32_t wifi_state_deadline_ms;

static uint8_t wifi_job_active;
static uint8_t wifi_job_needs_send;
static uint8_t wifi_job_acked;
static uint8_t wifi_job_retry_count;
static uint32_t wifi_job_event_id;
static uint32_t wifi_job_wait_deadline_ms;
static uint16_t wifi_job_frame_len;
static char wifi_job_frame[TESTER_WIFI_TX_FRAME_MAX];

static uint32_t wifi_cycles(void)
{
  return DWT->CYCCNT;
}

static void wifi_wait_until_cycles(uint32_t target_cycles)
{
  while((int32_t)(wifi_cycles() - target_cycles) < 0) {
    __asm volatile("nop");
  }
}

static void wifi_tx_level(uint8_t level)
{
  if(level != 0U) {
    TESTER_WIFI_TX_GPIO->scr = TESTER_WIFI_TX_PIN;
  } else {
    TESTER_WIFI_TX_GPIO->clr = TESTER_WIFI_TX_PIN;
  }
}

static uint8_t wifi_rx_level(void)
{
  /* This runs in the software-UART edge ISR.  Do not call the generic GPIO
   * driver here: at the tester's HICK clock its function-call overhead can
   * consume an entire UART bit period. */
  return ((TESTER_WIFI_RX_GPIO->idt & TESTER_WIFI_RX_PIN) != 0U) ? 1U : 0U;
}

static void wifi_write_byte(uint8_t value)
{
  uint8_t bit;
  uint32_t target_cycles;
  uint32_t primask;

  target_cycles = wifi_cycles();
  primask = __get_PRIMASK();
  __disable_irq();

  wifi_tx_level(0U);
  target_cycles += wifi_bit_cycles;
  wifi_wait_until_cycles(target_cycles);

  for(bit = 0U; bit < 8U; bit++) {
    wifi_tx_level((value & (uint8_t)(1U << bit)) != 0U ? 1U : 0U);
    target_cycles += wifi_bit_cycles;
    wifi_wait_until_cycles(target_cycles);
  }

  wifi_tx_level(1U);
  target_cycles += wifi_bit_cycles;
  wifi_wait_until_cycles(target_cycles);

  if(primask == 0U) {
    __enable_irq();
  }
}

static void wifi_write_bytes(const uint8_t *data, uint16_t length)
{
  uint16_t index;

  if(data == 0) {
    return;
  }
  for(index = 0U; index < length; index++) {
    wifi_write_byte(data[index]);
  }
}

static void wifi_write_text(const char *text)
{
  if(text == 0) {
    return;
  }

  while(*text != '\0') {
    wifi_write_byte((uint8_t)*text);
    text++;
  }
}

static void wifi_time_service(void)
{
  uint32_t now = wifi_cycles();
  uint32_t elapsed_cycles = now - wifi_time_last_cycles;
  uint64_t total_cycles;

  wifi_time_last_cycles = now;
  if(wifi_cycles_per_ms == 0U) {
    return;
  }

  total_cycles = (uint64_t)wifi_time_remainder_cycles + elapsed_cycles;
  wifi_time_ms += (uint32_t)(total_cycles / wifi_cycles_per_ms);
  wifi_time_remainder_cycles = (uint32_t)(total_cycles % wifi_cycles_per_ms);
}

static uint8_t wifi_time_reached(uint32_t deadline_ms)
{
  return (int32_t)(wifi_time_ms - deadline_ms) >= 0 ? 1U : 0U;
}

static uint8_t wifi_parse_uint32(const char *text, uint32_t *out_value)
{
  uint32_t value = 0U;
  uint8_t digits = 0U;

  if(text == 0 || out_value == 0) {
    return 0U;
  }

  while(*text >= '0' && *text <= '9') {
    uint32_t digit = (uint32_t)(*text - '0');

    if(value > ((0xFFFFFFFFUL - digit) / 10UL)) {
      return 0U;
    }
    value = (value * 10U) + digit;
    text++;
    digits++;
    if(digits > 10U) {
      return 0U;
    }
  }

  if(digits == 0U) {
    return 0U;
  }

  *out_value = value;
  return 1U;
}

static uint8_t wifi_frame_event_id(const char *frame, uint32_t *event_id)
{
  const char *value_start;

  if(frame == 0 || event_id == 0) {
    return 0U;
  }

  value_start = strstr(frame, "\"event_id\":");
  if(value_start != 0) {
    return wifi_parse_uint32(value_start + 11U, event_id);
  }

  /* Compact fallback accepted for production bring-up tools:
   * ACK,<event_id>,QUEUED / DONE,<event_id> / ERROR,<event_id>. */
  value_start = strchr(frame, ',');
  if(value_start == 0) {
    return 0U;
  }
  return wifi_parse_uint32(value_start + 1U, event_id);
}

static void wifi_queue_event(tester_wifi_print_event_t type, uint32_t event_id)
{
  uint8_t next_head;

  if(type == TESTER_WIFI_PRINT_EVENT_NONE) {
    return;
  }

  next_head = (uint8_t)((wifi_event_head + 1U) % TESTER_WIFI_EVENT_QUEUE_MAX);
  if(next_head == wifi_event_tail) {
    g_tester_wifi_print_rx_overflow_count++;
    return;
  }

  wifi_event_queue[wifi_event_head].type = type;
  wifi_event_queue[wifi_event_head].event_id = event_id;
  wifi_event_head = next_head;
}

static void wifi_event_queue_reset(void)
{
  wifi_event_head = 0U;
  wifi_event_tail = 0U;
}

static void wifi_at_line_queue_reset(void)
{
  wifi_at_line_head = 0U;
  wifi_at_line_tail = 0U;
}

static void wifi_at_queue_line(const char *line)
{
  uint8_t next_head;

  if(line == 0) {
    return;
  }

  next_head = (uint8_t)((wifi_at_line_head + 1U) % TESTER_WIFI_AT_LINE_QUEUE_MAX);
  if(next_head == wifi_at_line_tail) {
    g_tester_wifi_print_rx_overflow_count++;
    return;
  }

  (void)snprintf(wifi_at_line_queue[wifi_at_line_head],
                 TESTER_WIFI_RX_LINE_MAX,
                 "%s",
                 line);
  wifi_at_line_head = next_head;
  g_tester_wifi_print_rx_frame_count++;
}

static uint8_t wifi_append_char(char *text, uint16_t text_size, uint16_t *length, char value)
{
  if(text == 0 || length == 0 || (*length + 1U) >= text_size) {
    return 0U;
  }
  text[*length] = value;
  *length = (uint16_t)(*length + 1U);
  text[*length] = '\0';
  return 1U;
}

static uint8_t wifi_append_text(char *text,
                                uint16_t text_size,
                                uint16_t *length,
                                const char *value)
{
  if(value == 0) {
    return 0U;
  }

  while(*value != '\0') {
    if(wifi_append_char(text, text_size, length, *value) == 0U) {
      return 0U;
    }
    value++;
  }
  return 1U;
}

static uint8_t wifi_append_at_escaped(char *text,
                                      uint16_t text_size,
                                      uint16_t *length,
                                      const char *value)
{
  if(value == 0) {
    return 0U;
  }

  while(*value != '\0') {
    if(*value == '\r' || *value == '\n') {
      return 0U;
    }
    if(*value == '"' || *value == '\\') {
      if(wifi_append_char(text, text_size, length, '\\') == 0U) {
        return 0U;
      }
    }
    if(wifi_append_char(text, text_size, length, *value) == 0U) {
      return 0U;
    }
    value++;
  }
  return 1U;
}

static uint8_t wifi_append_json_quoted(char *text,
                                       uint16_t text_size,
                                       uint16_t *length,
                                       const char *value)
{
  if(value == 0 || wifi_append_char(text, text_size, length, '"') == 0U) {
    return 0U;
  }

  while(*value != '\0') {
    unsigned char character = (unsigned char)*value;

    if(character < 0x20U) {
      return 0U;
    }
    if(*value == '"' || *value == '\\') {
      if(wifi_append_char(text, text_size, length, '\\') == 0U) {
        return 0U;
      }
    }
    if(wifi_append_char(text, text_size, length, *value) == 0U) {
      return 0U;
    }
    value++;
  }

  return wifi_append_char(text, text_size, length, '"');
}

static uint8_t wifi_append_uint32(char *text,
                                  uint16_t text_size,
                                  uint16_t *length,
                                  uint32_t value)
{
  char number[12];
  int written;

  written = snprintf(number, sizeof(number), "%lu", (unsigned long)value);
  if(written <= 0 || (uint32_t)written >= sizeof(number)) {
    return 0U;
  }
  return wifi_append_text(text, text_size, length, number);
}

static uint8_t wifi_build_join_command(char command[TESTER_WIFI_AT_COMMAND_MAX])
{
  const device_config_t *config = device_config_get();
  uint16_t length = 0U;

  if(config == 0 || config->wifi_ssid[0] == '\0') {
    return 0U;
  }

  command[0] = '\0';
  return wifi_append_text(command, TESTER_WIFI_AT_COMMAND_MAX, &length, "AT+CWJAP=\"") != 0U &&
         wifi_append_at_escaped(command, TESTER_WIFI_AT_COMMAND_MAX, &length,
                                config->wifi_ssid) != 0U &&
         wifi_append_text(command, TESTER_WIFI_AT_COMMAND_MAX, &length, "\",\"") != 0U &&
         wifi_append_at_escaped(command, TESTER_WIFI_AT_COMMAND_MAX, &length,
                                config->wifi_password) != 0U &&
         wifi_append_char(command, TESTER_WIFI_AT_COMMAND_MAX, &length, '"') != 0U;
}

static uint8_t wifi_build_connect_command(char command[TESTER_WIFI_AT_COMMAND_MAX])
{
  const device_config_t *config = device_config_get();
  uint16_t length = 0U;

  if(config == 0 || config->service_host[0] == '\0' || config->service_port == 0U) {
    return 0U;
  }

  command[0] = '\0';
  return wifi_append_text(command, TESTER_WIFI_AT_COMMAND_MAX, &length,
                          "AT+CIPSTART=\"TCP\",\"") != 0U &&
         wifi_append_at_escaped(command, TESTER_WIFI_AT_COMMAND_MAX, &length,
                                config->service_host) != 0U &&
         wifi_append_text(command, TESTER_WIFI_AT_COMMAND_MAX, &length, "\",") != 0U &&
         wifi_append_uint32(command, TESTER_WIFI_AT_COMMAND_MAX, &length,
                            (uint32_t)config->service_port) != 0U;
}

static uint8_t wifi_ap_config_is_complete(void)
{
  const device_config_t *config = device_config_get();

  return (config != 0 && device_config_validate(config) != 0U &&
          config->wifi_ssid[0] != '\0' &&
          config->wifi_password[0] != '\0') ? 1U : 0U;
}

static uint8_t wifi_production_config_is_complete(void)
{
  const device_config_t *config = device_config_get();

  return (wifi_ap_config_is_complete() != 0U &&
          config->service_host[0] != '\0' &&
          config->service_port != 0U &&
          device_config_station_number() != 0U) ? 1U : 0U;
}

static void wifi_publish_engine_state(wifi_engine_state_t state)
{
  wifi_engine_state = state;

  switch(state) {
  case WIFI_ENGINE_CONFIG_REQUIRED:
    g_tester_wifi_print_network_state = TESTER_WIFI_PRINT_NETWORK_CONFIG_REQUIRED;
    g_tester_wifi_print_online = 0U;
    break;
  case WIFI_ENGINE_BACKOFF:
    g_tester_wifi_print_network_state = TESTER_WIFI_PRINT_NETWORK_BACKOFF;
    g_tester_wifi_print_online = 0U;
    break;
  case WIFI_ENGINE_AT:
  case WIFI_ENGINE_SET_STA_MODE:
  case WIFI_ENGINE_CLOSE_OLD:
    g_tester_wifi_print_network_state = TESTER_WIFI_PRINT_NETWORK_PROBING;
    g_tester_wifi_print_online = 0U;
    break;
  case WIFI_ENGINE_JOIN_AP:
    g_tester_wifi_print_network_state = TESTER_WIFI_PRINT_NETWORK_JOINING_AP;
    g_tester_wifi_print_online = 0U;
    break;
  case WIFI_ENGINE_READ_IP:
    g_tester_wifi_print_network_state = TESTER_WIFI_PRINT_NETWORK_JOINING_AP;
    g_tester_wifi_print_online = 0U;
    break;
  case WIFI_ENGINE_AP_ONLINE:
    g_tester_wifi_print_network_state = TESTER_WIFI_PRINT_NETWORK_AP_ONLINE;
    /* AP association is deliberately distinct from a TCP print session. */
    g_tester_wifi_print_online = 0U;
    break;
  case WIFI_ENGINE_SET_MUX:
  case WIFI_ENGINE_SET_MODE:
  case WIFI_ENGINE_CONNECT_HOST:
    g_tester_wifi_print_network_state = TESTER_WIFI_PRINT_NETWORK_CONNECTING_HOST;
    g_tester_wifi_print_online = 0U;
    break;
  case WIFI_ENGINE_ONLINE:
    g_tester_wifi_print_network_state = TESTER_WIFI_PRINT_NETWORK_ONLINE;
    g_tester_wifi_print_online = 1U;
    break;
  case WIFI_ENGINE_SEND_PROMPT:
  case WIFI_ENGINE_SEND_RESULT:
    g_tester_wifi_print_network_state = TESTER_WIFI_PRINT_NETWORK_SENDING;
    g_tester_wifi_print_online = 1U;
    break;
  case WIFI_ENGINE_STOPPED:
  default:
    g_tester_wifi_print_network_state = TESTER_WIFI_PRINT_NETWORK_STOPPED;
    g_tester_wifi_print_online = 0U;
    break;
  }
}

static void wifi_job_clear(void)
{
  wifi_job_active = 0U;
  wifi_job_needs_send = 0U;
  wifi_job_acked = 0U;
  wifi_job_retry_count = 0U;
  wifi_job_event_id = 0U;
  wifi_job_wait_deadline_ms = 0U;
  wifi_job_frame_len = 0U;
  wifi_job_frame[0] = '\0';
}

static void wifi_job_fail(void)
{
  if(wifi_job_active == 0U) {
    return;
  }

  g_tester_wifi_print_network_error_count++;
  wifi_queue_event(TESTER_WIFI_PRINT_EVENT_ERROR, wifi_job_event_id);
  wifi_job_clear();
}

static void wifi_job_note_transport_failure(void)
{
  if(wifi_job_active == 0U) {
    return;
  }

  wifi_job_needs_send = 1U;
  g_tester_wifi_print_tx_retry_count++;
  wifi_job_retry_count++;
  if(wifi_job_retry_count >= TESTER_WIFI_JOB_MAX_RETRIES) {
    wifi_job_fail();
  }
}

static void wifi_schedule_reconnect(uint8_t job_transport_failure)
{
  g_tester_wifi_print_ap_connected = 0U;
  wifi_ap_ip_seen = 0U;
  g_tester_wifi_print_online = 0U;

  if(job_transport_failure != 0U) {
    wifi_job_note_transport_failure();
  }

  if(wifi_auto_start_enabled == 0U) {
    wifi_publish_engine_state(WIFI_ENGINE_STOPPED);
    wifi_state_deadline_ms = 0U;
    return;
  }

  g_tester_wifi_print_reconnect_count++;
  wifi_publish_engine_state(WIFI_ENGINE_BACKOFF);
  wifi_state_deadline_ms = wifi_time_ms + TESTER_WIFI_BACKOFF_MS;
}

static void wifi_begin_send(void);

static uint8_t wifi_issue_engine_state(wifi_engine_state_t state)
{
  char command[TESTER_WIFI_AT_COMMAND_MAX];
  uint32_t timeout_ms = TESTER_WIFI_AT_TIMEOUT_MS;

  command[0] = '\0';
  switch(state) {
  case WIFI_ENGINE_AT:
    (void)snprintf(command, sizeof(command), "AT");
    break;
  case WIFI_ENGINE_SET_STA_MODE:
    (void)snprintf(command, sizeof(command), "AT+CWMODE=1");
    break;
  case WIFI_ENGINE_JOIN_AP:
    if(wifi_build_join_command(command) == 0U) {
      return 0U;
    }
    timeout_ms = TESTER_WIFI_JOIN_TIMEOUT_MS;
    break;
  case WIFI_ENGINE_READ_IP:
    (void)snprintf(command, sizeof(command), "AT+CIFSR");
    break;
  case WIFI_ENGINE_SET_MUX:
    (void)snprintf(command, sizeof(command), "AT+CIPMUX=0");
    break;
  case WIFI_ENGINE_SET_MODE:
    (void)snprintf(command, sizeof(command), "AT+CIPMODE=0");
    break;
  case WIFI_ENGINE_CLOSE_OLD:
    (void)snprintf(command, sizeof(command), "AT+CIPCLOSE");
    break;
  case WIFI_ENGINE_CONNECT_HOST:
    if(wifi_build_connect_command(command) == 0U) {
      return 0U;
    }
    timeout_ms = TESTER_WIFI_CONNECT_TIMEOUT_MS;
    break;
  default:
    return 0U;
  }

  wifi_publish_engine_state(state);
  wifi_state_deadline_ms = wifi_time_ms + timeout_ms;
  wifi_write_text(command);
  wifi_write_text("\r\n");
  return 1U;
}

static void wifi_begin_production_session(void)
{
  if(wifi_auto_start_enabled == 0U) {
    wifi_publish_engine_state(WIFI_ENGINE_STOPPED);
    wifi_state_deadline_ms = 0U;
    return;
  }

  /* The AP link is useful on its own (and is what the LCDM WiFi indicator
   * reports).  A missing station/print-host tuple must not prevent the ESP
   * from joining the saved SSID; it only prevents print JSON from being sent. */
  if(wifi_ap_config_is_complete() == 0U) {
    wifi_publish_engine_state(WIFI_ENGINE_CONFIG_REQUIRED);
    wifi_state_deadline_ms = 0U;
    return;
  }

  if(wifi_issue_engine_state(WIFI_ENGINE_AT) == 0U) {
    wifi_schedule_reconnect(1U);
  }
}

static void wifi_set_ap_online(void)
{
  g_tester_wifi_print_ap_connected = 1U;
  wifi_ap_ip_seen = 1U;
  wifi_publish_engine_state(WIFI_ENGINE_AP_ONLINE);
  wifi_state_deadline_ms = 0U;
}

static void wifi_set_online(void)
{
  g_tester_wifi_print_ap_connected = 1U;
  wifi_ap_ip_seen = 1U;
  wifi_publish_engine_state(WIFI_ENGINE_ONLINE);
  wifi_state_deadline_ms = 0U;
  g_tester_wifi_print_connect_count++;
  if(wifi_job_active != 0U && wifi_job_needs_send != 0U) {
    wifi_begin_send();
  }
}

static void wifi_complete_send(void)
{
  wifi_publish_engine_state(WIFI_ENGINE_ONLINE);
  wifi_state_deadline_ms = 0U;
  g_tester_wifi_print_tx_payload_count++;

  if(wifi_job_active != 0U) {
    wifi_job_needs_send = 0U;
    wifi_job_wait_deadline_ms = wifi_time_ms +
      ((wifi_job_acked != 0U) ? TESTER_WIFI_DONE_TIMEOUT_MS : TESTER_WIFI_ACK_TIMEOUT_MS);
  }
}

static void wifi_begin_send(void)
{
  char command[32];
  int written;

  if(wifi_engine_state != WIFI_ENGINE_ONLINE || wifi_job_active == 0U ||
     wifi_job_needs_send == 0U || wifi_job_frame_len == 0U) {
    return;
  }

  written = snprintf(command, sizeof(command), "AT+CIPSEND=%u",
                     (unsigned int)wifi_job_frame_len);
  if(written <= 0 || (uint32_t)written >= sizeof(command)) {
    wifi_job_fail();
    return;
  }

  wifi_publish_engine_state(WIFI_ENGINE_SEND_PROMPT);
  wifi_state_deadline_ms = wifi_time_ms + TESTER_WIFI_SEND_TIMEOUT_MS;
  wifi_write_text(command);
  wifi_write_text("\r\n");
}

static void wifi_production_command_ok(void)
{
  switch(wifi_engine_state) {
  case WIFI_ENGINE_AT:
    /* Close a former production socket before changing WiFi/TCP mode.  This
     * makes a K3 retry reliable even when the host disappeared mid-job. */
    if(wifi_issue_engine_state(WIFI_ENGINE_CLOSE_OLD) == 0U) {
      wifi_schedule_reconnect(1U);
    }
    break;
  case WIFI_ENGINE_SET_STA_MODE:
    if(wifi_issue_engine_state(WIFI_ENGINE_JOIN_AP) == 0U) {
      wifi_schedule_reconnect(1U);
    }
    break;
  case WIFI_ENGINE_JOIN_AP:
    if(wifi_issue_engine_state(WIFI_ENGINE_READ_IP) == 0U) {
      wifi_schedule_reconnect(1U);
    }
    break;
  case WIFI_ENGINE_READ_IP:
    if(wifi_ap_ip_seen == 0U) {
      wifi_schedule_reconnect(1U);
    } else if(wifi_production_config_is_complete() != 0U) {
      if(wifi_issue_engine_state(WIFI_ENGINE_SET_MUX) == 0U) {
        wifi_schedule_reconnect(1U);
      }
    } else {
      wifi_set_ap_online();
    }
    break;
  case WIFI_ENGINE_SET_MUX:
    if(wifi_issue_engine_state(WIFI_ENGINE_SET_MODE) == 0U) {
      wifi_schedule_reconnect(1U);
    }
    break;
  case WIFI_ENGINE_SET_MODE:
    if(wifi_issue_engine_state(WIFI_ENGINE_CONNECT_HOST) == 0U) {
      wifi_schedule_reconnect(1U);
    }
    break;
  case WIFI_ENGINE_CLOSE_OLD:
    if(wifi_issue_engine_state(WIFI_ENGINE_SET_STA_MODE) == 0U) {
      wifi_schedule_reconnect(1U);
    }
    break;
  case WIFI_ENGINE_CONNECT_HOST:
    wifi_set_online();
    break;
  case WIFI_ENGINE_AP_ONLINE:
    /* A stray OK from an asynchronous AP indication is harmless. */
    break;
  case WIFI_ENGINE_ONLINE:
    /* A trailing OK from an ESP-AT asynchronous indication is harmless. */
    break;
  default:
    break;
  }
}

static void wifi_production_command_error(void)
{
  /* AT+CIPCLOSE returns ERROR when no previous socket exists.  That is the
   * expected clean-start case, not a production failure. */
  if(wifi_engine_state == WIFI_ENGINE_CLOSE_OLD) {
    if(wifi_issue_engine_state(WIFI_ENGINE_SET_STA_MODE) == 0U) {
      wifi_schedule_reconnect(1U);
    }
    return;
  }

  wifi_schedule_reconnect(1U);
}

static const char *wifi_ipd_payload(const char *frame)
{
  const char *separator;

  if(frame == 0 || strncmp(frame, "+IPD,", 5U) != 0) {
    return frame;
  }

  separator = strchr(frame, ':');
  return (separator == 0) ? 0 : separator + 1;
}

static uint8_t wifi_handle_print_frame(const char *frame)
{
  tester_wifi_print_event_t event = TESTER_WIFI_PRINT_EVENT_NONE;
  const char *payload = wifi_ipd_payload(frame);
  uint32_t event_id;

  if(payload == 0 || wifi_frame_event_id(payload, &event_id) == 0U) {
    return 0U;
  }

  if((strstr(payload, "\"type\":\"print_ack\"") != 0 &&
      strstr(payload, "\"state\":\"QUEUED\"") != 0) ||
     strstr(payload, "ACK,") == payload) {
    event = TESTER_WIFI_PRINT_EVENT_ACK_QUEUED;
  } else if((strstr(payload, "\"type\":\"print_status\"") != 0 &&
             strstr(payload, "\"state\":\"DONE\"") != 0) ||
            strstr(payload, "DONE,") == payload) {
    event = TESTER_WIFI_PRINT_EVENT_DONE;
  } else if((strstr(payload, "\"type\":\"print_status\"") != 0 &&
             strstr(payload, "\"state\":\"ERROR\"") != 0) ||
            strstr(payload, "ERROR,") == payload) {
    event = TESTER_WIFI_PRINT_EVENT_ERROR;
  }

  if(event == TESTER_WIFI_PRINT_EVENT_NONE) {
    return 0U;
  }

  g_tester_wifi_print_rx_frame_count++;
  wifi_queue_event(event, event_id);

  if(wifi_job_active != 0U && event_id == wifi_job_event_id) {
    if(event == TESTER_WIFI_PRINT_EVENT_ACK_QUEUED) {
      wifi_job_acked = 1U;
      wifi_job_needs_send = 0U;
      wifi_job_wait_deadline_ms = wifi_time_ms + TESTER_WIFI_DONE_TIMEOUT_MS;
    } else if(event == TESTER_WIFI_PRINT_EVENT_DONE ||
              event == TESTER_WIFI_PRINT_EVENT_ERROR) {
      wifi_job_clear();
    }
  }
  return 1U;
}

static uint8_t wifi_handle_production_line(const char *line)
{
  if(line == 0 || line[0] == '\0' || strncmp(line, "+IPD,", 5U) == 0) {
    return 0U;
  }

  /* ESP-AT command echo is informational.  AP progress, however, updates a
   * separate flag so the tester can show WiFi PASS before a print host is
   * configured. */
  if(strncmp(line, "AT", 2U) == 0 || strcmp(line, "WIFI CONNECTED") == 0) {
    return 1U;
  }

  if(strcmp(line, "WIFI GOT IP") == 0) {
    wifi_ap_ip_seen = 1U;
    g_tester_wifi_print_ap_connected = 1U;
    return 1U;
  }

  if(strstr(line, "+CIFSR:STAIP,\"") != 0) {
    /* ESP-AT reports 0.0.0.0 before DHCP completes. */
    if(strstr(line, "\"0.0.0.0\"") == 0) {
      wifi_ap_ip_seen = 1U;
      g_tester_wifi_print_ap_connected = 1U;
    }
    return 1U;
  }

  if(strstr(line, "invalid header: 0xffffffff") != 0) {
    g_tester_wifi_print_network_error_count++;
    wifi_schedule_reconnect(1U);
    return 1U;
  }

  if(strcmp(line, "CLOSED") == 0) {
    if(wifi_engine_state != WIFI_ENGINE_CLOSE_OLD) {
      wifi_schedule_reconnect((wifi_job_active != 0U) ? 1U : 0U);
    }
    return 1U;
  }
  if(strcmp(line, "WIFI DISCONNECT") == 0 ||
     strcmp(line, "WIFI DISCONNECTED") == 0) {
    /* ESP-AT commonly reports a transient disconnect while AT+CWJAP is
     * replacing the previous association.  It is followed by WIFI
     * CONNECTED/WIFI GOT IP for a successful join.  Treating that progress
     * indication as a failed session aborts the JOIN_AP state before the
     * subsequent AT+CIFSR command can be issued, causing an endless
     * AT/CWJAP/backoff loop.  Let the join deadline decide whether the AP
     * really failed; disconnects after the join/read-IP phase remain real
     * reconnect triggers.
     */
    if(wifi_engine_state == WIFI_ENGINE_SET_STA_MODE ||
       wifi_engine_state == WIFI_ENGINE_JOIN_AP ||
       wifi_engine_state == WIFI_ENGINE_READ_IP) {
      return 1U;
    }
    wifi_schedule_reconnect((wifi_job_active != 0U) ? 1U : 0U);
    return 1U;
  }

  if(wifi_engine_state == WIFI_ENGINE_SEND_PROMPT &&
     (strcmp(line, ">") == 0 || line[0] == '>')) {
    wifi_write_bytes((const uint8_t *)wifi_job_frame, wifi_job_frame_len);
    wifi_publish_engine_state(WIFI_ENGINE_SEND_RESULT);
    wifi_state_deadline_ms = wifi_time_ms + TESTER_WIFI_SEND_TIMEOUT_MS;
    return 1U;
  }
  if(wifi_engine_state == WIFI_ENGINE_SEND_RESULT && strcmp(line, "SEND OK") == 0) {
    wifi_complete_send();
    return 1U;
  }
  if(wifi_engine_state == WIFI_ENGINE_SEND_RESULT && strcmp(line, "SEND FAIL") == 0) {
    wifi_schedule_reconnect(1U);
    return 1U;
  }

  if(wifi_engine_state == WIFI_ENGINE_CONNECT_HOST &&
     (strcmp(line, "CONNECT") == 0 || strstr(line, "ALREADY CONNECTED") != 0)) {
    if(strstr(line, "ALREADY CONNECTED") != 0) {
      wifi_set_online();
    }
    return 1U;
  }

  if(strcmp(line, "OK") == 0) {
    wifi_production_command_ok();
    return 1U;
  }
  if(strcmp(line, "ERROR") == 0 || strcmp(line, "FAIL") == 0 ||
     strstr(line, "busy p...") != 0 || strstr(line, "link is not valid") != 0) {
    wifi_production_command_error();
    return 1U;
  }

  return 0U;
}

static void wifi_handle_frame(const char *frame)
{
  /* ESP32-C3 has no built-in JSON command parser.  The isolated link
   * diagnostic uses the standard ESP-AT command "AT\\r\\n" and accepts its
   * documented text response. */
  if(wifi_link_test_sequence != 0U && strcmp(frame, "OK") == 0) {
    g_tester_wifi_print_rx_frame_count++;
    wifi_queue_event(TESTER_WIFI_PRINT_EVENT_LINK_ACK, wifi_link_test_sequence);
    return;
  }
  if(wifi_link_test_sequence != 0U && strcmp(frame, "ERROR") == 0) {
    g_tester_wifi_print_rx_frame_count++;
    wifi_queue_event(TESTER_WIFI_PRINT_EVENT_LINK_ERROR, wifi_link_test_sequence);
    return;
  }
  /* ESP32-C3 ROM output captured on PB9: an all-FF flash has no boot image,
   * repeatedly reports this line, and cannot execute ESP-AT to answer AT. */
  if(wifi_link_test_sequence != 0U &&
     strstr(frame, "invalid header: 0xffffffff") != 0) {
    g_tester_wifi_print_rx_frame_count++;
    wifi_queue_event(TESTER_WIFI_PRINT_EVENT_LINK_FLASH_INVALID,
                     wifi_link_test_sequence);
    return;
  }

  if(wifi_handle_production_line(frame) != 0U) {
    return;
  }
  if(wifi_handle_print_frame(frame) == 0U) {
    /* Ignore ESP boot banners and other informational text.  Count malformed
     * network payloads, because they would otherwise silently hide a broken
     * print-host protocol. */
    if(strncmp(frame, "+IPD,", 5U) == 0 || frame[0] == '{' ||
       strstr(frame, "ACK,") == frame || strstr(frame, "DONE,") == frame ||
       strstr(frame, "ERROR,") == frame) {
      g_tester_wifi_print_rx_error_count++;
    }
  }
}

static void wifi_store_rx_byte(uint8_t value)
{
  if(value == '\r') {
    return;
  }

  /* ESP-AT normally emits ">\\r\\n", but accepting the prompt immediately
   * also covers versions that omit the trailing LF. */
  if(value == '>' && wifi_at_capture_enabled == 0U && wifi_rx_line_len == 0U) {
    wifi_handle_frame(">");
    return;
  }

  if(value == '\n') {
    if(wifi_rx_line_len != 0U) {
      wifi_rx_line[wifi_rx_line_len] = '\0';
      if(wifi_at_capture_enabled != 0U) {
        wifi_at_queue_line(wifi_rx_line);
      } else {
        wifi_handle_frame(wifi_rx_line);
      }
    }
    wifi_rx_line_len = 0U;
    return;
  }

  if(value < 0x20U || value > 0x7EU) {
    wifi_rx_line_len = 0U;
    g_tester_wifi_print_rx_error_count++;
    return;
  }

  if(wifi_rx_line_len >= (TESTER_WIFI_RX_LINE_MAX - 1U)) {
    wifi_rx_line_len = 0U;
    g_tester_wifi_print_rx_overflow_count++;
    return;
  }

  wifi_rx_line[wifi_rx_line_len] = (char)value;
  wifi_rx_line_len++;
}

static uint8_t wifi_edge_level_at(uint32_t edge_start,
                                  uint32_t edge_end,
                                  uint32_t sample_cycles)
{
  uint32_t edge;
  uint8_t level = wifi_rx_edge_levels[edge_start & TESTER_WIFI_RX_EDGE_MASK];

  for(edge = edge_start + 1U; edge != edge_end; edge++) {
    uint32_t edge_cycles = wifi_rx_edge_cycles[edge & TESTER_WIFI_RX_EDGE_MASK];
    if((int32_t)(edge_cycles - sample_cycles) > 0) {
      break;
    }
    level = wifi_rx_edge_levels[edge & TESTER_WIFI_RX_EDGE_MASK];
  }

  return level;
}

static uint32_t wifi_edge_after(uint32_t edge_start,
                                uint32_t edge_end,
                                uint32_t sample_cycles)
{
  uint32_t edge = edge_start + 1U;

  while(edge != edge_end) {
    uint32_t edge_cycles = wifi_rx_edge_cycles[edge & TESTER_WIFI_RX_EDGE_MASK];
    if((int32_t)(edge_cycles - sample_cycles) > 0) {
      break;
    }
    edge++;
  }

  return edge;
}

static void wifi_decode_rx_edges(void)
{
  uint32_t head_snapshot = wifi_rx_edge_head;
  uint32_t now_cycles = wifi_cycles();

  if((head_snapshot - wifi_rx_edge_tail) >= (TESTER_WIFI_RX_EDGE_MAX - 2U)) {
    wifi_rx_edge_tail = head_snapshot;
    g_tester_wifi_print_rx_overflow_count++;
    return;
  }

  while(wifi_rx_edge_tail != head_snapshot) {
    uint8_t bit;
    uint8_t value = 0U;
    uint32_t start_cycles;
    uint32_t stop_sample;

    if(wifi_rx_edge_levels[wifi_rx_edge_tail & TESTER_WIFI_RX_EDGE_MASK] != 0U) {
      wifi_rx_edge_tail++;
      continue;
    }

    start_cycles = wifi_rx_edge_cycles[wifi_rx_edge_tail & TESTER_WIFI_RX_EDGE_MASK];
    if((uint32_t)(now_cycles - start_cycles) < (wifi_bit_cycles * 10U)) {
      break;
    }

    for(bit = 0U; bit < 8U; bit++) {
      uint32_t sample_cycles = start_cycles + wifi_bit_cycles +
                               (wifi_bit_cycles / 2U) + (wifi_bit_cycles * bit);
      if(wifi_edge_level_at(wifi_rx_edge_tail, head_snapshot, sample_cycles) != 0U) {
        value |= (uint8_t)(1U << bit);
      }
    }

    stop_sample = start_cycles + (wifi_bit_cycles * 9U) + (wifi_bit_cycles / 2U);
    if(wifi_edge_level_at(wifi_rx_edge_tail, head_snapshot, stop_sample) == 0U) {
      g_tester_wifi_print_rx_error_count++;
      wifi_rx_edge_tail++;
      continue;
    }

    wifi_store_rx_byte(value);
    wifi_rx_edge_tail = wifi_edge_after(wifi_rx_edge_tail, head_snapshot, stop_sample);
  }
}

static void wifi_production_service(void)
{
  if(wifi_auto_start_enabled == 0U || wifi_at_capture_enabled != 0U) {
    return;
  }

  if(wifi_engine_state == WIFI_ENGINE_CONFIG_REQUIRED) {
    if(wifi_ap_config_is_complete() != 0U) {
      wifi_begin_production_session();
    }
    return;
  }

  if(wifi_engine_state == WIFI_ENGINE_BACKOFF) {
    if(wifi_time_reached(wifi_state_deadline_ms) != 0U) {
      wifi_begin_production_session();
    }
    return;
  }

  if(wifi_engine_state == WIFI_ENGINE_ONLINE) {
    if(wifi_job_active != 0U) {
      if(wifi_job_needs_send != 0U) {
        wifi_begin_send();
      } else if(wifi_job_wait_deadline_ms != 0U &&
                wifi_time_reached(wifi_job_wait_deadline_ms) != 0U) {
        /* Re-send exactly the same event ID.  The line print host must de-dupe
         * (device_uid,event_id), so a lost ACK/DONE cannot print twice. */
        wifi_job_note_transport_failure();
        if(wifi_job_active != 0U) {
          wifi_begin_send();
        }
      }
    }
    return;
  }

  if(wifi_engine_state == WIFI_ENGINE_AP_ONLINE) {
    /* Keep the AP association alive.  A print-host/station tuple can be
     * supplied later through LCDM; tester_wifi_print_restart() then runs the
     * full TCP sequence without disturbing the learned recipe. */
    return;
  }

  if(wifi_engine_state == WIFI_ENGINE_STOPPED) {
    wifi_begin_production_session();
    return;
  }

  if(wifi_state_deadline_ms != 0U && wifi_time_reached(wifi_state_deadline_ms) != 0U) {
    wifi_schedule_reconnect(1U);
  }
}

void tester_wifi_print_init(void)
{
  gpio_init_type gpio_init_struct;
  exint_init_type exint_init_struct;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  /* DWT counts the selected system core clock (8 MHz HICK for a fallback
   * image, or the shared 192 MHz HICK PLL for formal WiFi images), so derive
   * the exact bit target from the per-image UART baud setting. */
  wifi_bit_cycles = (system_core_clock + (TESTER_WIFI_PRINT_UART_BAUDRATE / 2U)) /
                    TESTER_WIFI_PRINT_UART_BAUDRATE;
  if(wifi_bit_cycles == 0U) {
    wifi_bit_cycles = 1U;
  }
  wifi_cycles_per_ms = system_core_clock / 1000U;
  if(wifi_cycles_per_ms == 0U) {
    wifi_cycles_per_ms = 1U;
  }
  wifi_time_last_cycles = wifi_cycles();
  wifi_time_remainder_cycles = 0U;
  wifi_time_ms = 0U;

  crm_periph_clock_enable(TESTER_WIFI_TX_GPIO_CLOCK, TRUE);
  crm_periph_clock_enable(TESTER_WIFI_RX_GPIO_CLOCK, TRUE);

  TESTER_WIFI_TX_GPIO->scr = TESTER_WIFI_TX_PIN;
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = TESTER_WIFI_TX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(TESTER_WIFI_TX_GPIO, &gpio_init_struct);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pins = TESTER_WIFI_RX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(TESTER_WIFI_RX_GPIO, &gpio_init_struct);

  wifi_rx_edge_head = 0U;
  wifi_rx_edge_tail = 0U;
  wifi_rx_line_len = 0U;
  wifi_event_queue_reset();
  wifi_at_line_queue_reset();
  wifi_at_capture_enabled = 0U;
  wifi_link_test_sequence = 0U;
  wifi_auto_start_enabled = 0U;
  wifi_restart_after_raw = 0U;
  wifi_ap_ip_seen = 0U;
  wifi_state_deadline_ms = 0U;
  wifi_job_clear();
  wifi_publish_engine_state(WIFI_ENGINE_STOPPED);

  g_tester_wifi_print_tx_request_count = 0U;
  g_tester_wifi_print_rx_frame_count = 0U;
  g_tester_wifi_print_rx_error_count = 0U;
  g_tester_wifi_print_rx_overflow_count = 0U;
  g_tester_wifi_print_connect_count = 0U;
  g_tester_wifi_print_reconnect_count = 0U;
  g_tester_wifi_print_tx_payload_count = 0U;
  g_tester_wifi_print_tx_retry_count = 0U;
  g_tester_wifi_print_network_error_count = 0U;
  g_tester_wifi_print_ap_connected = 0U;

  crm_periph_clock_enable(CRM_SCFG_PERIPH_CLOCK, TRUE);
  scfg_exint_line_config(TESTER_WIFI_RX_PORT_SOURCE, TESTER_WIFI_RX_PIN_SOURCE);
  exint_default_para_init(&exint_init_struct);
  exint_init_struct.line_enable = TRUE;
  exint_init_struct.line_mode = EXINT_LINE_INTERRUPT;
  exint_init_struct.line_select = TESTER_WIFI_RX_EXINT_LINE;
  exint_init_struct.line_polarity = EXINT_TRIGGER_BOTH_EDGE;
  exint_init(&exint_init_struct);
  exint_flag_clear(TESTER_WIFI_RX_EXINT_LINE);
  nvic_irq_enable(EXINT9_5_IRQn, 1U, 0U);

  g_tester_wifi_print_ready = 1U;
}

void tester_wifi_print_start(void)
{
  if(g_tester_wifi_print_ready == 0U) {
    return;
  }

  wifi_auto_start_enabled = 1U;
  wifi_link_test_sequence = 0U;
  if(wifi_at_capture_enabled != 0U) {
    wifi_restart_after_raw = 1U;
    return;
  }

  wifi_publish_engine_state(WIFI_ENGINE_STOPPED);
  wifi_state_deadline_ms = 0U;
  wifi_begin_production_session();
}

void tester_wifi_print_restart(void)
{
  if(g_tester_wifi_print_ready == 0U) {
    return;
  }

  /* Settings can only be entered from a safe idle/reset state.  There is no
   * valid product job to retain across an AP/host replacement. */
  wifi_job_clear();
  wifi_event_queue_reset();
  tester_wifi_print_start();
}

uint8_t tester_wifi_print_is_online(void)
{
  return g_tester_wifi_print_online;
}

uint8_t tester_wifi_print_is_ap_connected(void)
{
  return g_tester_wifi_print_ap_connected;
}

uint8_t tester_wifi_print_is_configured(void)
{
  return wifi_production_config_is_complete();
}

static uint8_t wifi_build_print_frame(uint32_t event_id,
                                      uint32_t test_count,
                                      uint16_t pair_count,
                                      uint16_t point_count,
                                      char frame[TESTER_WIFI_TX_FRAME_MAX],
                                      uint16_t *out_length)
{
  const device_config_t *config = device_config_get();
  char uid[32];
  uint16_t length = 0U;
  uint8_t station;

  if(config == 0 || out_length == 0 || wifi_production_config_is_complete() == 0U) {
    return 0U;
  }
  station = device_config_station_number();
  if(station == 0U) {
    return 0U;
  }

  device_config_format_uid(uid, sizeof(uid));
  frame[0] = '\0';
  if(wifi_append_text(frame, TESTER_WIFI_TX_FRAME_MAX, &length,
                      "{\"type\":\"print_request\",\"ver\":1,\"event_id\":") == 0U ||
     wifi_append_uint32(frame, TESTER_WIFI_TX_FRAME_MAX, &length, event_id) == 0U ||
     wifi_append_text(frame, TESTER_WIFI_TX_FRAME_MAX, &length, ",\"device_uid\":") == 0U ||
     wifi_append_json_quoted(frame, TESTER_WIFI_TX_FRAME_MAX, &length, uid) == 0U ||
     wifi_append_text(frame, TESTER_WIFI_TX_FRAME_MAX, &length, ",\"machine_id\":") == 0U ||
     wifi_append_json_quoted(frame, TESTER_WIFI_TX_FRAME_MAX, &length,
                             config->machine_id) == 0U ||
     wifi_append_text(frame, TESTER_WIFI_TX_FRAME_MAX, &length, ",\"line_id\":") == 0U ||
     wifi_append_json_quoted(frame, TESTER_WIFI_TX_FRAME_MAX, &length,
                             config->line_id) == 0U ||
     wifi_append_text(frame, TESTER_WIFI_TX_FRAME_MAX, &length, ",\"station_id\":") == 0U ||
     wifi_append_json_quoted(frame, TESTER_WIFI_TX_FRAME_MAX, &length,
                             config->station_id) == 0U ||
     wifi_append_text(frame, TESTER_WIFI_TX_FRAME_MAX, &length, ",\"station\":") == 0U ||
     wifi_append_uint32(frame, TESTER_WIFI_TX_FRAME_MAX, &length, station) == 0U ||
     wifi_append_text(frame, TESTER_WIFI_TX_FRAME_MAX, &length, ",\"test_count\":") == 0U ||
     wifi_append_uint32(frame, TESTER_WIFI_TX_FRAME_MAX, &length, test_count) == 0U ||
     wifi_append_text(frame, TESTER_WIFI_TX_FRAME_MAX, &length, ",\"pairs\":") == 0U ||
     wifi_append_uint32(frame, TESTER_WIFI_TX_FRAME_MAX, &length, pair_count) == 0U ||
     wifi_append_text(frame, TESTER_WIFI_TX_FRAME_MAX, &length, ",\"points\":") == 0U ||
     wifi_append_uint32(frame, TESTER_WIFI_TX_FRAME_MAX, &length, point_count) == 0U ||
     wifi_append_text(frame, TESTER_WIFI_TX_FRAME_MAX, &length, "}\n") == 0U) {
    return 0U;
  }

  *out_length = length;
  return 1U;
}

uint8_t tester_wifi_print_request(uint32_t event_id,
                                  uint32_t test_count,
                                  uint16_t pair_count,
                                  uint16_t point_count)
{
  if(g_tester_wifi_print_ready == 0U || event_id == 0U ||
     wifi_at_capture_enabled != 0U || wifi_job_active != 0U) {
    return 0U;
  }

  if(wifi_build_print_frame(event_id,
                            test_count,
                            pair_count,
                            point_count,
                            wifi_job_frame,
                            &wifi_job_frame_len) == 0U) {
    return 0U;
  }

  /* Old responses must never complete a newly triggered product. */
  wifi_event_queue_reset();
  wifi_job_active = 1U;
  wifi_job_needs_send = 1U;
  wifi_job_acked = 0U;
  wifi_job_retry_count = 0U;
  wifi_job_event_id = event_id;
  wifi_job_wait_deadline_ms = 0U;
  g_tester_wifi_print_tx_request_count++;

  if(wifi_auto_start_enabled == 0U) {
    tester_wifi_print_start();
  }
  if(wifi_engine_state == WIFI_ENGINE_ONLINE) {
    wifi_begin_send();
  }
  return 1U;
}

uint8_t tester_wifi_print_link_test_request(uint32_t sequence)
{
  if(g_tester_wifi_print_ready == 0U || sequence == 0U ||
     wifi_at_capture_enabled != 0U) {
    return 0U;
  }

  /* The diagnostic has no print job in flight.  Discard a stale answer,
   * retain its local sequence token, then issue a real ESP-AT command on
   * PC3.  ESP-AT responds with the line "OK\\r\\n" or "ERROR\\r\\n". */
  wifi_event_queue_reset();
  wifi_link_test_sequence = sequence;
  wifi_write_text("AT\r\n");
  return 1U;
}

void tester_wifi_print_service(void)
{
  if(g_tester_wifi_print_ready == 0U) {
    return;
  }

  wifi_decode_rx_edges();
  wifi_time_service();
  wifi_production_service();
}

tester_wifi_print_event_t tester_wifi_print_poll_event(uint32_t expected_event_id)
{
  while(wifi_event_tail != wifi_event_head) {
    tester_wifi_print_event_slot_t event = wifi_event_queue[wifi_event_tail];

    wifi_event_tail = (uint8_t)((wifi_event_tail + 1U) % TESTER_WIFI_EVENT_QUEUE_MAX);
    if(event.event_id == expected_event_id) {
      return event.type;
    }
  }

  return TESTER_WIFI_PRINT_EVENT_NONE;
}

void tester_wifi_print_cancel(void)
{
  wifi_event_queue_reset();
  wifi_job_clear();
  wifi_link_test_sequence = 0U;
  /* Deliberately retain an established production TCP session.  K3 clears
   * the current job but does not have an ESP EN wire to hard-reset the module. */
}

void tester_wifi_print_at_begin(void)
{
  if(g_tester_wifi_print_ready == 0U) {
    return;
  }

  /* Discard boot text or a former test's partial line before the diagnostic
   * begins its own command/response sequence.  Raw ownership pauses the
   * production state machine; at_end() reconnects it from saved Flash. */
  wifi_rx_edge_tail = wifi_rx_edge_head;
  wifi_rx_line_len = 0U;
  wifi_event_queue_reset();
  wifi_at_line_queue_reset();
  wifi_link_test_sequence = 0U;
  wifi_job_clear();
  g_tester_wifi_print_ap_connected = 0U;
  g_tester_wifi_print_online = 0U;
  wifi_ap_ip_seen = 0U;
  wifi_publish_engine_state(WIFI_ENGINE_STOPPED);
  wifi_state_deadline_ms = 0U;
  wifi_at_capture_enabled = 1U;
}

void tester_wifi_print_at_end(void)
{
  wifi_at_capture_enabled = 0U;
  wifi_at_line_queue_reset();
  wifi_rx_line_len = 0U;

  if(wifi_auto_start_enabled != 0U || wifi_restart_after_raw != 0U) {
    wifi_restart_after_raw = 0U;
    wifi_publish_engine_state(WIFI_ENGINE_STOPPED);
    wifi_begin_production_session();
  }
}

uint8_t tester_wifi_print_at_send(const char *command)
{
  size_t length;

  if(g_tester_wifi_print_ready == 0U || wifi_at_capture_enabled == 0U ||
     command == 0 || command[0] == '\0') {
    return 0U;
  }

  length = strlen(command);
  wifi_write_text(command);
  if(command[length - 1U] == '\n') {
    return 1U;
  }
  if(command[length - 1U] == '\r') {
    wifi_write_text("\n");
  } else {
    wifi_write_text("\r\n");
  }
  return 1U;
}

uint8_t tester_wifi_print_at_poll_line(char *line, uint16_t line_size)
{
  if(line == 0 || line_size == 0U || wifi_at_line_tail == wifi_at_line_head) {
    return 0U;
  }

  (void)snprintf(line,
                 line_size,
                 "%s",
                 wifi_at_line_queue[wifi_at_line_tail]);
  wifi_at_line_tail = (uint8_t)((wifi_at_line_tail + 1U) % TESTER_WIFI_AT_LINE_QUEUE_MAX);
  return 1U;
}

void tester_wifi_print_rx_edge_isr(void)
{
  uint32_t head;

  if(g_tester_wifi_print_ready == 0U) {
    return;
  }

  head = wifi_rx_edge_head;
  wifi_rx_edge_cycles[head & TESTER_WIFI_RX_EDGE_MASK] = wifi_cycles();
  wifi_rx_edge_levels[head & TESTER_WIFI_RX_EDGE_MASK] = wifi_rx_level();
  wifi_rx_edge_head = head + 1U;
}

/* This handler lives in the timing-optimised software UART translation unit,
 * rather than the normal -O0 interrupt file.  An edge may be only one bit
 * period after the previous edge, so capture must be direct and inlined with
 * no peripheral-driver function calls. */
void EXINT9_5_IRQHandler(void)
{
  if((EXINT->intsts & TESTER_WIFI_RX_EXINT_LINE) != 0U) {
    tester_wifi_print_rx_edge_isr();
    EXINT->intsts = TESTER_WIFI_RX_EXINT_LINE;
  }
}
