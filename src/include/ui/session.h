#ifndef CORDOS_SESSION_H
#define CORDOS_SESSION_H

#include "types.h"

/* Splash progress stages (Ola 0, text UI). */
void session_splash_begin(void);
void session_splash_stage(u32 stage); /* 0..3 */
void session_splash_finish(void);

/* Login → Home loop (never returns unless power path halts). */
void session_run(void);

#endif
