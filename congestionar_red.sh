#!/bin/bash
# chmod +x congestionar_red.sh; ./congestionar_red.sh

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Función para aplicar congestión
aplicar_congestion() {
    local delay_base=$1
    local delay_variation=$2
    
    echo -e "${BLUE}🚨 Aplicando congestión de red...${NC}"
    echo -e "   Latencia: ${delay_base}ms ± ${delay_variation}ms (rango: $((delay_base-delay_variation))-$((delay_base+delay_variation))ms)"
    echo -e "   Puertos afectados: 8001, 8002, 8003, 8004"
    
    # Limpiar reglas existentes
    sudo tc qdisc del dev lo root 2>/dev/null || true
    
    # Crear qdisc principal con 4 bandas
    sudo tc qdisc add dev lo root handle 1: prio bands 4
    
    # Agregar congestión en banda 4
    if sudo tc qdisc add dev lo parent 1:4 handle 40: netem delay ${delay_base}ms ${delay_variation}ms distribution normal; then
        echo -e "${GREEN}✅ Regla netem aplicada correctamente${NC}"
    else
        echo -e "${RED}❌ Error al aplicar regla netem${NC}"
        return 1
    fi
    
    # Filtros para puertos 8001-8004 (source y destination)
    local filtros_ok=0
    for p in 8001 8002 8003 8004; do
        if sudo tc filter add dev lo protocol ip parent 1:0 prio 1 u32 \
             match ip sport $p 0xffff flowid 1:4 2>/dev/null && \
           sudo tc filter add dev lo protocol ip parent 1:0 prio 1 u32 \
             match ip dport $p 0xffff flowid 1:4 2>/dev/null; then
            ((filtros_ok++))
        fi
    done
    
    echo -e "${GREEN}✅ Congestión aplicada a puertos 8001-8004 ($filtros_ok/4 puertos configurados)${NC}"
    
    # Validar configuración
    validar_congestion
}

# Función para validar que la congestión esté funcionando
validar_congestion() {
    echo -e "\n${BLUE}📊 VALIDANDO CONFIGURACIÓN:${NC}"
    
    # Verificar reglas tc
    echo -e "${YELLOW}🔍 Reglas tc activas:${NC}"
    local qdisc_output=$(tc qdisc show dev lo)
    if echo "$qdisc_output" | grep -q "netem"; then
        echo "$qdisc_output" | grep -E "(prio|netem)"
        echo -e "${GREEN}✅ Reglas netem encontradas${NC}"
    else
        echo -e "${RED}❌ No se encontraron reglas netem${NC}"
        return 1
    fi
    
    # Verificar filtros
    echo -e "\n${YELLOW}🔍 Filtros activos:${NC}"
    local filtros_count=$(tc filter show dev lo | grep -c "flowid 1:4" 2>/dev/null || echo "0")
    echo "Filtros configurados: $filtros_count/8 (4 puertos x 2 direcciones)"
    
    if [ "$filtros_count" -eq 8 ]; then
        echo -e "${GREEN}✅ Todos los filtros están configurados correctamente${NC}"
    elif [ "$filtros_count" -gt 0 ]; then
        echo -e "${YELLOW}⚠️ Solo $filtros_count filtros configurados (esperados: 8)${NC}"
    else
        echo -e "${RED}❌ No hay filtros configurados${NC}"
    fi
    
    # Probar conectividad TCP
    echo -e "\n${YELLOW}🔍 Probando puertos TCP:${NC}"
    for p in 8001 8002 8003 8004; do
        if timeout 2 nc -z localhost $p 2>/dev/null; then
            echo -e "Puerto $p: ${GREEN}✅ Accesible${NC}"
        else
            echo -e "Puerto $p: ${YELLOW}⚠️ Cerrado (normal si no hay servicio)${NC}"
        fi
    done
    
    echo -e "\n${BLUE}ℹ️ NOTAS:${NC}"
    echo "• La congestión solo afecta a los puertos TCP 8001-8004"
    echo "• El ping a localhost NO mostrará latencia (no usa estos puertos)"
    echo "• Para ver el efecto, ejecuta tu programa que use estos puertos"
    echo -e "• Monitorear tráfico: ${YELLOW}sudo tcpdump -i lo port 8001 or port 8002 or port 8003 or port 8004${NC}"
}

