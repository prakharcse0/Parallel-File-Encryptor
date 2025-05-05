#ifndef TASK_HPP
#define TASK_HPP
#include <string>
#include <fstream>
#include <sstream>
#include "../fileHandling/IO.hpp"

enum class Action {
    ENCRYPT,
    DECRYPT
};

struct Task {
    std::string filePath;
    std::fstream f_stream;
    Action action;

    Task(std::string filepath, std::fstream &&stream, Action action) : filePath(filepath), f_stream(std::move(stream)), action(action)  {}

    ~Task() {
        if (f_stream.is_open())
            f_stream.close();
    }

    std::string toString() {
        std::ostringstream oss;
        oss <<filePath <<"," <<(action == Action::ENCRYPT ? "ENCRYPT" : "DECRYPT");
        return oss.str();
    }

    static Task fromString(const std::string &taskData) {
        std::istringstream iss(taskData);
        std::string filePath;
        std::string actionString;

        if(!(getline(iss, filePath, ',') && getline(iss, actionString)))
            throw std::runtime_error("Invalid task data format");

        if(!(actionString == "ENCRYPT" || actionString == "DECRYPT"))
            throw std::runtime_error("Invalid Action type");
        Action action = (actionString == "ENCRYPT" ? Action::ENCRYPT : Action::DECRYPT);

        IO io(filePath);
        std::fstream f_stream = io.getFileStream();
        if(!f_stream.is_open())
            throw std::runtime_error("Failed to open file : " + filePath);
        
        return Task(filePath, std::move(f_stream), action);
    }
};

#endif
