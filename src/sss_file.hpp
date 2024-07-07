#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>

class SSS_File {
public:
  std::string file_path;
  std::size_t file_size{0};
  std::size_t read_pos{0};
  std::vector<char> file_buffer;
  std::ofstream in_file;
  bool end_of_file{false};

  SSS_File(std::string fp, bool input = false) : file_path(fp) {
    if (!input)
      read_file(fp);
    else
      open_file(fp);
  }

  char *float_to_char(float *buff, std::size_t n_bytes) {
    char *res = new char[n_bytes];
    std::memcpy(res, &buff, n_bytes);
    return res;
  }

  void open_file(std::string file_path) {
    in_file = std::ofstream(file_path, std::ios::binary);
    if (!in_file) {
      std::cerr << "Error: Failed to open file" << std::endl;
      return;
    }
    // std::cout << "file " << file_path << " opened\n";
  }

  void write_out_bytes(float *buff, std::size_t num_bytes) {
    // const auto res = float_to_char(buff, num_bytes);
    in_file.write(reinterpret_cast<char *>(buff), num_bytes);
    return;
  }

  void read_file(std::string path) {
    std::ifstream file(path);
    std::vector<char> content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

    file_buffer = content;
    file_size = file_buffer.size();
    read_pos = 0;
  }

  bool at_file_end() { return read_pos >= file_buffer.size(); }

  std::vector<char> get_buffer(std::size_t num_bytes) {
    if (end_of_file) {
      std::vector<char> out_buffer(num_bytes, 0);
      return out_buffer;
    }

    std::size_t diff = 0;
    if (read_pos + num_bytes > file_buffer.size()) {
      diff = file_size % (read_pos + num_bytes);
    }

    std::vector<char> out_buffer(file_buffer.begin() + read_pos - diff,
                                 file_buffer.begin() + read_pos + num_bytes -
                                     diff);
    read_pos += num_bytes;

    if (at_file_end()) {
      end_of_file = true;
    }

    return out_buffer;
  }
};
