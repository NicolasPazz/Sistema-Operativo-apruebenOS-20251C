# 📦 Dependencias
- [so-commons-library]

## 🎯 Compilación y ejecución

Cada módulo del proyecto se compila de forma independiente a través de un
archivo `makefile`. Para compilar un módulo, es necesario ejecutar el comando
`make` desde la carpeta correspondiente.

El ejecutable resultante de la compilación se guardará en la carpeta `bin` del
módulo. Ejemplo:

```sh
cd kernel
make
./bin/kernel
```

> [!NOTE]
> Tambien se puede compilar todo usando directamente el Makefile principal

## 🏁 Checkpoints

Para cada checkpoint de control obligatorio, se debe crear un tag en el
repositorio con el siguiente formato:

```
checkpoint-{número}
```

Donde `{número}` es el número del checkpoint, ejemplo: `checkpoint-1`.

Para crear un tag y subirlo al repositorio, podemos utilizar los siguientes
comandos:

```bash
git tag -a checkpoint-{número} -m "Checkpoint {número}"
git push origin checkpoint-{número}
```

> [!WARNING]
> Asegúrense de que el código compila y cumple con los requisitos del checkpoint
> antes de subir el tag.

## ✔ Entrega

Para desplegar el proyecto en una máquina Ubuntu Server, podemos utilizar el
script [so-deploy] de la cátedra:

```bash
git clone https://github.com/sisoputnfrba/so-deploy.git
cd so-deploy
./deploy.sh -r=release -p=utils -p=kernel -p=cpu -p=memoria -p=io "tp-{año}-{cuatri}-{grupo}"
```

El mismo se encargará de instalar las Commons, clonar el repositorio del grupo
y compilar el proyecto en la máquina remota.

> [!NOTE]
> Ante cualquier duda, pueden consultar la documentación en el repositorio de
> [so-deploy], o utilizar el comando `./deploy.sh --help`.