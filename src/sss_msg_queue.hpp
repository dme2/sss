#include "sss_node.hpp"
#include <mutex>
#include <optional>

enum Action { SAMPLE_NODE, HANDLE_INPUT, PAUSE, CHANGE_VOLUME };

struct SSS_Msg {
  size_t node_idx;
  std::string device_id;
  Action action;
  float vol;
  size_t req_bytes;

  SSS_Msg(size_t node, std::string dev, Action a, float v, size_t rb)
      : node_idx(node), device_id(dev), action(a), vol(v), req_bytes(rb) {}

  SSS_Msg() {}
};

class SSS_Msg_Queue {
public:
  SSS_Fifo<SSS_Msg> *msg_queue;
  std::mutex queue_mutex;

  void push_msg(size_t node_idx, std::string device_id, Action action,
                float volume, size_t rb) {
    auto msg = SSS_Msg(node_idx, device_id, action, volume, rb);
    msg_queue->enqueue(msg);
  }

  std::optional<SSS_Msg> pop_msg() {
    SSS_Msg res;
    if (queue_mutex.try_lock()) {
      if (msg_queue->dequeue(res)) {
        queue_mutex.unlock();
        return res;
      } else {
        queue_mutex.unlock();
        return {};
      }
    } else
      return {};
  }

  SSS_Msg_Queue() { msg_queue = new SSS_Fifo<SSS_Msg>(100); }
};
