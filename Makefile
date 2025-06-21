# /////////////////////// Compilar todos los módulos ///////////////////////
all:
	make -C ./utils
	make -C ./io
	make -C ./memoria
	make -C ./cpu
	make -C ./kernel

# /////////////////////// Ejecutar módulos ///////////////////////
.PHONY: run
run:
	@echo "Iniciando memoria..."
	@./memoria/bin/memoria > memoria/memoria.log 2>&1 &

	@echo "Iniciando kernel..."
	@./kernel/bin/kernel PROCESO_INICIAL 128

	@echo "Iniciando cpu..."
	@./cpu/bin/cpu CPU1 > cpu/cpu.log 2>&1 &

	@echo "Iniciando io..."
	@./io/bin/io IMPRESORA > io/io.log 2>&1 &

# /////////////////////// Detener todos los módulos ///////////////////////
.PHONY: stop
stop:
	@echo "Deteniendo procesos..."
	@pkill -f ./memoria/bin/memoria || true
	@pkill -f ./cpu/bin/cpu || true
	@pkill -f ./io/bin/io || true
	@pkill -f ./kernel/bin/kernel || true
	@echo "Todos los procesos fueron detenidos."

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
	make clean -C ./utils
	make clean -C ./io
	make clean -C ./memoria
	make clean -C ./cpu
	make clean -C ./kernel
