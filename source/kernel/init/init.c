#include"init.h"
#include"cpu/cpu.h"

#include"comm/boot_info.h"

void kernel_init(boot_info_t *boot_info){
    cpu_init();
}
void init_main(void){
    while(1){};
}