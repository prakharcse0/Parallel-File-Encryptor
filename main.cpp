#include <iostream>
#include <filesystem>
#include <algorithm>
#include "./src/app/processes/ProcessManagement.hpp"
#include "./src/app/processes/Task.hpp"
#include <limits> // For numeric_limits

namespace fs = std::filesystem;

int main(int argv, char **argc) {
    std::string directory;
    std::string action;
    int numWorkers;

    std::cout << "Enter the directory path: ";
    std::getline(std::cin, directory);

    std::cout << "Enter the action (encrypt/decrypt): ";
    std::getline(std::cin, action);

    std::cout << "Enter the number of worker processes: ";
    while (!(std::cin >> numWorkers) || numWorkers <= 0) {
        std::cout << "Invalid input. Please enter a positive integer for the number of workers: ";
        std::cin.clear(); // Clear error flags
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Discard invalid input
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Consume the newline after reading numWorkers

    try {
        if (!(fs::exists(directory) && fs::is_directory(directory))) {
            std::cerr << "Error: Invalid directory path." << std::endl;
            return 1; // Indicate error
        }

        ProcessManagement processManagement;

        // 1. Create worker processes first
        processManagement.createWorkerProcesses(numWorkers);

        std::cout << "Producer (main process) starting to add tasks..." << std::endl;
        // 2. Producer adds tasks to the queue
        for (const auto &entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                // We no longer need to open the fstream here in the producer
                // The task will be recreated by the consumer process when it pulls the string
                // This prevents issues with fstream ownership across processes.
                Task tempTask(filePath, std::fstream(), action); // Create a temporary task to get the string
                std::string taskString = tempTask.toString(); // Get the string representation

                if (!processManagement.submitTaskToSharedQueue(taskString)) {
                    std::cerr << "Failed to submit task for: " << filePath << std::endl;
                    // Decide how to handle this error (e.g., retry, log, exit)
                }
            }
        }
        std::cout << "Producer finished adding all tasks." << std::endl;

        // 3. Wait for all worker processes to finish
        processManagement.waitForWorkers();

        std::cout << "All tasks processed and workers finished." << std::endl;

    } catch (const fs::filesystem_error &ex) {
        std::cerr << "Filesystem error: " << ex.what() << std::endl;
        return 1;
    } catch (const std::runtime_error &ex) {
        std::cerr << "Runtime error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}