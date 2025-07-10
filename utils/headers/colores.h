#pragma once

// Colors en formato RGB con ANSI
#define ANSI_COLOR_RESET   "\033[0m"
#define ANSI_COLOR_ROJO    "\033[38;2;200;100;120m"
#define ANSI_COLOR_VERDE   "\033[38;2;179;236;111m"
#define ANSI_COLOR_AZUL    "\033[38;2;108;182;182m"
#define ANSI_COLOR1        "\033[38;2;200;150;120m"
#define ANSI_COLOR_AMARILLO  "\033[38;2;255;221;89m"
#define ANSI_COLOR_MAGENTA   "\033[38;2;255; 92;173m"
#define ANSI_COLOR_CYAN      "\033[38;2;70;235;240m"
#define ANSI_COLOR_NARANJA   "\033[38;2;255;165;0m"
#define ANSI_COLOR_PURPURA   "\033[38;2;160;113;255m"
#define ANSI_COLOR_GRIS      "\033[38;2;180;180;180m"

// Macros para formatear texto
#define ROJO(texto)    ANSI_COLOR_ROJO texto ANSI_COLOR_RESET
#define VERDE(texto)   ANSI_COLOR_VERDE texto ANSI_COLOR_RESET
#define AZUL(texto)    ANSI_COLOR_AZUL texto ANSI_COLOR_RESET
#define COLOR1(texto)  ANSI_COLOR1 texto ANSI_COLOR_RESET
#define AMARILLO(texto) ANSI_COLOR_AMARILLO texto ANSI_COLOR_RESET
#define MAGENTA(texto)  ANSI_COLOR_MAGENTA  texto ANSI_COLOR_RESET
#define CYAN(texto)     ANSI_COLOR_CYAN     texto ANSI_COLOR_RESET
#define NARANJA(texto)  ANSI_COLOR_NARANJA  texto ANSI_COLOR_RESET
#define PURPURA(texto)  ANSI_COLOR_PURPURA  texto ANSI_COLOR_RESET
#define GRIS(texto)     ANSI_COLOR_GRIS     texto ANSI_COLOR_RESET

// Ejemplo de Uso
/**
 * @note en un printf:
 * printf(AZUL("hola")ROJO("como")VERDE("estas"))
 * 
 * @note en un log:
 * log_info(logger, AZUL("hola")ROJO("como")VERDE("estas"));
 * 
 */