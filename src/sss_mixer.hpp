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

// N.B. FILE_OUT = reading + playing an audio file
//      FILE_IN  = write audio to a file
// enum NodeType { OUTPUT, INPUT, FILE_OUT, FILE_INPUT };

/* There are really only two important graph structures for the audio nodes
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

// struct NodeLists { SSS_Node* next;  }

// TODO:
// can node functions be coroutines? (i.e. generators)
/*
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
*/

template <typename T> class SSS_Mixer {
public:
  using fn_type = std::function<std::size_t(SSS_Node<T> *, std::size_t)>;
  int out_channels{2};
  bool run_multithreaded{false};
  std::size_t buff_size;
  SSS_Buffer<T> *mixer_buffer;
  std::vector<T> scratch_buff;
  SSS_ThreadPool *thread_pool;

  std::vector<SSS_Node<T> *> output_nodes;
  std::vector<SSS_Node<T> *> input_nodes;
  // device_id -> Node
  std::unordered_map<int, SSS_Node<T> *> input_node_map;
  std::unordered_map<int, SSS_Node<T> *> output_node_map;

  // mixer function
  std::function<void(SSS_Mixer<T> *mixer, T *buff, std::size_t n_samples)>
      mixer_fn;

  SSS_Mixer(std::size_t buff_size, bool multithread = false,
      int mt_output = 0, int mt_input = 0)
      : buff_size(buff_size), run_multithreaded(multithread) {
    mixer_buffer = new SSS_Buffer<T>(buff_size);
    if (multithread) {
        thread_pool = new SSS_ThreadPool(mt_output, mt_input);
    }
  }

  void register_node(SSS_Node<T> *node) {
    if (node->nt == OUTPUT || node->nt == FILE_OUT) {
      output_nodes.push_back(node);
      if (run_multithreaded)
        thread_pool->register_out_thread(node);
    }
    else {
      // input_nodes.push_back(node);
      input_node_map[79] = node;
      if (run_multithreaded)
        thread_pool->register_in_thread(node);
    }
  }

  void new_node(NodeType nt, fn_type fn, int ch, void *fn_data,
                std::string device_id = "", std::string file_path = "") {
    SSS_Node<T> *node = nullptr;
    if ((nt == FILE_OUT || nt == FILE_INPUT) && file_path != "") {
      node = new SSS_Node<T>(nt, fn, ch, buff_size, device_id, file_path);
    } else {
      node = new SSS_Node<T>(nt, fn, ch, buff_size, device_id);
    }

    node->fn_data = fn_data;

    // TODO:
    // clean up
    // with the current stup, sequential nodes will need to be run in the same
    // thread
    if ((nt != FILE_OUT && nt != FILE_INPUT) && run_multithreaded) {
      if (node->next != nullptr) { // i.e. a sequential node
        auto cur_node = node;
        while (cur_node != nullptr) {
          //thread_pool->push_node_list_fn_rr(cur_node->fun, cur_node,
           //                                 cur_node->buff_size);
          auto cur_node = node->next;
        }
      } else {
        thread_pool->push_node_tp(node->fun, node, node->buff_size);
      }
    }

    if (nt == INPUT || nt == FILE_INPUT)
      input_nodes.push_back(node);
    else
      output_nodes.push_back(node);
  }

  void sample_output_nodes() {
    if (run_multithreaded) {
      // if (!thread_pool->get_run_status()) {
      //  thread_pool->start_threads();
      //   return;
      //}
      thread_pool->signal_threads();
    } else {
      // TODO:
      // sampling should take into account sequential nodes
      for (SSS_Node<T> *n : output_nodes) {
        if (n->fun != nullptr) {
          n->run_fn();
          // thread_pool->enqueue(n->fun, n, n->buff_size);
        }
      }
    }
  }

  // TODO:
  //  refactor mixer buffer sampling
  //  lets say we have a node that depends on other nodes
  //  i.e. it's a tail node in a list of nodes, dependent
  //  on the previous nodes for it's data
  //
  //  should this node be the only node with data?
  void sample_mixer_buffer_out(std::size_t n_samples, T **buff) {
    // send some data first, then do the sampling is a
    // better strategy maybe?
    mixer_buffer->read_n(*buff, n_samples);
    scratch_buff = std::vector<T>(n_samples, 0);
    for (auto n : output_nodes) {
      // TODO:
      // move this to a generic file function
      // if (n->nt == FILE_OUT) {
      // auto file_res = n->file->get_buffer(n_samples * out_channels);
      // auto f32_res = convert_s16_to_f32(file_res, n_samples);
      // if (mixer_fn != nullptr) {
      // this->mixer_fn(this, f32_res, n_samples);
      //}
      //} else {
      auto cur_node_buff = new T[n_samples];
      if (n->nt == FILE_OUT) {
        // auto res_samples = n->node_buffer->read_n(cur_node_buff, n_samples);
        cur_node_buff = n->temp_buffer;
      } else {
        float res;
        for (int i = 0; i < n_samples; i++) {
          if (n->node_queue->dequeue(res))
            cur_node_buff[i] = res;
          else
            std::cout << "err on dequeue!\n";
        }
      }
      //  mix into scratch_buff
      if (mixer_fn != nullptr) {
        this->mixer_fn(this, cur_node_buff, n_samples);
      }
      //}
      //  then write the scratch_buff to the mixer after
      // mixer_buffer.write_n(scratch_buff.data(), n_samples);

      // mixer_buffer.write_n(cur_node_buff, res_samples);
    }
    mixer_buffer->write_n(scratch_buff.data(), n_samples);
    // mixer_buffer.read_n(*buff, n_samples);
  }

  // TODO:
  // fixup input node sampling, currently it will write a chunk
  // of silence before writing actual recorded audio
  //
  // also fix the thread pool
  void sample_mixer_buffer_in(std::size_t n_bytes, T **buff) {
    if (run_multithreaded) {
      // if (!thread_pool->get_run_status()) {
      //  thread_pool->start_threads();
      // }
      thread_pool->signal_in_threads();
    } else {
      for (auto n : input_nodes) {
        n->run_fn();
      }
    }
  }
};
