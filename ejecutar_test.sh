#!/bin/bash
# chmod +x ejecutar_test.sh; ./ejecutar_test.sh 1
# Estos tests solamente verifican que el codigo de retorno de cada proceso sea exitoso y que no haya log_error o log_warning.
# No verifica logica, deadlocks, memory leaks, condiciones de carrera, ni esperas activas.

if [[ -z "$1" ]]; then
    echo "  Error: Debe indicar el número de test como parámetro (1 a 11)."
    exit 1
fi

numero="$1"

if ! [[ "$numero" =~ ^[0-9]+$ ]] || (( numero < 1 || numero > 11 )); then
    echo "  Número inválido. Debe ser del 1 al 11."
    exit 1
fi

imprimir_alerta_multicolor() {
    local mensaje="$1"
    local reset="\e[0m"

    local estilos=(
        "\e[1;34;43m"  # azul oscuro sobre amarillo
        "\e[1;35;46m"  # magenta sobre cyan
        "\e[1;97;44m"  # blanco brillante sobre azul
        "\e[1;92;45m"  # verde claro sobre magenta
        "\e[1;97;41m"  # blanco brillante sobre rojo
        "\e[1;30;103m" # negro intenso sobre amarillo neón
        "\e[1;96;100m" # cyan brillante sobre gris oscuro
    )

    echo ""
    for estilo in "${estilos[@]}"; do
        echo -e "\t${estilo} ${mensaje} ${reset}"
    done
    echo ""
}


make clean
clear
make
clear
find . -name "*.log" -exec truncate -s 0 {} \;
# Limpiar archivo SWAP para asegurar estado consistente entre tests
rm -f memoria/files/swapfile.bin
clear

errores=0

case $numero in

