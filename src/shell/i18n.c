#include "i18n.h"

enum lang_id lang_os = LANG_ES;

extern const char *const i18n_codes[I18N_LANG_COUNT];
extern const char *const i18n_names[I18N_LANG_COUNT];
extern const u8 i18n_day_first[I18N_LANG_COUNT];
extern const char *const i18n_date_joins[I18N_LANG_COUNT];
extern const char *const i18n_weekdays[I18N_LANG_COUNT][7];
extern const char *const i18n_months[I18N_LANG_COUNT][12];
extern const char *const i18n_months_abbr[I18N_LANG_COUNT][12];
extern const char *const i18n_strings[I18N_LANG_COUNT][MSG_COUNT];

static u32 lang_index(enum lang_id lang)
{
    u32 i = (u32)lang;

    if (i >= I18N_LANG_COUNT) {
        return 0;
    }
    return i;
}

void i18n_init(void)
{
    lang_os = LANG_ES;
}

void i18n_set_lang(enum lang_id lang)
{
    lang_os = (enum lang_id)lang_index(lang);
}

enum lang_id i18n_lang(void)
{
    return lang_os;
}

u32 i18n_lang_count(void)
{
    return I18N_LANG_COUNT;
}

const char *i18n_lang_code(enum lang_id lang)
{
    return i18n_codes[lang_index(lang)];
}

const char *i18n_lang_name(enum lang_id lang)
{
    return i18n_names[lang_index(lang)];
}

const char *i18n(enum msg_id id)
{
    if ((u32)id >= (u32)MSG_COUNT) {
        return "?";
    }
    return i18n_strings[lang_index(lang_os)][id];
}

const char *i18n_month_abbr(u8 month_1_12)
{
    if (month_1_12 < 1u || month_1_12 > 12u) {
        return "?";
    }
    return i18n_months_abbr[lang_index(lang_os)][month_1_12 - 1u];
}

const char *i18n_month(u8 month_1_12)
{
    if (month_1_12 < 1u || month_1_12 > 12u) {
        return "?";
    }
    return i18n_months[lang_index(lang_os)][month_1_12 - 1u];
}

const char *i18n_weekday(u8 weekday_1_7)
{
    if (weekday_1_7 < 1u || weekday_1_7 > 7u) {
        return "?";
    }
    return i18n_weekdays[lang_index(lang_os)][weekday_1_7 - 1u];
}

bool i18n_date_day_first(void)
{
    return i18n_day_first[lang_index(lang_os)] != 0u;
}

const char *i18n_date_join(void)
{
    return i18n_date_joins[lang_index(lang_os)];
}

static bool code_match(const char *have, const char *want)
{
    u32 i;

    if (have == NULL || want == NULL) {
        return false;
    }
    for (i = 0; have[i] != '\0' && want[i] != '\0'; ++i) {
        char a = have[i];
        char b = want[i];

        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
        if (a == '-') {
            break;
        }
    }
    return i >= 2u;
}

bool i18n_set_lang_code(const char *code)
{
    u32 i;

    if (code == NULL || code[0] == '\0') {
        return false;
    }
    for (i = 0; i < I18N_LANG_COUNT; ++i) {
        if (code_match(i18n_codes[i], code)) {
            i18n_set_lang((enum lang_id)i);
            return true;
        }
    }
    return false;
}
