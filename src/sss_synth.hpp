#include <algorithm>
#include <chrono>
#include <vector>

struct synth_params {
  uint8_t channels;
  float pitch;
  float phase;
  float volume;
  float rate;
  float seconds_per_frame;

  synth_params(uint8_t chans, float pitch, float phase, float vol, float rate)
      : channels(chans), pitch(pitch), phase(phase), volume(vol), rate(rate) {
    seconds_per_frame = 1.0 / rate;
  }
};

template <typename T> class SSS_Synth {
public:
  std::size_t gen_sine(synth_params *params, std::vector<T> &out_vec,
                       size_t frame_count) {
    float phase_step = (2.0 * M_PI * params->pitch) / params->rate;
    for (size_t frame = 0; frame < frame_count / params->channels; frame++) {
      T sample = sin(params->phase) * params->volume;
      for (int i = 0; i < params->channels; i++) {
        out_vec[(frame * params->channels) + i] = sample;
      }
      params->phase += phase_step;
    }
    return frame_count;
  };
};
