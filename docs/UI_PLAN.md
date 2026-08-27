# CordOS — Plan de UI (arranque → home → sistema)

Documento de UX (viaje splash → login → Home). El plan de **kernel y userland**
está en [`ROADMAP.md`](ROADMAP.md) (canónico: 0–11 cerradas; 13 ELF hecho, spawn no). Motion / frame time:
[`VISUAL_ROADMAP.md`](VISUAL_ROADMAP.md).

**Estado:** Ola 0 (texto) y **Ola 1 (login/home con framebuffer) hechas.** El
desktop, dock, ventanas y Spotlight ya arrancan. Lo que queda aquí es pulido de
viaje (lock, editor, etc.), no “construir el Home”.

**Regla:** no reabrir el framebuffer como si no existiera. Trabajo visual nuevo
sigue las olas V1–V6 del roadmap visual.

Versión de producto: **0.1.0** (`config.c`). Un `LICENSE` en la raíz queda para cuando fijemos SPDX.

---

## 0. Objetivo

Una UI **propia** (no clon de Windows/macOS/Linux) que acompañe al usuario desde que enciende la máquina hasta un **Home** usable, con camino claro hacia apps, ajustes y apagado.

El destino gráfico **ya corre**. Este archivo describe el viaje y las superficies;
no es un plan de “un día tendremos login”.

---

## 1. Principios de experiencia

| Principio | Significado práctico |
|---|---|
| Un solo hilo narrativo | Boot → **Login** → Home: siempre, como cualquier OS de escritorio. |
| Sesión explícita | Nadie llega al Home sin pasar por la pantalla de login (aunque el usuario sea local/guest). |
| Silencio por defecto | Mensajes técnicos van a serial/debug; la pantalla muestra solo lo humano. |
| Teclado primero | Todo usable solo con teclado; ratón después. |
| Una acción primaria | Cada pantalla tiene una cosa obvia que hacer (Enter / clic). |
| Progresión honesta | No fingir desktop completo: cada ola entrega una superficie real. |
| Identidad visible | El nombre del OS es el héroe del **splash**, del **login** y del Home. |
| Recuperable | Siempre hay escape: volver atrás, a shell de emergencia, o reiniciar limpio. |

### Anti-objetivos (explícitos)

- No copiar la barra de tareas de Windows ni el Dock de macOS al pie de la letra.
- No dashboard con stats en el primer viewport del Home.
- No cards decorativas sin interacción.
- No depender de red ni de cuentas cloud para llegar al Home.

---

## 2. Identidad visual (dirección)

Definición provisional hasta reemplazar `temporal`. Elegimos una dirección **clara** para no improvisar ola a ola.

### 2.1 Concepto

**“Taller nocturno / instrumento”** — interfaz densa pero calmada: fondo atmosférico (no plano), tipografía con carácter, acento único, mucho aire en Home, contraste alto en texto.

### 2.2 Tokens (propuesta)

| Token | Valor provisional | Uso |
|---|---|---|
| `--bg0` | `#0E1114` | Lienzo base |
| `--bg1` | `#161B20` | Paneles / superficie Home |
| `--fg` | `#E8EEF2` | Texto principal |
| `--fg-dim` | `#8B9AA6` | Secundario |
| `--accent` | `#3DDC97` | CTA, foco, selección (verde-menta; **no** púrpura) |
| `--danger` | `#E85D4C` | Errores / apagar |
| `--border` | `#2A333C` | Separadores finos |
| `--font-ui` | Sans geométrica (cargar más adelante; evitar Inter/Roboto/Arial) | UI |
| `--font-display` | Display con personalidad para splash/Home brand | Marca |
| `--radius` | 0–4 px | Casi sin “pill”; esquinas discretas |
| `--space` | 8 px base | Escala 8/16/24/40 |

### 2.3 Motion (presencia, no ruido)

El plan de **animación, fluidez y presupuesto de RAM/CPU** (estación de juegos + desarrollo) está en [`VISUAL_ROADMAP.md`](VISUAL_ROADMAP.md). Aquí el mínimo de identidad:

