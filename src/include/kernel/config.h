#ifndef CORDOS_CONFIG_H
#define CORDOS_CONFIG_H

#include "types.h"

/*
 * Datos globales del sistema.
 * Semver de producto: 0.x = desktop usable, userland aún mínimo.
 */
extern const char *name_os;
extern const char *version_os;
extern const char *codename_os;
extern const char *arch_os;
extern const char *build_os;
extern const char *author_os;
extern const char *license_os;

extern u32 major_os;
extern u32 minor_os;
extern u32 patch_os;

#endif
