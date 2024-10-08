#include "config.h"
#include <unordered_map>

#if SSS_HAVE_COREAUDIO
#include "sss_coreaudio.hpp"
#endif

#if SSS_HAVE_ALSA
#include "sss_alsa.hpp"
#endif

#include <set>

#if SSS_HAVE_ALSA
bool alsa_output_want_pause = false;
bool alsa_input_want_pause = false;
std::mutex pause_alsa_output_mutex;
std::mutex pause_alsa_input_mutex;

void pcm_write_cb(AlsaBackend *alsa_backend, snd_pcm_t *handle) {
  auto sss_backend = alsa_backend->sss_backend;
  auto buff_size = alsa_backend->period_size;
  float *buffer = new float[buff_size];

  while (!alsa_output_want_pause) {
    sss_backend->stage_out_nodes("plughw:0,0", buff_size / 2); // TODO
    sss_backend->mixer->tick_mixer();
    sss_backend->get(buff_size / 2, &buffer, "plughw:0,0"); // TODO

    auto res = snd_pcm_writei(handle, buffer,
                              (snd_pcm_uframes_t)buff_size / 2);
  }
}

void pcm_read_cb(AlsaInputBackend *alsa_backend, snd_pcm_t *handle) {
  auto sss_backend = alsa_backend->sss_backend;
  auto buff_size = alsa_backend->period_size;
  float *buffer = new float[buff_size];

  while (!alsa_input_want_pause) {
    auto res = snd_pcm_readi(handle, buffer,
                             (snd_pcm_uframes_t)buff_size);
    sss_backend->stage_in_nodes("plughw:0,0", buff_size, &buffer); // TODO
    sss_backend->mixer->tick_mixer();
  }
}

#endif

