#ifndef PROCESS_MANAGEMENT_HPP
#define PROCESS_MANAGEMENT_HPP
#include "Task.hpp"
#include <queue>
#include <memory>
#include <atomic>
#include <semaphore.h>
#include <mutex>

class ProcessManagement {
    sem_t* itemsSemaphore;
    sem_t* emptySlotsSemaphore;

public:
    ProcessManagement();
    ~ProcessManagement();
    bool submitToQueue(std::unique_ptr<Task> task);
    void executeTasks();

private:
    // std::queue<std::unique_ptr<Task>> taskQueue;
    struct SharedMemory {
        std::atomic<size_t> size;
        char tasks[1000][256];
        int front;
        int rear;
    };

    SharedMemory *sharedMem;
    int shmFd;
    const char *SHM_NAME = "/my_queue";
    std::mutex queueLock;
};

#endif