1. **Fade + lift** suave al pasar splash → **login** (200–320 ms).
2. **Foco** que respira en el campo/botón seleccionado (opacidad/borde accent).
3. **Abrir superficie** (panel o ventana desde Home) con slide corto desde un borde.
4. **Login → Home**: crossfade corto tras autenticación OK.

Sin glow excesivo, sin partículas, sin parallax en boot. Sin demonios de fondo para animar.

### 2.4 Branding en primer viewport

En splash, **login** y Home, al quitar chrome secundario, debe seguir siendo reconocible el producto: nombre grande + ancla visual, no solo un icono de 16 px. El login es una pantalla de marca, no un diálogo genérico.

---

## 3. Arquitectura técnica de UI (capas)

```text
┌─────────────────────────────────────────────┐
│  Apps / Shell de usuario (futuro ring 3)    │
├─────────────────────────────────────────────┤
│  Toolkit (widgets: label, button, list…)    │
├─────────────────────────────────────────────┤
│  Compositor / Window manager (wm)           │
├─────────────────────────────────────────────┤
│  Draw: framebuffer 32-bit + font renderer   │
├─────────────────────────────────────────────┤
│  Input: teclado → (luego mouse/tablet)      │
├─────────────────────────────────────────────┤
│  Kernel: fb, irq, heap, VFS (ya existe)     │
└─────────────────────────────────────────────┘
```

### 3.1 Modos de presentación

| Modo | Cuándo | Rol |
|---|---|---|
| `text` | Emergencia / debug | Shell VGA actual |
| `boot_gfx` | Splash y progreso de carga | Framebuffer fullscreen, sin ventanas |
| `session` | Login + Home + apps | Compositor + toolkit |

`make`/QEMU: Multiboot2 framebuffer tag (o modo VESA/GOP más adelante). Si no hay FB gráfico → caer a `text` sin panic.

### 3.2 Piezas de código objetivo (nombres)

```text
src/ui/
  boot_ui.c          # splash + stages
  session.c          # login/home orquestación
  compositor.c       # superficies, z-order mínimo
  draw.c             # fill, blit, rect, text
  font.c             # bitmap font propia primero
  theme.c            # tokens
  input_ui.c         # foco, atajos
  widgets/
    label.c button.c list.c field.c
apps/                # más adelante: settings, files, about
docs/UI_PLAN.md      # este archivo
```

La shell de texto (`shell.c`) permanece como **consola de emergencia** (`Ctrl+Alt+F1` conceptual o comando `shell`).

---

## 4. Viaje completo del usuario (storyboard)

### Fase A — Firmware / bootloader

1. POST / BIOS o UEFI (fuera de nuestro control visual al inicio).
2. Bootloader (GRUB hoy; nativo después): pantalla negra o menú mínimo.
3. Entrega control al kernel con framebuffer si está disponible.

**UI nuestra aún no:** opcional mensaje estático “CordOS” en stage2 más adelante.

### Fase B — Kernel early (invisible o casi)

- GDT/IDT/memoria/initrd: **sin volcar logs a pantalla**.
- Serial recibe el detalle.
- Máximo: un pixel de acento o barra de progreso indeterminada si FB listo muy pronto.

### Fase C — Splash de marca (primera UI propia)

Pantalla completa:

- Nombre del OS (hero).
- Una línea: “Iniciando…” / etapas humanas.
- Barra de progreso **por etapas** (no por %).

Etapas humanas (map a init real):

| # | Etiqueta UI | Init real detrás |
|---|---|---|
| 1 | Preparando memoria | PMM/VMM/heap |
| 2 | Dispositivos | PIC/teclado/serial/PCI |
| 3 | Archivos | initrd/VFS |
| 4 | Sesión | scheduler + session UI |

Si algo falla → pantalla de error simple + “Enter: shell de emergencia”.

### Fase D — Login (obligatoria)

Pantalla de **inicio de sesión** clara, a pantalla completa, antes de cualquier Home.
No es un tip ni un “presiona Enter”: es el umbral de sesión, como en Windows / macOS / las distros gráficas.

#### Contenido mínimo (toda ola que tenga UI de sesión)

