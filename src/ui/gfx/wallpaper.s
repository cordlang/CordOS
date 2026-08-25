/* Embedded wallpapers (RGB888 1920x1080). */
    .section .rodata
    .align 16
    .global wallpaper_rgb
wallpaper_rgb:
    .incbin "out/wallpaper.rgb"

    .align 16
    .global wallpaper_login_rgb
wallpaper_login_rgb:
    .incbin "out/login_wallpaper.rgb"

    .align 16
    .global wallpaper_login_alt_rgb
wallpaper_login_alt_rgb:
    .incbin "out/login_wallpaper_alt.rgb"
