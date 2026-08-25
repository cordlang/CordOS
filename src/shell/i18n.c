#include "i18n.h"

enum lang_id lang_os = LANG_ES;

static const char *const strings_es[MSG_COUNT] = {
    [MSG_LANG_TITLE] = "CordOS",
    [MSG_LANG_SUBTITLE] = "Elige idioma / Choose language",
    [MSG_LANG_HINT] = "1/2, Tab, w/s o a/e + Enter",
    [MSG_READY] = "listo.",
    [MSG_SHELL_BANNER] = "Shell de emergencia.",
    [MSG_SHELL_PROMPT_HELP] = "help | ping | net | exit",
    [MSG_HELP_HEADER] = "Comandos:",
    [MSG_HELP_CMDS] = "  help  echo  clear  ls  cat  ping [ip]  net",
    [MSG_HELP_LANG] = "  ticks  mem  lang [es|en]  exit",
    [MSG_HELP_UTF8] = "UTF-8: AltGr + vocal/n/y/c. Mayusculas con Shift.",
    [MSG_HELP_ENTER] = "Enter confirma. Backspace borra. exit vuelve al Home.",
    [MSG_CAT_USAGE] = "uso: cat <archivo>",
    [MSG_CAT_NOT_FOUND] = "cat: no encontrado",
    [MSG_LS_ERROR] = "ls: error",
    [MSG_UNKNOWN_CMD] = " — escribe help",
    [MSG_STATUS_SUFFIX] = "  |  exit=Home",
    [MSG_LOGIN_USER] = "Usuario",
    [MSG_LOGIN_PASSWORD] = "Contraseña",
    [MSG_LOGIN_ENTER] = "Entrar",
    [MSG_LOGIN_BAD] = "Contraseña incorrecta",
    [MSG_LOGIN_HINT] = "Tab campos  Enter  F1 shell",
    [MSG_SPLASH_TITLE] = "Iniciando",
    [MSG_SPLASH_STAGE0] = "Preparando memoria",
    [MSG_SPLASH_STAGE1] = "Dispositivos",
    [MSG_SPLASH_STAGE2] = "Archivos",
    [MSG_SPLASH_STAGE3] = "Sesión",
    [MSG_HOME_READY] = "Escritorio listo",
    [MSG_HOME_FILES] = "Archivos",
    [MSG_HOME_TERMINAL] = "Terminal",
    [MSG_HOME_SETTINGS] = "Ajustes",
    [MSG_HOME_ABOUT] = "Acerca de",
    [MSG_HOME_LOGOUT] = "Cerrar sesión",
    [MSG_HOME_POWER] = "Apagar",
    [MSG_HOME_HINT] = "Clic iconos o Menu  arrastra ventanas  F1 shell",
    [MSG_FILES_TITLE] = "Archivos (/)",
    [MSG_FILES_HINT] = "Clic un archivo  Esc cierra",
    [MSG_ABOUT_BODY] = "Kernel freestanding. Escritorio grafico con raton.",
    [MSG_SETTINGS_BODY] = "Idioma, fondo de login y estilo de iconos.",
    [MSG_SETTINGS_WP] = "Fondo de inicio de sesion",
    [MSG_WP_DEFAULT] = "Predeterminado",
    [MSG_WP_ABSTRACT] = "Abstracto",
    [MSG_SETTINGS_ICONS] = "Estilo de iconos",
    [MSG_IC_LINEAR] = "Linear",
    [MSG_IC_BOLD] = "Bold",
    [MSG_IC_BROKEN] = "Broken",
    [MSG_IC_BULK] = "Bulk",
    [MSG_POWER_MSG] = "Apagando...",
    [MSG_SHELL_EXIT_HINT] = "exit vuelve al escritorio.",
    [MSG_LAUNCHER] = "Menú",
    [MSG_POWER_CONFIRM] = "Apagar el sistema ahora?",
    [MSG_POWER_CANCEL] = "Cancelar",
    [MSG_LANG_CLICK] = "Clic para cambiar idioma",
    [MSG_TERM_BANNER] = "help  ping  net  ls  cat  clear  exit",
    [MSG_DESKTOP_HINT] = "Clic derecho en el fondo para personalizar",
    [MSG_CTX_TITLE] = "Escritorio",
    [MSG_CTX_SHOW_ICONS] = "Mostrar iconos",
    [MSG_CTX_HIDE_ICONS] = "Ocultar iconos",
    [MSG_FILE_PREVIEW] = "Selecciona un archivo",
    [MSG_OB_LANG_TITLE] = "Elige tu idioma",
    [MSG_OB_LANG_BODY] = "Asi se vera CordOS. Puedes cambiarlo luego en Ajustes.",
    [MSG_OB_WELCOME_TITLE] = "Bienvenido a CordOS",
    [MSG_OB_WELCOME_BODY] = "Vamos a crear tu cuenta. Solo tardara un momento.",
    [MSG_OB_NAME_TITLE] = "Como te llamas?",
    [MSG_OB_NAME_BODY] = "Este nombre aparecera en el inicio de sesion.",
    [MSG_OB_NAME_PLACE] = "Tu nombre",
    [MSG_OB_PASS_TITLE] = "Elige una contrasena",
    [MSG_OB_PASS_BODY] = "La necesitaras cada vez que entres.",
    [MSG_OB_PASS_PLACE] = "Contrasena",
    [MSG_OB_WIFI_TITLE] = "Conectate al Wi-Fi",
    [MSG_OB_WIFI_BODY] = "Wi-Fi es la conexion principal. Elige una red o omite.",
    [MSG_OB_WIFI_CHECK] = "Comprobando conexion...",
    [MSG_OB_WIFI_OK] = "Conexion lista.",
    [MSG_OB_WIFI_FAIL] = "Sin Internet. Puedes continuar igual.",
    [MSG_OB_WIFI_NONE] = "No hay redes Wi-Fi a rango. Puedes usar Ethernet u omitir.",
    [MSG_OB_NET_WIRED] = "Ethernet",
    [MSG_OB_NET_WIFI0] = "Casa",
    [MSG_OB_NET_WIFI1] = "Invitados",
    [MSG_OB_NET_WIFI2] = "CordOS-Guest",
    [MSG_OB_WIFI_SCAN] = "Buscando redes Wi-Fi...",
    [MSG_OB_WIFI_PASS] = "Contrasena de la red",
    [MSG_OB_WIFI_OPEN] = "Red abierta",
    [MSG_OB_CONNECT] = "Conectar",
    [MSG_OB_SKIP] = "Omitir",
    [MSG_OB_NEXT] = "Continuar",
    [MSG_OB_BACK] = "Atras",
    [MSG_OB_ANOTHER_TITLE] = "Cuenta lista",
    [MSG_OB_ANOTHER_BODY] = "Quieres anadir otro usuario?",
    [MSG_OB_ADD] = "Anadir otro",
    [MSG_OB_CREATING] = "Creando tu espacio..."
};

