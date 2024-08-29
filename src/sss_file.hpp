#include <cstring>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

class SSS_File {
public:
  std::string file_path;
  std::size_t file_size{0};
  std::size_t read_pos{0};
  std::vector<char> file_buffer;
  std::ofstream in_file;
  std::ifstream cur_file;
  bool end_of_file{false};

  SSS_File(std::string fp, bool input = false) : file_path(fp) {
    if (!input)
      read_file(fp);
    else
      open_file(fp);
  }

  SSS_File() {}

  std::ifstream get_file_handle(std::string fp) {
    std::ifstream file(fp);
    return file;
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
    cur_file.open(path, std::ios::binary);
    // std::vector<char> content((std::istreambuf_iterator<char>(file)),
    //                          std::istreambuf_iterator<char>());

    // file_buffer = content;
    // file_size = file_buffer.size();
    if (!cur_file) {
      std::cerr << "Error: Failed to open file" << std::endl;
    }
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

  std::vector<std::byte> read_n_bytes(size_t n) {
    std::vector<char> bytes(n);
    bytes.reserve(n);
    std::vector<std::byte> out_bytes;
    cur_file.read(bytes.data(), n);
    for (auto i : bytes) {
      out_bytes.push_back(static_cast<std::byte>(i));
    }

    return out_bytes;
  }

  std::string read_string(size_t n) {
    std::string res = "";
    auto bytes = read_n_bytes(n);
    for (auto i : bytes) {
      res.push_back(static_cast<char>(i));
    }
    return res;
  }

  uint8_t read_uint8() {
    auto temp = read_n_bytes(1);

    return std::to_integer<uint8_t>(temp[0]);
  }

  int16_t read_int16_le() {
    auto bytes = read_n_bytes(2);
    int16_t res = (int16_t)(bytes[1] << 8 | bytes[0]);
    return res;
  }

  int16_t read_int16_be() {
    auto bytes = read_n_bytes(2);
    int16_t res = (int16_t)(bytes[0] << 8 | bytes[1]);
    return res;
  }

  int32_t read_int32_le() {
    auto bytes = read_n_bytes(4);
    int32_t res =
        (int32_t)(bytes[0] << 24 | bytes[1] << 16 | bytes[2] << 8 | bytes[3]);

    return res;
  }

  int32_t read_int32_be() {
    auto bytes = read_n_bytes(4);
    int32_t res =
        (int32_t)(bytes[3] << 24 | bytes[2] << 16 | bytes[1] << 8 | bytes[0]);

    return res;
  }

  // TODO
  int32_t read_int32_variable() {
    int32_t acc = 0;
    int8_t count = 0;
    while (count < 5) {
      auto val = read_uint8();
      acc = (acc << 7) | (val & 127);
      if ((val & 128) == 0) {
        break;
      }
      count++;
    }
    if (count == 5)
      return -1;
    return acc;
  }

  void discard_bytes(size_t n) { read_n_bytes(n); }
};