1)
    echo -e "\e[1;34;47m =====    Ejecutando Test 1: Corto Plazo - FIFO con 2 CPUs + 2 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_plani_corto_plazo.config &
    pid_memoria=$!
    sleep 0.5

    (
    ./kernel/bin/kernel PLANI_CORTO_PLAZO 0 kernel_plani_corto_plazo_fifo.config --action
    ) &
    pid_kernel=$!
    sleep 0.5

    ./cpu/bin/cpu 1 cpu_plani_corto_plazo.config &
    pid_cpu1=$!
    sleep 0.5

    ./cpu/bin/cpu 2 cpu_plani_corto_plazo.config &
    pid_cpu2=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    sleep $(echo "5 - 0.5 * 4" | bc)

    sleep 63

    ./io/bin/io DISCO & 
    pid_io2=$!
    sleep 15

    echo -e "\e[1;34;47m =====    🔄 Matando IOs con Ctrl+C (SIGINT)    ===== \e[0m"
    kill -SIGINT "$pid_io1"
    kill -SIGINT "$pid_io2"

    main_pid=$$
    ( sleep 60 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado, los modulos tardaron mucho en finalizar \e[0m" && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_cpu2 $pid_io1 $pid_io2 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
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

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_cpu2    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 2 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))
    [[ $exit_io2     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 2 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE SE HAYAN USADO AMBAS IO DE FORMA PARALELA    =!=!=!=!=!"
    ;;
2)
    echo -e "\e[1;34;47m =====    Ejecutando Test 2: Corto Plazo - SJF con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_plani_corto_plazo.config &
    pid_memoria=$!
    sleep 0.5

    (
    ./kernel/bin/kernel PLANI_CORTO_PLAZO 0 kernel_plani_corto_plazo_sjf.config --action
    ) &
    pid_kernel=$!
    sleep 0.5

    ./cpu/bin/cpu 1 cpu_plani_corto_plazo.config &
    pid_cpu1=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    sleep $(echo "5 - 0.5 * 3" | bc)

    sleep 78

    echo -e "\e[1;34;47m =====    🔄 Matando IO con Ctrl+C (SIGINT)    ===== \e[0m"
    kill -SIGINT "$pid_io1"

    main_pid=$$
    ( sleep 60 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado, los modulos tardaron mucho en finalizar \e[0m" && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE EL PID 5 ES EL DE MENOR PROMEDIO DE ESPERA    =!=!=!=!=!"
    ;;
3)
    echo -e "\e[1;34;47m =====    Ejecutando Test 3: Corto Plazo - SRT con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_plani_corto_plazo.config &
    pid_memoria=$!
    sleep 0.5

    (
    ./kernel/bin/kernel PLANI_CORTO_PLAZO 0 kernel_plani_corto_plazo_srt.config --action
    ) &
    pid_kernel=$!
    sleep 0.5

    ./cpu/bin/cpu 1 cpu_plani_corto_plazo.config &
    pid_cpu1=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    sleep $(echo "5 - 0.5 * 3" | bc)

    sleep 90

    echo -e "\e[1;34;47m =====    🔄 Matando IO con Ctrl+C (SIGINT)    ===== \e[0m"
    kill -SIGINT "$pid_io1"

    main_pid=$$
    ( sleep 60 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado, los modulos tardaron mucho en finalizar \e[0m" && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE OCURRE DESALOJO;"
    imprimir_alerta_multicolor "Y QUE EL PID 5 ES EL DE MENOR PROMEDIO DE ESPERA, Y ES MENOR QUE EN SJF"
    ;;
4)
    echo -e "\e[1;34;47m =====    Ejecutando Test 4: Mediano/Largo Plazo - FIFO con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_plani_lym_plazo.config &
    pid_memoria=$!
    sleep 0.5

    (
    ./kernel/bin/kernel PLANI_LYM_PLAZO 0 kernel_plani_lym_plazo_fifo.config --action
    ) &
    pid_kernel=$!
    sleep 0.5

    ./cpu/bin/cpu 1 cpu_plani_lym_plazo.config &
    pid_cpu1=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 150 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado, los modulos tardaron mucho en finalizar \e[0m" && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE PID 1, 2, 3, 4, 6, 7 Y 8 SON SUSPENDIDOS Y ENVIADOS A SWAP"
    ;;
5)
    echo -e "\e[1;34;47m =====    Ejecutando Test 5: Mediano/Largo Plazo - PMCP con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_plani_lym_plazo.config &
    pid_memoria=$!
    sleep 0.5

    (
    ./kernel/bin/kernel PLANI_LYM_PLAZO 0 kernel_plani_lym_plazo_pmcp.config --action
    ) &
    pid_kernel=$!
    sleep 0.5

    ./cpu/bin/cpu 1 cpu_plani_lym_plazo.config &
    pid_cpu1=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 120 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado, los modulos tardaron mucho en finalizar \e[0m" && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE EL PID 5 ES EL ULTIMO EN INGRESAR A READY;"
    imprimir_alerta_multicolor "Y QUE PID 1, 2, 3, 4, 6, 7 Y 8 SON SUSPENDIDOS Y ENVIADOS A SWAP"
    ;;
6)
    echo -e "\e[1;34;47m =====    Ejecutando Test 6: SWAP con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_memoria_io.config &
    pid_memoria=$!
    sleep 0.5

    (
    ./kernel/bin/kernel MEM_IO_GH_ACTION 90 kernel_memoria_io.config --action
    ) &
    pid_kernel=$!
    sleep 0.5

    ./cpu/bin/cpu 1 cpu_memoria_io.config &
    pid_cpu1=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 150 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado, los modulos tardaron mucho en finalizar \e[0m" && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE AL INICIAR LA IO DE 999999 SE SUSPENDE;"
    imprimir_alerta_multicolor "Y QUE LOS ARCHIVOS DE DUMP Y EL DE SWAP CON CONSISTENTES CON LA PRUEBA"
    ;;
7)
    echo -e "\e[1;34;47m =====    Ejecutando Test 7: Memoria Caché - CLOCK con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_memoria_base.config &
    pid_memoria=$!
    sleep 0.5

    (
    ./kernel/bin/kernel MEM_BASE_GH_ACTION 256 kernel_memoria_base.config --action
    ) &
    pid_kernel=$!
    sleep 0.5

    ./cpu/bin/cpu 1 cpu_memoria_base_clock.config &
    pid_cpu1=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 150 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado, los modulos tardaron mucho en finalizar \e[0m" && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE LOS REEMPLAZOS DE CACHÉ SE REALIZAN CORRECTAMENTE DE ACUERDO A CLOCK"
    ;;
8)
    echo -e "\e[1;34;47m =====    Ejecutando Test 8: Memoria Caché - CLOCK-M con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_memoria_base.config &
    pid_memoria=$!
    sleep 0.5

    (
    ./kernel/bin/kernel MEM_BASE_GH_ACTION 256 kernel_memoria_base.config --action
    ) &
    pid_kernel=$!
    sleep 0.5

    ./cpu/bin/cpu 1 cpu_memoria_base_clock_m.config &
    pid_cpu1=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 150 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado, los modulos tardaron mucho en finalizar \e[0m" && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE LOS REEMPLAZOS DE CACHÉ SE REALIZAN CORRECTAMENTE DE ACUERDO A CLOCK-M"
    ;;
9)
    echo -e "\e[1;34;47m =====    Ejecutando Test 9: Memoria TLB - FIFO con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_memoria_base.config &
    pid_memoria=$!
    sleep 0.5

    (
    ./kernel/bin/kernel MEM_BASE_GH_ACTION 256 kernel_memoria_base.config --action
    ) &
    pid_kernel=$!
    sleep 0.5

    ./cpu/bin/cpu 1 cpu_memoria_base_fifo.config &
    pid_cpu1=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 1080 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado, los modulos tardaron mucho en finalizar \e[0m" && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE LOS REEMPLAZOS DE LA TLB SE REALIZAN CORRECTAMENTE DE ACUERDO A FIFO"
    ;;
10)
    echo -e "\e[1;34;47m =====    Ejecutando Test 10: Memoria TLB - LRU con 1 CPU + 1 IO    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_memoria_base.config &
    pid_memoria=$!
    sleep 0.5

    (
    ./kernel/bin/kernel MEM_BASE_GH_ACTION 256 kernel_memoria_base.config --action
    ) &
    pid_kernel=$!
    sleep 0.5

    ./cpu/bin/cpu 1 cpu_memoria_base_lru.config &
    pid_cpu1=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    sleep $(echo "5 - 0.5 * 3" | bc)

    main_pid=$$
    ( sleep 1080 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado, los modulos tardaron mucho en finalizar \e[0m" && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) &
    watcher_pid=$!

    wait $pid_memoria; exit_memoria=$?
    wait $pid_kernel; exit_kernel=$?
    wait $pid_cpu1; exit_cpu1=$?
    wait $pid_io1; exit_io1=$?

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"

    [[ $exit_memoria -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Memoria finalizó con error \e[0m" && ((errores++))
    [[ $exit_kernel  -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Kernel finalizó con error \e[0m"  && ((errores++))
    [[ $exit_cpu1    -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ CPU 1 finalizó con error \e[0m"   && ((errores++))
    [[ $exit_io1     -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ IO 1 finalizó con error \e[0m"    && ((errores++))

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE LOS REEMPLAZOS DE LA TLB SE REALIZAN CORRECTAMENTE DE ACUERDO A LRU"
    ;;
11)
    echo -e "\e[1;34;47m =====    Ejecutando Test 11: ESTABILIDAD GENERAL con 4 CPUS + 4 IOs    ===== \e[0m"
    sleep 3

    ./memoria/bin/memoria memoria_estabilidad_general.config &
    pid_memoria=$!
    sleep 0.5

    (
    ./kernel/bin/kernel ESTABILIDAD_GENERAL 256 kernel_estabilidad_general.config --action
    ) &
    pid_kernel=$!
    sleep 0.5

    ./cpu/bin/cpu 1 cpu_1_estabilidad_general.config &
    pid_cpu1=$!
    sleep 0.5

    ./cpu/bin/cpu 2 cpu_2_estabilidad_general.config &
    pid_cpu2=$!
    sleep 0.5

    ./cpu/bin/cpu 3 cpu_3_estabilidad_general.config &
    pid_cpu3=$!
    sleep 0.5

    ./cpu/bin/cpu 4 cpu_4_estabilidad_general.config & 
    pid_cpu4=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    ./io/bin/io DISCO & 
    pid_io1=$!
    sleep 0.5

    sleep $(echo "5 - 0.5 * 9" | bc)

    sleep $(echo "1800 / 3" | bc)   # Objetivo: 30 minutos; Actual: 10 minutos

    echo -e "\e[1;34;47m =====    🔄 Matando IOs con Ctrl+C (SIGINT)    ===== \e[0m"
    kill -SIGINT "$pid_io1"
    kill -SIGINT "$pid_io2"
    kill -SIGINT "$pid_io3"
    kill -SIGINT "$pid_io4"

    main_pid=$$
    ( sleep 180 && echo -e "\t\e[1;97;41m ⏰ Timeout alcanzado, los modulos tardaron mucho en finalizar \e[0m" && kill $pid_memoria $pid_kernel $pid_cpu1 $pid_cpu2 $pid_cpu3 $pid_cpu4 $pid_io1 2>/dev/null && chmod +x unir_logs.sh && ./unir_logs.sh && kill -TERM "$main_pid" ) & 
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

    kill $watcher_pid 2>/dev/null

    echo ""
    echo -e "\e[1;34;47m =====    Resultados de los procesos    ===== \e[0m"

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

    [[ $errores -ne 0 ]] && echo -e "\t\e[1;97;41m ❌ Fallaron $errores módulos \e[0m" || echo -e "\t\e[1;30;42m ✅ Todos los procesos finalizaron correctamente \e[0m"

    imprimir_alerta_multicolor "VERIFICÁ MANUALMENTE QUE LOS REEMPLAZOS DE LA TLB SE REALIZAN CORRECTAMENTE DE ACUERDO A LRU"
    ;;
esac

echo ""
chmod +x unir_logs.sh
./unir_logs.sh

echo ""
echo -e "\e[1;34;47m =====    Analizando logs por errores o advertencias    ===== \e[0m"

log_file="./log_global_ordenado.log"

if [[ ! -f "$log_file" ]]; then
    echo -e "\t\e[1;97;41m 🔴 No se encontró el archivo $log_file \e[0m"
    errores=1
else
    log_matches=$(grep -E "\[ERROR\]|\[WARNING\]" "$log_file")

    if [[ -n "$log_matches" ]]; then
        echo -e "\t\e[1;97;41m 🔴 Se encontraron errores o advertencias en $log_file: \e[0m"
        echo "$log_matches"
        errores=1
    else
        echo -e "\t\e[1;30;42m 🟢 No se encontraron errores ni advertencias en $log_file \e[0m"
    fi
fi

echo ""
exit $errores