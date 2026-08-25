# CordOS — Roadmap visual, motion y rendimiento

Documento de producto. Complementa [`UI_PLAN.md`](UI_PLAN.md) (viaje splash → login → home) y [`ROADMAP.md`](ROADMAP.md) (kernel). Aquí el foco es: **que se mueva fluido, que se vea extremo, y que no coma la máquina**.

**Destino de uso:** estación para **videojuegos** y **desarrollo**. No oficina genérica. No “plataforma de servicios”. El OS existe para que el juego y el compilador tengan CPU, RAM y frame time.

---

## 1. Tesis

Windows es lento en el escritorio porque el shell, indexadores, telemetría, antivirus, widgets y un compositor 3D viven siempre encendidos. Eso no es “el precio de una UI bonita”. Es basura de fondo.

CordOS hace lo contrario:

| Windows (lo que no copiamos) | CordOS |
|---|---|
| Cientos de procesos idle | Kernel + compositor + lo que el usuario abrió |
| RAM “reservada” por servicios | RAM del juego / del compilador |
| UI a 60 fps sobre un stack de GB | UI 2D propia, damage rects, halt si no hay input |
| Animar = GPU + DWM + toolkit + XAML | Un reloj, un compositor, un toolkit |

La UI tiene que verse **exageradamente bien** y costar **casi nada** cuando no se toca. El frame del juego no se lo come Explorer.

### Reglas que no se negocian

1. **Nada de terceros** (no GTK, Qt, Electron, Chromium-shell, .NET). Motion y chrome son nuestros.
2. **Nada que corra sin que el usuario lo haya pedido.** Ni indexador, ni telemetría, ni “experiencias”, ni antivirus residente.
3. **El compositor no es un juego 3D.** Capas 2D, vsync, damage. La GPU es para el *juego*, no para el dock.
4. **Idle = `hlt`.** Si no hay input ni animación, no hay spin.
5. **Un frame de UI tiene presupuesto.** Si una animación no cabe en él, no se añade partículas: se recorta el trabajo.

---

## 2. Presupuesto (números para no mentirnos)

Objetivo en 1920×1080×32, 1 núcleo, 512 MB de VM de desarrollo (producto real: el usuario pone más para juegos).

| Recurso | Techo UI (sesión gráfica) | Notas |
|---|---|---|
| RAM del compositor | ≤ 32 MiB | escena + wallpaper + 1 scratch. Hoy el backbuffer 1080p ya son ~8 MiB |
| RAM de chrome (dock, login, fuentes, iconos) | ≤ 16 MiB | bitmaps propios, no TTF gigante hasta que haga falta |
| Tiempo de un hover | ≤ 1 ms | `present_rect` del widget, no blit de 1920×1080 |
| Frame de UI a 60 Hz | ≤ 4 ms de trabajo / 16.6 ms de vsync | el resto es espera |
| CPU idle (escritorio quieto) | ~0 % (halt) | si `top` existiera, no habría “Service Host” |
| Procesos siempre vivos | 0 extra | hasta ring 3: ni siquiera un “searchui” |

Si un cambio de UI sube el techo, se documenta *qué se come* o no entra.

**Juego en primer plano:** el compositor deja de animar chrome (dock freeze), present mínimo, input con prioridad. El frame time es del juego, no de CordOS.

---

## 3. Motor de motion (cómo se mueve, no “que se mueva”)

Hoy: `animation.c` hace fade/crossfade a pasos fijos con `wait_ms` (bloquea el loop). El toolkit ya tiene **lerp de hover** (`widget.c`, paso 48/frame). El compositor ya puede copiar **solo un rect**.

Eso no es motion de producto todavía. Falta un reloj.

```text
tick (PIT / más adelante HPET)
  → dt_ms
  → toolkit: hover, press, caret, ventanas
  → compositor: interpola x/y/opacity de superficies
  → present damage
  → halt hasta el siguiente vsync o input
```

### Gestos de sistema (pocos, caros de pulir, baratos de correr)

No un catálogo de 40 transiciones. Cinco, y todas a 60 Hz con `dt`:

