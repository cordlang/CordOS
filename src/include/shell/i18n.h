#ifndef NUEVOOS_I18N_H
#define NUEVOOS_I18N_H

#include "types.h"
#include "i18n_gen.h"

extern enum lang_id lang_os;

void i18n_init(void);
void i18n_set_lang(enum lang_id lang);
enum lang_id i18n_lang(void);
u32 i18n_lang_count(void);
const char *i18n_lang_code(enum lang_id lang);
const char *i18n_lang_name(enum lang_id lang);
const char *i18n(enum msg_id id);
const char *i18n_month_abbr(u8 month_1_12);
const char *i18n_month(u8 month_1_12);
const char *i18n_weekday(u8 weekday_1_7);
bool i18n_date_day_first(void);
const char *i18n_date_join(void);

/* Optional: "es", "en", "es-ES", "en-US" → true if applied. */
bool i18n_set_lang_code(const char *code);

#endif
