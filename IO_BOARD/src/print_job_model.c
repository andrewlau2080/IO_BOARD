#include "print_job_model.h"

#include <stdio.h>
#include <string.h>

static print_job_status_t copy_field(char *dst, uint16_t dst_len, const char *src)
{
  uint16_t i;

  if(dst == 0 || dst_len == 0U || src == 0) {
    return PRINT_JOB_BAD_ARGUMENT;
  }

  for(i = 0U; i < (uint16_t)(dst_len - 1U) && src[i] != '\0'; i++) {
    dst[i] = src[i];
  }
  dst[i] = '\0';

  return PRINT_JOB_OK;
}

void print_job_init_default(print_job_t *job)
{
  if(job == 0) {
    return;
  }

  memset(job, 0, sizeof(*job));
  (void)copy_field(job->title, PRINT_FIELD_TITLE_LEN, "HARNESS TEST");
  (void)copy_field(job->item, PRINT_FIELD_ITEM_LEN, "MODEL");
  (void)copy_field(job->content, PRINT_FIELD_CONTENT_LEN, "CONTENT");
  (void)copy_field(job->code, PRINT_FIELD_CODE_LEN, "CODE000001");
  job->quantity = 1U;
  job->copies = 1U;
  job->pass = 1U;
}

print_job_status_t print_job_set_title(print_job_t *job, const char *text)
{
  return (job == 0) ? PRINT_JOB_BAD_ARGUMENT : copy_field(job->title, PRINT_FIELD_TITLE_LEN, text);
}

print_job_status_t print_job_set_item(print_job_t *job, const char *text)
{
  return (job == 0) ? PRINT_JOB_BAD_ARGUMENT : copy_field(job->item, PRINT_FIELD_ITEM_LEN, text);
}

print_job_status_t print_job_set_content(print_job_t *job, const char *text)
{
  return (job == 0) ? PRINT_JOB_BAD_ARGUMENT : copy_field(job->content, PRINT_FIELD_CONTENT_LEN, text);
}

print_job_status_t print_job_set_code(print_job_t *job, const char *text)
{
  return (job == 0) ? PRINT_JOB_BAD_ARGUMENT : copy_field(job->code, PRINT_FIELD_CODE_LEN, text);
}

print_job_status_t print_job_format_ascii_label(const print_job_t *job,
                                                char *out_text,
                                                uint16_t out_capacity)
{
  int written;

  if(job == 0 || out_text == 0 || out_capacity == 0U) {
    return PRINT_JOB_BAD_ARGUMENT;
  }

  written = snprintf(out_text,
                     out_capacity,
                     "TITLE:%s\nITEM:%s\nCONTENT:%s\nCODE:%s\nQTY:%u\nRESULT:%s\n",
                     job->title,
                     job->item,
                     job->content,
                     job->code,
                     (unsigned int)job->quantity,
                     job->pass ? "PASS" : "NG");

  if(written < 0 || written >= (int)out_capacity) {
    return PRINT_JOB_BUFFER_TOO_SMALL;
  }

  return PRINT_JOB_OK;
}
