#ifndef PRINT_JOB_MODEL_H
#define PRINT_JOB_MODEL_H

#include <stdint.h>

#define PRINT_FIELD_TITLE_LEN        24U
#define PRINT_FIELD_ITEM_LEN         24U
#define PRINT_FIELD_CONTENT_LEN      48U
#define PRINT_FIELD_CODE_LEN         32U
#define PRINT_LABEL_TEXT_MAX         192U

typedef enum {
  PRINT_JOB_OK = 0,
  PRINT_JOB_BAD_ARGUMENT,
  PRINT_JOB_BUFFER_TOO_SMALL
} print_job_status_t;

typedef struct {
  char title[PRINT_FIELD_TITLE_LEN];
  char item[PRINT_FIELD_ITEM_LEN];
  char content[PRINT_FIELD_CONTENT_LEN];
  char code[PRINT_FIELD_CODE_LEN];
  uint16_t quantity;
  uint8_t copies;
  uint8_t pass;
} print_job_t;

void print_job_init_default(print_job_t *job);
print_job_status_t print_job_set_title(print_job_t *job, const char *text);
print_job_status_t print_job_set_item(print_job_t *job, const char *text);
print_job_status_t print_job_set_content(print_job_t *job, const char *text);
print_job_status_t print_job_set_code(print_job_t *job, const char *text);
print_job_status_t print_job_format_ascii_label(const print_job_t *job,
                                                char *out_text,
                                                uint16_t out_capacity);

#endif
