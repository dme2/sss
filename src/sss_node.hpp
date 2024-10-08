#include "sss_buffer.hpp"
#include "sss_fifo.hpp"
#include "sss_midi.hpp"
#include "sss_synth.hpp"
#include "sss_util.hpp"

#include <CoreFoundation/CoreFoundation.h> // ??
#include <array>
#include <cmath>
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
enum NodeType { OUTPUT, INPUT, FILE_OUT, FILE_INPUT, MIDI_IN, MIDI_OUT };

#define MAX_NODES 100

template <typename T> class SSS_Node {
public:
  using fn_type = std::function<std::size_t(SSS_Node *, std::size_t)>;
  SSS_Fifo<std::vector<T>> *node_buffer_fifo;
  SSS_Fifo<T *> *node_input_buffer_fifo;
  std::size_t buff_size;
  SSS_Buffer<T> *node_buffer;
  SSS_Fifo<T> *node_queue;
  void *fn_data;
  fn_type fun;
  NodeType nt;
  int channels;
  std::string device_id;
  std::string node_id{"node"};
  SSS_File *file;
  float *temp_buffer;
  std::function<void(SSS_Node<T>)> input_fn;

  // does this node produce output that needs to be sampled
  // by the mixer?
  // or should its output be sent to another node
  bool sample_output;
  bool pause{false};

  size_t ecs_idx;
  SSS_MIDI *midi_handle;
  double cur_midi_time{0.0};
  SSS_Synth<T> *midi_synth;
  synth_params *midi_synth_params;
  std::unordered_map<float, std::vector<midi_message>> midi_time_map;
  size_t msg_index{0};

  std::size_t run_fn(size_t n_samples) { return fun(this, n_samples); }

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
    node_buffer_fifo = new SSS_Fifo<std::vector<T>>(32);
    node_input_buffer_fifo = new SSS_Fifo<T *>(32);

    for (std::size_t i = 0; i < s * 4; i++)
      node_queue->enqueue(0);
  }

  SSS_Node(NodeType type, fn_type fn, int ch, std::size_t s,
           std::string node_id, std::string id)
      : nt(type), channels(ch), buff_size(s * 2), device_id(id),
        node_id(node_id) {
    this->fun = fn;
    node_buffer = new SSS_Buffer<T>(s * 4);
    node_queue = new SSS_Fifo<T>(s * 4);
    node_buffer_fifo = new SSS_Fifo<std::vector<T>>(32);
    node_input_buffer_fifo = new SSS_Fifo<T *>(32);

    for (std::size_t i = 0; i < s * 4; i++)
      node_queue->enqueue(0);
  }

  SSS_Node(NodeType type, fn_type fn, int ch, std::size_t s,
           std::string node_id, std::string device_id, void *fn_data)
      : nt(type), channels(ch), buff_size(s), node_id(node_id),
        device_id(device_id), fn_data(fn_data) {
    this->fun = fn;
    node_buffer = new SSS_Buffer<T>(s * 4);
    node_queue = new SSS_Fifo<T>(s * 4);
    node_buffer_fifo = new SSS_Fifo<std::vector<T>>(32);
    node_input_buffer_fifo = new SSS_Fifo<T *>(32);

    for (std::size_t i = 0; i < s * 4; i++)
      node_queue->enqueue(0);
  }

  SSS_Node(NodeType type, fn_type fn, int ch, std::size_t s,
           std::string node_id, std::string id, std::string file_path)
      : nt(type), channels(ch), buff_size(s), device_id(id) {
    this->fun = fn;
    node_buffer = new SSS_Buffer<T>(s);
    node_queue = new SSS_Fifo<T>(s * 2);
    node_buffer_fifo = new SSS_Fifo<std::vector<T>>(16);
    node_input_buffer_fifo = new SSS_Fifo<T *>(32);
    temp_buffer = new float[s];
    if (type == FILE_INPUT)
      file = new SSS_File(file_path, true);
    else
      file = new SSS_File(file_path);
  }

  SSS_Node(NodeType type, std::size_t s) : nt(type) {
    node_buffer = new SSS_Buffer<T>(s);
    node_queue = new SSS_Fifo<T>(s * 2);
    node_buffer_fifo = new SSS_Fifo<std::vector<T>>(16);
  }

  SSS_Node(NodeType type, std::string node_id, std::string device_id,
           midi_file_data *md = nullptr)
      : nt(type), node_id(node_id), device_id(device_id) {
    if (type == MIDI_IN || type == MIDI_OUT) {
      midi_handle = new SSS_MIDI();
      setup_midi_file(md);
      midi_synth_params = new synth_params(2, 440.0, 0.0, 0.2, 48000.0);
      this->node_buffer_fifo = new SSS_Fifo<std::vector<T>>(16);
    }
  }

  void pause_node() {
    if (pause)
      return;
    else
      pause = true;
  }

  // sets up the midi_time_map
  void setup_midi_file(midi_file_data *md) {
    if (md->tick_times.size() != md->messages.size()) {
      std::cout << "MIDI file size error!\n";
      return;
    }
    for (int i = 0; i < md->tick_times.size(); i++) {
      this->midi_time_map[md->tick_times[i]].push_back(md->messages[i]);
    }
  }

  void setup_midi_synth(SSS_Synth<T> *synth) { this->midi_synth = synth; }

  size_t render_midi_file(size_t n_samples) {
    if (this->midi_synth == nullptr ||
        this->msg_index == this->midi_handle->midi_file->messages.size())
      return 0;
    auto t = this->midi_handle->midi_file->tick_times[this->msg_index];
    auto m = this->midi_handle->midi_file->messages[this->msg_index];
    if (m.data2 == std::byte(0)) {
      msg_index += 1;
      auto t = this->midi_handle->midi_file->tick_times[this->msg_index];
      auto m = this->midi_handle->midi_file->messages[this->msg_index];
    }
    std::vector<T> samples(n_samples, 0);
    float note = midi_handle->get_note_freq(m.data1);
    if (cur_midi_time >= t) {
      note = midi_handle->get_note_freq(m.data1);
      this->msg_index += 1;
    }
    midi_synth_params->pitch = note;
    auto res =
        this->midi_synth->gen_sine(midi_synth_params, samples, n_samples);
    if (!this->node_buffer_fifo->enqueue(samples))
      std::cout << "no space!\n";
    this->cur_midi_time += /* tempo */ 1 * n_samples / 44800.0; // TODO
    return res;
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

  void remove_node(SSS_Node<float> *node) {
    node_ecs[node->ecs_idx]->pause_node();
    // probably need to spinlock or something here
    // don't want to set it to a nullptr while its
    // in the middle of sampling
    node_ecs[node->ecs_idx] = nullptr;
  }

  // garbage collect dead nodes
  // move all the current nodes back into  a linear configuration
  // i.e. no space between the currently alive nodes
  //
  // size_t compact();

  bool has_space() { return cur_ptr < max_nodes; }

  bool has_n_spaces(int n) { return cur_ptr + n < max_nodes; }

  int num_avail_spaces() { return max_nodes - cur_ptr; }

  std::array<SSS_Node<float> *, ecs_size> node_ecs;

  size_t max_nodes{ecs_size};
  size_t cur_ptr{0};
};
