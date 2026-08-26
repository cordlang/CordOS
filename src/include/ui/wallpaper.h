#ifndef CORDOS_WALLPAPER_H
#define CORDOS_WALLPAPER_H

#include "types.h"

#define WALLPAPER_W 1920u
#define WALLPAPER_H 1080u

#define LOGIN_WP_DEFAULT  0u
#define LOGIN_WP_ABSTRACT 1u
#define LOGIN_WP_COUNT    2u

#define DESK_WP_DEFAULT  0u
#define DESK_WP_ABSTRACT 1u
#define DESK_WP_COUNT    2u

/* RGB888 packed, row-major. Desktop and login share login.jpg / abstract. */
extern const u8 wallpaper_rgb[];
extern const u8 wallpaper_login_rgb[];
extern const u8 wallpaper_login_alt_rgb[];

void wallpaper_set_login(u32 id);
u32 wallpaper_login_id(void);
const u8 *wallpaper_login_pixels(void);
const u8 *wallpaper_login_pixels_id(u32 id);

void wallpaper_set_desk(u32 id);
u32 wallpaper_desk_id(void);
const u8 *wallpaper_desk_pixels(void);
const u8 *wallpaper_desk_pixels_id(u32 id);

#endif
