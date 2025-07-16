#!/bin/bash
# chmod +x unir_logs.sh; ./unir_logs.sh

OUTPUT="./log_global_ordenado.log"

echo "Unificando logs en $OUTPUT..."

SEPARADOR="========================================================================================================================="
mapfile -t kernel_logs < <(find kernel -type f -name "*.log" | sort)
mapfile -t memoria_logs < <(find memoria -type f -name "*.log" | sort)
mapfile -t cpu_logs < <(find cpu -type f -name "*.log" | sort)
mapfile -t io_logs < <(find io -type f -name "*.log" | sort)
log_files=(
    "${kernel_logs[@]}" "${memoria_logs[@]}" "${cpu_logs[@]}" "${io_logs[@]}"
)

# Validar que se encontraron archivos .log
if [ ${#log_files[@]} -eq 0 ]; then
    echo -e "\t[x] ERROR: No se encontraron archivos .log en las carpetas especificadas."
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

# Paso 3: recorrer timestamps y logs por archivo
: > "$OUTPUT"

for ts in $timestamps; do
    first=true

    for archivo in "${log_files[@]}"; do
        lineas=$(grep "$ts" "$archivo")
        if [ -n "$lineas" ]; then
            if [ "$first" = false ]; then
                echo "$SEPARADOR" >> "$OUTPUT"
            fi
            echo "$lineas" >> "$OUTPUT"
            first=false
        fi
    done

    if [ "$first" = false ]; then
        echo "" >> "$OUTPUT"
    fi
done

echo -e "\t[✔] Logs limpiados, ordenados y agrupados con separadores condicionales en: $OUTPUT"

# Paso 4: Validación de integridad
tmp_orig=$(mktemp)
tmp_final=$(mktemp)

cat "${log_files[@]}" | grep -v '^$' | sed 's/\x1B\[[0-9;]*[mK]//g' | sort > "$tmp_orig"
grep -vE "^$|^$SEPARADOR" "$OUTPUT" | sort > "$tmp_final"

if diff -q "$tmp_orig" "$tmp_final" > /dev/null; then
    echo -e "\t[✔] Validación OK: el log global contiene todas las líneas originales."
else
    echo -e "\t[x] ERROR: Hay inconsistencias entre los logs originales y el log global."
fi

rm -f "$tmp_orig" "$tmp_final"