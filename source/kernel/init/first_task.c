#include "core/task.h"
#include "tools/log.h"

int first_task_main(void)
{
    while (1)
    {
        log_printf("first task .");
        sys_sleep(1000);
    }
    return 0;
}