#ifndef IOHPP
#define IOHPP

#include <fstream>
#include <string>

class IO{
    public:
        IO(const std::string &file_path);
        ~IO();
        std::fstream getFileStream();
}