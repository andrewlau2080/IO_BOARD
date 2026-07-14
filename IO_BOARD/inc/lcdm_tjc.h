#ifndef LCDM_TJC_H
#define LCDM_TJC_H

#include <stdint.h>

#ifndef LCDM_TJC_BAUDRATE
#define LCDM_TJC_BAUDRATE 230400U
#endif

#define LCDM_TJC_RX_PACKET_MAX 96U
#define LCDM_TJC_RX_DEBUG_MAX 32U
#define LCDM_TJC_RX_QUEUE_DEPTH 8U

typedef enum {
  LCDM_TJC_EVENT_NONE = 0,
  LCDM_TJC_EVENT_TOUCH,
  LCDM_TJC_EVENT_TOUCH_COORD,
  LCDM_TJC_EVENT_NUMBER,
  LCDM_TJC_EVENT_ASCII
} lcdm_tjc_event_type_t;

typedef struct {
  lcdm_tjc_event_type_t type;
  uint8_t page_id;
  uint8_t component_id;
  uint8_t touch_event;
  uint16_t x;
  uint16_t y;
  uint32_t number;
  uint8_t len;
  char ascii[LCDM_TJC_RX_PACKET_MAX];
} lcdm_tjc_event_t;

extern volatile uint32_t g_lcdm_tjc_tx_cmd_count;
extern volatile uint32_t g_lcdm_tjc_tx_error_count;
extern volatile uint32_t g_lcdm_tjc_rx_byte_count;
extern volatile uint32_t g_lcdm_tjc_rx_packet_count;
extern volatile uint32_t g_lcdm_tjc_rx_overflow_count;
extern volatile uint32_t g_lcdm_tjc_rx_queue_overflow_count;
extern volatile uint32_t g_lcdm_tjc_uart_error_count;
extern volatile uint8_t g_lcdm_tjc_last_packet_len;
extern volatile uint8_t g_lcdm_tjc_last_packet0;
extern volatile uint8_t g_lcdm_tjc_last_packet1;
extern volatile uint8_t g_lcdm_tjc_last_packet2;
extern volatile uint8_t g_lcdm_tjc_last_event_type;
extern volatile uint16_t g_lcdm_tjc_last_x;
extern volatile uint16_t g_lcdm_tjc_last_y;
extern volatile uint8_t g_lcdm_tjc_last_touch_event;
extern volatile uint8_t g_lcdm_tjc_last_page_id;
extern volatile uint8_t g_lcdm_tjc_last_component_id;
extern volatile uint32_t g_lcdm_tjc_last_number;
extern volatile uint8_t g_lcdm_tjc_last_raw0;
extern volatile uint8_t g_lcdm_tjc_last_raw1;
extern volatile uint8_t g_lcdm_tjc_last_raw2;
extern volatile uint8_t g_lcdm_tjc_last_raw3;
extern volatile uint8_t g_lcdm_tjc_last_raw4;
extern volatile uint8_t g_lcdm_tjc_last_raw5;
extern volatile uint8_t g_lcdm_tjc_last_raw6;
extern volatile uint8_t g_lcdm_tjc_last_raw7;
extern volatile uint8_t g_lcdm_tjc_debug_page;

void lcdm_tjc_init(void);
uint8_t lcdm_tjc_probe(uint8_t attempts);
void lcdm_tjc_set_baudrate(uint32_t baudrate);
void lcdm_tjc_force_baudrate(uint32_t target_baudrate);
void lcdm_tjc_rx_falling_edge_isr(void);
void lcdm_tjc_usart_irq_handler(void);
void lcdm_tjc_send_cmd(const char *cmd);
void lcdm_tjc_set_text(const char *obj, const char *text);
void lcdm_tjc_set_num(const char *obj, int32_t value);
void lcdm_tjc_page(uint8_t page_id);
uint8_t lcdm_tjc_poll_event(lcdm_tjc_event_t *event);

#endif
