# /////////////////////// Compilar todos los módulos ///////////////////////
all:
	make -C ./utils
	make -C ./io
	make -C ./memoria
	make -C ./cpu
	make -C ./kernel

# /////////////////////// Ejecutar módulos ///////////////////////
.PHONY: run
run: clean all
	@echo "Limpiando logs..."
	@find . -type f -name "*.log" -exec rm -f {} +

	@echo -e "\033[38;2;179;236;111mIniciando memoria...\033[38;2;179;236;111m"
	@./memoria/bin/memoria > memoria/memoria.log 2>&1 &

	@sleep 1

	@echo -e "\033[38;2;179;236;111mIniciando kernel (en primer plano)...\033[38;2;179;236;111m"
	@bash levantar_modulos.sh &   # Levanta CPU e IO en background
	@./kernel/bin/kernel PROCESO_INICIAL 128

# /////////////////////// Limpiar ANSI codes de los logs ///////////////////////
.PHONY: logs
logs:
	@echo "Eliminando códigos ANSI de los logs..."
	@find . -type f -name "*.log" -exec sed -i 's/\x1B\[[0-9;]*[0-9;]*m//g' {} +
	@echo "Listo. Logs limpios."

# /////////////////////// Detener todos los módulos ///////////////////////
.PHONY: stop
stop:
	@echo "Deteniendo procesos por nombre..."
	@pkill -f ./memoria/bin/memoria || true
	@pkill -f ./cpu/bin/cpu || true
	@pkill -f ./io/bin/io || true
	@pkill -f ./kernel/bin/kernel || true

	@echo "Forzando cierre de puertos usados (8000-8004)..."
	@for port in 8000 8001 8002 8003 8004; do \
		fuser -k $$port/tcp 2>/dev/null || true; \
	done

	@echo "Todos los procesos y puertos fueron liberados."

# /////////////////////// Ejecutar módulos individualmente ///////////////////////
.PHONY: kernel
kernel:
	./kernel/bin/kernel PROCESO_INICIAL 128

.PHONY: memoria
memoria:
	./memoria/bin/memoria

.PHONY: cpu
cpu:
	./cpu/bin/cpu CPU1

.PHONY: io
io:
	./io/bin/io IMPRESORA

# /////////////////////// Formatear saltos de línea: dos -> unix ///////////////////////
dos2unix:
	find . -type f -name "*.config" -exec dos2unix {} +

# /////////////////////// Eliminar archivos de compilación ///////////////////////
clean:
	@echo "Limpiando archivos de dump (.dmp)..."
	@find . -type f -name "*.dmp" -exec rm -f {} + || true
	make clean -C ./utils
	make clean -C ./io
	make clean -C ./memoria
	make clean -C ./cpu
	make clean -C ./kernel