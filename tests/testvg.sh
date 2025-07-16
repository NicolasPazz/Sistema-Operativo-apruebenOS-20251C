#!/bin/bash
set -e

# Cambiar al directorio raíz del proyecto
cd "$(dirname "$0")/.."

# Limitar RAM a 512 MB por proceso
#ulimit -v $((512 * 1024))

# Matar procesos colgados de ejecuciones anteriores
fuser -k 8001/tcp 2>/dev/null || true
fuser -k 8002/tcp 2>/dev/null || true
fuser -k 8003/tcp 2>/dev/null || true
fuser -k 8004/tcp 2>/dev/null || true
pkill -f valgrind 2>/dev/null || true

# Compilar
make clean
make

# Limpiar logs previos
rm -f memoria/memoria.valgrind kernel/kernel.valgrind cpu/cpu.valgrind io/io.valgrind
rm -f memoria/memoria.log kernel/kernel.log cpu/cpu.log io/io.log

############################

# INICIAR MEMORIA
valgrind --leak-check=full --log-file=memoria/memoria.valgrind ./memoria/bin/memoria memoria_estabilidad_general.config &
PID_MEMORIA=$!
sleep 5

# INICIAR KERNEL
valgrind --leak-check=full --log-file=kernel/kernel.valgrind ./kernel/bin/kernel ESTABILIDAD_GENERAL 0 kernel_estabilidad_general.config --action &
PID_KERNEL=$!
sleep 5

# INICIAR CPU 1
valgrind --leak-check=full --log-file=cpu/cpu.valgrind ./cpu/bin/cpu 1 cpu_1_estabilidad_general.config &
PID_CPU1=$!
sleep 1

# INICIAR CPU 2
valgrind --leak-check=full --log-file=cpu/cpu.valgrind ./cpu/bin/cpu 2 cpu_2_estabilidad_general.config &
PID_CPU2=$!
sleep 1

# INICIAR CPU 3
valgrind --leak-check=full --log-file=cpu/cpu.valgrind ./cpu/bin/cpu 3 cpu_3_estabilidad_general.config &
PID_CPU3=$!
sleep 1

# INICIAR CPU 4
valgrind --leak-check=full --log-file=cpu/cpu.valgrind ./cpu/bin/cpu 4 cpu_4_estabilidad_general.config &
PID_CPU4=$!
sleep 1

# INICIAR IO
valgrind --leak-check=full --log-file=io/io.valgrind ./io/bin/io DISCO &
PID_IO1=$!
sleep 1

# INICIAR IO
valgrind --leak-check=full --log-file=io/io.valgrind ./io/bin/io DISCO &
PID_IO2=$!
sleep 1

# INICIAR IO
valgrind --leak-check=full --log-file=io/io.valgrind ./io/bin/io DISCO &
PID_IO3=$!
sleep 1

# INICIAR IO
valgrind --leak-check=full --log-file=io/io.valgrind ./io/bin/io DISCO &
PID_IO4=$!
sleep 1

############################

# ─────────────────────────────────────────────
# 1) Ejecutar el banco de pruebas durante 30 min
# ─────────────────────────────────────────────
echo "⏱ Ejecutando prueba de estabilidad durante 30 min…"
sleep 1800

# ─────────────────────────────────────────────
# 2) Matar las IO y esperar a los demás
# ─────────────────────────────────────────────
echo "💀 Finalizando instancias IO…"
kill $PID_IO1 $PID_IO2 $PID_IO3 $PID_IO4 2>/dev/null || true

# Esperar sus salidas (las IO deberían terminar rápido tras la señal)
wait $PID_IO1 2>/dev/null; EXIT_IO1=$?
wait $PID_IO2 2>/dev/null; EXIT_IO2=$?
wait $PID_IO3 2>/dev/null; EXIT_IO3=$?
wait $PID_IO4 2>/dev/null; EXIT_IO4=$?

echo "⌛ Esperando finalización de Kernel, Memoria y CPUs…"
wait $PID_KERNEL;  EXIT_KERNEL=$?
wait $PID_MEMORIA; EXIT_MEMORIA=$?
wait $PID_CPU1;    EXIT_CPU1=$?
wait $PID_CPU2;    EXIT_CPU2=$?
wait $PID_CPU3;    EXIT_CPU3=$?
wait $PID_CPU4;    EXIT_CPU4=$?

set +e   # a partir de aquí no abortar en error

############################

