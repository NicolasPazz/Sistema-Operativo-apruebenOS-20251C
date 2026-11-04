# Distributed Operating System Simulation (apruebenOS)

Simulation of a distributed operating system written in C for Linux, built by a team of five for the Operating Systems course at UTN FRBA (2025). Four modules run as separate processes and talk to each other over sockets, using POSIX threads with semaphores and mutexes for concurrency.

## Team

| Module | Owner |
| --- | --- |
| Kernel | [@NicolasPazz](https://github.com/NicolasPazz) |
| Memory | [@matefernandezcc](https://github.com/matefernandezcc), [@Agustoledo10](https://github.com/Agustoledo10) |
| CPU | [@elucangioli](https://github.com/elucangioli), [@lucasmartin2004](https://github.com/lucasmartin2004) |
| IO | Whole team |

## Modules

- **Kernel:** long, medium and short-term scheduling; process states (NEW, READY, EXEC, BLOCKED, SUSPENDED, EXIT); CPU assignment, IO device management and syscalls coming from the CPU.
  - Short-term scheduling: FIFO, SJF and SRT (preemptive).
  - Admission to READY: FIFO and smallest-process-first (PMCP).
- **Memory:** main memory and swap with multi-level paging, frame allocation, process suspension and resumption, usage metrics and memory dumps.
- **CPU:** fetch–decode–execute–interrupt cycle, MMU address translation, TLB (FIFO/LRU) and page cache (CLOCK/CLOCK-M). Several CPU instances can run at once.
- **IO:** simulated devices that block processes for the requested time and notify the kernel when done.

## Build and run

Requires Linux, `gcc`, `make` and the course's [so-commons-library](https://github.com/sisoputnfrba/so-commons-library).

```bash
make all     # build utils, io, memory, cpu and kernel
make run     # start every module (optional log level: make run INFO)
make stop    # stop every module and free ports 8000-8004
```

Each module can also be built and started from its own folder or with `make kernel`, `make memoria`, `make cpu`, `make io`. Parameters are set in each module's `.config` file.

## Tests

`pruebas/` contains the course's test scenarios (scheduling, memory, TLB, IO and stability). `ejecutar_test.sh N` runs scenario `N` (1–13) and checks that every module exits successfully without errors or warnings in the logs. The same scenarios run on GitHub Actions.

```bash
./ejecutar_test.sh 1
```

## Deployment

The project is deployed on Ubuntu Server with the course's [so-deploy](https://github.com/sisoputnfrba/so-deploy) script, which installs the commons library and builds every module.
