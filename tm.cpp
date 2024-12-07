/**
 * @file   tm.cpp
 * @author Luis Bustamante Martin-Ibanez
 *
 * @section LICENSE
 *
 * MIT License
 *
 * @section DESCRIPTION
 *
 * Implementation of TL2-like transactional memory.
 * Some of the comments that explain the code are
 * taken from the reference implementation paper.
 *
**/

// External headers
#include <iostream>
#include <cstdlib>
#include <mutex>
#include <atomic>
#include <stdint.h>
#include <memory>
#include <string.h>
#include <cstdlib>

// Internal headers
#include "tm.hpp"
#include "macros.h"
#include "Region.h"
#include "VersionSpinLock.h"
#include "glob_constants.h"
#include "Transaction.h"
#include "LinkedList.h"


/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) noexcept {
    try {
        return new Region(size, align);
    } catch (std::bad_alloc& e) {
        return invalid_shared;
    }

    return invalid_shared;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) noexcept {
    Region* region = static_cast<Region*>(shared);
    delete region;
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t shared) noexcept {
    return static_cast<Region*>(shared)->getStart();
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t shared) noexcept {
    return static_cast<Region*>(shared)->size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t shared) noexcept {
    return static_cast<Region*>(shared)->align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t shared, bool is_ro) noexcept {

    Region* region = static_cast<Region*>(shared);
    Transaction *transaction = new Transaction(is_ro,region->getClockVersion());

    return (tx_t) transaction;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t shared, tx_t tx) noexcept {

    Region* region = static_cast<Region*>(shared);
    Transaction *transaction = (Transaction *) tx;

    if(transaction->is_ro || transaction->writeList->getHead() == nullptr) {
        delete transaction;
        return true;
    }

    // Upper bound of concurrent accesses to locks to avoid starvation
    if(region->current_txs.load() > MAX_SIMUL_TXS) {
        delete transaction;
        return false;
    }

    // Increment the number of transactions
    region->current_txs.fetch_add(1);

    // Try to aquire all locks in the write-set, if any lock is already taken, abort the transaction
    Node *node = transaction->writeList->getHead();
    while(node) {
        int lock_index = ((uintptr_t) node->address) % LOCK_ARRAY_SIZE;
        if(!region->acquireSpinLock(lock_index)) {

            // Release the locks that were aquired
            Node *locked_node = transaction->writeList->getHead();
            while(locked_node && locked_node != node) {
                region->releaseSpinLock(((uintptr_t) locked_node->address) % LOCK_ARRAY_SIZE);
                locked_node = locked_node->next;
            }

            region->current_txs.fetch_sub(1);
            delete transaction;
            return false;
        }

        node = node->next;
    }

    // Increment the global version clock and store it in wv
    transaction->wv = region->incrementClockVersion();

    // validate for each location in the read-set that the
    // version number associated with the versioned-write-lock is <= rv. We also
    // verify that these memory locations have not been locked by other threads.
    node = transaction->readList->getHead();
    if(transaction->rv + 1 != transaction->wv) {
        while(node) {
            int lock_index = ((uintptr_t) node->address) % LOCK_ARRAY_SIZE;
            int lock_state = region->getSpinLockState(lock_index);

            if(lock_state >> 0x1 > transaction->rv || lock_state & 0x1) {
                // Release all the locks that were aquired
                Node *locked_node = transaction->writeList->getHead();
                while(locked_node) {
                    region->releaseSpinLock(((uintptr_t) locked_node->address) % LOCK_ARRAY_SIZE);
                    locked_node = locked_node->next;
                }

                region->current_txs.fetch_sub(1);
                delete transaction;
                return false;
            }

            node = node->next;
        }
    }

    // Commit and release the locks: For each location in the write-set, store
    // to the location the new value from the write-set and release the locations
    // lock by setting the version value to the write-version wv and clearing the
    // write-lock bit
    transaction_commit_and_release_locks(transaction, region->getSpinLocks(), region->align);

    region->current_txs.fetch_sub(1);
    delete transaction;
    return true;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t shared, tx_t tx, void const* source, size_t size, void* target) noexcept {

    Region* region = static_cast<Region*>(shared);
    Transaction *transaction = (Transaction *) tx;

    if(transaction->is_ro) {
        for(size_t i = 0; i < size; i += region->align) {
            uintptr_t source_word_add = (uintptr_t) source + i;
            uintptr_t target_word_add = (uintptr_t) target + i;
            int lock_index = ((uintptr_t) source + i) % LOCK_ARRAY_SIZE;

            // Speculative execution
            int pre_lock_status = region->getSpinLockState(lock_index);
            memcpy((void *) target_word_add, (void *) source_word_add, region->align);
            int post_lock_status = region->getSpinLockState(lock_index);

            // Check if the lock has changed, if the version is greater than the transaction version or if the lock is taken
            if (pre_lock_status != post_lock_status
                    || post_lock_status >> 0x1 > transaction->rv
                    || post_lock_status & 0x1) {

                // Abort the transaction
                delete transaction;
                return false;
            }
        }
    }
    else {
        for(size_t i = 0; i < size; i += region->align) {
            uintptr_t source_word_add = (uintptr_t) source + i;
            uintptr_t target_word_add = (uintptr_t) target + i;

            // see if the load address already appears in the write-set.
            Node *node = transaction->writeList->get((void *) source_word_add);
            if(node) {
                memcpy((void *) target_word_add, node->val, region->align);
                continue;
            }
            else {
                int lock_index = (source_word_add) % LOCK_ARRAY_SIZE;

                // Speculative execution
                int pre_lock_status = region->getSpinLockState(lock_index);
                memcpy((void *) target_word_add, (void *) source_word_add, region->align);
                int post_lock_status = region->getSpinLockState(lock_index);

                // Check if the lock has changed, if the version is greater than the transaction version or if the lock is taken
                if (pre_lock_status != post_lock_status
                        || post_lock_status >> 0x1 > transaction->rv
                        || post_lock_status & 0x1) {

                    // Abort the transaction
                    delete transaction;
                    return false;
                }

                // Add the address to the read-set
                Node *newNode = new Node((void *) source_word_add, nullptr, region->align);
                transaction->readList->add(newNode);
            }
        }
    }

    return true;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t shared, tx_t tx, void const* source, size_t size, void* target) noexcept {

    Region* region = static_cast<Region*>(shared);
    Transaction *transaction = (Transaction *) tx;

    for (size_t i = 0; i < size; i += region->align) {
        uintptr_t source_word_add = (uintptr_t) source + i;
        uintptr_t target_word_add = (uintptr_t) target + i;
        LinkedList *writeList = transaction->writeList;

        Node *node = writeList->get((void *) target_word_add);
        if(node) {
            memcpy(node->val, (void *) source_word_add, region->align);
        }
        else {
            Node *newNode = new Node((void *) target_word_add, (void *) source_word_add, region->align);
            writeList->add(newNode);
        }
    }

    return true;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
Alloc tm_alloc(shared_t shared, tx_t unused(tx), size_t size, void** target) noexcept {

    Region* region = static_cast<Region*>(shared);

    // We allocate the dynamic segment such that its words are correctly aligned.
    // Moreover, the alignment of the 'next' and 'prev' pointers must be satisfied.
    // Thus, we use align on max(align, struct segment_node*).
    size_t align = region->align;
    align = align < sizeof(struct segment_node*) ? sizeof(void*) : align;

    struct segment_node* sn;
    if (unlikely(posix_memalign((void**)&sn, align, sizeof(struct segment_node) + size) != 0)) {
        return Alloc::nomem;
    }

    // Lock the segment list
    region->lockSegmentList();

    // Insert in the linked list
    sn->prev = nullptr;
    sn->next = region->getAllocs();
    if (sn->next) sn->next->prev = sn;
    region->setAllocs(sn);

    void* segment = (void*) ((uintptr_t) sn + sizeof(struct segment_node));
    memset(segment, 0, size);
    *target = segment;

    // Unlock the segment list
    region->unlockSegmentList();

    return Alloc::success;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) noexcept {
    // Already freed when the transaction ends
    return true;
}
