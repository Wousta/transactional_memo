//
// Created by luis on 12/2/24.
//

#ifndef CS453_2024_PROJECT_MASTER_VERSIONSPINLOCK_H
#define CS453_2024_PROJECT_MASTER_VERSIONSPINLOCK_H

#include <stdatomic.h>
#include <stdbool.h>
#include <atomic>

struct VersionSpinLock {
    std::atomic_int lock_state;
} ;

bool versionSpinLock_init(VersionSpinLock* lock);

bool versionSpinLock_acquire(VersionSpinLock* lock);

int versionSpinLock_get_state(VersionSpinLock* lock);

void versionSpinLock_release(VersionSpinLock* lock);

void versionSpinLock_set_and_release(VersionSpinLock* lock, int version);

#endif //CS453_2024_PROJECT_MASTER_VERSIONSPINLOCK_H
