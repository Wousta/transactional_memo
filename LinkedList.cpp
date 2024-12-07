#include "LinkedList.h"

LinkedList::LinkedList() {
    head = nullptr;
    tail = nullptr;
}

LinkedList::~LinkedList() {
    Node *node = head;
    while(node) {
        Node *next = node->next;
        delete node;
        node = next;
    }
}

/**
 * @brief Add a node to the linked list
 * @param node Node to add to the tail of the list
 */
void LinkedList::add(Node *node) {
    if(!head) {
        head = node;
        tail = node;
    }
    else {
        tail->next = node;
        tail = node;
    }
}

/**
 * @brief Get the node with the given address
 * @param address Address to search for
 * @return Node with the given address, or nullptr if not found
 */
Node *LinkedList::get(void *address) {
    Node *node = head;
    while(node) {

        if(node->address == address) {
            return node;
        }

        node = node->next;
    }

    return nullptr;
}