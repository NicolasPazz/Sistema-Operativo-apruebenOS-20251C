# /////////////////////// Compilar todos los módulos ///////////////////////
all:
	make -C ./utils
	make -C ./io
	make -C ./memoria
	make -C ./cpu
	make -C ./kernel

# /////////////////////// Ejecutar módulos desde el Makefile ///////////////////////
.PHONY: kernel
kernel:
	./kernel/bin/kernel
.PHONY: memoria
memoria:
	./memoria/bin/memoria
.PHONY: cpu
cpu:
	./cpu/bin/cpu
.PHONY: io
io:
	./io/bin/io

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