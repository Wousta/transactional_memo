//
// Created by luis on 12/3/24.
//

#ifndef CS453_2024_PROJECT_MASTER_REGION_H
#define CS453_2024_PROJECT_MASTER_REGION_H

#include <mutex>
#include "VersionSpinLock.h"
#include "glob_constants.h"

/**
 * @brief List of dynamically allocated segments.
 */
struct segment_node {
    struct segment_node* prev;
    struct segment_node* next;
    // uint8_t segment[] // segment of dynamic size
};

typedef struct segment_node* segment_list;

class Region {
    private:
        void* start;
        segment_list allocs;
        VersionSpinLock locks[LOCK_ARRAY_SIZE];
        std::mutex segmentListMutex;
        std::atomic_uint clock;

    public:
        const size_t size;
        const size_t align;


        Region(size_t size, size_t align);

        ~Region();

        void* getStart() {
            return start;
        }

        segment_list getAllocs() {
            return allocs;
        }

        void setAllocs(segment_list allocs) {
            this->allocs = allocs;
        }

        void unlockSegmentList() {
            segmentListMutex.unlock();
        }

        void lockSegmentList() {
            segmentListMutex.lock();
        }

        VersionSpinLock* getSpinLocks() {
            return locks;
        }

        int getSpinLockState(int index) {
            return versionSpinLock_get_state(&locks[index]);
        }

        bool acquireSpinLock(int index) {
            return versionSpinLock_acquire(&locks[index]);
        }

        void releaseSpinLock(int index) {
            versionSpinLock_release(&locks[index]);
        }

        int getClockVersion() {
            return clock.load();
        }

        int incrementClockVersion() {
            return clock.fetch_add(1) + 1;
        }
};


#endif //CS453_2024_PROJECT_MASTER_REGION_H
