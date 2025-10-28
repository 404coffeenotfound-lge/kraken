#include "kraken/kernel.h"
#include "esp_heap_caps.h"

void *kraken_malloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

void *kraken_calloc(size_t nmemb, size_t size)
{
    return heap_caps_calloc(nmemb, size, MALLOC_CAP_8BIT);
}

void *kraken_realloc(void *ptr, size_t size)
{
    return heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT);
}

void kraken_free(void *ptr)
{
    heap_caps_free(ptr);
}

size_t kraken_get_free_heap_size(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

size_t kraken_get_minimum_free_heap_size(void)
{
    return heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
}