# Función para limpiar congestión
limpiar_congestion() {
    echo -e "${BLUE}🧹 Limpiando TODAS las reglas de congestión...${NC}"
    
    # Mostrar reglas actuales antes de limpiar
    local reglas_antes=$(tc qdisc show dev lo | wc -l)
    if [ "$reglas_antes" -gt 1 ]; then
        echo -e "${YELLOW}🔍 Reglas actuales antes de limpiar:${NC}"
        tc qdisc show dev lo
    fi
    
    # Limpiar todas las reglas
    if sudo tc qdisc del dev lo root 2>/dev/null; then
        echo -e "${GREEN}✅ Reglas tc eliminadas correctamente${NC}"
    else
        echo -e "${YELLOW}⚠️ No había reglas tc que eliminar${NC}"
    fi
    
    # Verificar limpieza
    echo -e "\n${YELLOW}🔍 Verificando limpieza:${NC}"
    local reglas_despues=$(tc qdisc show dev lo)
    if echo "$reglas_despues" | grep -q "noqueue\|fq_codel"; then
        echo -e "${GREEN}✅ Interfaz limpia - solo reglas por defecto${NC}"
        echo "$reglas_despues"
    else
        echo -e "${RED}❌ Aún hay reglas personalizadas:${NC}"
        echo "$reglas_despues"
    fi
}

# Función para leer número entero con validación
leer_numero() {
    local prompt="$1"
    local min_val="$2"
    local max_val="$3"
    local numero
    
    while true; do
        read -p "$prompt" numero
        if [[ "$numero" =~ ^[0-9]+$ ]] && [ "$numero" -ge "$min_val" ] && [ "$numero" -le "$max_val" ]; then
            echo "$numero"
            return 0
        else
            echo -e "${RED}❌ Error: Ingresa un número entero entre $min_val y $max_val${NC}"
        fi
    done
}

# Función principal
main() {
    echo -e "${BLUE}╔═══════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║     SIMULADOR DE CONGESTIÓN DE RED    ║${NC}"
    echo -e "${BLUE}║           Puertos: 8001-8004          ║${NC}"
    echo -e "${BLUE}╚═══════════════════════════════════════╝${NC}\n"
    
    echo "Selecciona una opción:"
    echo "1) 🚨 Aplicar congestión de red"
    echo "2) 🧹 Limpiar congestión de red"
    echo -e "3) 📊 Solo validar configuración actual"
    echo "4) ❌ Salir"
    
    local opcion
    read -p "Opción [1-4]: " opcion
    
    case $opcion in
        1)
            echo -e "\n${YELLOW}📝 Configuración de latencia:${NC}"
            echo "La latencia final será: LATENCIA_BASE ± VARIACIÓN"
            echo "Ejemplo: 20ms ± 10ms = rango de 10ms a 30ms"
            echo ""
            
            local latencia_base=$(leer_numero "Latencia base (ms) [1-1000]: " 1 1000)
            local variacion=$(leer_numero "Variación (ms) [0-$latencia_base]: " 0 $latencia_base)
            
            echo ""
            aplicar_congestion "$latencia_base" "$variacion"
            ;;
        2)
            echo ""
            limpiar_congestion
            ;;
        3)
            echo ""
            if tc qdisc show dev lo | grep -q "netem"; then
                validar_congestion
            else
                echo -e "${YELLOW}ℹ️ No hay congestión configurada actualmente${NC}"
                tc qdisc show dev lo
            fi
            ;;
        4)
            echo -e "${BLUE}👋 ¡Hasta luego!${NC}"
            exit 0
            ;;
        *)
            echo -e "${RED}❌ Opción inválida. Usa 1, 2, 3 o 4.${NC}"
            exit 1
            ;;
    esac
    
    echo -e "\n${GREEN}✨ Operación completada${NC}"
}

# Verificar que el usuario tenga permisos sudo
if ! sudo -n true 2>/dev/null; then
    echo -e "${YELLOW}🔐 Este script requiere permisos sudo para configurar reglas de red${NC}"
    echo "Ingresa tu contraseña cuando se solicite..."
    sudo true || exit 1
fi

# Ejecutar función principal
main