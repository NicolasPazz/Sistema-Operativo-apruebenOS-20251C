#!/bin/bash
set -e

# Limitar RAM a 512 MB por proceso
ulimit -v $((512 * 1024))

# Matar procesos colgados de ejecuciones anteriores
fuser -k 8001/tcp 2>/dev/null || true
fuser -k 8002/tcp 2>/dev/null || true
fuser -k 8003/tcp 2>/dev/null || true
fuser -k 8004/tcp 2>/dev/null || true
pkill -f valgrind || true

# Compilar
make clean
make

# Limpiar logs previos
rm -f memoria/memoria.valgrind kernel/kernel.valgrind cpu/cpu.valgrind io/io.valgrind
rm -f memoria/memoria.log kernel/kernel.log cpu/cpu.log io/io.log

############################
# INICIAR MEMORIA
cd memoria
valgrind --leak-check=full --log-file=memoria.valgrind ./bin/memoria &
PID_MEMORIA=$!
cd ..
timeout 30 bash -c "tail -Fn0 memoria/memoria.log | grep -q 'Servidor de memoria iniciado correctamente. Esperando conexiones...'"

# INICIAR KERNEL
cd kernel
valgrind --leak-check=full --log-file=kernel.valgrind ./bin/kernel script/proceso_inicial.pseudo 128 --action &
PID_KERNEL=$!
cd ..
timeout 30 bash -c "tail -Fn0 kernel/kernel.log | grep -q 'Servidor Kernel IO escuchando en puerto 8003'"

# INICIAR CPU
cd cpu
valgrind --leak-check=full --log-file=cpu.valgrind ./bin/cpu 1 &
PID_CPU1=$!
cd ..
timeout 30 bash -c "tail -Fn0 cpu/cpu.log | grep -q 'HANDSHAKE_MEMORIA_CPU: CPU conectado exitosamente a Memoria'"

# INICIAR IO
cd io
valgrind --leak-check=full --log-file=io.valgrind ./bin/io teclado &
PID_IO1=$!
cd ..
timeout 30 bash -c "tail -Fn0 io/io.log | grep -q 'HANDSHAKE_IO_KERNEL: IO conectado exitosamente a Kernel'"

############################
# ESPERAR FINALIZACIÓN O TIMEOUT
( sleep 600 && echo "⏱ Timeout alcanzado" && kill $PID_MEMORIA $PID_KERNEL $PID_CPU1 $PID_IO1 2>/dev/null ) &
WAITER_PID=$!

wait $PID_MEMORIA; EXIT_MEMORIA=$?
wait $PID_KERNEL;  EXIT_KERNEL=$?
wait $PID_CPU1;    EXIT_CPU=$?
wait $PID_IO1;     EXIT_IO=$?

( kill $WAITER_PID 2>/dev/null )
set +e

############################
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
else
    echo "❌ Memoria: $EXIT_MEMORIA   ✗"
fi
if [ "$EXIT_KERNEL" -eq 0 ]; then
    echo "🧩 Kernel : $EXIT_KERNEL   ✓"
else
    echo "❌ Kernel : $EXIT_KERNEL   ✗"
fi
if [ "$EXIT_CPU" -eq 0 ]; then
    echo "🖥  CPU    : $EXIT_CPU   ✓"
else
    echo "❌ CPU    : $EXIT_CPU   ✗"
fi
if [ "$EXIT_IO" -eq 0 ]; then
    echo "⌨️  IO     : $EXIT_IO   ✓"
else
    echo "❌ IO     : $EXIT_IO   ✗"
fi
echo "------------------"

# VALIDAR ERRORES
ERROR=0

echo ""
echo "-------------------------------"
echo "Validando Segmentation fault..."
for val in memoria/memoria.valgrind kernel/kernel.valgrind cpu/cpu.valgrind io/io.valgrind; do
    if grep -q "Segmentation fault" "$val"; then
        grep -E "Segmentation fault" "$val"
        echo "::error ::❌ Segmentation fault en $val ↑"
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
        ((ERROR++))
    elif [[ "$def_num" -gt 0 || "$indir_num" -gt 0 ]]; then
        grep -E "definitely lost|indirectly lost" "$val"
        echo "::warning ::Leak menor detectado en $val (<= 1000 bytes) ↑"
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