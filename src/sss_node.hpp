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

  std::size_t run_fn() { return fun(this, buff_size); }

  // for node lists
  SSS_Node<T> *next;
  SSS_Node<T> *prev;

  // TODO:
  // should probably figure out optimal buffer sizes here
  SSS_Node(NodeType type, fn_type fn, int ch, std::size_t s, std::string id)
      : nt(type), channels(ch), buff_size(s * 2), device_id(id) {
    this->fun = fn;
    node_buffer = new SSS_Buffer<T>(s * 4);
    node_queue = new SSS_Fifo<T>(s * 4);

    for (std::size_t i = 0; i < s * 4; i++)
      node_queue->enqueue(0);
  }

  SSS_Node(NodeType type, fn_type fn, int ch, std::size_t s, std::string id,
           void *fn_data)
      : nt(type), channels(ch), buff_size(s), device_id(id), fn_data(fn_data) {
    this->fun = fn;
    node_buffer = new SSS_Buffer<T>(s * 4);
    node_queue = new SSS_Fifo<T>(s * 4);

    for (std::size_t i = 0; i < s * 4; i++)
      node_queue->enqueue(0);
  }

  SSS_Node(NodeType type, fn_type fn, int ch, std::size_t s, std::string id,
           std::string file_path)
      : nt(type), channels(ch), buff_size(s), device_id(id) {
    this->fun = fn;
    node_buffer = new SSS_Buffer<T>(s);
    node_queue = new SSS_Fifo<T>(s * 2);
    temp_buffer = new float[s];
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

template <typename T> struct SSS_NodeList {
  SSS_Node<T> *head;
  SSS_Node<T> *tail;

  // index->node
  std::unordered_map<size_t, SSS_Node<T> *> node_idx_map;
  // name->node
  std::unordered_map<std::string, SSS_Node<T> *> node_str_map;

  void add_node(SSS_Node<T> *node) {
    if (head == nullptr) {
      head = node;
      tail = node;
    } else {
      auto prev = tail;
      prev->next = node;
      node->prev = prev;
      tail = node;
    }

    node_str_map[node->device_id] = node;
  }

  void remove_node(std::string id) {
    // TODO:
    // handle several different cases
    // (removing head, removing tail, removing last remaining node)

    if (head == node_str_map[id] && tail == node_str_map[id]) {
      auto node = node_str_map[id];
      head = nullptr;
      tail = nullptr;
      delete node;
      return;
    }

    if (head == node_str_map[id]) {
      auto node = head;
      head = node->next;
      delete node;
      return;
    }

    if (tail == node_str_map[id]) {
      auto node = node_str_map[id];
      tail = node->prev;
      tail->next = nullptr;
      delete node;
      return;
    }

    auto node = node_str_map[id];
    auto prev_node = node->prev;
    prev_node->next = node->next;
    delete node;
  }

  void traverse_list_and_run() {
    auto cur = head;
    while (cur != nullptr) {
      // if cur->fn != nullptr...
      cur->run_fn();
      cur = cur->next;
    }
  }

  SSS_NodeList() {
    head = nullptr;
    tail = nullptr;
  }
};