ERROR=0
echo ""
echo ""
echo "--------------------------------------"
echo "Comenzando la validacion de errores..."
echo "--------------------------------------"
echo ""
echo ""
echo "------------------"
echo "Códigos de salida:"
if [ "$EXIT_MEMORIA" -eq 0 ]; then
    echo "🧠 Memoria: $EXIT_MEMORIA   ✓"
elif [ "$EXIT_MEMORIA" -eq 139 ]; then
    echo "::error ::❌ Memoria terminó por segmentation fault: $EXIT_MEMORIA ✗✗✗✗✗"
    ((ERROR++))
else
    echo "::error ::❌ Memoria: $EXIT_MEMORIA   ✗"
    ((ERROR++))
fi
if [ "$EXIT_KERNEL" -eq 0 ]; then
    echo "🧩 Kernel : $EXIT_KERNEL   ✓"
elif [ "$EXIT_KERNEL" -eq 139 ]; then
    echo "::error ::❌ Kernel terminó por segmentation fault: $EXIT_KERNEL ✗✗✗✗✗"
    ((ERROR++))
else
    echo "::error ::❌ Kernel : $EXIT_KERNEL   ✗"
    ((ERROR++))
fi
if [ "$EXIT_CPU" -eq 0 ]; then
    echo "🖥  CPU    : $EXIT_CPU   ✓"
elif [ "$EXIT_CPU" -eq 139 ]; then
    echo "::error ::❌ Cpu terminó por segmentation fault: $EXIT_CPU ✗✗✗✗✗"
    ((ERROR++))
else
    echo "::error ::❌ CPU    : $EXIT_CPU   ✗"
    ((ERROR++))
fi
if [ "$EXIT_IO" -eq 0 ]; then
    echo "⌨️  IO     : $EXIT_IO   ✓"
elif [ "$EXIT_IO" -eq 139 ]; then
    echo "::error ::❌ IO terminó por segmentation fault: $EXIT_IO ✗✗✗✗✗"
    ((ERROR++))
else
    echo "::error ::❌ IO     : $EXIT_IO   ✗"
    ((ERROR++))
fi
echo "------------------"

# VALIDAR ERRORES

echo ""
echo "-------------------------------"
echo "Validando Segmentation fault..."
for val in memoria/memoria.valgrind kernel/kernel.valgrind cpu/cpu.valgrind io/io.valgrind; do
    if grep -q "Segmentation fault" "$val"; then
        grep -E "Segmentation fault" "$val"
        echo "::error ::❌ Segmentation fault en $val ↑"
        echo ""
        ((ERROR++))
    fi
done
echo "-------------------------------"

echo ""
echo "----------------------------"
echo "Validando logs de errores..."
for log in kernel/kernel.log memoria/memoria.log cpu/cpu.log io/io.log; do
    errores_en_log=0
    while IFS= read -r linea; do
        echo "$linea"
        ((ERROR++))
        ((errores_en_log++))
    done < <(grep "\[ERROR\]" "$log")

    if [ "$errores_en_log" -gt 0 ]; then
        echo "::error ::❌ Error encontrado en $log ↑"
        echo ""
    fi
done
echo "----------------------------"

echo ""
echo "-------------------------------"
echo "Validando leaks con valgrind..."
for val in memoria/memoria.valgrind kernel/kernel.valgrind cpu/cpu.valgrind io/io.valgrind; do
    definitely_lost=$(grep "definitely lost:" "$val" | awk '{print $4}' | tr -d ',')
    indirectly_lost=$(grep "indirectly lost:" "$val" | awk '{print $4}' | tr -d ',')

    def_num=${definitely_lost:-0}
    indir_num=${indirectly_lost:-0}

    if [[ "$def_num" -gt 1000 || "$indir_num" -gt 1000 ]]; then
        grep -E "definitely lost|indirectly lost" "$val"
        echo "::error ::❌ Leak detectado en $val ↑"
        echo ""
        ((ERROR++))
    elif [[ "$def_num" -gt 0 || "$indir_num" -gt 0 ]]; then
        grep -E "definitely lost|indirectly lost" "$val"
        echo "::warning ::Leak menor detectado en $val (<= 1000 bytes) ↑"
        echo ""
    fi
done
echo "-------------------------------"

############################

# RESULTADO FINAL
echo ""
echo "----------------------------------------------------"
if [ "$ERROR" -eq 0 ]; then
    echo "✅ Validación completa: todo OK :)"
    echo "----------------------------------------------------"
    exit 0
else
    echo "::error ::❌ Validación fallida: se encontraron $ERROR errores :'("
    echo "----------------------------------------------------"
    exit 1
fi