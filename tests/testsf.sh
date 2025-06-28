#!/bin/bash
set -e

# Cambiar al directorio raíz del proyecto
cd "$(dirname "$0")/.."

# Limitar RAM a 512 MB por proceso
ulimit -v $((512 * 1024))

# Matar procesos colgados de ejecuciones anteriores
fuser -k 8001/tcp 2>/dev/null || true
fuser -k 8002/tcp 2>/dev/null || true
fuser -k 8003/tcp 2>/dev/null || true
fuser -k 8004/tcp 2>/dev/null || true

# Compilar
make clean
make
set +e

# Limpiar logs previos
rm -f memoria/memoria.log kernel/kernel.log cpu/cpu.log io/io.log

############################
# INICIAR MEMORIA
./memoria/bin/memoria &
PID_MEMORIA=$!
sleep 10 

# INICIAR KERNEL
./kernel/bin/kernel PROCESO_INICIAL 128 --action &
PID_KERNEL=$!
sleep 10 

# INICIAR CPU
./cpu/bin/cpu 1 &
PID_CPU1=$!
sleep 10 

# INICIAR IO
./io/bin/io teclado &
PID_IO1=$!
sleep 10 

############################
# ESPERAR FINALIZACIÓN O TIMEOUT
( sleep 600 && echo "⏱ Timeout alcanzado" && kill $PID_MEMORIA $PID_KERNEL $PID_CPU1 $PID_IO1 2>/dev/null ) &
WAITER_PID=$!

wait $PID_MEMORIA; EXIT_MEMORIA=$?
wait $PID_KERNEL;  EXIT_KERNEL=$?
wait $PID_CPU1;    EXIT_CPU=$?
wait $PID_IO1;     EXIT_IO=$?

( kill $WAITER_PID 2>/dev/null )

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