#!/bin/bash

# Esperar a que el kernel esté escuchando en el puerto 8001
echo -e "\033[38;2;179;236;111mEsperando que el kernel esté disponible en el puerto 8001...\033[0m"
while ! nc -z localhost 8001; do
  sleep 0.5
done

echo -e "\033[38;2;179;236;111mIniciando CPU...\033[0m"
./cpu/bin/cpu 1 > cpu/cpu.log 2>&1 &

echo -e "\033[38;2;179;236;111mIniciando dispositivos IO...\033[0m"
./io/bin/io DISCO > io/io.log 2>&1 &
