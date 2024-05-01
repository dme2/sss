#include "sss_buffer.hpp"
#include "sss_fifo.hpp"
#include "sss_file.hpp"
// #include "sss_thread.hpp"
#include "sss_util.hpp"

#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#
// N.B. FILE_OUT = reading + playing an audio file
//      FILE_IN  = write audio to a file
enum NodeType { OUTPUT, INPUT, FILE_OUT, FILE_INPUT };

// TODO:
// can node functions be coroutines? (i.e. generators)
template <typename T> class SSS_Node {
public:
  using fn_type = std::function<std::size_t(SSS_Node *, std::size_t)>;
  std::size_t buff_size;
  SSS_Buffer<T> *node_buffer;
  SSS_Fifo<T> *node_queue;
  void *fn_data;
  fn_type fun;
  NodeType nt;
  int channels;
  std::string device_id;
  SSS_File *file;
  float *temp_buffer;
  std::function<void(SSS_Node<T>)> input_fn;

  // for NodeLists
  SSS_Node<T> *next;

  void run_fn() { fun(this, buff_size); }

  SSS_Node(NodeType type, fn_type fn, int ch, std::size_t s, std::string id)
      : nt(type), channels(ch), buff_size(s), device_id(id) {
    this->fun = fn;
    node_buffer = new SSS_Buffer<T>(s * 4);
    node_queue = new SSS_Fifo<T>(s * 4);
  }

  SSS_Node(NodeType type, fn_type fn, int ch, std::size_t s, std::string id,
           std::string file_path)
      : nt(type), channels(ch), buff_size(s), device_id(id) {
    this->fun = fn;
    node_buffer = new SSS_Buffer<T>(s);
    node_queue = new SSS_Fifo<T>(s * 2);
    if (type == FILE_INPUT)
      file = new SSS_File(file_path, true);
    else
      file = new SSS_File(file_path);
  }

  SSS_Node(NodeType type, std::size_t s) : nt(type) {
    node_buffer = new SSS_Buffer<T>(s);
    node_queue = new SSS_Fifo<T>(s * 2);
  }
};
