#include "Cryption.hpp"
#include "../processes/Task.hpp"
#include "../fileHandling/ReadEnv.cpp"
#include <vector>
#include <random>
#include <ctime>

// A key derivation function that expands a seed into a robust key
std::vector<uint8_t> deriveKey(const std::string& seed, size_t length) {
    std::vector<uint8_t> key(length);
    
    // Use seed to initialize random generator for reproducible results
    std::mt19937 gen(std::hash<std::string>{}(seed));
    std::uniform_int_distribution<> distrib(0, 255);
    
    // Fill key with deterministic but pseudorandom values
    for (size_t i = 0; i < length; i++) {
        key[i] = static_cast<uint8_t>(distrib(gen));
    }
    
    return key;
}

int executeCryption(const std::string& taskData) {
    Task task = Task::fromString(taskData);
    ReadEnv env;
    std::string envKey = env.getenv();
    
    // Get file size to derive appropriate key length
    task.f_stream.seekg(0, std::ios::end);
    size_t fileSize = task.f_stream.tellg();
    task.f_stream.seekg(0, std::ios::beg);
    
    // Derive a key stream from the seed (envKey)
    std::vector<uint8_t> keyStream = deriveKey(envKey, fileSize);
    
    // Buffer for efficient file processing
    const size_t BUFFER_SIZE = 4096;
    std::vector<char> buffer(BUFFER_SIZE);
    
    size_t position = 0;
    
    if (task.action == Action::ENCRYPT) {
        while (task.f_stream) {
            // Read a chunk
            task.f_stream.read(buffer.data(), BUFFER_SIZE);
            std::streamsize bytesRead = task.f_stream.gcount();
            if (bytesRead <= 0) break;
            
            // Apply encryption
            for (std::streamsize i = 0; i < bytesRead; i++) {
                buffer[i] = buffer[i] ^ keyStream[(position + i) % fileSize];
            }
            
            // Write back the encrypted data
            task.f_stream.seekp(position, std::ios::beg);
            task.f_stream.write(buffer.data(), bytesRead);
            
            position += bytesRead;
        }
    } else { // Decryption - same operation since XOR is symmetrical
        while (task.f_stream) {
            // Read a chunk
            task.f_stream.read(buffer.data(), BUFFER_SIZE);
            std::streamsize bytesRead = task.f_stream.gcount();
            if (bytesRead <= 0) break;
            
            // Apply decryption (same as encryption with XOR)
            for (std::streamsize i = 0; i < bytesRead; i++) {
                buffer[i] = buffer[i] ^ keyStream[(position + i) % fileSize];
            }
            
            // Write back the decrypted data
            task.f_stream.seekp(position, std::ios::beg);
            task.f_stream.write(buffer.data(), bytesRead);
            
            position += bytesRead;
        }
    }
    
    task.f_stream.close();
    return 0;
}