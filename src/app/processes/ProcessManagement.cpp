#include "ProcessManagement.hpp"
#include <iostream>
#include <cstring>
#include <sys/wait.h>
#include "../encryptDecrypt/Cryption.hpp" // Assuming this exists and handles the actual crypto
#include <ctime>
#include <iomanip>
#include <sys/mman.h>
#include <atomic>
#include <sys/fcntl.h>
#include <unistd.h> // For fork, exit

ProcessManagement::ProcessManagement() {
    // Clean up previous semaphores and shared memory in case of a crash
    sem_unlink("/items_semaphore");
    sem_unlink("/empty_slots_semaphore");
    shm_unlink(SHM_NAME);

    itemsSemaphore = sem_open("/items_semaphore", O_CREAT, 0666, 0); // Initially 0 items
    emptySlotsSemaphore = sem_open("/empty_slots_semaphore", O_CREAT, 0666, 1000); // 1000 empty slots

    if (itemsSemaphore == SEM_FAILED || emptySlotsSemaphore == SEM_FAILED) {
        perror("sem_open failed");
        throw std::runtime_error("Failed to open semaphores");
    }

    shmFd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shmFd == -1) {
        perror("shm_open failed");
        throw std::runtime_error("Failed to open shared memory");
    }

    if (ftruncate(shmFd, sizeof(SharedMemory)) == -1) {
        perror("ftruncate failed");
        close(shmFd);
        throw std::runtime_error("Failed to truncate shared memory");
    }

    sharedMem = static_cast<SharedMemory *>(mmap(nullptr, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0));
    if (sharedMem == MAP_FAILED) {
        perror("mmap failed");
        close(shmFd);
        throw std::runtime_error("Failed to map shared memory");
    }

    // Initialize shared memory
    sharedMem->front = 0;
    sharedMem->rear = 0;
    sharedMem->size.store(0);
    sharedMem->producerFinished.store(false); // Producer is not finished initially
}

bool ProcessManagement::submitTaskToSharedQueue(const std::string& taskString) {
    // Wait for an empty slot
    if (sem_wait(emptySlotsSemaphore) == -1) {
        perror("sem_wait emptySlotsSemaphore failed");
        return false;
    }

    // Acquire lock to safely modify shared memory
    std::unique_lock<std::mutex> lock(queueLock);
    if (sharedMem->size.load() >= 1000) { // Should ideally not happen due to semaphore
        lock.unlock();
        sem_post(emptySlotsSemaphore); // Release the slot we just acquired
        return false;
    }

    strcpy(sharedMem->tasks[sharedMem->rear], taskString.c_str());
    sharedMem->rear = (sharedMem->rear + 1) % 1000;
    sharedMem->size.fetch_add(1); // Increment task count
    lock.unlock();

    // Signal that an item is available
    if (sem_post(itemsSemaphore) == -1) {
        perror("sem_post itemsSemaphore failed");
        return false;
    }
    return true;
}

void ProcessManagement::executeTaskFromSharedQueue() {
    // Loop continuously, trying to get tasks until producer is finished AND queue is empty
    while (true) {
        // Try to wait for an item, but don't block indefinitely if producer is finished
        if (sem_trywait(itemsSemaphore) == -1) {
            if (errno == EAGAIN) { // No items currently
                if (sharedMem->producerFinished.load() && sharedMem->size.load() == 0) {
                    // Producer finished and queue is empty, so this worker can exit
                    break;
                }
                // Small sleep to avoid busy-waiting if no items
                usleep(10000); // 10ms
                continue;
            } else {
                perror("sem_trywait itemsSemaphore failed");
                break; // Error, exit worker
            }
        }

        // Acquire lock to safely modify shared memory
        std::unique_lock<std::mutex> lock(queueLock);
        if (sharedMem->size.load() == 0) { // Should ideally not happen due to semaphore
             lock.unlock();
             sem_post(itemsSemaphore); // Release the item we just acquired (if any)
             continue;
        }

        char taskStr[256];
        strcpy(taskStr, sharedMem->tasks[sharedMem->front]);
        sharedMem->front = (sharedMem->front + 1) % 1000;
        sharedMem->size.fetch_sub(1); // Decrement task count
        lock.unlock();

        // Signal that an empty slot is available
        if (sem_post(emptySlotsSemaphore) == -1) {
            perror("sem_post emptySlotsSemaphore failed");
            // This is a critical error, but for a worker process, just log and continue/exit
        }

        std::cout << "[PID " << getpid() << "] Executing task: " << taskStr << std::endl;
        // Call your actual encryption/decryption function
        // Make sure executeCryption handles its own errors and doesn't rely on std::cin/cout directly
        executeCryption(taskStr);
    }
}

void ProcessManagement::createWorkerProcesses(int numWorkers) {
    std::cout << "Creating " << numWorkers << " worker processes..." << std::endl;
    for (int i = 0; i < numWorkers; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            // Handle error: clean up resources, maybe exit
            return;
        } else if (pid == 0) { // Child process
            // Child processes will run the executeTaskFromSharedQueue loop
            std::cout << "[PID " << getpid() << "] Worker process started." << std::endl;
            executeTaskFromSharedQueue();
            std::cout << "[PID " << getpid() << "] Worker process finished and exiting." << std::endl;
            exit(0); // Child process exits after completing its work
        } else { // Parent process
            workerPids.push_back(pid); // Store child PID
        }
    }
}

void ProcessManagement::waitForWorkers() {
    // Set the producerFinished flag to true, so workers know no more tasks are coming
    sharedMem->producerFinished.store(true);

    // Unblock any workers that might be stuck on sem_wait(itemsSemaphore)
    // by posting a number of items equal to the number of workers.
    // This allows them to check the producerFinished flag.
    for (size_t i = 0; i < workerPids.size(); ++i) {
        sem_post(itemsSemaphore);
    }

    std::cout << "Waiting for worker processes to finish..." << std::endl;
    for (pid_t pid : workerPids) {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
        } else {
            std::cout << "Worker PID " << pid << " exited with status " << WEXITSTATUS(status) << std::endl;
        }
    }
}

ProcessManagement::~ProcessManagement() {
    // Unmap shared memory
    if (sharedMem != MAP_FAILED) {
        if (munmap(sharedMem, sizeof(SharedMemory)) == -1) {
            perror("munmap failed");
        }
    }

    // Close shared memory file descriptor
    if (close(shmFd) == -1) {
        perror("close shmFd failed");
    }

    // Unlink shared memory (delete it from the system)
    if (shm_unlink(SHM_NAME) == -1) {
        perror("shm_unlink failed");
    }

    // Close semaphores
    if (sem_close(itemsSemaphore) == -1) {
        perror("sem_close itemsSemaphore failed");
    }
    if (sem_close(emptySlotsSemaphore) == -1) {
        perror("sem_close emptySlotsSemaphore failed");
    }

    // Unlink semaphores (delete them from the system)
    if (sem_unlink("/items_semaphore") == -1) {
        perror("sem_unlink itemsSemaphore failed");
    }
    if (sem_unlink("/empty_slots_semaphore") == -1) {
        perror("sem_unlink emptySlotsSemaphore failed");
    }
}