#include "print_terminal.h"

#include "lcdm_tjc.h"
#include "line_comm_transport.h"
#include "print_driver.h"
#include "print_job_model.h"
#include "print_rs485.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define PRINT_TERMINAL_PAGE_EDIT 0U

volatile uint32_t g_print_terminal_event_count;
volatile uint32_t g_print_terminal_refresh_count;
volatile uint32_t g_print_terminal_print_count;
volatile uint32_t g_print_terminal_ir_request_count;
volatile uint32_t g_print_terminal_ir_print_count;
volatile uint8_t g_print_terminal_state;

static print_job_t active_job;
static char preview_text[PRINT_LABEL_TEXT_MAX];
static uint8_t refresh_requested;
static uint8_t config_refresh_requested;

static uint8_t ascii_is_allowed(char ch)
{
  return (ch >= 0x20 && ch <= 0x7E) ? 1U : 0U;
}

static void sanitize_ascii(char *text)
{
  uint16_t i;

  if(text == 0) {
    return;
  }

  for(i = 0U; text[i] != '\0'; i++) {
    if(ascii_is_allowed(text[i]) == 0U) {
      text[i] = ' ';
    } else if(text[i] == '"') {
      text[i] = '\'';
    } else if(text[i] == '\\') {
      text[i] = '/';
    }
  }
}

static uint16_t parse_u16(const char *text, uint16_t fallback)
{
  uint32_t value = 0U;
  uint8_t any = 0U;

  if(text == 0) {
    return fallback;
  }

  while(*text != '\0') {
    if(isdigit((unsigned char)*text) == 0) {
      break;
    }
    any = 1U;
    value = (value * 10UL) + (uint32_t)(*text - '0');
    if(value > 65535UL) {
      return 65535U;
    }
    text++;
  }

  return (any != 0U) ? (uint16_t)value : fallback;
}

static uint32_t parse_u32(const char *text, uint32_t fallback)
{
  uint32_t value = 0U;
  uint8_t any = 0U;

  if(text == 0) {
    return fallback;
  }

  while(*text != '\0') {
    if(isdigit((unsigned char)*text) == 0) {
      break;
    }
    any = 1U;
    if(value > 429496729UL) {
      return fallback;
    }
    value = (value * 10UL) + (uint32_t)(*text - '0');
    text++;
  }

  return (any != 0U) ? value : fallback;
}

static void update_preview(void)
{
  if(print_job_format_ascii_label(&active_job,
                                  preview_text,
                                  (uint16_t)sizeof(preview_text)) != PRINT_JOB_OK) {
    (void)snprintf(preview_text, sizeof(preview_text), "LABEL FORMAT ERROR");
  }
}

static void send_job_to_lcdm(void)
{
  update_preview();
  lcdm_tjc_set_num("nStation", active_job.station_id);
  lcdm_tjc_set_text("tTitle", active_job.title);
  lcdm_tjc_set_text("tItem", active_job.item);
  lcdm_tjc_set_text("tContent", active_job.content);
  lcdm_tjc_set_text("tCode", active_job.code);
  lcdm_tjc_set_num("nQty", active_job.quantity);
  lcdm_tjc_set_num("nCopies", active_job.copies);
  lcdm_tjc_set_text("tResult", active_job.pass ? "PASS" : "NG");
  lcdm_tjc_set_text("tPreview", preview_text);
}

