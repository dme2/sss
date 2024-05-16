#include "config.h"

#if SSS_HAVE_COREAUDIO
#include "sss_coreaudio.hpp"
#endif

#if SSS_HAVE_ALSA
#include "sss_alsa.hpp"
#endif


// the aim of this file is to expose a good portion of the
// simple sound system api
// such as
// pause()
// play()
// startup()
// new_node()

// T here refers to the format of the buffer data
// probably want to use SSS_FMT in the future
template <typename T> class SSS {
public:
  using fn_type = std::function<std::size_t(SSS_Node<T> *, std::size_t)>;
  using mixer_fn =
      std::function<void(SSS_Mixer<T> *mixer, T *buff, std::size_t n_samples)>;
  std::size_t frame_count;
  uint8_t channels;
  int32_t rate;
  std::size_t n_bytes;
  uint8_t bits_per_sample;
  uint8_t bytes_per_frame;
  SSS_Backend<T> *sss_backend; // holds mixer handle

  #if SSS_HAVE_COREAUDIO
  CoreAudioBackend<T> *ca_backend;
  CoreAudioInputBackend<T> *ca_input_backend;
  #endif

  #if SSS_HAVE_ALSA
  AlsaAlsaBackend *alsa_backend;
  AlsaAlsaInputBackend *alsa_input_backend;
  #endif

  void push_node(NodeType nt, fn_type fn, void *fn_data,
                 std::string device_id = "default", std::string fp = "") {
    this->sss_backend->mixer->new_node(nt, fn, channels, fn_data, device_id,
                                       fp);
  }

  void register_mixer_node(SSS_Node<T> *node) {
    this->sss_backend->mixer->register_node(node);
  }

  SSS(std::size_t frame_count, uint8_t channels, int32_t rate, SSS_FMT fmt)
      : channels(channels), rate(rate), frame_count(frame_count) {
    bits_per_sample = sizeof(T) * 8;
    bytes_per_frame = channels * (bits_per_sample / 8);
    ca_backend = new CoreAudioBackend<T>(rate, bits_per_sample, channels,
                                         bytes_per_frame, frame_count, fmt);
    n_bytes = frame_count * channels * sizeof(T);
    sss_backend = new SSS_Backend<T>(channels, rate, frame_count, fmt, n_bytes);
    ca_backend->sss_backend = sss_backend;
    ca_input_backend = new CoreAudioInputBackend<T>(
        rate, bits_per_sample, channels, bytes_per_frame, frame_count, fmt);
    ca_input_backend->backend = sss_backend;
  }

  void set_mixer_fn(mixer_fn m_fn) { sss_backend->set_mixer_fn(m_fn); }
  void init_output_backend() {
      // warmup our nodes
      // TODO: probably want a safer way to do this
      sss_backend->mixer->sample_output_nodes();
      ca_backend->ca_open();
  }
  void init_input_backend() { ca_input_backend->ca_open_input(); }
  void start_output_backend() { ca_backend->start(); }
  void start_input_backend() { ca_input_backend->start_input(); }
  void pause_output_backend() { ca_backend->stop(); }
  void pause_input_backend() { ca_input_backend->stop_input(); }
  void list_devices() { ca_backend->list_devices(); }
  // void pause_input_backend() { ca_input_backend->stop(); }
};
