//
// Created by luis on 12/3/24.
//

#ifndef CS453_2024_PROJECT_MASTER_LINKEDLIST_H
#define CS453_2024_PROJECT_MASTER_LINKEDLIST_H

#include <stdint.h>
#include <cstdlib>
#include <string.h>
#include "macros.h"

struct Node {
     Node(void *address, void *val, size_t val_size)
        : address(address), val(nullptr), next(nullptr) {
        if(val) {
            this->val = new uint8_t[val_size];
            memcpy(this->val, val, val_size);
        }
        else {
            this->val = nullptr;
        }
    }

    ~Node() {
        delete[] static_cast<uint8_t*>(val);
    }

    void *address;      // the lock and location address are related so we need to keep only one of them in the read-set.
    void *val;          // Only used in the write-set
    struct Node* next;
};

class LinkedList {
    private:
        Node *head;
        Node *tail;

    public:
        LinkedList();
        ~LinkedList();

        Node *getHead() {
            return head;
        }

        Node *getTail() {
            return tail;
        }

        void add(Node *node);
        // No remove, not necessary in this implementation
        Node *get(void *address);
};


#endif //CS453_2024_PROJECT_MASTER_LINKEDLIST_H
