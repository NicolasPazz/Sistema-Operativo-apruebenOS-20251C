#!/bin/bash
# chmod +x ejecutar_test.sh; ./ejecutar_test.sh N

# Estos tests solamente verifican que el codigo de retorno de cada proceso sea exitoso y que no haya log_error o log_warning.
# No verifica logica, deadlocks, memory leaks, condiciones de carrera, ni esperas activas.

if [[ -z "$1" ]]; then
    echo "  Error: Debe indicar el número de test como parámetro (1 a 13)."
    exit 1
fi

numero="$1"
inicio_13="${2:-1}"

if ! [[ "$numero" =~ ^[0-9]+$ ]] || (( numero < 1 || numero > 13 )); then
    echo "  Número inválido. Debe ser del 1 al 13."
    exit 1
fi

# Function to display a message in multiple color styles for better visibility.
imprimir_alerta_multicolor() {
    local mensaje="$1"
    local reset="\e[0m"

    local estilos=(
        "\e[1;34;43m "
        "\e[1;97;44m "
        "\e[1;92;45m "
        "\e[1;97;41m "
        "\e[1;96;100m "
    )

    echo ""
    for estilo in "${estilos[@]}"; do
        echo -e "\t${estilo} ${mensaje} ${reset}"
    done
    echo ""
}

# ---------------------- helpers valgrind ----------------------
extraer_bloque_valgrind() {
    # $1 = archivo .valgrind ; $2 = regex con | separados
    awk -v PATTERN="$2" -v INDENT='\t\t' '
        BEGIN { inblock = 0 }

        # Arranca cuando encuentra el error buscado
        $0 ~ PATTERN      { inblock = 1 }

        # Si aparece la cabecera de HEAP o LEAK, terminamos el bloque antes de imprimirla
        inblock && /^==[0-9]+== (HEAP SUMMARY:|LEAK SUMMARY:)/ {
            inblock = 0; print ""; next
        }

        # Imprimimos todo lo que esté dentro del bloque
        inblock           { print INDENT $0 }

        # Cerramos al llegar al ERROR SUMMARY
        inblock && /^==[0-9]+== .*ERROR SUMMARY/ {
            inblock = 0; print ""
        }
    ' "$1"
}

extraer_leaks_valgrind() {
    # $1 = archivo .valgrind
    awk -v INDENT='\t\t' '
        /LEAK SUMMARY:/ { inblock = 1 }
        inblock         { print INDENT $0 }
        inblock && /suppressed:/ {
            inblock = 0; print ""; next
        }
    ' "$1"
}

# Configurable threshold for memory leaks (in bytes)
MEMORY_LEAK_THRESHOLD=1000

# Detected errors counter
errores=0

if [[ $numero -ne 13 ]]; then
    make clean

    # Liberar puertos 8001 a 8004
    fuser -k 8001/tcp 2>/dev/null || true
    fuser -k 8002/tcp 2>/dev/null || true
    fuser -k 8003/tcp 2>/dev/null || true
    fuser -k 8004/tcp 2>/dev/null || true
    pkill -f valgrind || true

    clear
    echo -e "\t\e[34m Compilando...\e[0m"
    echo ""
    if ! make; then
    echo -e "\t\e[1;97;41m ❌ Error: Falló la compilación con make. Abortando.\e[0m"
    exit 1
    fi
    clear
    echo ""
fi