| Gesto | Dónde | Duración | Ease |
|---|---|---|---|
| **Hover lift** | botón, pill, icono dock | 80–120 ms | out-cubic |
| **Press** | mismo widget, scale 0.98 | 60 ms | in-out |
| **Page** | onboarding / settings | 220–280 ms | out-cubic, slide 24–32 px + fade |
| **Open surface** | ventana desde dock | 180–240 ms | opacity + 12 px lift |
| **Login → desktop** | sesión | 280 ms | crossfade (ya existe, pasar a dt) |

Prohibido: parallax de wallpaper, partículas, glow animado de marca, bounce elástico en cada icono. Eso es ruido y RAM.

### Reloj

- `ui_tick(dt_ms)` en el toolkit. Si `ui_busy()`, no `hlt` hasta que las lerps terminen.
- Nada de `wait_ms` dentro de un fade de producto (bloquea input y parece “lag”).
- El loop de sesión: input → tick → present → halt. Un solo sitio.

Archivos: `src/ui/widgets/widget.c`, `src/ui/gfx/animation.c`, `src/ui/gfx/compositor.c`.

---

## 4. Olas visuales (en orden)

Cada ola deja ISO arrancable. No saltar a GPU para “que se vea más liso”: primero el reloj y el damage.

### Ola V0 — Base (hecha)

- Compositor propio + toolkit propio (`compositor.c`, `widget.c`).
- Hover lerp en onboarding y dock.
- Present de transiciones vs present_rect de hover.
- Login glass = oro visual. No se rediseña por moda.

### Ola V1 — Reloj y hover de verdad

**Meta:** todo lo que se ilumina usa `dt`, no “si el mouse se movió, un paso”.

- [ ] `ui_tick(dt)` / `ui_busy()` como contrato del loop (onboarding, login, desktop).
- [ ] Hover 80–120 ms en **todos** los widgets (login go/power/campo, no solo idioma).
- [ ] Press visible (alpha o scale 1→0.98) en click.
- [ ] Caret y reloj no disparan blit de pantalla completa: damage del campo / de la hora.
- [ ] Quitar `wait_ms` de los fades de sesión; el crossfade usa ticks.

**Criterio:** pasar el mouse por English → Continuar se siente aceite, no un parpadeo. F1 sigue abriendo shell.

### Ola V2 — Páginas que viajan

**Meta:** cambiar de paso (idioma → welcome → nombre) no es un corte.

- [ ] `ui_comp` interpola `opacity` + `y` de una superficie (la página).
- [ ] Onboarding: page slide 24 px + fade, 240 ms.
- [ ] Login → desktop: el crossfade actual, con dt, sin congelar el cursor.
- [ ] Abrir ventana: lift 12 px desde el dock, 200 ms. Cerrar: inverso.
- [ ] Dock: hover de slot con lift 4 px (barato: un rect, no todo el dock).

**Criterio:** ninguna transición “salta” un frame a opacidad 255. Si el host (VirtualBox) recaptura LFB, el compositor no reintroduce la raya de 1 px: cursor overlay, no memcpy full en cada tick.

### Ola V3 — Capas (rendimiento de verdad)

**Meta:** el wallpaper se pinta **una vez**. El chrome no lo vuelve a tocar.

```text
capa 0  wallpaper + overlay   (estática, 8 MiB)
capa 1  superficies (ventanas, onboarding)
capa 2  dock / menú
capa 3  cursor
```

- [ ] Wallpaper frozen: no `draw_bg_login()` en cada hover.
- [ ] Damage = unión de widgets sucios contra la capa 1/2, compose sobre wallpaper.
- [ ] Present pesado (resync / touch_band) **solo** al entrar a una pantalla, nunca en tick.
- [ ] Presupuesto: hover < 1 ms medido con PIT (ciclo start/end en serial, debug).

**Criterio:** mover el mouse 5 s sobre el dock no copia 1920×1080 ni una vez.

### Ola V4 — Vsync y fluidez host/guest

**Meta:** 60 fps de UI estables en VirtualBox (VBoxVGA) y, más adelante, en hardware.

