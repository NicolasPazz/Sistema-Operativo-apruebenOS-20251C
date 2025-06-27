#pragma once

// Colors en formato RGB con ANSI
#define ANSI_COLOR_RESET   "\033[0m"
#define ANSI_COLOR_ROJO    "\033[38;2;192;31;38m"
#define ANSI_COLOR_VERDE   "\033[38;2;145;193;67m"
#define ANSI_COLOR_AZUL    "\033[38;2;108;182;182m"
#define ANSI_COLOR1    "\033[38;2;239;228;77m"

// Macros para formatear texto
#define ROJO(texto)    ANSI_COLOR_ROJO texto ANSI_COLOR_RESET
#define VERDE(texto)   ANSI_COLOR_VERDE texto ANSI_COLOR_RESET
#define AZUL(texto)    ANSI_COLOR_AZUL texto ANSI_COLOR_RESET
#define COLOR1(texto)  ANSI_COLOR1 texto ANSI_COLOR_RESET

// Ejemplo de Uso
/**
 * @note en un printf:
 * printf(AZUL("hola")ROJO("como")VERDE("estas"))
 * 
 * @note en un log:
 * log_info(logger, AZUL("hola")ROJO("como")VERDE("estas"));
 * 
 */