case $numero in
1)
    echo -e "\e[1;34;47m =====    Ejecutando Test 1: Corto Plazo - FIFO con 2 CPUs + 2 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_plani_corto_plazo.config &
    pid_memoria=$!
    sleep 1

    (
    ./kernel/bin/kernel PLANI_CORTO_PLAZO 0 kernel_plani_corto_plazo_fifo.config --action
    ) &
    pid_kernel=$!
    sleep 1

    ./cpu/bin/cpu 1 cpu_plani_corto_plazo.config &
    pid_cpu1=$!
    sleep 1

    ./cpu/bin/cpu 2 cpu_plani_corto_plazo.config &
    pid_cpu2=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 1

    sleep $(echo "5 - 0.5 * 4" | bc)

    sleep 63

    ./io/bin/io DISCO & 
    pid_io2=$!
    sleep 15

    echo -e "\e[1;34;47m =====    🔄 Matando IOs con Ctrl+C (SIGINT)    ===== \e[0m"
    kill -SIGINT "$pid_io1"
    kill -SIGINT "$pid_io2"
    echo ""
    echo ""

    main_pid=$$
    ( sleep 130 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado el $(date '+%d/%m/%Y a las %H:%M:%S'), los modulos tardaron mucho en finalizar (posible Deadlock) \e[0m" && sleep 0.1 && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_cpu2 $pid_io1 $pid_io2 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_cpu2; exit_cpu2=$?
    wait $pid_io1; exit_io1=$?
    wait $pid_io2; exit_io2=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"
    echo ""

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_cpu2    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 2 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io2     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 2 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" && chmod +x ./detectar_deadlock_kernel.sh && ./detectar_deadlock_kernel.sh || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE SE HAYAN USADO AMBAS IO DE FORMA PARALELA"
    ;;
2)
    echo -e "\e[1;34;47m =====    Ejecutando Test 2: Corto Plazo - SJF con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_plani_corto_plazo.config &
    pid_memoria=$!
    sleep 1

    (
    ./kernel/bin/kernel PLANI_CORTO_PLAZO 0 kernel_plani_corto_plazo_sjf.config --action
    ) &
    pid_kernel=$!
    sleep 1

    ./cpu/bin/cpu 1 cpu_plani_corto_plazo.config &
    pid_cpu1=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 1

    sleep $(echo "5 - 0.5 * 3" | bc)

    sleep 78

    echo -e "\e[1;34;47m =====    🔄 Matando IO con Ctrl+C (SIGINT)    ===== \e[0m"
    kill -SIGINT "$pid_io1"
    echo ""
    echo ""

    main_pid=$$
    ( sleep 130 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado el $(date '+%d/%m/%Y a las %H:%M:%S'), los modulos tardaron mucho en finalizar (posible Deadlock) \e[0m" && sleep 0.1 && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"
    echo ""

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" && chmod +x ./detectar_deadlock_kernel.sh && ./detectar_deadlock_kernel.sh || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE EL PID 5 ES EL DE MENOR PROMEDIO DE ESPERA"
    ;;
3)
    echo -e "\e[1;34;47m =====    Ejecutando Test 3: Corto Plazo - SRT con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_plani_corto_plazo.config &
    pid_memoria=$!
    sleep 1

    (
    ./kernel/bin/kernel PLANI_CORTO_PLAZO 0 kernel_plani_corto_plazo_srt.config --action
    ) &
    pid_kernel=$!
    sleep 1

    ./cpu/bin/cpu 1 cpu_plani_corto_plazo.config &
    pid_cpu1=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 1

    sleep $(echo "5 - 0.5 * 3" | bc)

    sleep 90

    echo -e "\e[1;34;47m =====    🔄 Matando IO con Ctrl+C (SIGINT)    ===== \e[0m"
    kill -SIGINT "$pid_io1"
    echo ""
    echo ""

    main_pid=$$
    ( sleep 150 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado el $(date '+%d/%m/%Y a las %H:%M:%S'), los modulos tardaron mucho en finalizar (posible Deadlock) \e[0m" && sleep 0.1 && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"
    echo ""

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" && chmod +x ./detectar_deadlock_kernel.sh && ./detectar_deadlock_kernel.sh || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE OCURRE DESALOJO;"
    imprimir_alerta_multicolor "Y QUE EL PID 5 ES EL DE MENOR PROMEDIO DE ESPERA, Y ES MENOR QUE EN SJF"
    ;;
4)
    echo -e "\e[1;34;47m =====    Ejecutando Test 4: Mediano/Largo Plazo - FIFO con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_plani_lym_plazo.config &
    pid_memoria=$!
    sleep 1

    (
    ./kernel/bin/kernel PLANI_LYM_PLAZO 0 kernel_plani_lym_plazo_fifo.config --action
    ) &
    pid_kernel=$!
    sleep 1

    ./cpu/bin/cpu 1 cpu_plani_lym_plazo.config &
    pid_cpu1=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 1

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 280 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado el $(date '+%d/%m/%Y a las %H:%M:%S'), los modulos tardaron mucho en finalizar (posible Deadlock) \e[0m" && sleep 0.1 && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"
    echo ""

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" && chmod +x ./detectar_deadlock_kernel.sh && ./detectar_deadlock_kernel.sh || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE PID 1, 2, 3, 4, 6, 7 Y 8 SON SUSPENDIDOS Y ENVIADOS A SWAP"
    ;;
5)
    echo -e "\e[1;34;47m =====    Ejecutando Test 5: Mediano/Largo Plazo - PMCP con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_plani_lym_plazo.config &
    pid_memoria=$!
    sleep 1

    (
    ./kernel/bin/kernel PLANI_LYM_PLAZO 0 kernel_plani_lym_plazo_pmcp.config --action
    ) &
    pid_kernel=$!
    sleep 1

    ./cpu/bin/cpu 1 cpu_plani_lym_plazo.config &
    pid_cpu1=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 1

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 180 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado el $(date '+%d/%m/%Y a las %H:%M:%S'), los modulos tardaron mucho en finalizar (posible Deadlock) \e[0m" && sleep 0.1 && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"
    echo ""

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" && chmod +x ./detectar_deadlock_kernel.sh && ./detectar_deadlock_kernel.sh || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE EL PID 5 ES EL ULTIMO EN INGRESAR A READY;"
    imprimir_alerta_multicolor "Y QUE PID 1, 2, 3, 4, 6, 7 Y 8 SON SUSPENDIDOS Y ENVIADOS A SWAP"
    ;;
6)
    echo -e "\e[1;34;47m =====    Ejecutando Test 6: SWAP con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_memoria_io.config &
    pid_memoria=$!
    sleep 1

    (
    ./kernel/bin/kernel MEM_IO_GH_ACTION 90 kernel_memoria_io.config --action
    ) &
    pid_kernel=$!
    sleep 1

    ./cpu/bin/cpu 1 cpu_memoria_io.config &
    pid_cpu1=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 1

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 280 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado el $(date '+%d/%m/%Y a las %H:%M:%S'), los modulos tardaron mucho en finalizar (posible Deadlock) \e[0m" && sleep 0.1 && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"
    echo ""

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" && chmod +x ./detectar_deadlock_kernel.sh && ./detectar_deadlock_kernel.sh || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE AL INICIAR LA IO DE 999999 SE SUSPENDE;"
    imprimir_alerta_multicolor "Y QUE LOS ARCHIVOS DE DUMP Y EL DE SWAP CON CONSISTENTES CON LA PRUEBA"
    ;;
7)
    echo -e "\e[1;34;47m =====    Ejecutando Test 7: Memoria Caché - CLOCK con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_memoria_base.config &
    pid_memoria=$!
    sleep 1

    (
    ./kernel/bin/kernel MEM_BASE_GH_ACTION 256 kernel_memoria_base.config --action
    ) &
    pid_kernel=$!
    sleep 1

    ./cpu/bin/cpu 1 cpu_memoria_base_clock.config &
    pid_cpu1=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 1

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 210 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado el $(date '+%d/%m/%Y a las %H:%M:%S'), los modulos tardaron mucho en finalizar (posible Deadlock) \e[0m" && sleep 0.1 && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"
    echo ""

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" && chmod +x ./detectar_deadlock_kernel.sh && ./detectar_deadlock_kernel.sh || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE LOS REEMPLAZOS DE CACHÉ SE REALIZAN CORRECTAMENTE DE ACUERDO A CLOCK"
    ;;
8)
    echo -e "\e[1;34;47m =====    Ejecutando Test 8: Memoria Caché - CLOCK-M con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_memoria_base.config &
    pid_memoria=$!
    sleep 1

    (
    ./kernel/bin/kernel MEM_BASE_GH_ACTION 256 kernel_memoria_base.config --action
    ) &
    pid_kernel=$!
    sleep 1

    ./cpu/bin/cpu 1 cpu_memoria_base_clock_m.config &
    pid_cpu1=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 1

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 210 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado el $(date '+%d/%m/%Y a las %H:%M:%S'), los modulos tardaron mucho en finalizar (posible Deadlock) \e[0m" && sleep 0.1 && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"
    echo ""

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" && chmod +x ./detectar_deadlock_kernel.sh && ./detectar_deadlock_kernel.sh || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE LOS REEMPLAZOS DE CACHÉ SE REALIZAN CORRECTAMENTE DE ACUERDO A CLOCK M"
    ;;
9)
    echo -e "\e[1;34;47m =====    Ejecutando Test 9: Memoria TLB - FIFO con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_memoria_base.config &
    pid_memoria=$!
    sleep 1

    (
    ./kernel/bin/kernel MEM_BASE_GH_ACTION 256 kernel_memoria_base.config --action
    ) &
    pid_kernel=$!
    sleep 1

    ./cpu/bin/cpu 1 cpu_memoria_base_fifo.config &
    pid_cpu1=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 1

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 1100 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado el $(date '+%d/%m/%Y a las %H:%M:%S'), los modulos tardaron mucho en finalizar (posible Deadlock) \e[0m" && sleep 0.1 && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"
    echo ""

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" && chmod +x ./detectar_deadlock_kernel.sh && ./detectar_deadlock_kernel.sh || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE LOS REEMPLAZOS DE LA TLB SE REALIZAN CORRECTAMENTE DE ACUERDO A FIFO"
    ;;
10)
    echo -e "\e[1;34;47m =====    Ejecutando Test 10: Memoria TLB - LRU con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_memoria_base.config &
    pid_memoria=$!
    sleep 1

    (
    ./kernel/bin/kernel MEM_BASE_GH_ACTION 256 kernel_memoria_base.config --action
    ) &
    pid_kernel=$!
    sleep 1

    ./cpu/bin/cpu 1 cpu_memoria_base_lru.config &
    pid_cpu1=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 1

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 1100 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado el $(date '+%d/%m/%Y a las %H:%M:%S'), los modulos tardaron mucho en finalizar (posible Deadlock) \e[0m" && sleep 0.1 && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"
    echo ""

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" && chmod +x ./detectar_deadlock_kernel.sh && ./detectar_deadlock_kernel.sh || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE LOS REEMPLAZOS DE LA TLB SE REALIZAN CORRECTAMENTE DE ACUERDO A LRU"
    ;;
11)
    echo -e "\e[1;34;47m =====    Ejecutando Test 11: ESTABILIDAD GENERAL con 4 CPUS + 4 IOs    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_estabilidad_general.config &
    pid_memoria=$!
    sleep 1

    (
    ./kernel/bin/kernel ESTABILIDAD_GENERAL 0 kernel_estabilidad_general.config --action
    ) &
    pid_kernel=$!
    sleep 1

    ./cpu/bin/cpu 1 cpu_1_estabilidad_general.config &
    pid_cpu1=$!
    sleep 1

    ./cpu/bin/cpu 2 cpu_2_estabilidad_general.config &
    pid_cpu2=$!
    sleep 1

    ./cpu/bin/cpu 3 cpu_3_estabilidad_general.config &
    pid_cpu3=$!
    sleep 1

    ./cpu/bin/cpu 4 cpu_4_estabilidad_general.config & 
    pid_cpu4=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io2=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io3=$!
    sleep 1

    ./io/bin/io DISCO & 
    pid_io4=$!

    sleep 200

    ./io/bin/io DISCO & 
    pid_io5=$!
    sleep 1

    ./io/bin/io DISCO &
    pid_io6=$!
    sleep 1

    ./io/bin/io DISCO &
    pid_io7=$!
    sleep 1

    ./io/bin/io DISCO &
    pid_io8=$!
    sleep 1

    sleep 200

    echo -e "\e[1;34;47m =====    🔄 Matando todos los modulos (SIGINT)    ===== \e[0m"
    kill -SIGINT "$pid_io1"
    kill -SIGINT "$pid_io2"
    kill -SIGINT "$pid_io3"
    kill -SIGINT "$pid_io4"
    kill -SIGINT "$pid_io5"
    kill -SIGINT "$pid_io6"
    kill -SIGINT "$pid_io7"
    kill -SIGINT "$pid_io8"
    kill -SIGINT "$pid_kernel"
    kill -SIGINT "$pid_cpu1"
    kill -SIGINT "$pid_cpu2"
    kill -SIGINT "$pid_cpu3"
    kill -SIGINT "$pid_cpu4"
    kill -SIGINT "$pid_memoria"
    echo ""
    echo ""

    main_pid=$$
    ( sleep 60 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado el $(date '+%d/%m/%Y a las %H:%M:%S'), los modulos tardaron mucho en finalizar (posible Deadlock) \e[0m" && sleep 0.1 && kill $pid_io1 $pid_io2 $pid_io3 $pid_io4 $pid_io5 $pid_io6 $pid_io7 $pid_io8 $pid_kernel $pid_cpu1 $pid_cpu2 $pid_cpu3 $pid_cpu4 $pid_memoria >/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) & 
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_cpu2; exit_cpu2=$?
    wait $pid_cpu3; exit_cpu3=$?
    wait $pid_cpu4; exit_cpu4=$?
    wait $pid_io1; exit_io1=$?
    wait $pid_io2; exit_io2=$?
    wait $pid_io3; exit_io3=$?
    wait $pid_io4; exit_io4=$?
    wait $pid_io5; exit_io5=$?
    wait $pid_io6; exit_io6=$?
    wait $pid_io7; exit_io7=$?
    wait $pid_io8; exit_io8=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"
    echo ""

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_cpu2    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 2 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_cpu3    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 3 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_cpu4    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 4 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io2     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 2 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io3     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 3 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io4     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 4 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io5     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 5 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io6     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 6 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io7     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 7 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io8     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 8 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" && chmod +x ./detectar_deadlock_kernel.sh && ./detectar_deadlock_kernel.sh || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ CON HTOP QUE NO HAYA ESPERAS ACTIVAS NI MEMORY LEAKS; NO HAY ERRORES NI FINALIZA DE MANERA ABRUPTA"
    ;;
12)
    echo -e "\e[1;34;47m =====    Ejecutando Test 12: ESTABILIDAD GENERAL con 4 CPUS + 4 IOs y valgrind    ===== \e[0m"
    sleep 3

    valgrind --leak-check=full --log-file=memoria/memoria.valgrind ./memoria/bin/memoria memoria_estabilidad_general.config &
    pid_memoria=$!
    sleep 1

    (
    valgrind --leak-check=full --log-file=kernel/kernel.valgrind ./kernel/bin/kernel ESTABILIDAD_GENERAL 0 kernel_estabilidad_general.config --action
    ) &
    pid_kernel=$!
    sleep 1

    valgrind --leak-check=full --log-file=cpu/cpu_1.valgrind ./cpu/bin/cpu 1 cpu_1_estabilidad_general.config &
    pid_cpu1=$!
    sleep 1

    valgrind --leak-check=full --log-file=cpu/cpu_2.valgrind ./cpu/bin/cpu 2 cpu_2_estabilidad_general.config &
    pid_cpu2=$!
    sleep 1

    valgrind --leak-check=full --log-file=cpu/cpu_3.valgrind ./cpu/bin/cpu 3 cpu_3_estabilidad_general.config &
    pid_cpu3=$!
    sleep 1

    valgrind --leak-check=full --log-file=cpu/cpu_4.valgrind ./cpu/bin/cpu 4 cpu_4_estabilidad_general.config & 
    pid_cpu4=$!
    sleep 1

    valgrind --leak-check=full --log-file=io/io_disco_1.valgrind ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 1

    valgrind --leak-check=full --log-file=io/io_disco_2.valgrind ./io/bin/io DISCO & 
    pid_io2=$!
    sleep 1

    valgrind --leak-check=full --log-file=io/io_disco_3.valgrind ./io/bin/io DISCO & 
    pid_io3=$!
    sleep 1

    valgrind --leak-check=full --log-file=io/io_disco_4.valgrind ./io/bin/io DISCO & 
    pid_io4=$!
    sleep 200
    
    valgrind --leak-check=full --log-file=io/io_disco_5.valgrind ./io/bin/io DISCO & 
    pid_io5=$!
    sleep 1

    valgrind --leak-check=full --log-file=io/io_disco_6.valgrind ./io/bin/io DISCO & 
    pid_io6=$!
    sleep 1

    valgrind --leak-check=full --log-file=io/io_disco_7.valgrind ./io/bin/io DISCO & 
    pid_io7=$!
    sleep 1

    valgrind --leak-check=full --log-file=io/io_disco_8.valgrind ./io/bin/io DISCO & 
    pid_io8=$!
    sleep 1

    sleep 200

    echo -e "\e[1;34;47m =====    🔄 Matando todos los modulos (SIGINT)    ===== \e[0m"
    kill -SIGINT "$pid_io1"
    kill -SIGINT "$pid_io2"
    kill -SIGINT "$pid_io3"
    kill -SIGINT "$pid_io4"
    kill -SIGINT "$pid_io5"
    kill -SIGINT "$pid_io6"
    kill -SIGINT "$pid_io7"
    kill -SIGINT "$pid_io8"
    kill -SIGINT "$pid_kernel"
    kill -SIGINT "$pid_cpu1"
    kill -SIGINT "$pid_cpu2"
    kill -SIGINT "$pid_cpu3"
    kill -SIGINT "$pid_cpu4"
    kill -SIGINT "$pid_memoria"
    echo ""
    echo ""

    main_pid=$$
    ( sleep 60 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado el $(date '+%d/%m/%Y a las %H:%M:%S'), los modulos tardaron mucho en finalizar (posible Deadlock) \e[0m" && sleep 0.1 && kill $pid_io1 $pid_io2 $pid_io3 $pid_io4 $pid_io5 $pid_io6 $pid_io7 $pid_io8 $pid_kernel $pid_cpu1 $pid_cpu2 $pid_cpu3 $pid_cpu4 $pid_memoria >/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) & 
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_cpu2; exit_cpu2=$?
    wait $pid_cpu3; exit_cpu3=$?
    wait $pid_cpu4; exit_cpu4=$?
    wait $pid_io1; exit_io1=$?
    wait $pid_io2; exit_io2=$?
    wait $pid_io3; exit_io3=$?
    wait $pid_io4; exit_io4=$?
    wait $pid_io5; exit_io5=$?
    wait $pid_io6; exit_io6=$?
    wait $pid_io7; exit_io7=$?
    wait $pid_io8; exit_io8=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"
    echo ""

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_cpu2    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 2 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_cpu3    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 3 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_cpu4    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 4 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io2     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 2 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io3     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 3 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io4     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 4 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io5     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 5 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io6     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 6 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io7     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 7 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io8     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 8 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" && chmod +x ./detectar_deadlock_kernel.sh && ./detectar_deadlock_kernel.sh || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ CON HTOP QUE NO HAYA ESPERAS ACTIVAS NI MEMORY LEAKS; NO HAY ERRORES NI FINALIZA DE MANERA ABRUPTA"

    # ---------- Definiciones para Valgrind ----------
    # ERR_RE: Expresiones regulares para detectar errores críticos de Valgrind.
    # Coincide con: invalid read/write (acceso inválido a memoria), uninitialised value (uso de valores no inicializados),
    # jump to (salto a dirección inválida), overlap (solapamiento de memoria), stack'd/malloc'd (problemas con memoria dinámica o de stack).
    # Estos errores suelen indicar bugs graves de memoria que pueden causar corrupción, crashes o comportamientos inesperados.
    ERR_RE="invalid read|invalid write|uninitialised value|jump to|overlap|stack'd|malloc'd"
    
    echo ""
    echo -e "\n\e[1;34;47m =====    Validando Memory Leaks con Valgrind    ===== \e[0m\n"
    echo ""

    for val in memoria/memoria.valgrind kernel/kernel.valgrind \
               cpu/cpu_{1..4}.valgrind io/io_disco_{1..4}.valgrind; do

        [[ ! -f $val ]] && {
            echo -e "\e[1;97;41m Archivo $val no encontrado. Abortando. \e[0m"
            exit 1
        }

        definitely=$(grep -m1 "definitely lost:"  "$val" | awk '{gsub(/,/,"",$4); print $4+0}')
        indirectly=$(grep -m1 "indirectly lost:" "$val" | awk '{gsub(/,/,"",$4); print $4+0}')

        if (( definitely > MEMORY_LEAK_THRESHOLD || indirectly > MEMORY_LEAK_THRESHOLD )); then
            extraer_leaks_valgrind "$val"
            echo -e "\t\e[1;97;41m ❌ Leak detectado en $val (> $MEMORY_LEAK_THRESHOLD bytes) ↑ \e[0m\n"
            ((errores++))
        elif (( definitely > 0 || indirectly > 0 )); then
            extraer_leaks_valgrind "$val"
            echo -e "\t\e[1;30;43m Leak menor detectado en $val (≤ $MEMORY_LEAK_THRESHOLD bytes) ↑ \e[0m\n"
        else
            echo -e "\t\e[1;30;42m ✅ No se detectaron memory leaks en $val. \e[0m\n"
        fi
    done

    echo ""
    echo -e "\n\e[1;34;47m =====    Validando Errores Críticos con Valgrind    ===== \e[0m\n"
    echo ""

    for val in memoria/memoria.valgrind kernel/kernel.valgrind \
               cpu/cpu_{1..4}.valgrind io/io_disco_{1..4}.valgrind; do

        bloque_err=$(extraer_bloque_valgrind "$val" "$ERR_RE")

        if [[ -n $bloque_err ]]; then
            printf "%s\n" "$bloque_err"
            echo -e "\t\e[1;97;41m 🚨 Error crítico en ejecución detectado por Valgrind en $val ↑ \e[0m\n"
            ((errores++))
        else
            echo -e "\t\e[1;30;42m ✅ No se detectaron errores críticos en $val. \e[0m\n"
        fi
    done
    ;;
13) 
    # Se le puede pasar como ultimo parametro el test del cual comenzar
    while true; do
        total_tests_ejecutados=0
        for ((t=inicio_13; t<=12; t++)); do
            clear
            echo -e "\t\t\e[1;34;47m =====    Ejecutando Test 13: Loop de Tests 1-12 (finaliza al fallar alguno)    ===== \e[0m"
            sleep 3
            ./ejecutar_test.sh "$t"
            res=$?
            ((total_tests_ejecutados++))
            if [[ $res -ne 0 ]]; then
                echo ""
                echo -e "\t\e[1;97;41m Test $t falló con código $res. Deteniendo ejecución. \e[0m"
                echo -e "\t\e[1;34;47m Total de tests ejecutados: $total_tests_ejecutados \e[0m"
                exit $res
            fi
        done
        inicio_13=1
    done
    ;;
esac

echo ""
chmod +x unir_logs.sh
./unir_logs.sh

echo ""
echo -e "\e[1;34;47m =====   Analizando logs por errores o advertencias   ===== \e[0m"
echo ""

log_file="./log_global_ordenado.log"

if [[ ! -f "$log_file" ]]; then
    echo -e "\t\e[1;97;41m Archivo $log_file no encontrado. Abortando. \e[0m"
    ((errores++))
else
    log_matches=$(grep -E "\[ERROR\]|\[WARNING\]" "$log_file")

    if [[ -n "$log_matches" ]]; then
        echo -e "\t\e[1;97;41m 🔴 Se encontraron errores o advertencias en $log_file: \e[0m"
        echo "$log_matches"
        ((errores++))
    else
        echo -e "\t\e[1;30;42m 🟢 No se encontraron errores ni advertencias en $log_file \e[0m"
    fi
fi



echo ""
echo " Test $numero finalizado "
sleep 5
exit $errores