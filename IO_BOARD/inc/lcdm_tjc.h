#ifndef LCDM_TJC_H
#define LCDM_TJC_H

#include <stdint.h>

#ifndef LCDM_TJC_BAUDRATE
#define LCDM_TJC_BAUDRATE 115200U
#endif

#define LCDM_TJC_RX_PACKET_MAX 96U
#define LCDM_TJC_RX_DEBUG_MAX 32U

typedef enum {
  LCDM_TJC_EVENT_NONE = 0,
  LCDM_TJC_EVENT_TOUCH,
  LCDM_TJC_EVENT_ASCII
} lcdm_tjc_event_type_t;

typedef struct {
  lcdm_tjc_event_type_t type;
  uint8_t page_id;
  uint8_t component_id;
  uint8_t touch_event;
  uint8_t len;
  char ascii[LCDM_TJC_RX_PACKET_MAX];
} lcdm_tjc_event_t;

extern volatile uint32_t g_lcdm_tjc_tx_cmd_count;
extern volatile uint32_t g_lcdm_tjc_rx_byte_count;
extern volatile uint32_t g_lcdm_tjc_rx_packet_count;
extern volatile uint32_t g_lcdm_tjc_rx_overflow_count;
extern volatile uint32_t g_lcdm_tjc_rx_start_count;
extern volatile uint32_t g_lcdm_tjc_rx_frame_error_count;
extern volatile uint32_t g_lcdm_tjc_rx_low_sample_count;
extern volatile uint32_t g_lcdm_tjc_exint_count;
extern volatile uint32_t g_lcdm_tjc_exint_rx_ok_count;
extern volatile uint32_t g_lcdm_tjc_exint_rx_fail_count;
extern volatile uint8_t g_lcdm_tjc_last_rx_byte;
extern volatile uint8_t g_lcdm_tjc_rx_level_now;
extern volatile uint8_t g_lcdm_tjc_last_packet0;
extern volatile uint8_t g_lcdm_tjc_last_packet1;
extern volatile uint8_t g_lcdm_tjc_last_packet2;
extern volatile uint8_t g_lcdm_tjc_last_packet_len;
extern volatile uint8_t g_lcdm_tjc_rx_debug_index;
extern volatile uint8_t g_lcdm_tjc_rx_debug_bytes[LCDM_TJC_RX_DEBUG_MAX];
extern volatile uint8_t g_lcdm_tjc_rx_edge_debug_index;
extern volatile uint8_t g_lcdm_tjc_rx_edge_debug_levels[16];
extern volatile uint16_t g_lcdm_tjc_rx_edge_debug_deltas[16];

void lcdm_tjc_init(void);
void lcdm_tjc_rx_falling_edge_isr(void);
void lcdm_tjc_send_cmd(const char *cmd);
void lcdm_tjc_set_text(const char *obj, const char *text);
void lcdm_tjc_set_num(const char *obj, int32_t value);
void lcdm_tjc_page(uint8_t page_id);
uint8_t lcdm_tjc_poll_event(lcdm_tjc_event_t *event);

#endif
