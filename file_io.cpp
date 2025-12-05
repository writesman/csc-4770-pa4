#include "file_io.hpp"
#include <fstream>
#include <iostream>

void write_to_disk(const std::string &filename, const std::string &data) {
  std::ofstream outfile(filename, std::ios::trunc);
  if (!outfile) {
    std::cerr << "[IO Error] Could not open " << filename << " for writing.\n";
    return;
  }
  outfile << data;
}

std::string read_from_disk(const std::string &filename) {
  std::ifstream infile(filename);
  if (!infile)
    return "";
  return std::string((std::istreambuf_iterator<char>(infile)),
                     std::istreambuf_iterator<char>());
}