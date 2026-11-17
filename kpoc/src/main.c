  
                                                                   
                             
   
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
