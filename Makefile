# Variable default
LOG_LEVEL := INFO

# Captura primeros goals
FIRST_GOAL := $(firstword $(MAKECMDGOALS))
SECOND_GOAL := $(word 2, $(MAKECMDGOALS))

# Si el primer goal es run y hay segundo goal, usamos ese para LOG_LEVEL
ifeq ($(FIRST_GOAL),run)
  ifneq ($(SECOND_GOAL),)
    LOG_LEVEL := $(SECOND_GOAL)
  endif
endif

# Evitar que make falle buscando el segundo goal como target
.PHONY: $(SECOND_GOAL)
$(SECOND_GOAL):

# /////////////////////// Compilar todos los módulos ///////////////////////
all:
	make -C ./utils
	make -C ./io
	make -C ./memoria
	make -C ./cpu
	make -C ./kernel

# /////////////////////// Regla para setear LOG_LEVEL en todos los .config ////
.PHONY: set_log_level
set_log_level:
	@echo "Seteando LOG_LEVEL=$(LOG_LEVEL) en todos los archivos .config"
	@find . -type f -name "*.config" -exec sed -i -E "s/^LOG_LEVEL=.*/LOG_LEVEL=$(LOG_LEVEL)/" {} +

# /////////////////////// Regla para limpiar ANSI codes de los logs ////
.PHONY: logs
logs:
	@echo "Eliminando códigos ANSI de los archivos .log..."
	@find . -type f -name "*.log" -exec sed -i -r "s/\x1B\[[0-9;]*[mK]//g" {} +
	@echo "Filtrando solo líneas [INFO] en los archivos .log..."
	@find . -type f -name "*.log" -exec sed -i -n '/\[INFO\]/p' {} +

# /////////////////////// Ejecutar módulos ///////////////////////
.PHONY: run
run: set_log_level clean all
	@echo "Limpiando logs..."
	@find . -type f -name "*.log" -exec rm -f {} +

	@echo -e "\033[38;2;179;236;111mIniciando memoria...\033[38;2;179;236;111m"
	@./memoria/bin/memoria > memoria/memoria.log 2>&1 &
	@sleep 1

	@echo -e "\033[38;2;179;236;111mIniciando kernel (en primer plano)...\033[38;2;179;236;111m"
	@sleep 1
	@bash levantar_modulos.sh &   # Levanta CPU e IO en background
	@sleep 1
	@./kernel/bin/kernel PLANI_LYM_PLAZO 0

	@echo "Corrigiendo logs luego de que kernel terminó..."
	@$(MAKE) logs

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
	./kernel/bin/kernel proceso_inicial 128

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
