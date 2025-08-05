# 💻 TP 2025 1C - apruebenOS

Simulación de un sistema operativo distribuido, desarrollado para la materia Sistemas Operativos (UTN-FRBA). El proyecto implementa los conceptos de planificación de procesos, administración de memoria, manejo de dispositivos de entrada/salida y comunicación entre módulos, todo bajo una arquitectura modular y concurrente.

---

## 👥 Integrantes

| Módulo  | Responsable(s) |
|---------|----------------|
| **Kernel** | [@NicolasPazz](https://github.com/NicolasPazz) |
| **Memoria** | [@matefernandezcc](https://github.com/matefernandezcc), [@Agustoledo10](https://github.com/Agustoledo10) |
| **CPU** | [@elucangioli](https://github.com/elucangioli), [@lucasmartin2004](https://github.com/lucasmartin2004) |
| **IO** | Todos los integrantes |

---

## 🧩 Descripción general de los módulos

### 🟦 Kernel
El corazón del sistema. Se encarga de la planificación de procesos (largo, mediano y corto plazo), la gestión de los estados de los procesos (NEW, READY, EXEC, BLOCKED, SUSPENDED, EXIT), la coordinación con la memoria, la asignación de CPUs y la administración de dispositivos de IO. Orquesta la ejecución de los procesos y resuelve las syscalls provenientes de la CPU.

### 🟩 Memoria
Simula la memoria principal y el espacio de swap. Implementa paginación multinivel, administra la asignación y liberación de marcos, responde a pedidos de lectura/escritura y gestiona la suspensión y reanudación de procesos. Lleva métricas detalladas de uso y permite realizar dumps del estado de la memoria.

### 🟧 CPU
Simula el ciclo de instrucción de una CPU real: fetch, decode, execute y check interrupt. Interpreta instrucciones, traduce direcciones lógicas a físicas (MMU), implementa TLB y caché de páginas, y ejecuta syscalls que requieren intervención del Kernel o Memoria. Puede ser instanciada múltiples veces para simular multiprocesamiento.

### 🟨 IO
Simula dispositivos de entrada/salida (por ejemplo, impresoras). Atiende solicitudes de IO enviadas por el Kernel, bloquea procesos durante el tiempo requerido y notifica al Kernel al finalizar la operación. Permite simular múltiples dispositivos con diferentes nombres.

---

## 🛠️ Uso del Makefile principal

El Makefile principal permite compilar, limpiar, ejecutar y administrar todos los módulos del proyecto de forma centralizada.

### Principales reglas disponibles

- **all**
  > Compila todos los módulos (`utils`, `io`, `memoria`, `cpu`, `kernel`).
  ```sh
  make all
  ```

- **run [LOG_LEVEL]**
  > Compila, limpia, setea el nivel de log y ejecuta todos los módulos. Levanta memoria, kernel (en foreground), y lanza CPU e IO en background. El LOG_LEVEL es opcional (por defecto TRACE).
  ```sh
  make run
  make run INFO   # Ejecuta con nivel de log INFO
  ```

- **stop**
  > Detiene todos los procesos de los módulos y libera los puertos utilizados (8000-8004).
  ```sh
  make stop
  ```

- **kernel / memoria / cpu / io**
  > Ejecuta individualmente el módulo correspondiente. Ejemplo:
  ```sh
  make kernel
  make memoria
  make cpu
  make io
  ```

- **set_log_level**
  > Cambia el nivel de log (`LOG_LEVEL`) en todos los archivos `.config` del proyecto.
  ```sh
  make set_log_level LOG_LEVEL=INFO
  ```

- **logs**
  > Limpia los códigos de color ANSI de todos los archivos `.log` (útil para ver logs sin colores en editores o herramientas que no los soportan).
  ```sh
  make logs
  ```

- **clean**
  > Elimina archivos de dump (`.dmp`) y limpia los archivos de compilación de todos los módulos.
  ```sh
  make clean
  ```

- **dos2unix**
  > Convierte los saltos de línea de los archivos `.config` a formato Unix (útil si se editaron en Windows).
  ```sh
  make dos2unix
  ```

---

## 🚀 Ejecución rápida

1. **Compilar todo:**
   ```sh
   make all
   ```
2. **Ejecutar todo (con logs limpios):**
   ```sh
   make run
   ```
3. **Detener todos los módulos:**
   ```sh
   make stop
   ```

---

## 📦 Dependencias
- [so-commons-library] (instalada automáticamente por el script de deploy de la cátedra)

---

## 📝 Notas
- Cada módulo puede compilarse y ejecutarse de forma independiente desde su carpeta.
- Los logs de cada módulo se encuentran en su respectiva carpeta (`*.log`).
- Los archivos de configuración (`*.config`) permiten ajustar parámetros de cada módulo.
- Para más detalles sobre la arquitectura y consignas, consultar el enunciado oficial de la materia.

---

## ✔ Entrega y despliegue

Para desplegar el proyecto en una máquina Ubuntu Server, se recomienda utilizar el script [so-deploy] de la cátedra:

```sh
git clone https://github.com/sisoputnfrba/so-deploy.git
cd so-deploy
./deploy.sh -r=release -p=utils -p=kernel -p=cpu -p=memoria -p=io "tp-{año}-{cuatri}-{grupo}"
```

El script instalará las Commons, clonará el repo y compilará el proyecto en la máquina remota.

---

> Ante cualquier duda, consultar la documentación oficial de la cátedra o el foro de la materia.
