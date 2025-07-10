#!/bin/bash

LOGS_DIR="./"
OUTPUT="./log_global_ordenado.log"
SEPARADOR="========================================================================================================================="

# Función para limpiar ANSI codes
limpiar_ansi_en_archivo() {
    local archivo="$1"
    sed -i 's/\x1B\[[0-9;]*[mK]//g' "$archivo"
}

# Paso 1: limpiar logs individuales
for modulo in kernel cpu memoria io; do
    limpiar_ansi_en_archivo "${LOGS_DIR}/${modulo}/${modulo}.log"
done

# Paso 2: obtener timestamps únicos
timestamps=$(cat ${LOGS_DIR}/*/*.log \
    | grep -oP '\d{2}:\d{2}:\d{2}:\d{3}' \
    | sort -u)

# Paso 3: recorrer timestamps y logs por módulo
> "$OUTPUT"

for ts in $timestamps; do
    declare -A lineas_modulo
    modulos_con_lineas=()

    for modulo in kernel io cpu memoria; do
        archivo="${LOGS_DIR}/${modulo}/${modulo}.log"
        lineas=$(grep "$ts" "$archivo")
        if [ ! -z "$lineas" ]; then
            lineas_modulo[$modulo]="$lineas"
            modulos_con_lineas+=("$modulo")
        fi
    done

    count=${#modulos_con_lineas[@]}
    for i in "${!modulos_con_lineas[@]}"; do
        modulo="${modulos_con_lineas[$i]}"
        echo "${lineas_modulo[$modulo]}" >> "$OUTPUT"
        if (( count > 1 && i < count - 1 )); then
            echo "$SEPARADOR" >> "$OUTPUT"
        fi
    done

    echo "" >> "$OUTPUT"
    unset lineas_modulo
    unset modulos_con_lineas
done

echo "✔ Logs limpiados, ordenados y agrupados con separadores condicionales en: $OUTPUT"

# Paso 4: Validación de integridad
lineas_originales=$(cat ${LOGS_DIR}/*/*.log | grep -v '^$' | wc -l)
lineas_finales=$(grep -vE "^$|^$SEPARADOR" "$OUTPUT" | wc -l)

if [[ "$lineas_originales" -ne "$lineas_finales" ]]; then
    echo "❌ ERROR: Hay inconsistencia en la cantidad de líneas."
    echo "  → Líneas originales: $lineas_originales"
    echo "  → Líneas en log global (sin separadores/saltos): $lineas_finales"
else
    echo "✅ Validación OK: el log global contiene todas las líneas originales."
fi

# Abrir el archivo en VS Code al finalizar
code -r "$OUTPUT"
