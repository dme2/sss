// #include "sss_buffer.hpp"
// #include "sss_fifo.hpp"
// #include "sss_file.hpp"
// #include "sss_node.hpp"
#include "sss_thread.hpp"
//  #include "sss_util.hpp"
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

// FILE_OUT = reading + playing an audio file
// FILE_IN  = write audio to a file
// enum NodeType { OUTPUT, INPUT, FILE_OUT, FILE_INPUT };

/*TODO There are really only two important graph structures for the audio nodes
 *   1. sequential. i.e. one must be processed before another
 *        (can be represented as a linked list)
 *
 *   2. simultaneous. (or, able to be processed concurrently, whenever)
 *
 *  The representation isn't too important, but how the nodes are processed is.
 *  We can operate either multithreaded or single threaded, but that shouldn't
 *  change the execution algorith too much.
 *
 *  Basically, we should check whether a node is dependent (i.e. relies on, or
 *  should trigger the processing of other nodes) or not. If a node's
 * independent we should
 *
 */

template <typename T> class SSS_Mixer {
public:
  using fn_type = std::function<std::size_t(SSS_Node<T> *, std::size_t)>;
  int out_channels{2};
  bool run_multithreaded{false};
  std::size_t buff_size;
  SSS_Buffer<T> *mixer_buffer;
  std::vector<T> scratch_buff;

  // a mixer and scratch buffer for each device (indexed by device)
  std::unordered_map<std::string, SSS_Buffer<T> *> mixer_buffers;
  std::vector<std::vector<T>> scratch_buffs;

  SSS_ThreadPool *thread_pool;

  std::vector<SSS_Node<T> *> output_nodes;
  std::vector<SSS_Node<T> *> input_nodes;
  // device_id -> Node
  std::unordered_map<std::string, SSS_Node<T> *> input_node_map;

  std::unordered_map<std::string, std::vector<int>> output_node_map;
  std::unordered_map<std::string, int> output_node_idx_count;

  SSS_NodeList<T> *input_node_list;
  SSS_NodeList<T> *output_node_list;

  SSS_NodeECS<MAX_NODES> node_ecs;

  std::vector<int> in_node_ecs_idx;
  size_t num_out_idx;
  size_t num_in_idx;

  SSS_Msg_Queue *msg_queue;

  // mixer function
  std::function<void(SSS_Mixer<T> *mixer, std::vector<T> *buff,
                     std::size_t n_samples)>
      mixer_fn;

  SSS_Mixer(std::size_t buff_size, bool multithread = false, int mt_output = 0)
      : buff_size(buff_size), run_multithreaded(multithread) {
    mixer_buffer = new SSS_Buffer<T>(buff_size);
    output_node_list = new SSS_NodeList<T>;
    input_node_list = new SSS_NodeList<T>;
    node_ecs = SSS_NodeECS<MAX_NODES>();
    msg_queue = new SSS_Msg_Queue();

    if (multithread) {
      thread_pool = new SSS_ThreadPool(mt_output, &node_ecs, msg_queue);
      thread_pool->node_ecs_ = &node_ecs;
    }
  }

  size_t register_node_ecs(SSS_Node<T> *node) {
    std::optional<int> res = node_ecs.add_node(node);
    if (!res.has_value())
      return -1;
    if (run_multithreaded) {
      if (node->nt == FILE_INPUT) {
        input_node_map[node->device_id] = node;
        this->in_node_ecs_idx.push_back(res.value());
        num_in_idx += 1;
      } else {
        if (!mixer_buffers.contains(node->device_id)) {
          mixer_buffers[node->device_id] =
              new SSS_Buffer<T>(5000); // TODO: this->buff_size ?
        }
        this->output_node_map[node->device_id].push_back(res.value());
        this->output_node_idx_count[node->device_id] += 1;
        num_out_idx += 1;
      }
    } else {
      if (node->nt == FILE_INPUT) {
        input_node_map[node->device_id] = node;
        this->in_node_ecs_idx.push_back(res.value());
        num_in_idx += 1;
      } else {
        if (!mixer_buffers.contains(node->device_id)) {
          mixer_buffers[node->device_id] =
              new SSS_Buffer<T>(5000); // TODO: this->buff_size
        }
        this->output_node_map[node->device_id].push_back(res.value());
        this->output_node_idx_count[node->device_id] += 1;
        // num_out_idx += 1;
      }
    }
    return res.value();
  }

  bool pop_and_run_node() {
    auto msg = msg_queue->pop_msg();
    if (msg.has_value()) {
      auto node = node_ecs.node_ecs[msg->node_idx];
      node->run_fn(msg->req_bytes);
      return true;
    } else
      return false;
  }

  void tick_mixer(int n_ticks = 10) {
    if (run_multithreaded) {
      thread_pool->signal_threads();
    } else {
      do {
        // ...
      } while (pop_and_run_node());
    }
  }

  void sample_output_nodes_ecs() {
    if (run_multithreaded) {
      thread_pool->signal_threads();
    } else {
      do {
        // ...
      } while (pop_and_run_node());
      /*
      for (int i = 0; i < output_node_idx_count[device_id]; i++) {
        // for (size_t i = 0; i < num_out_idx; i++) {
        auto idx = output_node_map[device_id][i];
        auto n = node_ecs.node_ecs[idx];
        n->run_fn();
      }
    } */
    }
  }

  // TODO:
  // FIX!!
  //
  //  refactor mixer buffer sampling
  //  lets say we have a node that depends on other nodes
  //  i.e. it's a tail node in a list of nodes, dependent
  //  on the previous nodes for it's data
  void sample_mixer_buffer_out(std::size_t n_samples, T **buff,
                               std::string device_id) {
    auto cur_mixer_buffer = mixer_buffers[device_id];
    cur_mixer_buffer->read_n(*buff, n_samples);
    scratch_buff = std::vector<T>(n_samples, 0);
    for (int i = 0; i < output_node_idx_count[device_id]; i++) {
      auto idx = output_node_map[device_id][i];
      auto n = node_ecs.node_ecs[idx];
      std::vector<T> cur_node_buff;
      if (!n->node_buffer_fifo->dequeue(cur_node_buff)) {
        std::cout << "err on getting buffer sending silence\n";
        cur_node_buff = std::vector<T>(n_samples, 0);
      }
      if (scratch_buff.size() != cur_node_buff.size())
        std::cout << "mismatch!\n";

      if (mixer_fn != nullptr) {
        this->mixer_fn(this, &cur_node_buff, n_samples);
      }
    }
    cur_mixer_buffer->write_n(scratch_buff.data(), n_samples);
  }

  void sample_mixer_buffer_in(std::size_t n_bytes, T **buff,
                              std::string device_id) {
    if (run_multithreaded) {
      // if (!thread_pool->get_run_status()) {
      //  thread_pool->start_threads();
      // }
      // thread_pool->signal_in_threads();
    } else {
      for (size_t i = 0; i < num_in_idx; i++) {
        // TODO
        // only handle nodes that are associated with the device_id
        // auto node = node_ecs.node_ecs[in_node_ecs_idx[i]];
        // node->run_fn();
      }
    }
  }
};
