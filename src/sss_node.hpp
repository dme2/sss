#include "sss_buffer.hpp"
#include "sss_fifo.hpp"
#include "sss_file.hpp"
#include "sss_util.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// N.B. FILE_OUT = reading + playing an audio file
//      FILE_IN  = write audio to a file
enum NodeType { OUTPUT, INPUT, FILE_OUT, FILE_INPUT };

#define MAX_NODES 100

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
  // does this node produce output that needs to be sampled
  // by the mixer?
  // or should its output be sent to another node
  bool sample_output;
  bool pause;

  size_t ecs_idx;

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

  size_t cur_size;
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
    cur_size += 1;
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
      cur_size -= 1;
      return;
    }

    if (head == node_str_map[id]) {
      auto node = head;
      head = node->next;
      delete node;
      cur_size -= 1;
      return;
    }

    if (tail == node_str_map[id]) {
      auto node = node_str_map[id];
      tail = node->prev;
      tail->next = nullptr;
      delete node;
      cur_size -= 1;
      return;
    }

    auto node = node_str_map[id];
    auto prev_node = node->prev;
    prev_node->next = node->next;
    cur_size -= 1;
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

template <size_t ecs_size = MAX_NODES> class SSS_NodeECS {
public:
  SSS_NodeECS() {
    for (size_t i = 0; i < ecs_size; i++)
      node_ecs[i] = nullptr;
  }

  // either fails and returns nothing
  // or returns a list of the set indices
  std::optional<int *> add_node_list(SSS_NodeList<float> *node_list) {
    if (!has_n_spaces(node_list->cur_size))
      return {};
    int *res;
    return res;
  }

  std::optional<int> add_node(SSS_Node<float> *node) {
    if (!has_space())
      return {};
    node_ecs[cur_ptr] = node;
    return cur_ptr++;
  }

  void remove_node(SSS_Node<float> *node) { node_ecs[node->ecs_idx] = nullptr; }

  // garbage collect dead nodes
  // move all the current nodes back into  a linear configuration
  // i.e. no space between the currently alive nodes
  //
  // size_t compact();

  bool has_space() { return cur_ptr < max_nodes; }

  bool has_n_spaces(int n) { return cur_ptr + n < max_nodes; }

  std::array<SSS_Node<float> *, ecs_size> node_ecs;

  size_t max_nodes{ecs_size};
  size_t cur_ptr{0};
};
