#!/usr/bin/env bash
# chmod +x ./detectar_deadlock_kernel_refactorizado.sh; ./detectar_deadlock_kernel_refactorizado.sh
#
# Lógica REFACTORIZADA 100% según especificación:
# 1. Lee kernel/kernel.log línea por línea.
# 2. Cuando encuentra  «…: esperando en …»:
#       • Avanza hasta hallar:
#           a) «…: bloqueado en …»                       →  DESCARTA (solo latencia CPU).
#           b) «…: liberado en …»                        →  sigue hasta «…: bloqueado …», calcula dif.  Si > TH ms ⇒ LOG.
#           c) EOF                                       →  DEADLOCK → log en rojo.
# 3. Continúa desde donde quedó.
#
# Vars de entorno:
#   TH   – Umbral en ms (default 1)
#   EXCL – Mutex a excluir (espacios) (default "mutex_planificador_lp")
#   LOG  – Ruta al log (default kernel/kernel.log)
# ---------------------------------------------------------------------------
LOG=${LOG:-kernel/kernel.log}
TH=${TH:-1}
EXCL=${EXCL:-mutex_planificador_lp}

[[ -f $LOG ]] || { echo "[x] No existe $LOG"; exit 1; }

echo
echo -e "\e[1;34;47m =====   Detectando posibles problemas de mutex en kernel   ===== \e[0m"
echo

# Procesamiento secuencial línea por línea según la lógica especificada
exec 3< "$LOG"
declare -a lines=()
line_num=0

# Leer todas las líneas en array para procesamiento secuencial
while IFS= read -r line <&3; do
    lines[line_num++]="$line"
done
exec 3<&-

# Variables de control
pares=0
dead=0

