/*
 * main.c - daemon entry point.
 *
 * Installs the signal handlers, initialises the context against the real
 * platform ops and enters the lifecycle sequence. Nothing here should block:
 * thread creation and device waits belong to lifecycle.c.
 */

#include "kpoc.h"

int main(void)
{
    KpocCtx ctx;

    if (kpoc_ctx_init(&ctx, kpoc_ops_real(), kpoc_threads_real()) != 0)
        return 1;
    kpoc_main_sequence(&ctx);
    kpoc_ctx_deinit(&ctx);                                                         
    return 0;
}
