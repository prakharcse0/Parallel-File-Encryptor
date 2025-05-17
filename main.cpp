#include <iostream>
#include <filesystem>
#include <algorithm>
#include "./src/app/processes/ProcessManagement.hpp"
#include "./src/app/processes/Task.hpp"

namespace fs = std::filesystem;

int main(int argv, char **argc) {
    std::string directory;
    std::string action;

    std::cout <<"Enter the directory path: " <<std::endl;
    std::getline(std::cin, directory);

    std::cout <<"Enter the action (encrypt/decrypt): " <<std::endl;
    std::getline(std::cin, action);

    try {
        if(!(fs::exists(directory) && fs::is_directory(directory))) 
            std::cout <<"Invalid directory path: " <<std::endl;

        ProcessManagement processManagement;
        for(const auto &entry: fs::recursive_directory_iterator(directory)) {
            if(entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                IO io(filePath);
                std::fstream f_stream = io.getFileStream();
                if(!f_stream.is_open())
                    std::cout <<"Unable to open the file: " <<filePath <<std::endl;
                
                auto task = std::make_unique<Task>(filePath, std::move(f_stream), action);
                processManagement.submitToQueue(std::move(task));
            }
        }
        processManagement.executeTasks();
    }
    catch(fs::filesystem_error &ex) {
        std::cout << "Filesystem error: " << ex.what() << std::endl;
    }

    return 0;
}