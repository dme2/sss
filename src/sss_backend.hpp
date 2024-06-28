// #if SSS_HAVE_ALSA
// #include "sss_alsa.hpp"
// #endif

// #if SSS_HAVE_CORE
// #include "sss_coreaudio.hpp"
// #endif
#include "sss_mixer.hpp"
// #include "sss_util.hpp"
#include <iostream>
#include <string>
#include <thread>

template <typename T, std::size_t S>
std::thread start_mixer(SSS_Mixer<T> *mixer) {
  std::thread athread(&SSS_Mixer<T>::run_mixer, mixer);
  athread.detach();
  return athread;
}

template <typename T> class SSS_Backend {
public:
  uint8_t channels;
  int sample_rate;
  int frame_count; // 512, 1024, 2048...
  SSS_FMT fmt;
  int fmt_bits;
  int fmt_bytes;
  std::size_t num_bytes; // frame_count * channels * fmt_bytes;
  std::unordered_map<uint32_t, std::vector<size_t>> device_node_map;
  SSS_Mixer<T> *mixer;

  SSS_Backend(uint8_t channels, int sample_rate, int frame_count, SSS_FMT fmt,
              std::size_t num_bytes, bool run_multithreaded = false,
              int mt_out = 0, int mt_in = 0)
      : channels(channels), sample_rate(sample_rate), frame_count(frame_count),
        fmt(fmt) {
    mixer = new SSS_Mixer<T>(num_bytes, run_multithreaded, mt_out, mt_in);
    fmt_bits = fmt_to_bits(fmt);
    fmt_bytes = fmt_to_bytes(fmt);
    num_bytes = num_bytes;

    // start_mixer<T, S>(mixer);
  }
  using mixer_fn =
      std::function<void(SSS_Mixer<T> *mixer, T *buff, std::size_t n_samples)>;
  void set_mixer_fn(mixer_fn mixer_fn) { mixer->mixer_fn = mixer_fn; }

  // TODO: rename?
  void get(std::size_t n_frames, T **buff, uint32_t device_id) {
    mixer->sample_mixer_buffer_out(n_frames * channels, buff, device_id);
  }

  // adds output nodes belonging to device_id to the message queue
  void stage_out_nodes(uint32_t device_id) {
    for (auto n : device_node_map[device_id]) {
      // std::cout << "enqueu!\n";
      mixer->msg_queue->push_msg(n, device_id, SAMPLE_NODE, 0);
    }
  }

  void stage_in_nodes(uint32_t device_id) {}

  void handle_in(std::size_t n_bytes, T **buff, uint32_t device_id) {
    mixer->sample_mixer_buffer_in(n_bytes, buff, device_id);
  }
};