template <typename T> class SSS {
public:
  using fn_type = std::function<std::size_t(SSS_Node<T> *, std::size_t)>;
  using mixer_fn = std::function<void(SSS_Mixer<T> *mixer, std::vector<T> *buff,
                                      std::size_t n_samples)>;
  std::size_t frame_count;
  uint8_t channels;
  int32_t rate;
  std::size_t n_bytes;
  uint8_t bits_per_sample;
  uint8_t bytes_per_frame;
  SSS_Backend<T> *sss_backend; // holds mixer handle
  std::set<std::string> open_devices;

#if SSS_HAVE_COREAUDIO
  CoreAudioBackend<T> *ca_backend;
  CoreAudioInputBackend<T> *ca_input_backend;
#endif

#if SSS_HAVE_ALSA
  AlsaBackend *alsa_backend;
  AlsaInputBackend *alsa_input_backend;

  std::vector<snd_pcm_t *> input_handles;
  std::vector<snd_pcm_t *> output_handles;

#endif

  void push_node(NodeType nt, fn_type fn, void *fn_data,
                 std::string device_id = "default", std::string fp = "") {
    this->sss_backend->mixer->new_node(nt, fn, channels, fn_data, device_id,
                                       fp);
  }

  void register_mixer_node(SSS_Node<T> *node) {
    this->sss_backend->mixer->register_node(node);
  }

  void register_mixer_node_ecs(SSS_Node<T> *node) {
    if (!open_devices.contains(node->device_id)) {
#if SSS_HAVE_COREAUDIO
      if (node->nt != FILE_INPUT)
        ca_backend->ca_open_device(node->device_id);
      else
        ca_input_backend->ca_open_input();
#endif
#if SSS_HAVE_ALSA
      snd_pcm_t *pcm_handle;
      if (node->nt != FILE_INPUT) {
        pcm_handle = alsa_backend->init_alsa_out(node->device_id);
        output_handles.push_back(pcm_handle);
      } else {
        pcm_handle = alsa_input_backend->init_alsa_in(node->device_id);
        input_handles.push_back(pcm_handle);
      }
#endif
      open_devices.insert(node->device_id);
    }
    auto ecs_idx = this->sss_backend->mixer->register_node_ecs(node);
    if (node->nt != FILE_INPUT)
      sss_backend->output_device_node_map[node->device_id].push_back(ecs_idx);
    else
      sss_backend->input_device_node_map[node->device_id].push_back(ecs_idx);
  }

  SSS(std::size_t frame_count, uint8_t channels, int32_t rate, SSS_FMT fmt,
      bool run_multithreaded = false, int mt_out = 0, int mt_in = 0)
      : channels(channels), rate(rate), frame_count(frame_count) {
    bits_per_sample = sizeof(T) * 8;
    bytes_per_frame = channels * (bits_per_sample / 8);
    n_bytes = frame_count * channels * sizeof(T);
    sss_backend = new SSS_Backend<T>(channels, rate, frame_count, fmt, n_bytes,
                                     run_multithreaded, mt_out, mt_in);

#if SSS_HAVE_COREAUDIO
    ca_backend = new CoreAudioBackend<T>(rate, bits_per_sample, channels,
                                         bytes_per_frame, frame_count, fmt);
    ca_backend->sss_backend = sss_backend;
    ca_input_backend = new CoreAudioInputBackend<T>(
        rate, bits_per_sample, channels, bytes_per_frame, frame_count, fmt);
    ca_input_backend->backend = sss_backend;
#endif

#if SSS_HAVE_ALSA
    alsa_backend =
        new AlsaBackend(rate, channels, bytes_per_frame, frame_count);
    alsa_backend->sss_backend = sss_backend;

    alsa_input_backend =
        new AlsaInputBackend(rate, channels, bytes_per_frame, frame_count);
    alsa_input_backend->sss_backend = sss_backend;
#endif
  }

  void set_mixer_fn(mixer_fn m_fn) { sss_backend->set_mixer_fn(m_fn); }

  void init_output_backend() {
#if SSS_HAVE_COREAUDIO
    ca_backend->ca_open_device();
    open_devices.insert(); // 73 = default coreaudio
#endif
#if SSS_HAVE_ALSA
    // alsa_backend->init_alsa_out();
#endif
  }

  void init_input_backend() {
#if SSS_HAVE_COREAUDIO
    ca_input_backend->ca_open_input();
    open_devices.insert("80"); // 80 = default coreaudio input
#endif
#if SSS_HAVE_ALSA
    // alsa_input_backend->init_alsa_in();
#endif
  }
  void start_output_backend() {

#if SSS_HAVE_COREAUDIO
    for (auto i : open_devices)
      ca_backend->start(std::stoi(i));
#endif
#if SSS_HAVE_ALSA
    for (auto i : output_handles) {
      //alsa_backend->start_alsa_output(i);
      std::jthread pcm_thread(pcm_write_cb, this->alsa_backend, i);
      pcm_thread.detach();
    }
#endif
  }
  void start_input_backend() {
#if SSS_HAVE_COREAUDIO
    ca_input_backend->start_input();
#endif
#if SSS_HAVE_ALSA
    for (auto i : input_handles) {
      //alsa_input_backend->start_alsa_input();
      std::jthread pcm_read_thread(pcm_read_cb, this->alsa_input_backend, i);
      pcm_read_thread.detach();
    }
#endif
  }

  void pause_output_backend() {
#if SSS_HAVE_COREAUDIO
    ca_backend->stop();
#endif
#if SSS_HAVE_ALSA
  if(pause_alsa_output_mutex.try_lock()) {
    alsa_output_want_pause = true;
    pause_alsa_output_mutex.unlock(); 
  }

	for (auto n : output_handles)
	  alsa_backend->alsa_pause(n);
#endif
  }
  void pause_input_backend() {
#if SSS_HAVE_COREAUDIO
    ca_input_backend->stop_input();
#endif
#if SSS_HAVE_ALSA
    if(pause_alsa_input_mutex.try_lock()) {
      alsa_input_want_pause = true;
      pause_alsa_input_mutex.unlock(); 
    }
	
    for (auto n : input_handles)
	    alsa_input_backend->alsa_pause_input(n);
#endif
  }
  void list_devices() {
#if SSS_HAVE_COREAUDIO
    ca_backend->list_devices();
#endif
  }

  // void pause_input_backend() { ca_input_backend->stop(); }
};
