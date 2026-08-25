/* Embedded CordOS boot marks (RGBA8888). */
    .section .rodata
    .align 16
    .global brand_logo_rgba
brand_logo_rgba:
    .incbin "out/brand_logo.rgba"

    .align 16
    .global brand_login_rgba
brand_login_rgba:
    .incbin "out/brand_login.rgba"

    .align 16
    .global brand_name_rgba
brand_name_rgba:
    .incbin "out/brand_name.rgba"
