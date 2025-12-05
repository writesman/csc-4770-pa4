#include "file_io.hpp"
#include <fstream>
#include <iostream>
#include <sstream> // Added for std::stringstream

// Writes content to disk, truncating the file if it exists.
void write_to_disk(const std::string &filename, const std::string &data) {
  std::ofstream outfile(filename, std::ios::trunc);
  if (!outfile) {
    std::cerr << "[IO Error] Could not open " << filename << " for writing.\n";
    return;
  }
  outfile << data;
}

// Reads the entire content of a file into a single string.
std::string read_from_disk(const std::string &filename) {
  std::ifstream infile(filename);
  if (!infile)
    return "";
  std::stringstream buffer;
  buffer << infile.rdbuf();
  return buffer.str();
}