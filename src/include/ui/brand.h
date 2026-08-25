#ifndef NUEVOOS_BRAND_H
#define NUEVOOS_BRAND_H

#include "types.h"

#define BRAND_LOGO_W 180u
#define BRAND_LOGO_H 180u
#define BRAND_LOGIN_W 96u
#define BRAND_LOGIN_H 96u
#define BRAND_NAME_W 320u
#define BRAND_NAME_H 84u

extern const u8 brand_logo_rgba[];
extern const u8 brand_login_rgba[];
extern const u8 brand_name_rgba[];

void draw_boot_splash(u8 progress);
void draw_boot_splash_ex(u8 progress, u8 mark_alpha, i32 y_shift);
void draw_boot_splash_to_back(u8 progress, u8 mark_alpha, i32 y_shift);

#endif
