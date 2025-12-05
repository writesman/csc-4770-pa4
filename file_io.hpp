#ifndef FILE_IO_HPP
#define FILE_IO_HPP

#include <string>

// Interface for simple file access simulating the critical section work.
void write_to_disk(const std::string &filename, const std::string &data);
std::string read_from_disk(const std::string &filename);

#endif