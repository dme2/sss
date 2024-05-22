
#include "../sss.hpp"
#include <algorithm>
#include <chrono>

/* This is an example program that plays 3 sine waves corresponding
 *  to an AMajor chord
 */

struct fn_data {
  double pitch{440.0};
  double phase{0.0};
  double volume{0.2};
};

std::size_t gen_sine1(SSS_Node<float> *node, std::size_t num_samples) {
  auto sine_data = (fn_data *)node->fn_data;
  auto chans = node->channels;
  double float_sample_rate = 48000.0; // TODO
  double seconds_per_frame = 1.0 / float_sample_rate;
  double phaseStep = (2.0 * M_PI * sine_data->pitch) / float_sample_rate;
  std::size_t frame_count = fmax(num_samples, node->node_queue->get_capacity());
  bool enq;
  for (int frame = 0; frame < frame_count / chans; frame++) {
    auto sample = sin(sine_data->phase) * sine_data->volume;
    for (int i = 0; i < chans; i++) {
      enq = node->node_queue->enqueue(sample);
      if (!enq)
        return frame_count;
    }

    sine_data->phase += phaseStep;
  }

  while (sine_data->phase >= 2.0 * M_PI) {
    sine_data->phase -= 2.0 * M_PI;
  }

  return frame_count;
}

void mixer_fn(SSS_Mixer<float> *mixer, float *buff, std::size_t n_samples) {
  auto clamp = [](float a, float b) {
    if (a == 0)
      return b;
    return (a + b) / 2;
  };

  std::transform(mixer->scratch_buff.begin(),
                 mixer->scratch_buff.begin() + n_samples, buff,
                 mixer->scratch_buff.begin(), clamp);
  return;
}

int main() {
  using fn_type = std::function<std::size_t(SSS_Node<float> *, std::size_t)>;

  auto sss_handle = new SSS<float>(512, 2, 48000, SSS_FMT_S32, true, 3, 1);
  sss_handle->set_mixer_fn(mixer_fn);

  fn_type fn = gen_sine1;
  fn_data *fn_d1 = new fn_data();
  fn_data *fn_d2 = new fn_data();
  fn_data *fn_d3 = new fn_data();
  fn_d2->pitch = 277.183;
  fn_d3->pitch = 329.628;

  auto node1 = new SSS_Node<float>(OUTPUT, fn, 2, 1024, "A", fn_d1);
  auto node2 = new SSS_Node<float>(OUTPUT, fn, 2, 1024, "Csharp", fn_d2);
  // auto node3 = new SSS_Node<float>(OUTPUT, fn, 2, 1024, "E", fn_d3);

  sss_handle->register_mixer_node(node1);
  sss_handle->register_mixer_node(node2);
  // sss_handle->register_mixer_node(node3);

  sss_handle->init_output_backend();
  sss_handle->start_output_backend();
  std::this_thread::sleep_for(std::chrono::seconds(8));
  sss_handle->pause_output_backend();
  std::cout << "paused!\n";
  return 0;
}