```text
┌──────────────────────────────────────────────────────────┐
│  [fondo atmosférico — mismo lenguaje que Home]           │
│                                                          │
│              CORDOS                                     │
│                                                          │
│         ┌────────────────────────────┐                   │
│         │  Usuario                   │                   │
│         │  [ guest            ]      │                   │
│         │                            │                   │
│         │  Contraseña                │                   │
│         │  [ ••••••           ]      │                   │
│         │                            │                   │
│         │     [  Entrar  ]           │                   │
│         └────────────────────────────┘                   │
│                                                          │
│         Tab campos · Enter entrar · F1 emergencia        │
└──────────────────────────────────────────────────────────┘
```

#### Reglas de producto

| Regla | Detalle |
|---|---|
| Obligatoria | Tras splash siempre `auth.login`. Nunca splash → Home directo. |
| Marca hero | El nombre del OS domina por encima del formulario. |
| Un formulario | Usuario + contraseña + CTA “Entrar” (Ola 0 puede aceptar guest + clave vacía o clave fija de desarrollo). |
| Feedback | Error visible bajo el formulario (“No se pudo entrar”) sin logs de kernel. |
| Teclado | `Tab`/`↑↓` entre campos y botón; `Enter` en CTA o en último campo envía. |
| Escape de emergencia | `F1` o `Ctrl+Alt+F1` → consola texto (no salta el login en el camino feliz). |
| Sin red | Login 100% local; nada de “iniciar con cuenta cloud”. |
| Post-login | Solo entonces `home.root`. Cerrar sesión vuelve a `auth.login`. |

#### Evolución por olas

| Ola | Comportamiento del login |
|---|---|
| **Ola 0** | Pantalla login completa. Usuario `guest`, contraseña `temporal` (documentada). |
| **Ola 2+** | Campos reales con widget `field`, eco `•` en password. |
| **Ola 5** | Usuarios en VFS/disco, hash de clave, bloqueo tras N intentos, “Cambiar usuario”. |

### Fase E — Home

Solo accesible **después** de un login OK.
Composición única (no dashboard):

1. **Brand** hero (nombre).
2. **Una frase** corta (status: “Listo” / hora si tenemos reloj).
3. **Launcher** vertical u horizontal con pocos destinos.
4. **Una CTA** primaria (abrir Archivos o Terminal).
5. Ancla visual de fondo (gradiente/patrón/escena), no collage de cards.

### Fase F — Uso cotidiano

- Abrir app → superficie enfocada.
- Volver a Home (tecla `Home` / `Esc` según contexto).
- Ajustes, Acerca de, Apagar/Reiniciar.

### Fase G — Apagado

- Confirmación clara.
- Animación corta → pantalla negra “Puedes apagar” / poweroff ACPI más adelante.

---

## 5. Inventario de pantallas / superficies

### 5.1 Sistema (obligatorias a largo plazo)

| ID | Pantalla | Prioridad | Entrada | Salida |
|---|---|---|---|---|
| `boot.splash` | Splash + progreso | P0 | auto | **`auth.login` siempre** |
| `boot.error` | Error de arranque | P0 | Enter→shell | — |
| `auth.login` | **Inicio de sesión (obligatoria)** | **P0** | usuario+clave+Entrar | `home.root` |
| `home.root` | Home (solo tras login) | P0 | launcher | apps / power / logout |
| `app.files` | Explorador initrd/FS | P1 | teclado | Home |
| `app.term` | Terminal (shell texto embebida o TTY) | P1 | teclado | Home |
| `app.settings` | Ajustes (idioma, tema, teclado) | P2 | teclado | Home |
| `app.about` | Acerca de CordOS | P1 | teclado | Home |
| `sys.power` | Apagar / reiniciar | P1 | confirmar | off |
| `sys.logout` | Cerrar sesión → vuelve a login | P1 | confirmar | `auth.login` |
| `sys.crash` | Panic humanizado | P1 | Enter shell | — |
| `sys.lock` | Bloqueo sesión (vuelve a unlock≈login) | P2 | unlock | Home |

### 5.2 Opcionales / más tarde

| ID | Notas |
|---|---|
| `app.editor` | Bloc de notas mínimo |
| `app.calc` | Calculadora |
| `app.net` | Estado de red (cuando exista stack) |
| `wm.overview` | Vista de ventanas |
| `notify.toast` | Toasts no bloqueantes |

