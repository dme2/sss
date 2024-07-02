#include "sss_node.hpp"
#include <mutex>
#include <optional>

enum Action { SAMPLE_NODE, HANDLE_INPUT, PAUSE, CHANGE_VOLUME };

struct SSS_Msg {
  size_t node_idx;
  uint32_t device_id;
  Action action;
  float vol;

  SSS_Msg(size_t node, uint32_t dev, Action a, float v)
      : node_idx(node), device_id(dev), action(a), vol(v) {}

  SSS_Msg() {}
};

class SSS_Msg_Queue {
public:
  SSS_Fifo<SSS_Msg> *msg_queue;
  std::mutex queue_mutex;

  void push_msg(size_t node_idx, uint32_t device_id, Action action,
                float volume) {
    auto msg = SSS_Msg(node_idx, device_id, action, volume);
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