static const char *const strings_en[MSG_COUNT] = {
    [MSG_LANG_TITLE] = "CordOS",
    [MSG_LANG_SUBTITLE] = "Choose language / Elige idioma",
    [MSG_LANG_HINT] = "1/2, Tab, w/s or a/e + Enter",
    [MSG_READY] = "ready.",
    [MSG_SHELL_BANNER] = "Emergency shell.",
    [MSG_SHELL_PROMPT_HELP] = "help | ping | net | exit",
    [MSG_HELP_HEADER] = "Commands:",
    [MSG_HELP_CMDS] = "  help  echo  clear  ls  cat  ping [ip]  net",
    [MSG_HELP_LANG] = "  ticks  mem  lang [es|en]  exit",
    [MSG_HELP_UTF8] = "UTF-8: AltGr + vowel/n/y/c. Capitals with Shift.",
    [MSG_HELP_ENTER] = "Enter confirms. Backspace deletes. exit returns Home.",
    [MSG_CAT_USAGE] = "usage: cat <file>",
    [MSG_CAT_NOT_FOUND] = "cat: not found",
    [MSG_LS_ERROR] = "ls: error",
    [MSG_UNKNOWN_CMD] = " — type help",
    [MSG_STATUS_SUFFIX] = "  |  exit=Home",
    [MSG_LOGIN_USER] = "Username",
    [MSG_LOGIN_PASSWORD] = "Enter Password",
    [MSG_LOGIN_ENTER] = "Sign in",
    [MSG_LOGIN_BAD] = "Incorrect password",
    [MSG_LOGIN_HINT] = "Tab fields  Enter  F1 shell",
    [MSG_SPLASH_TITLE] = "Starting",
    [MSG_SPLASH_STAGE0] = "Preparing memory",
    [MSG_SPLASH_STAGE1] = "Devices",
    [MSG_SPLASH_STAGE2] = "Files",
    [MSG_SPLASH_STAGE3] = "Session",
    [MSG_HOME_READY] = "Desktop ready",
    [MSG_HOME_FILES] = "Files",
    [MSG_HOME_TERMINAL] = "Terminal",
    [MSG_HOME_SETTINGS] = "Settings",
    [MSG_HOME_ABOUT] = "About",
    [MSG_HOME_LOGOUT] = "Sign out",
    [MSG_HOME_POWER] = "Shut down",
    [MSG_HOME_HINT] = "Click icons or Menu  drag windows  F1 shell",
    [MSG_FILES_TITLE] = "Files (/)",
    [MSG_FILES_HINT] = "Click a file  Esc closes",
    [MSG_ABOUT_BODY] = "Freestanding kernel. Graphical desktop with mouse.",
    [MSG_SETTINGS_BODY] = "Language, sign-in background and icon style.",
    [MSG_SETTINGS_WP] = "Sign-in background",
    [MSG_WP_DEFAULT] = "Default",
    [MSG_WP_ABSTRACT] = "Abstract",
    [MSG_SETTINGS_ICONS] = "Icon style",
    [MSG_IC_LINEAR] = "Linear",
    [MSG_IC_BOLD] = "Bold",
    [MSG_IC_BROKEN] = "Broken",
    [MSG_IC_BULK] = "Bulk",
    [MSG_POWER_MSG] = "Shutting down...",
    [MSG_SHELL_EXIT_HINT] = "exit returns to the desktop.",
    [MSG_LAUNCHER] = "Menu",
    [MSG_POWER_CONFIRM] = "Shut down the system now?",
    [MSG_POWER_CANCEL] = "Cancel",
    [MSG_LANG_CLICK] = "Click to change language",
    [MSG_TERM_BANNER] = "help  ping  net  ls  cat  clear  exit",
    [MSG_DESKTOP_HINT] = "Right-click the background to personalize",
    [MSG_CTX_TITLE] = "Desktop",
    [MSG_CTX_SHOW_ICONS] = "Show icons",
    [MSG_CTX_HIDE_ICONS] = "Hide icons",
    [MSG_FILE_PREVIEW] = "Select a file",
    [MSG_OB_LANG_TITLE] = "Choose your language",
    [MSG_OB_LANG_BODY] = "This is how CordOS will look. You can change it later in Settings.",
    [MSG_OB_WELCOME_TITLE] = "Welcome to CordOS",
    [MSG_OB_WELCOME_BODY] = "Let's set up your account. It only takes a moment.",
    [MSG_OB_NAME_TITLE] = "What is your name?",
    [MSG_OB_NAME_BODY] = "This name will show on the sign-in screen.",
    [MSG_OB_NAME_PLACE] = "Your name",
    [MSG_OB_PASS_TITLE] = "Choose a password",
    [MSG_OB_PASS_BODY] = "You will need it every time you sign in.",
    [MSG_OB_PASS_PLACE] = "Password",
    [MSG_OB_WIFI_TITLE] = "Connect to Wi-Fi",
    [MSG_OB_WIFI_BODY] = "Wi-Fi is the main connection. Pick a network or skip.",
    [MSG_OB_WIFI_CHECK] = "Checking connection...",
    [MSG_OB_WIFI_OK] = "You are online.",
    [MSG_OB_WIFI_FAIL] = "No Internet. You can continue anyway.",
    [MSG_OB_WIFI_NONE] = "No Wi-Fi networks in range. You can use Ethernet or skip.",
    [MSG_OB_NET_WIRED] = "Ethernet",
    [MSG_OB_NET_WIFI0] = "Home",
    [MSG_OB_NET_WIFI1] = "Guest",
    [MSG_OB_NET_WIFI2] = "CordOS-Guest",
    [MSG_OB_WIFI_SCAN] = "Looking for Wi-Fi networks...",
    [MSG_OB_WIFI_PASS] = "Network password",
    [MSG_OB_WIFI_OPEN] = "Open network",
    [MSG_OB_CONNECT] = "Connect",
    [MSG_OB_SKIP] = "Skip",
    [MSG_OB_NEXT] = "Continue",
    [MSG_OB_BACK] = "Back",
    [MSG_OB_ANOTHER_TITLE] = "Account ready",
    [MSG_OB_ANOTHER_BODY] = "Do you want to add another user?",
    [MSG_OB_ADD] = "Add another",
    [MSG_OB_CREATING] = "Creating your space..."
};

void i18n_init(void)
{
    lang_os = LANG_ES;
}

void i18n_set_lang(enum lang_id lang)
{
    if (lang != LANG_ES && lang != LANG_EN) {
        lang = LANG_ES;
    }
    lang_os = lang;
}

enum lang_id i18n_lang(void)
{
    return lang_os;
}

const char *i18n_lang_name(enum lang_id lang)
{
    if (lang == LANG_EN) {
        return "English";
    }
    return "Espanol";
}

const char *i18n(enum msg_id id)
{
    if ((u32)id >= (u32)MSG_COUNT) {
        return "?";
    }
    if (lang_os == LANG_EN) {
        return strings_en[id];
    }
    return strings_es[id];
}

bool i18n_set_lang_code(const char *code)
{
    if (code == NULL || code[0] == '\0') {
        return false;
    }

    if ((code[0] == 'e' || code[0] == 'E') &&
        (code[1] == 'n' || code[1] == 'N')) {
        i18n_set_lang(LANG_EN);
        return true;
    }

    if ((code[0] == 'e' || code[0] == 'E') &&
        (code[1] == 's' || code[1] == 'S')) {
        i18n_set_lang(LANG_ES);
        return true;
    }

    return false;
}
