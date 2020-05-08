#include "main.h"
#include "fs.h"
#include "elf.h"
#include "userspace.h"
#include "alloc.h"
#include "paging.h"
#include "vmem.h"
#include "runtime.h"
#include "sys_fs.h"
#include "debugout.h"
#include "bundle.h"

extern char userspace_test[];
extern unsigned int userspace_test_len;

int main()
{
    struct file* executable = bundle->tests[0].fs;
    struct fd f;
    if(fs_open(&f, executable, bundle->executable, 0) < 0)
    {
        debug_puts("main: fs_open(\"test\") failed\n");
        return 1;
    }
    struct entry entry;
    const char* argv[] = {"./test", 0};
    const char* envp[] = {0};
    if(load_executable(executable, &f, &entry, argv[0], argv, envp) < 0)
    {
        debug_puts("load_executable(): failed to load ELF\n");
        return 1;
    }
    {
        set_ml(bundle->tests[0].ml);
        setup_entry_regs(&entry);
        reset_runtime();
        the_fs = bundle->tests[0].fs;
        if(fs_open(fds, the_fs, bundle->tests[0].input_txt, 0) < 0)
        {
            debug_puts("main: fs_open(input_txt) failed\n");
            return 1;
        }
        if(fs_open(fds+1, the_fs, bundle->tests[0].output_txt, 1) < 0)
        {
            debug_puts("main: fs_open(output_txt) failed\n");
            return 1;
        }
        int status = main_loop(bundle->tests[0].tl);
        debug_puts("main: pre-run exited with ");
        debug_putns(status, 10);
        debug_putc('\n');
        if(status != RESTART)
        {
            debug_puts("main: pre-run finished with non-RESTART status, no need to run tests\n");
            for(int i = 1; i <= bundle->ntests; i++)
                bundle->tests[i].outcome = status;
            return 0;
        }
    }
    save_runtime();
    cow_prepare();
    for(int iteration = 1; iteration <= bundle->ntests; iteration++)
    {
        clone_paging();
        restore_runtime();
        set_ml(bundle->tests[iteration].ml);
        the_fs = bundle->tests[iteration].fs;
        if(fs_open(fds, the_fs, bundle->tests[iteration].input_txt, 0) < 0)
        {
            debug_puts("main: fs_open(input_txt) failed\n");
            return 1;
        }
        if(fs_open(fds+1, the_fs, bundle->tests[iteration].output_txt, 1) < 0)
        {
            debug_puts("main: fs_open(output_txt) failed\n");
            return 1;
        }
        struct file* output_txt = fds[1].file;
        int status = main_loop(bundle->tests[iteration].tl);
        debug_puts("main: program exited with ");
        debug_putns(status, 10);
        debug_putc('\n');
        debug_puts("Output (size ");
        debug_putn(output_txt->sz, 10);
        debug_puts(")\n");
        char* data = (char*)output_txt->data;
        for(int i = 0; i < output_txt->sz; i++)
            debug_putc(data[i]);
        bundle->tests[iteration].outcome = status;
        pop_paging();
    }
    return 0;
}
