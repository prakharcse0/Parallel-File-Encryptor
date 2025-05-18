#include "ProcessManagement.hpp"
#include <iostream>
#include <cstring>
#include <sys/wait.h>
#include "../encryptDecrypt/Cryption.hpp"
#include <ctime>
#include <iomanip>
#include <sys/mman.h>
#include <atomic>
#include <sys/fcntl.h>


ProcessManagement::ProcessManagement() {
    itemsSemaphore = sem_open("/items_semaphore", O_CREAT, 0666, 0);
    emptySlotsSemaphore = sem_open("/empty_slots_semaphore", O_CREAT, 0666, 1000);
    shmFd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shmFd, sizeof(SharedMemory));
    sharedMem = static_cast<SharedMemory *> (mmap(nullptr, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0));
    sharedMem->front = 0;
    sharedMem->rear = 0;
    sharedMem->size.store(0);
}

bool ProcessManagement::submitToQueue(std::unique_ptr<Task> task) {
    sem_wait(emptySlotsSemaphore);
    std::unique_lock<std::mutex> lock(queueLock);
    strcpy(sharedMem->tasks[sharedMem->rear], task->toString().c_str());
    sharedMem->rear = (sharedMem->rear + 1)%1000;
    lock.unlock();
    sem_post(itemsSemaphore);
    int pid = fork();
    if(pid < 0)
        return false;
    else if(pid > 0)
        std::cout <<"Entering the parent process" <<std::endl;
    else {
        std::cout <<"Entering the child process" <<std::endl;
        executeTasks();
        std::cout <<"Exiting the child process" <<std::endl;
        exit(0);
    }
    return true;
}

void ProcessManagement::executeTasks() {
    sem_wait(itemsSemaphore);
    std::unique_lock<std::mutex> lock(queueLock);
    char taskStr[256];
    strcpy(taskStr, sharedMem->tasks[sharedMem->front]);
    sharedMem->front = (sharedMem->front + 1) % 1000;
    sharedMem->size.fetch_sub(1);
    lock.unlock();
    sem_post(emptySlotsSemaphore);
    std::cout <<"Executing child process" <<std::endl;
    executeCryption(taskStr);
}

ProcessManagement::~ProcessManagement() {
    munmap(sharedMem, sizeof(sharedMem));
    shm_unlink(SHM_NAME);
}