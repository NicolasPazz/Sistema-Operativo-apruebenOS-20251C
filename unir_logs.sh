#!/bin/bash
# chmod +x unir_logs.sh; ./unir_logs.sh

LOGS_DIR="./"
OUTPUT="./log_global_ordenado.log"
SEPARADOR="========================================================================================================================="
log_files=($(find kernel cpu memoria io -type f -name "*.log" | sort))

# Validar que se encontraron archivos .log
if [ ${#log_files[@]} -eq 0 ]; then
    echo "❌ ERROR: No se encontraron archivos .log en las carpetas especificadas."
    exit 1
fi

# Función para limpiar ANSI codes
limpiar_ansi_en_archivo() {
    local archivo="$1"
    sed -i 's/\x1B\[[0-9;]*[mK]//g' "$archivo"
}

# Paso 1: limpiar logs individuales
for archivo in "${log_files[@]}"; do
    limpiar_ansi_en_archivo "$archivo"
done

# Paso 2: obtener timestamps únicos
timestamps=$(cat "${log_files[@]}" \
    | grep -oP '\d{2}:\d{2}:\d{2}:\d{3}' \
    | sort -u)

# Paso 3: recorrer timestamps y logs por módulo
> "$OUTPUT"

for ts in $timestamps; do
    declare -A lineas_modulo
    modulos_con_lineas=()

    for archivo in "${log_files[@]}"; do
        modulo=$(basename "$(dirname "$archivo")")
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
lineas_originales=$(cat "${log_files[@]}" | grep -v '^$' | wc -l)
lineas_finales=$(grep -vE "^$|^$SEPARADOR" "$OUTPUT" | wc -l)

if [[ "$lineas_originales" -ne "$lineas_finales" ]]; then
    echo "❌ ERROR: Hay inconsistencia en la cantidad de líneas."
    echo "  → Líneas originales: $lineas_originales"
    echo "  → Líneas en log global (sin separadores/saltos): $lineas_finales"
else
    echo "✅ Validación OK: el log global contiene todas las líneas originales."
fi