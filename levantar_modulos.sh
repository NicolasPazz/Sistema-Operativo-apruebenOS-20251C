#!/bin/bash

# Esperar a que el kernel esté escuchando en el puerto 8001
echo "Esperando que el kernel esté disponible en el puerto 8001..."
while ! nc -z localhost 8001; do
  sleep 0.5
done

echo "Iniciando CPU..."
./cpu/bin/cpu CPU1 > cpu/cpu.log 2>&1 &

echo "Iniciando IO..."
./io/bin/io IMPRESORA > io/io.log 2>&1 &