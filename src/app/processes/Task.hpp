#ifndef TASK_HPP
#define TASK_HPP
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
// #include "../fileHandling/IO.hpp" // This header is needed for IO class if used

enum class Action {
    ENCRYPT,
    DECRYPT
};

struct Task {
    std::string filePath;
    // f_stream should only be opened by the process that will *use* it
    // It's not suitable for being moved across processes via shared memory or forked state.
    // The consumer process will open its own fstream based on filePath.
    std::fstream f_stream;
    Action action;

    // Constructor for consumer side (will open its own fstream)
    Task(std::string filepath, Action action) : filePath(filepath), action(action) {}

    // Constructor for producer side (just to create the string) - fstream is a dummy here
    Task(std::string filepath, std::fstream &&stream, Action action) : filePath(filepath), f_stream(std::move(stream)), action(action)  {}

    // Constructor for producer side (just to create the string) - fstream is a dummy here
    Task(std::string filepath, std::fstream &&stream, std::string actionString) : filePath(filepath), f_stream(std::move(stream))  {
        for (char& c : actionString) c = std::toupper(c);
        if(actionString == "ENCRYPT") action = Action::ENCRYPT;
        else if(actionString == "DECRYPT") action = Action::DECRYPT;
        else throw std::runtime_error("Invalid action type");
    }

    ~Task() {
        if (f_stream.is_open())
            f_stream.close();
    }

    std::string toString() const { // Made const as it doesn't modify the object
        std::ostringstream oss;
        oss << filePath << "," << (action == Action::ENCRYPT ? "ENCRYPT" : "DECRYPT");
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

        // This is crucial: the consumer process needs to open its own file stream
        // The f_stream member of the Task struct received by the consumer
        // should be opened by *that* consumer process, not passed from another.
        // So, we use the new constructor that doesn't take fstream, and the
        // calling code (executeCryption or directly in executeTaskFromSharedQueue)
        // will handle opening the file.
        // For now, we'll return a Task object without an open f_stream.
        // The `executeCryption` function *must* open the file itself.
        return Task(filePath, action);
    }
};

#endif