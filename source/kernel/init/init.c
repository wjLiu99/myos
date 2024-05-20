#include"init.h"
#include"cpu/cpu.h"
#include"cpu/irq.h"
#include"dev/time.h"
#include"tools/log.h"
#include"comm/boot_info.h"
#include"os_conf.h"

void kernel_init(boot_info_t *boot_info){
    cpu_init();
    log_init();
    irq_init();
    time_init();
}

void init_main(void){
    log_printf("kernel is running...");
    log_printf("Version: %s",OS_VERSION);
    log_printf("%d %d %x %c",123,-123,100,'a');
    //irq_enable_global();
    while(1){};
}