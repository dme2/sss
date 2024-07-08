
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
  size_t frame_count = num_samples;
  std::vector<float> samples(num_samples, 0);

  // bool enq;
  for (int frame = 0; frame < frame_count / chans; frame++) {
    float sample = sin(sine_data->phase) * sine_data->volume;
    for (int i = 0; i < chans; i++) {
      samples[(frame * chans) + i] = sample;
      // enq = node->node_queue->enqueue(sample);
      // if (!enq) {
      //  return frame_count;
      // }
    }
    sine_data->phase += phaseStep;
  }

  while (sine_data->phase >= 2.0 * M_PI) {
    sine_data->phase -= 2.0 * M_PI;
  }

  if (!node->node_buffer_fifo->enqueue(samples))
    std::cout << "no space!\n";

  return frame_count;
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

  auto sss_handle = new SSS<float>(512, 2, 48000, SSS_FMT_S32, true, 3, 1);
  sss_handle->set_mixer_fn(mixer_fn);

  fn_type fn = gen_sine1;
  fn_data *fn_d1 = new fn_data();
  fn_data *fn_d2 = new fn_data();
  fn_data *fn_d3 = new fn_data();
  fn_d2->pitch = 277.183;
  fn_d3->pitch = 329.628;

  auto node1 = new SSS_Node<float>(OUTPUT, fn, 2, 1024, "A", "73", fn_d1);
  // auto node2 = new SSS_Node<float>(OUTPUT, fn, 2, 1024, "C#", "default",
  // fn_d2);
  //  auto node3 = new SSS_Node<float>(OUTPUT, fn, 2, 1024, "E", 85, fn_d3);

  // sss_handle->register_mixer_node(node1);
  // sss_handle->register_mixer_node(node2);
  // sss_handle->register_mixer_node(node3);
  sss_handle->register_mixer_node_ecs(node1);
  // sss_handle->register_mixer_node_ecs(node2);
  //   sss_handle->register_mixer_node_ecs(node3);

  // warmup the nodes
  for (int i = 0; i < 10; i++) {
    // node1->run_fn(1024);
    // node2->run_fn();
  }

  sss_handle->init_output_backend();
  // sss_handle->list_devices();
  sss_handle->start_output_backend();
  std::this_thread::sleep_for(std::chrono::seconds(5));
  sss_handle->pause_output_backend();
  std::cout << "paused!\n";
  return 0;
}
