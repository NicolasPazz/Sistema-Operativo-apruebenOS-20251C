#!/usr/bin/env bash
# chmod +x cambiar_ips.sh; ./cambiar_ips.sh

#──────────────────────────────────────────────────────
# 0) IP privada de la máquina donde se ejecuta
#──────────────────────────────────────────────────────
PRIVATE_IP=$(hostname -I 2>/dev/null | awk '{print $1}')
echo -e "IP privada del host: ${PRIVATE_IP:-desconocida}\n"

#──────────────────────────────────────────────────────
# 1) Recolectar valores actuales en todos los *.config
#──────────────────────────────────────────────────────
CONFIGS=($(find . -type f -iname "*.config"))
[[ ${#CONFIGS[@]} -eq 0 ]] && { echo "No se encontraron archivos .config"; exit 1; }

declare -A MEM_SET KER_SET LOG_SET
for cfg in "${CONFIGS[@]}"; do
    mem=$(grep -h "^IP_MEMORIA=" "$cfg" | cut -d= -f2-)
    ker=$(grep -h "^IP_KERNEL="  "$cfg" | cut -d= -f2-)
    log=$(grep -h "^LOG_LEVEL="  "$cfg" | cut -d= -f2-)
    [[ -n $mem ]] && MEM_SET["$mem"]=1
    [[ -n $ker ]] && KER_SET["$ker"]=1
    [[ -n $log ]] && LOG_SET["$log"]=1
done

echo "Valores actuales detectados:"
printf " · IP_MEMORIA: %s\n" "${!MEM_SET[@]}"
printf " · IP_KERNEL : %s\n" "${!KER_SET[@]}"
printf " · LOG_LEVEL : %s\n\n" "${!LOG_SET[@]}"

#──────────────────────────────────────────────────────
# 2) Pedir nuevos valores al usuario
#──────────────────────────────────────────────────────
read -rp "Nuevo IP_MEMORIA (Enter = mantener): " IP_MEMORIA_NEW
read -rp "Nuevo IP_KERNEL  (Enter = mantener): " IP_KERNEL_NEW
read -rp "Nuevo LOG_LEVEL  (Enter = mantener): " LOG_LEVEL_NEW

declare -A CAMBIOS
[[ -n $IP_MEMORIA_NEW ]] && CAMBIOS[IP_MEMORIA]=$IP_MEMORIA_NEW
[[ -n $IP_KERNEL_NEW  ]] && CAMBIOS[IP_KERNEL]=$IP_KERNEL_NEW
[[ -n $LOG_LEVEL_NEW  ]] && CAMBIOS[LOG_LEVEL]=$LOG_LEVEL_NEW

# Objetivos para la validación posterior
TARGET_MEM=${IP_MEMORIA_NEW:-${!MEM_SET[@]}}
TARGET_KER=${IP_KERNEL_NEW :-${!KER_SET[@]}}
TARGET_LOG=${LOG_LEVEL_NEW:-${!LOG_SET[@]}}

# Aplicar cambios
if [[ ${#CAMBIOS[@]} -gt 0 ]]; then
    echo -e "\nAplicando cambios..."
    for cfg in "${CONFIGS[@]}"; do
        for key in "${!CAMBIOS[@]}"; do
            val=${CAMBIOS[$key]}
            grep -q "^$key=" "$cfg" && sed -i "s|^$key=.*|$key=$val|" "$cfg"
        done
    done
else
    echo "Sin cambios. Continuando…"
fi

#──────────────────────────────────────────────────────
# 3) Compilar y limpiar pantalla
#──────────────────────────────────────────────────────
echo -e "\n> make clean && make\n"
make clean && make || { echo "❌ Compilación fallida"; exit 1; }

clear

#──────────────────────────────────────────────────────
# 4) Validar que todos los .config contengan los valores esperados
#──────────────────────────────────────────────────────
GREEN="\e[32m"; RED="\e[31m"; RESET="\e[0m"
OK_EMOJI="${GREEN}✔${RESET}"
ERR_EMOJI="${RED}✖${RESET}"

# volver a recolectar después de la edición
unset MEM_SET KER_SET LOG_SET
declare -A MEM_SET KER_SET LOG_SET
for cfg in "${CONFIGS[@]}"; do
    mem=$(grep -h "^IP_MEMORIA=" "$cfg" | cut -d= -f2-)
    ker=$(grep -h "^IP_KERNEL="  "$cfg" | cut -d= -f2-)
    log=$(grep -h "^LOG_LEVEL="  "$cfg" | cut -d= -f2-)
    [[ -n $mem ]] && MEM_SET["$mem"]=1
    [[ -n $ker ]] && KER_SET["$ker"]=1
    [[ -n $log ]] && LOG_SET["$log"]=1
done

OK=1
# 1) IP_MEMORIA
if [[ ${#MEM_SET[@]} -ne 1 ]]; then
    OK=0
elif [[ -n $IP_MEMORIA_NEW && ${!MEM_SET[@]} != "$IP_MEMORIA_NEW" ]]; then
    OK=0
fi
# 2) IP_KERNEL
if [[ ${#KER_SET[@]} -ne 1 ]]; then
    OK=0
elif [[ -n $IP_KERNEL_NEW && ${!KER_SET[@]} != "$IP_KERNEL_NEW" ]]; then
    OK=0
fi
# 3) LOG_LEVEL
if [[ ${#LOG_LEVEL_NEW[@]} -ne 1 ]]; then
    OK=0
elif [[ -n $LOG_LEVEL_NEW && ${!LOG_SET[@]} != "$LOG_LEVEL_NEW" ]]; then
    OK=0
fi

if (( OK )); then
    echo -e "${OK_EMOJI} Todos los valores coinciden:"
    echo -e "   → IP_MEMORIA : ${GREEN}${!MEM_SET[@]}${RESET}"
    echo -e "   → IP_KERNEL  : ${GREEN}${!KER_SET[@]}${RESET}"
    echo -e "   → LOG_LEVEL  : ${GREEN}${!LOG_SET[@]}${RESET}"
else
    echo -e "${ERR_EMOJI} Al menos un .config no coincide con los valores requeridos."
fi