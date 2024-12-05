//
// Created by luis on 12/3/24.
//

#include "Transaction.h"
#include "macros.h"
#include "glob_constants.h"

void transaction_commit_and_release_locks(Transaction *transaction, VersionSpinLock *ver_wr_spinlocks, size_t align) {
    Node *node = transaction->writeList->getHead();
    while(node) {
        int lock_index = ((uintptr_t) node->address) % LOCK_ARRAY_SIZE;
        memcpy(node->address, node->val, align);
        versionSpinLock_set_and_release(&(ver_wr_spinlocks[lock_index]), transaction->wv);
        node = node->next;
    }
}