- [ ] Present alineado a vsync del dispositivo (Bochs/VBox: no fiarse de VGA `0x3DA`; usar PIT 60 Hz o el hint del FB).
- [ ] Triple-buffer opcional **solo** si no reabre la raya de 1 px (page-flip ya se intentó y falló en VBox). Preferir backbuffer RAM + present_rect.
- [ ] Si el guest es 1920×1080, la ventana host es 1:1. Escalado del host = prohibido en el camino de producto (`run-vbox.ps1` ya lo fuerza).
- [ ] Modo “juego”: una syscall o flag que pausa animaciones de chrome y entrega el LFB / GPU al proceso.

**Criterio:** grabar el dock a 60 fps sin stutter a ojo. Serial no muestra present > 4 ms.

### Ola V5 — Texto y detalle (sin inflar RAM)

- [ ] Un peso de fuente UI (la actual) + display solo en splash/login.
- [ ] Hinting / overlap de glifos en motion (texto que viaja no trepida).
- [ ] Sombras: un blur de 4 px precalculado por radio de widget, no Gaussian en vivo.
- [ ] Glass: frost ya cacheado; no re-blur el wallpaper.

### Ola V6 — GPU para juegos, no para el panel

Cuando haya un proceso de juego:

- [ ] Camino **fullscreen exclusive**: el compositor suelta el LFB / el framebuffer nativo.
- [ ] Alt+Tab / Super: vuelve el chrome en un frame, sin reindexar el universo.
- [ ] Input: cola corta, sin coalescer clicks; el hover de UI sí puede coalescer moves.
- [ ] Nada de overlay de “Xbox Game Bar”. Un FPS counter opcional en serial o un overlay de 12 px que el juego puede ignorar.

La GPU 3D (VirtIO-GPU / luego driver real) entra **aquí**, no para animar el dock.

---

## 5. Rendimiento del sistema (no solo UI)

La UI rápida no sirve si el kernel desperdicia igual.

| Frente | Qué hacer | Qué no hacer |
|---|---|---|
| RAM | Contar páginas del compositor y del heap; panic o log si el chrome crece | Caches “por si acaso”, prefetch de iconos no vistos |
| CPU | Un loop de sesión; drivers en IRQ; idle halt | Threads de “optimización”, GC, JIT en el shell |
| Disco | NOSFS cuando haga falta el proyecto; no indexar | Windows Search, Superfetch, SysMain |
| Red | Stack cuando el juego/dev lo pida | Telemetría, CDN de widgets, update silencioso |
| Procesos | Ring 3 más adelante: el compositor puede ser 1 proceso | 80 svchost |
| Scheduler | Prioridad al proceso foreground (juego / `make`) | Fairness de 50 tareas de background |

**Desarrollo en CordOS:** terminal + editor + compilador. El dock no recompila CSS. El compositor no parsea HTML. Un frame de UI no toca el FS.

**Juegos en CordOS:** latencia de input y frame time. El OS se aparta.

---

## 6. Cómo se ve el producto (dirección, no clon)

Sigue el oro de login: glass, luma Rec. 601, acento menta, fondo fotográfico. Motion *de instrumento*, no de red social.

- Hover = el control se llena de luz, no un outline Windows.
- El dock no escala a 1.2× (caro y borroso en software). Lift 4 px + sheen.
- Las ventanas no proyectan sombras de 64 px. Una sombra 8 px precomputada.
- 60 fps de chrome es suficiente. 144 Hz es para el juego, cuando haya GPU.

---

## 7. Orden de implementación (cuando digamos “adelante”)

1. **V1** reloj `dt` + hover/press en login y onboarding (se siente ya).
2. **V3** wallpaper frozen + present_rect medido (se *nota* la RAM/CPU).
3. **V2** page y open-window (se ve “OS de verdad”).
4. **V4** vsync / modo juego.
5. **V5–V6** detalle y GPU.

No invertir 2 y 3: animar páginas copiando 1080p a 60 Hz es el anti-CordOS.

---

## 8. Criterio de “estamos listos para juegos/dev”

Un desconocido en una máquina CordOS:

- Llega al desktop en segundos, no a un “getting ready”.
- El hover es fluido. El idle no calienta el CPU.
- Abre terminal, el compositor no se inmuta.
- Lanza un fullscreen: chrome desaparece, input no pasa por 12 procesos.
- `F1` siempre es la válvula de escape.

Si para una animación hay que dejar un demonio encendido, la animación no entra.