---

## 6. Home — especificación detallada

### 6.1 Layout (desktop)

```text
┌──────────────────────────────────────────────────────────┐
│  [fondo atmosférico]                                     │
│                                                          │
│         CORDOS (display)                                │
│         Listo — teclado activo                           │
│                                                          │
│         › Archivos                                       │
│           Terminal                                       │
│           Ajustes                                        │
│           Acerca de                                      │
│           Cerrar sesión                                  │
│           Apagar                                         │
│                                                          │
│                                      ↑↓ Enter  Esc=atrás │
└──────────────────────────────────────────────────────────┘
```

- Sin reloj/stats en la primera versión del Home (van a Ajustes o barra secundaria en ola posterior).
- Foco visible en un solo ítem del launcher.
- Brand no puede ser más pequeño que el ítem de menú.
- **Cerrar sesión** siempre vuelve a `auth.login` (no al splash).

### 6.2 Layout (móvil / ventana chica) — mismo contenido

Una columna; tipografía y spacing reducidos; mismas acciones.

### 6.3 Atajos globales (sesión)

| Tecla | Acción |
|---|---|
| `↑` `↓` | Mover foco |
| `Enter` | Activar |
| `Esc` | Atrás / cerrar superficie |
| `H` o `Home` | Volver a Home (cuando no haya modal) |
| `F1` o `Ctrl+Alt+T` | Terminal |
| `Ctrl+Alt+F1` | Consola emergencia (texto) |

---

## 7. Toolkit mínimo (widgets)

Orden de implementación:

1. `label` — texto
2. `button` — acción con foco
3. `list` / `menu` — launcher y files
4. `field` — login / rename después
5. `progress` — splash
6. `panel` — contenedor de superficie
7. `icon` — bitmap 16/32 (después)

Estados: normal, focused, pressed, disabled.

---

## 8. Input y accesibilidad temprana

- Teclado PS/2 (ya existe) → eventos UI (`KeyDown`, `KeyRepeat`).
- Repeat de teclas en UI (después de 400 ms, cada 40 ms).
- Contraste alto obligatorio en tema default.
- No depender solo del color para el foco (borde + prefijo `›`).
- Mouse: ola posterior (cursor + hit-test).

---

## 9. Datos y personalización

| Dato | Dónde | Notas |
|---|---|---|
| `name_os`, versión | `config.c` | Dejar de ser “temporal” cuando cerremos marca |
| Tema | `theme.c` + archivo en VFS más adelante | Un tema primero |
| Usuario activo | sesión en RAM (+ VFS después) | Rellenado en login; logout lo limpia |
| Credenciales | Ola 0: hardcode/`guest`; luego archivo usuarios | Nunca en pantalla en claro tras Enter |
| Wallpaper | recurso embebido / archivo | Gradiente procedural al inicio |

---

## 10. Plan por olas (de lo pequeño a lo grande)

### Ola 0 — “Splash + Login + Home mínimo” (primera a construir)

**Meta:** camino de producto real: splash → **pantalla de login** → Home.

**Estado:** implementada en texto VGA (`src/ui/session.c`). Credenciales dev: `guest` / `temporal`. Shell vía Terminal o F1 (`exit` vuelve).

Entregables:

- Boot sin spam en VGA (detalle en serial).
- `boot.splash` (FB o texto limpio a pantalla completa).
- **`auth.login` obligatorio**: marca + usuario + contraseña + “Entrar”.
  - Dev: usuario `guest`, contraseña `temporal` (o vacía documentada).
  - Fallo de clave: mensaje en UI, se queda en login.
- Tras Enter OK → `home.root` mínimo: Archivos, Terminal, Cerrar sesión, Apagar.
- ↑↓ / Tab / Enter / Backspace en login y Home.

**Fuera de ola 0:** ventanas flotantes, mouse, hash fuerte, tipografías TTF, multi-usuario en disco.

### Ola 1 — Framebuffer real + font + Home gráfico

**Estado:** MVP gráfico — `src/ui/gfx_session.c` + `draw`/`font`; GRUB `gfxpayload=1920x1080x32`; fallback a texto si no hay FB.

