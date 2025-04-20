#include "../headers/utils.h"


bool config_has_all_properties(t_config* cfg, char** properties) {
    for(uint8_t i = 0; properties[i] != NULL; i++) {
        if(!config_has_property(cfg, properties[i]))
            return false;
    }

    return true;
}