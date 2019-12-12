#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <libconfig.h>
#include "read_cfg.h"
int main()
{
    srslte_config_t main_config;
    read_config_master(&main_config);
    return 0;
}
