#include "Region.h"
#include "glob_constants.h"
#include "VersionSpinLock.h"
#include <stdlib.h>
#include <string.h>


Region::Region(size_t size, size_t align) : size(size), align(align) {
    if(posix_memalign(&start, align, size) != 0) {
        throw std::bad_alloc();
    }

    // Initialize versioned write spinlocks
    for (int i = 0; i < LOCK_ARRAY_SIZE; i++) {
        versionSpinLock_init(&locks[i]);
    }

    // Initialize the region global version clock
    memset(start, 0, size);
    clock.store(0);
    allocs = nullptr;
}

Region::~Region() {
    while (allocs) { // Free allocated segments
        segment_list tail = allocs->next;
        free(allocs);
        allocs = tail;
    }

    free(start);
}