static void send_config_to_lcdm(void)
{
  print_driver_config_t driver_cfg;
  print_rs485_config_t rs485_cfg;
  char text[8];

  print_driver_config_get(&driver_cfg);
  print_rs485_config_get(&rs485_cfg);

  lcdm_tjc_set_num("nBaud", (int32_t)rs485_cfg.baudrate);
  lcdm_tjc_set_num("nDataBits", rs485_cfg.data_bits);
  lcdm_tjc_set_num("nStopBits", rs485_cfg.stop_bits);
  if(rs485_cfg.parity == 1U) {
    (void)snprintf(text, sizeof(text), "EVEN");
  } else if(rs485_cfg.parity == 2U) {
    (void)snprintf(text, sizeof(text), "ODD");
  } else {
    (void)snprintf(text, sizeof(text), "NONE");
  }
  lcdm_tjc_set_text("tParity", text);
  lcdm_tjc_set_num("nDirEn", rs485_cfg.direction_enabled);
  lcdm_tjc_set_num("nDirHi", rs485_cfg.direction_active_high);

  lcdm_tjc_set_num("nLblW", driver_cfg.label_width_dot);
  lcdm_tjc_set_num("nLblL", driver_cfg.label_length_dot);
  lcdm_tjc_set_num("nOrgX", driver_cfg.origin_x_dot);
  lcdm_tjc_set_num("nOrgY", driver_cfg.origin_y_dot);
  lcdm_tjc_set_num("nRot", driver_cfg.rotation);
  lcdm_tjc_set_num("nSpeed", driver_cfg.print_speed_ips);
  lcdm_tjc_set_num("nDark", driver_cfg.print_darkness);
  lcdm_tjc_set_num("nTitleFont", driver_cfg.title_font_dot);
  lcdm_tjc_set_num("nBodyFont", driver_cfg.body_font_dot);
  lcdm_tjc_set_num("nFootFont", driver_cfg.footer_font_dot);
  lcdm_tjc_set_num("nBarW", driver_cfg.barcode_module_width);
  lcdm_tjc_set_num("nBarRatio", driver_cfg.barcode_ratio);
  lcdm_tjc_set_num("nBarH", driver_cfg.barcode_height_dot);
  lcdm_tjc_set_num("nBarText", driver_cfg.barcode_human_readable);
}

static void set_status_text(const char *text)
{
  lcdm_tjc_set_text("tStatus", text);
}

static void submit_current_print_job(const char *ok_text)
{
  if(print_driver_submit(&active_job) == PRINT_DRIVER_OK) {
    g_print_terminal_print_count++;
    g_print_terminal_state = PRINT_TERMINAL_STATE_PRINTED;
    set_status_text(ok_text);
  } else {
    g_print_terminal_state = PRINT_TERMINAL_STATE_ERROR;
    set_status_text("PRINT ERROR");
  }
  refresh_requested = 1U;
}

static uint8_t starts_with(const char *text, const char *prefix)
{
  return (strncmp(text, prefix, strlen(prefix)) == 0) ? 1U : 0U;
}

