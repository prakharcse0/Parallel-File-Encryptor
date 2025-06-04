#ifndef PROCESS_MANAGEMENT_HPP
#define PROCESS_MANAGEMENT_HPP
#include "Task.hpp"
#include <queue>
#include <memory>
#include <atomic>
#include <semaphore.h>
#include <mutex>
#include <vector> // Added for storing child PIDs

class ProcessManagement {
    sem_t* itemsSemaphore;
    sem_t* emptySlotsSemaphore;

public:
    ProcessManagement();
    ~ProcessManagement();

    // Producer method: submits a task string to the shared queue
    bool submitTaskToSharedQueue(const std::string& taskString);

    // Consumer method: executed by worker processes
    void executeTaskFromSharedQueue();

    // New: Method to create worker processes
    void createWorkerProcesses(int numWorkers);

    // New: Method to wait for worker processes to finish
    void waitForWorkers();

private:
    struct SharedMemory {
        std::atomic<size_t> size; // Current number of tasks in the queue
        char tasks[1000][256];    // Array to store task strings
        int front;                // Index of the front of the queue
        int rear;                 // Index of the rear of the queue
        std::atomic<bool> producerFinished; // Flag for workers to know when no more tasks will be added
    };

    SharedMemory *sharedMem;
    int shmFd;
    const char *SHM_NAME = "/my_queue";
    std::mutex queueLock; // Protects sharedMem->front, sharedMem->rear, and tasks array
    std::vector<pid_t> workerPids; // To store PIDs of worker processes
};

#endif