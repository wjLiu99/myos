#include"init.h"
#include"comm/boot_info.h"
int test(int a,int b){return a+b;}
void kernel_init(boot_info_t *boot_info){
    int a=1,b=2,c=10,d=5;
    test(a,b);
    for(; ;);

}