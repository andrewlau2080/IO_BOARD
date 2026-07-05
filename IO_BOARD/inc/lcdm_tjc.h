#ifndef LCDM_TJC_H
#define LCDM_TJC_H

#include <stdint.h>

#ifndef LCDM_TJC_BAUDRATE
#define LCDM_TJC_BAUDRATE 9600U
#endif

#define LCDM_TJC_RX_PACKET_MAX 96U

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
extern volatile uint8_t g_lcdm_tjc_last_packet0;
extern volatile uint8_t g_lcdm_tjc_last_packet_len;

void lcdm_tjc_init(void);
void lcdm_tjc_send_cmd(const char *cmd);
void lcdm_tjc_set_text(const char *obj, const char *text);
void lcdm_tjc_set_num(const char *obj, int32_t value);
void lcdm_tjc_page(uint8_t page_id);
uint8_t lcdm_tjc_poll_event(lcdm_tjc_event_t *event);

#endif