- Draw rect/fill/text con font bitmap propia.
- Home gráfico según layout §6.
- `app.about`, `app.files` gráfico simple.
- Terminal como superficie (consola FB encima del shell).

### Ola 2 — Toolkit + navegación de sesión

- Widgets reutilizables.
- Stack de pantallas (push/pop).
- `sys.power`, `boot.error` humanizados.
- Tema tokens centralizados.

### Ola 3 — Compositor mínimo

- 2–3 superficies; foco; z-order.
- Abrir Terminal y Files a la vez (opcional tiles, no overlapping free-move aún).

### Ola 4 — Mouse + ajustes

- Cursor, clic, hover = foco.
- `app.settings` (tema claro/oscuro, keymap).

### Ola 5 — Auth endurecida + pulido producto

- Usuarios/claves en almacenamiento; hash; intentos limitados.
- Lock screen.
- Motion §2.3.
- Recursos de marca definitivos (cuando salgamos de `temporal`).

### Ola 6+ — Apps y escritorio rico

- Editor, notificaciones, red UI, multi-idioma, etc.

---

## 11. Dependencias de kernel (para no mentirnos)

| Necesidad UI | Estado kernel hoy | Bloqueo |
|---|---|---|
| Teclado | OK | — |
| Framebuffer | Stub + tag Multiboot2 | Ola 1 necesita FB estable en QEMU |
| Timer | OK (ticks) | animaciones |
| VFS/initrd | OK | Files app |
| Heap | OK | toolkit |
| Procesos/user | Parcial | apps aisladas (Ola 5+) |
| Mouse | No | Ola 4 |
| Audio | No | mucho después |
| Persistencia disco | No | settings permanentes |

---

## 12. Decisiones a cerrar antes de Ola 0

Respuestas propuestas (cambiar aquí si no):

| # | Pregunta | Propuesta |
|---|---|---|
| D1 | ¿Login obligatorio? | **Sí — cerrado.** Siempre `auth.login` antes de Home. |
| D2 | ¿Home en texto limpio si no hay FB? | **Sí** — mismo flujo splash→login→Home |
| D3 | ¿Terminal = shell actual? | **Sí** hasta TTY real |
| D4 | ¿Apagar real o mensaje? | Mensaje + `hlt` hasta ACPI |
| D5 | ¿Marca visual definitiva? | Mantener tokens §2; renombrar `temporal` en sesión aparte |
| D6 | ¿Idioma UI? | **Hecho (MVP):** selector al boot + `lang=es\|en` (cmdline/GRUB) + `lang` en shell; tablas en `src/i18n.c` |
| D7 | ¿Credencial Ola 0? | Usuario `guest` / clave `temporal` (documentada en USER.md) |

---

## 13. Criterios de “UI lista” por capa

| Capa | Lista cuando… |
|---|---|
| Boot amable | Ningún log de debug en pantalla en camino feliz |
| Splash | Se entiende la marca y el progreso en &lt; 3 s de lectura |
| Login | Se reconoce como pantalla de sesión; no se puede saltar en el camino feliz |
| Home | Solo tras login; 5–6 acciones alcanzables solo con teclado en &lt; 10 s |
| Toolkit | Se puede montar una pantalla nueva sin tocar el compositor |
| Sesión | Abrir / cerrar app sin corromper Home |
| Producto | Un desconocido llega a Files y vuelve sin instrucciones orales |

---

## 14. Cómo usamos este plan

1. No implementar features UI fuera de la ola acordada.
2. Cada PR/cambio UI cita: `UI Ola N` + pantalla `id` (§5).
3. Si surge una pantalla nueva, se añade a §5 **antes** de codearla.
4. La **Ola 0** es el único siguiente paso de implementación cuando digas “adelante”.

---

## 15. Resumen ejecutivo

- **Destino:** splash → **login obligatorio** → Home → Files / Terminal / About / Logout / Power.
- **Estilo:** taller nocturno, acento menta, sin look “AI purple / cream terracotta”.
- **Stack:** draw → widgets → session → (luego) compositor.
- **Empezar pequeño:** Ola 0 = splash + **login screen** + Home mínimo.

Para implementar Ola 0, di **adelante**.