static void handle_config_packet(const char *packet)
{
  print_driver_config_t driver_cfg;
  print_rs485_config_t rs485_cfg;
  const char *value;

  if(packet == 0 || starts_with(packet, "cfg.") == 0U) {
    return;
  }

  value = strchr(packet, '=');
  if(value == 0) {
    return;
  }
  value++;

  print_driver_config_get(&driver_cfg);
  print_rs485_config_get(&rs485_cfg);

  if(starts_with(packet, "cfg.baud=") != 0U) {
    rs485_cfg.baudrate = parse_u32(value, rs485_cfg.baudrate);
    print_rs485_config_set(&rs485_cfg);
  } else if(starts_with(packet, "cfg.databits=") != 0U) {
    rs485_cfg.data_bits = (uint8_t)parse_u16(value, rs485_cfg.data_bits);
    print_rs485_config_set(&rs485_cfg);
  } else if(starts_with(packet, "cfg.stop=") != 0U) {
    rs485_cfg.stop_bits = (uint8_t)parse_u16(value, rs485_cfg.stop_bits);
    print_rs485_config_set(&rs485_cfg);
  } else if(starts_with(packet, "cfg.parity=") != 0U) {
    if(strcmp(value, "EVEN") == 0 || strcmp(value, "even") == 0 || strcmp(value, "1") == 0) {
      rs485_cfg.parity = 1U;
    } else if(strcmp(value, "ODD") == 0 || strcmp(value, "odd") == 0 || strcmp(value, "2") == 0) {
      rs485_cfg.parity = 2U;
    } else {
      rs485_cfg.parity = 0U;
    }
    print_rs485_config_set(&rs485_cfg);
  } else if(starts_with(packet, "cfg.dir_en=") != 0U) {
    rs485_cfg.direction_enabled = (parse_u16(value, rs485_cfg.direction_enabled) == 0U) ? 0U : 1U;
    print_rs485_config_set(&rs485_cfg);
  } else if(starts_with(packet, "cfg.dir_hi=") != 0U) {
    rs485_cfg.direction_active_high = (parse_u16(value, rs485_cfg.direction_active_high) == 0U) ? 0U : 1U;
    print_rs485_config_set(&rs485_cfg);
  } else if(starts_with(packet, "cfg.width=") != 0U) {
    driver_cfg.label_width_dot = parse_u16(value, driver_cfg.label_width_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.length=") != 0U) {
    driver_cfg.label_length_dot = parse_u16(value, driver_cfg.label_length_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.orgx=") != 0U) {
    driver_cfg.origin_x_dot = parse_u16(value, driver_cfg.origin_x_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.orgy=") != 0U) {
    driver_cfg.origin_y_dot = parse_u16(value, driver_cfg.origin_y_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.rot=") != 0U) {
    driver_cfg.rotation = (uint8_t)parse_u16(value, driver_cfg.rotation);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.speed=") != 0U) {
    driver_cfg.print_speed_ips = (uint8_t)parse_u16(value, driver_cfg.print_speed_ips);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.dark=") != 0U) {
    driver_cfg.print_darkness = (uint8_t)parse_u16(value, driver_cfg.print_darkness);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.titlefont=") != 0U) {
    driver_cfg.title_font_dot = parse_u16(value, driver_cfg.title_font_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.bodyfont=") != 0U) {
    driver_cfg.body_font_dot = parse_u16(value, driver_cfg.body_font_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.footfont=") != 0U) {
    driver_cfg.footer_font_dot = parse_u16(value, driver_cfg.footer_font_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.barw=") != 0U) {
    driver_cfg.barcode_module_width = (uint8_t)parse_u16(value, driver_cfg.barcode_module_width);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.barratio=") != 0U) {
    driver_cfg.barcode_ratio = (uint8_t)parse_u16(value, driver_cfg.barcode_ratio);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.barh=") != 0U) {
    driver_cfg.barcode_height_dot = parse_u16(value, driver_cfg.barcode_height_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.bartext=") != 0U) {
    driver_cfg.barcode_human_readable = (parse_u16(value, driver_cfg.barcode_human_readable) == 0U) ? 0U : 1U;
    (void)print_driver_config_set(&driver_cfg);
  } else if(strcmp(packet, "cfg.default=1") == 0) {
    print_driver_config_reset_default();
    print_rs485_config_reset_default();
  }

  config_refresh_requested = 1U;
  refresh_requested = 1U;
  set_status_text("CFG OK");
}

static void handle_field_packet(const char *packet)
{
  char field_text[PRINT_FIELD_CONTENT_LEN];
  const char *value;

  if(packet == 0) {
    return;
  }

  if(starts_with(packet, "cfg.") != 0U) {
    handle_config_packet(packet);
    return;
  }

  if(starts_with(packet, "title=") != 0U) {
    value = &packet[6];
    (void)snprintf(field_text, sizeof(field_text), "%s", value);
    sanitize_ascii(field_text);
    (void)print_job_set_title(&active_job, field_text);
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "station=") != 0U) {
    uint16_t station = parse_u16(&packet[8], active_job.station_id);
    if(station >= 1U && station <= 10U) {
      (void)print_job_set_station(&active_job, (uint8_t)station);
    }
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "item=") != 0U) {
    value = &packet[5];
    (void)snprintf(field_text, sizeof(field_text), "%s", value);
    sanitize_ascii(field_text);
    (void)print_job_set_item(&active_job, field_text);
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "content=") != 0U) {
    value = &packet[8];
    (void)snprintf(field_text, sizeof(field_text), "%s", value);
    sanitize_ascii(field_text);
    (void)print_job_set_content(&active_job, field_text);
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "code=") != 0U) {
    value = &packet[5];
    (void)snprintf(field_text, sizeof(field_text), "%s", value);
    sanitize_ascii(field_text);
    (void)print_job_set_code(&active_job, field_text);
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "qty=") != 0U) {
    active_job.quantity = parse_u16(&packet[4], active_job.quantity);
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "copies=") != 0U) {
    active_job.copies = (uint8_t)parse_u16(&packet[7], active_job.copies);
    if(active_job.copies == 0U) {
      active_job.copies = 1U;
    }
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "pass=") != 0U) {
    active_job.pass = (parse_u16(&packet[5], active_job.pass) == 0U) ? 0U : 1U;
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "result=") != 0U) {
    value = &packet[7];
    if(strcmp(value, "PASS") == 0 || strcmp(value, "pass") == 0 || strcmp(value, "1") == 0) {
      active_job.pass = 1U;
    } else {
      active_job.pass = 0U;
    }
    refresh_requested = 1U;
    return;
  }

  if(strcmp(packet, "preview") == 0) {
    g_print_terminal_state = PRINT_TERMINAL_STATE_PREVIEW;
    refresh_requested = 1U;
    return;
  }

  if(strcmp(packet, "print") == 0) {
    submit_current_print_job("PRINT OK");
    return;
  }

  if(strcmp(packet, "refresh") == 0 || starts_with(packet, "page=") != 0U) {
    refresh_requested = 1U;
    config_refresh_requested = 1U;
  }
}

