# /////////////////////// Compilar todos los módulos ///////////////////////
all:
	make -C ./utils
	make -C ./io
	make -C ./memoria
	make -C ./cpu
	make -C ./kernel

# /////////////////////// Ejecutar módulos desde el Makefile ///////////////////////
.PHONY: run
run:
	@mkdir -p logs
	@echo "Iniciando memoria..."
	@./memoria/bin/memoria > logs/memoria.log 2>&1 &

	@echo "Iniciando cpu..."
	@./cpu/bin/cpu CPU1 > logs/cpu.log 2>&1 &

	@echo "Iniciando io..."
	@./io/bin/io IMPRESORA > logs/io.log 2>&1 &

	@echo "Iniciando kernel..."
	@./kernel/bin/kernel PROCESO_INICIAL 128

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
