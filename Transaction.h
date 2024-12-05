//
// Created by luis on 12/3/24.
//

#ifndef CS453_2024_PROJECT_MASTER_TRANSACTION_H
#define CS453_2024_PROJECT_MASTER_TRANSACTION_H

#include <stdint.h>
#include <cstdlib>
#include <stdlib.h>
#include <string.h>
#include "VersionSpinLock.h"
#include "LinkedList.h"

struct Transaction {
    Transaction(bool is_ro, int clockVersion) :
        is_ro(is_ro), writeList(new LinkedList()), readList(new LinkedList()), rv(clockVersion), wv(-1) {}

    ~Transaction() {
        delete writeList;
        delete readList;
    }

    bool is_ro;
    LinkedList *writeList;
    LinkedList *readList;
    int rv;
    int wv;
};

void transaction_commit_and_release_locks(Transaction *transaction, VersionSpinLock *ver_wr_spinlocks, size_t align);


#endif //CS453_2024_PROJECT_MASTER_TRANSACTION_H