static void handle_touch_event(const lcdm_tjc_event_t *event)
{
  if(event == 0 || event->touch_event == 0U) {
    return;
  }

  if(event->component_id == 1U) {
    handle_field_packet("preview");
  } else if(event->component_id == 2U) {
    handle_field_packet("print");
  } else if(event->component_id == 3U) {
    print_job_init_default(&active_job);
    g_print_terminal_state = PRINT_TERMINAL_STATE_EDIT;
    refresh_requested = 1U;
  }
}

void print_terminal_init(void)
{
  g_print_terminal_state = PRINT_TERMINAL_STATE_EDIT;
  print_job_init_default(&active_job);
  print_driver_init();
  line_comm_transport_init(LINE_COMM_TRANSPORT_IR);
  lcdm_tjc_init();
  lcdm_tjc_send_cmd("bkcmd=0");
  lcdm_tjc_page(PRINT_TERMINAL_PAGE_EDIT);
  send_job_to_lcdm();
  send_config_to_lcdm();
  set_status_text("READY");
}

void print_terminal_service(void)
{
  lcdm_tjc_event_t event;
  line_comm_print_request_t request;

  while(lcdm_tjc_poll_event(&event) != 0U) {
    g_print_terminal_event_count++;
    if(event.type == LCDM_TJC_EVENT_ASCII) {
      handle_field_packet(event.ascii);
    } else if(event.type == LCDM_TJC_EVENT_TOUCH) {
      handle_touch_event(&event);
    }
  }

  if(line_comm_transport_poll_print_request(&request, 1U) == LINE_COMM_OK) {
    g_print_terminal_ir_request_count++;
    if(request.source_station >= 1U && request.source_station <= 10U) {
      (void)print_job_set_station(&active_job, request.source_station);
    }
    if(request.pass != 0U) {
      active_job.pass = 1U;
    }
    submit_current_print_job("IR PRINT");
    g_print_terminal_ir_print_count++;
  }

  if(refresh_requested != 0U) {
    refresh_requested = 0U;
    g_print_terminal_refresh_count++;
    send_job_to_lcdm();
    if(config_refresh_requested != 0U) {
      config_refresh_requested = 0U;
      send_config_to_lcdm();
    }
  }
}
