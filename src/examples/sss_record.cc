
#include "../sss.hpp"
#include <algorithm>
#include <chrono>

/* This is an example program that records using the default input device
 */

void mixer_fn(SSS_Mixer<float> *mixer, std::vector<float> *buff,
              std::size_t n_samples) {
  auto clamp = [](float a, float b) {
    if (a == 0)
      return b;
    return (a + b) / 2;
  };

  std::transform(mixer->scratch_buff.begin(),
                 mixer->scratch_buff.begin() + n_samples, buff->begin(),
                 mixer->scratch_buff.begin(), clamp);
  return;
}

std::size_t input_fn(SSS_Node<float> *node, std::size_t num_bytes) {
  float *rec_buff;
  node->node_input_buffer_fifo->dequeue(rec_buff);
  node->file->write_out_bytes(rec_buff, num_bytes);
  return 0;
}

int main() {
  using fn_type = std::function<std::size_t(SSS_Node<float> *, std::size_t)>;

  auto sss_handle = new SSS<float>(512, 2, 48000, SSS_FMT_S32);
  sss_handle->set_mixer_fn(mixer_fn);

  fn_type i_fn = input_fn;
  auto node1 =
      new SSS_Node<float>(FILE_INPUT, i_fn, 2, 1024, "80", "recording.raw");

  sss_handle->register_mixer_node_ecs(node1);

  // sss_handle->init_input_backend();
  sss_handle->start_input_backend();
  sss_handle->list_devices();
  std::this_thread::sleep_for(std::chrono::seconds(8));
  sss_handle->pause_input_backend();
  std::cout << "paused!\n";
  return 0;
}
