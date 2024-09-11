#include "../sss.hpp"
#include <algorithm>
#include <chrono>

/* This is an example program that plays a sine wave at 440 hz
 */

#if SSS_HAVE_COREAUDIO
std::string device_id = "73";
#endif
#if SSS_HAVE_ALSA
std::string device_id = "plughw:0,0";
#endif

auto params = new synth_params(2, 440.0, 0.0, 0.2, 48000.0);
auto sine_osc = new SSS_Synth<float>();

std::size_t node_fn(SSS_Node<float> *node, std::size_t num_samples) {
  std::vector<float> samples(num_samples, 0);
  auto res = sine_osc->gen_sine(params, samples, num_samples);
  if (!node->node_buffer_fifo->enqueue(samples))
    std::cout << "no space!\n";

  return res;
}

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

int main() {
  using fn_type = std::function<std::size_t(SSS_Node<float> *, std::size_t)>;

  auto sss_handle = new SSS<float>(512, 2, 48000, SSS_FMT_S32, true, 4, 1);
  sss_handle->set_mixer_fn(mixer_fn);

  fn_type fn = node_fn;

  auto node1 = new SSS_Node<float>(OUTPUT, fn, 2, 1024, "A", device_id);

  sss_handle->register_mixer_node_ecs(node1);

  //  sss_handle->list_devices();
  sss_handle->start_output_backend();
  std::this_thread::sleep_for(std::chrono::seconds(5));
  sss_handle->pause_output_backend();
  std::cout << "paused!\n";
  return 0;
}
