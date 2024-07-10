Simple Sound System.

A node based audio programming backend. Currently supports CoreAudio and Alsa, more audio backends will be added in the future.

To build on Linux or macos, run the following commands:

```
mkdir build
cd build
cmake ..
make
```

Several example programs will be built and runnable from the build directory.

Here's a simplified version of one of the examples, which plays a sine wave at 440hz

```c++
#include "sss.hpp"
#include <algorithm>
#include <chrono>

struct fn_data {
  double pitch{440.0};
  double phase{0.0};
  double volume{0.2};
};

std::size_t gen_sine1(SSS_Node<float> *node, std::size_t num_samples) {
  auto sine_data = (fn_data *)node->fn_data;
  auto chans = node->channels;
  double float_sample_rate = 48000.0;
  double seconds_per_frame = 1.0 / float_sample_rate;
  double phaseStep = (2.0 * M_PI * sine_data->pitch) / float_sample_rate;

  size_t frame_count = num_samples;
  std::vector<float> samples(num_samples, 0);

  for (int frame = 0; frame < frame_count / chans; frame++) {
    float sample = sin(sine_data->phase) * sine_data->volume;
    for (int i = 0; i < chans; i++) {
      samples[(frame * chans) + i] = sample;
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

  auto sss_handle = new SSS<float>(1024, 2, 48000, SSS_FMT_FL32, true, 4, 1);
  sss_handle->set_mixer_fn(mixer_fn);

  fn_type fn = gen_sine1;
  fn_data *fn_d1 = new fn_data();

  // 73 = my default coreaudio output device
  auto node1 = new SSS_Node<float>(OUTPUT, fn, 2, 1024, "A", "73", fn_d1);

  sss_handle->register_mixer_node_ecs(node1);

  sss_handle->start_output_backend();
  std::this_thread::sleep_for(std::chrono::seconds(5));
  sss_handle->pause_output_backend();
  std::cout << "paused!\n";
  return 0;
```

(a work in progress!)
