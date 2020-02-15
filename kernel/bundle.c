#include "bundle.h"
#include "alloc.h"
#include "paging.h"

struct bundle* bundle;

void locate_bundle()
{
    unsigned int page = pop_page_high();
    struct bundle* b = (struct bundle*)page;
    alloc_addr_high = (page + b->size + 4095) & ~4095u;
    bundle = (struct bundle*)LINEAR(page);
}