# Función para convertir timestamp a milisegundos
ts2ms() {
    local ts="$1"
    IFS=':' read -r h m s ms <<< "$ts"
    # Eliminar ceros iniciales para evitar interpretación octal
    h=$((10#$h))
    m=$((10#$m))
    s=$((10#$s))
    ms=$((10#$ms))
    echo $(( (h*3600 + m*60 + s)*1000 + ms ))
}

# Función para extraer información de la línea
extract_info() {
    local line="$1"
    local -n info_ref="$2"
    
    # Extraer timestamp
    if [[ $line =~ [0-9]{2}:[0-9]{2}:[0-9]{2}:[0-9]{3} ]]; then
        info_ref[timestamp]="${BASH_REMATCH[0]}"
    else
        return 1
    fi
    
    # Extraer hilo (PID:TID)
    if [[ $line =~ \(([0-9]+:[0-9]+)\) ]]; then
        info_ref[hilo]="${BASH_REMATCH[1]}"
    else
        return 1
    fi
    
    # Extraer mutex - usar grep para expresiones más complejas
    local mutex_match
    mutex_match=$(echo "$line" | grep -o 'LOCK_CON_LOG([^)]*)' | sed 's/LOCK_CON_LOG(\(.*\))/\1/')
    if [[ -n "$mutex_match" ]]; then
        info_ref[mutex]="$mutex_match"
    else
        return 1
    fi
    
    return 0
}

# Función para verificar si un mutex está excluido
is_excluded() {
    local mutex="$1"
    for excluded in $EXCL; do
        [[ "$mutex" == "$excluded" ]] && return 0
    done
    return 1
}

# Procesamiento principal: línea por línea secuencial
i=0
while (( i < line_num )); do
    line="${lines[i]}"
    
    # 2. Cuando encuentra «…: esperando en …»
    if [[ $line =~ LOCK_CON_LOG ]] && [[ $line =~ "esperando en" ]]; then
        
        declare -A info=()
        if ! extract_info "$line" info; then
            ((i++))
            continue
        fi
        
        mutex="${info[mutex]}"
        hilo="${info[hilo]}"
        start_ts=$(ts2ms "${info[timestamp]}")
        
        # Verificar si el mutex está excluido
        if is_excluded "$mutex"; then
            ((i++))
            continue
        fi
        
        # Avanzar hasta hallar: bloqueado, liberado, o EOF
        j=$((i + 1))
        liberado_line=""
        liberado_ts=0
        found_resolution=false
        
        while (( j < line_num )) && [[ $found_resolution == false ]]; do
            next_line="${lines[j]}"
            
            # Solo procesar líneas LOCK_CON_LOG del mismo hilo y mutex
            if [[ $next_line =~ LOCK_CON_LOG ]] && [[ $next_line =~ \($hilo\) ]] && [[ $next_line == *"LOCK_CON_LOG($mutex)"* ]]; then
                
                declare -A next_info=()
                if ! extract_info "$next_line" next_info; then
                    ((j++))
                    continue
                fi
                
                # Caso (a): «…: bloqueado en …» → DESCARTA (solo latencia CPU)
                if [[ $next_line =~ "bloqueado en" ]]; then
                    found_resolution=true
                    # No hacer nada, solo avanzar (DESCARTA)
                    
                # Caso (b): «…: liberado en …» → sigue hasta «…: bloqueado …»
                elif [[ $next_line =~ "liberado en" ]]; then
                    liberado_line="$next_line"
                    liberado_ts=$(ts2ms "${next_info[timestamp]}")
                    
                    # Continuar buscando el próximo "bloqueado"
                    k=$((j + 1))
                    while (( k < line_num )); do
                        blocked_line="${lines[k]}"
                        
                        if [[ $blocked_line =~ LOCK_CON_LOG ]] && [[ $blocked_line =~ \($hilo\) ]] && [[ $blocked_line == *"LOCK_CON_LOG($mutex)"* ]] && [[ $blocked_line =~ "bloqueado en" ]]; then
                            
                            declare -A blocked_info=()
                            if extract_info "$blocked_line" blocked_info; then
                                blocked_ts=$(ts2ms "${blocked_info[timestamp]}")
                                diff=$((blocked_ts - start_ts))
                                
                                if (( diff > TH )); then
                                    echo -e "\t$line"
                                    [[ -n $liberado_line ]] && echo -e "\t$liberado_line"
                                    echo -e "\t$blocked_line"
                                    echo
                                    ((pares++))
                                fi
                                
                                found_resolution=true
                                break
                            fi
                        fi
                        ((k++))
                    done
                    
                    # Si no encontramos "bloqueado" después de "liberado"
                    if [[ $found_resolution == false ]]; then
                        echo -e "\t$line"
                        echo -e "\t$liberado_line"
                        echo -e "\t\033[1;31m(no se encontró \"bloqueado\" posterior al \"liberado\", DEADLOCK)\033[0m"
                        echo
                        ((dead++))
                        found_resolution=true
                    fi
                fi
            fi
            ((j++))
        done
        
        # Caso (c): EOF → DEADLOCK
        if [[ $found_resolution == false ]]; then
            echo -e "\t$line"
            if [[ -n $liberado_line ]]; then
                echo -e "\t$liberado_line"
                echo -e "\t\033[1;31m(EOF después de \"liberado\" sin \"bloqueado\", DEADLOCK)\033[0m"
            else
                echo -e "\t\033[1;31m(EOF sin \"bloqueado\" ni \"liberado\", DEADLOCK)\033[0m"
            fi
            echo
            ((dead++))
        fi
        
        # 3. Continúa desde donde quedó
        i=$((i + 1))  # Siempre avanzar una línea para no saltar otras líneas "esperando"
        unset info next_info blocked_info
    else
        ((i++))
    fi
done

# Mostrar resultados finales
if (( pares == 0 && dead == 0 )); then
    echo -e "\t\033[1;32m✔ No se detectaron contenciones > $TH ms.\033[0m"
else
    echo -e "\t\033[1;31mHallados: $pares pares lentos, $dead deadlocks.\033[0m"
fi
