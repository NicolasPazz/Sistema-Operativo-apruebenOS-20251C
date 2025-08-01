#!/bin/bash
# chmod +x congestionar_red.sh ; ./congestionar_red.sh

#######################################
# COLORES
#######################################
RED='\033[0;31m'     ; GREEN='\033[0;32m'
BLUE='\033[0;34m'    ; YELLOW='\033[1;33m'
NC='\033[0m'         # Reset

#######################################
# APLICAR CONGESTIÓN
#  arg1 = delay_base  (ms)
#  arg2 = delay_jitter(ms)
#  arg3 = limit_pkts  (cola netem)
#  arg4 = rate_mbit   (0 => sin TBF)
#######################################
aplicar_congestion() {
    local delay_base=$1
    local delay_var=$2
    local limit_pkts=$3
    local rate_mbit=$4

    echo -e "${BLUE}🚨 Aplicando congestión...${NC}"
    echo -e "   Latencia: ${delay_base}ms ± ${delay_var}ms"
    echo -e "   Cola     : ${limit_pkts} paquetes"
    if [[ "$rate_mbit" -gt 0 ]]; then
        echo -e "   Rate TBF : ${rate_mbit} Mbit/s"
    else
        echo -e "   Rate TBF : (sin limitación)"
    fi
    echo -e "   Puertos  : 8001-8004\n"

    # 1) Limpio reglas previas
    sudo tc qdisc del dev lo root 2>/dev/null || true

    # 2) qdisc prio base
    sudo tc qdisc add dev lo root handle 1: prio bands 4

    # 3) Netem con cola parametrizable
    sudo tc qdisc add dev lo parent 1:4 handle 40: netem \
        delay ${delay_base}ms ${delay_var}ms distribution normal \
        limit ${limit_pkts} || {
            echo -e "${RED}❌ Error al aplicar netem${NC}"
            return 1
        }

    # 4) (Opcional) TBF
    if [[ "$rate_mbit" -gt 0 ]]; then
        sudo tc qdisc add dev lo parent 40:1 handle 400: tbf \
            rate ${rate_mbit}mbit burst 256kbit latency 400ms || {
                echo -e "${RED}❌ Error al aplicar TBF${NC}"
                return 1
            }
    fi

    # 5) Filtros para los puertos 8001-8004
    for p in 8001 8002 8003 8004; do
        sudo tc filter add dev lo protocol ip parent 1:0 prio 1 u32 \
            match ip sport $p 0xffff flowid 1:4
        sudo tc filter add dev lo protocol ip parent 1:0 prio 1 u32 \
            match ip dport $p 0xffff flowid 1:4
    done

    echo -e "${GREEN}✅ Reglas aplicadas correctamente${NC}"
    validar_congestion
}

#######################################
# VALIDAR CONFIGURACIÓN
#######################################
validar_congestion() {
    echo -e "\n${BLUE}📊 VALIDANDO CONFIGURACIÓN:${NC}"
    echo -e "${YELLOW}🔍 Reglas tc activas:${NC}"
    tc qdisc show dev lo | grep -E "(prio|netem|tbf)" || true

    echo -e "\n${YELLOW}🔍 Filtros activos:${NC}"
    local filtros=$(tc filter show dev lo | grep -c "flowid 1:4")
    echo -e "Filtros configurados: $filtros/8"
    [[ "$filtros" -eq 8 ]] && echo -e "${GREEN}✅ OK${NC}" \
                           || echo -e "${RED}⚠️ Falta(n) filtro(s)${NC}"

    echo -e "\n${BLUE}ℹ️ Recuerda: ping a localhost no mostrará latencia${NC}"
}

#######################################
# LIMPIAR
#######################################
limpiar_congestion() {
    echo -e "${BLUE}🧹 Limpiando reglas...${NC}"
    sudo tc qdisc del dev lo root 2>/dev/null && \
        echo -e "${GREEN}✅ Limpieza completada${NC}" || \
        echo -e "${YELLOW}ℹ️ No había reglas personalizadas${NC}"
}

#######################################
# INPUT VALIDADO (ENTERO)
#######################################
leer_numero() {
    local prompt="$1" min="$2" max="$3" n
    while true; do
        read -p "$prompt" n
        if [[ "$n" =~ ^[0-9]+$ ]] && [ "$n" -ge "$min" ] && [ "$n" -le "$max" ]; then
            echo "$n"; return 0
        fi
        echo -e "${RED}❌ Ingresa un valor entre $min y $max${NC}"
    done
}

#######################################
# MAIN
#######################################
main() {
echo -e "${BLUE}\n╔═════════ SIMULADOR DE CONGESTIÓN ═════════╗${NC}"
echo "1) Aplicar congestión"
echo "2) Limpiar congestión"
echo "3) Validar configuración actual"
echo "4) Salir"
read -p "Opción [1-4]: " op

case $op in
    1)
        echo -e "\n${YELLOW}📝 CONFIGURAR PARÁMETROS${NC}"
        base=$(leer_numero "Latencia base (ms)  [10-300]: " 10 300)
        var=$(leer_numero  "Jitter (ms)         [0-$base]: " 0 $base)
        limit=$(leer_numero "Tamaño cola pkts    [100-10000]: " 100 10000)
        rate=$(leer_numero  "Rate TBF Mbit (0 = sin TBF) [0-100]: " 0 100)
        aplicar_congestion "$base" "$var" "$limit" "$rate"
        ;;
    2) limpiar_congestion ;;
    3) validar_congestion ;;
    4) echo -e "${BLUE}👋 Bye${NC}"; exit 0 ;;
    *) echo -e "${RED}Opción inválida${NC}" ;;
esac
}

# Verificar sudo
sudo -n true 2>/dev/null || { echo -e "${YELLOW}Se solicitará sudo...${NC}"; sudo true; }

main