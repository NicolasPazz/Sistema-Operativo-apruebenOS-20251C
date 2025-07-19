#!/usr/bin/env bash
# chmod +x unir_logs.sh && ./unir_logs.sh
set -euo pipefail

OUTPUT="log_global_ordenado.log"
SEP="$(printf '=%.0s' {1..120})"
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

###############################################################################
# 1) limpiar ANSI de cada .log y contar líneas
###############################################################################
total_src=0
cleans=()

for dir in kernel memoria cpu io; do
    [[ -d $dir ]] || { echo "[x] Falta la carpeta '$dir'"; exit 1; }
done

# Usar la expresión regular que funciona del código original
sed_cleanup='s/\x1B\[[0-9;]*[mK]//g'

# Usar mapfile para procesar los archivos de una vez
mapfile -t log_files < <(find kernel memoria cpu io -name "*.log" -type f)

# Primero limpiar los archivos originales directamente
for f in "${log_files[@]}"; do
    sed -i "$sed_cleanup" "$f"
done

# Luego procesar para el log global
for f in "${log_files[@]}"; do
    clean="$TMPDIR/$(basename "$f")"
    cp "$f" "$clean"
    lines=$(wc -l < "$f")
    total_src=$((total_src + lines))
    cleans+=("$clean")
    echo "[√] Procesado: $f ($lines líneas)"
done

[[ ${#cleans[@]} -gt 0 ]] || { echo "[x] No hay .log"; exit 1; }

###############################################################################
# 2) concatenar, ordenar por ms, agrupar y separar por proceso
###############################################################################
cat "${cleans[@]}" |
  sort -t' ' -k2,2V -k3,3V |              # orden HH:MM:SS:ms  +  process_name
  awk -v SEP="$SEP" '
      function proc(line) {               # MEMORIA, KERNEL, CPU_1, …
          split(line, a, " ")
          split(a[3], b, "/")
          return b[1]
      }
      {
          ts = $2
          pn = proc($0)

          # 1-era línea global → sólo imprimir
          if (NR == 1) { prev_ts = ts; prev_pn = pn; print; next }

          # cambio de milisegundo → salto de línea y “reset” de proceso
          if (ts != prev_ts) { print ""; prev_pn = "" }

          # cambio de proceso **y** ya había un proceso impreso en este ms
          if (pn != prev_pn && prev_pn != "") print SEP

          print
          prev_ts = ts; prev_pn = pn
      }
  ' > "$OUTPUT"

###############################################################################
# 3) validar conteo de líneas (sin saltos ni separadores)
###############################################################################
lines_global=$(grep -vE "^$|^${SEP}$" "$OUTPUT" | wc -l)

if [[ "$lines_global" -eq "$total_src" ]]; then
    echo "✔ Log global creado: $OUTPUT  —  $lines_global líneas verificadas."
else
    echo "⚠ Inconsistencia: originales=$total_src vs global=$lines_global"
fi
