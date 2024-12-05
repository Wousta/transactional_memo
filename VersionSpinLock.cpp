//
// Created by luis on 12/2/24.
//

#include "VersionSpinLock.h"


bool versionSpinLock_init(VersionSpinLock* lock) {
    lock->lock_state.store(0);
    return true;
}

bool versionSpinLock_acquire(VersionSpinLock* lock) {
    int state = lock->lock_state.load();

    // If the least significant bit is set, lock is taken
    if (state & 1) {
        return false;
    }

    // Try to acquire lock and set the least significant bit
    return lock->lock_state.compare_exchange_strong(state, state | 1);
}

int versionSpinLock_get_state(VersionSpinLock* lock) {
    return lock->lock_state.load();
}

void versionSpinLock_release(VersionSpinLock* lock) {
    // Unset the least significant bit to release the lock
    lock->lock_state.fetch_sub(1);
}

void versionSpinLock_set_and_release(VersionSpinLock* lock, int version) {
    // The version is shifted one bit to the left to avoid overriding the lock bit
    lock->lock_state.store(version << 1);
}