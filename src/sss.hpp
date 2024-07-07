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
  void pcm_write_cb(AlsaBackend* alsa_backend) {
	auto sss_backend = alsa_backend->sss_backend;
		auto buff_size = alsa_backend->period_size;
		float* buffer = new float[buff_size];

	while(1) {
		//auto avail = snd_pcm_avail_update(alsa_backend->handle);
		//if (avail>0) {
  		sss_backend->stage_out_nodes(alsa_backend->device_id, buff_size/2);
  		sss_backend->mixer->sample_output_nodes_ecs();
  		sss_backend->get(buff_size/2, &buffer, alsa_backend->device_id);

    	auto res = snd_pcm_writei(alsa_backend->handle, buffer, (snd_pcm_uframes_t)buff_size/2);

		//}
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
      ca_backend->ca_open_device(node->device_id);
#endif 
#if SSS_HAVE_ALSA
	 alsa_backend->init_alsa_out(node->device_id);
#endif
      open_devices.insert(node->device_id);
    }
    auto ecs_idx = this->sss_backend->mixer->register_node_ecs(node);
    sss_backend->device_node_map[node->device_id].push_back(ecs_idx);
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

    alsa_input_backend = new AlsaInputBackend(rate, channels,
                                              bytes_per_frame, frame_count);
    alsa_input_backend->sss_backend = sss_backend;
#endif
  }

  void set_mixer_fn(mixer_fn m_fn) { sss_backend->set_mixer_fn(m_fn); }


  void init_output_backend() {
#if SSS_HAVE_COREAUDIO
    ca_backend->ca_open_device();
    open_devices.insert(73); // 73 = default coreaudio
#endif
#if SSS_HAVE_ALSA
    //alsa_backend->init_alsa_out();
#endif
  }

  void init_input_backend() {
#if SSS_HAVE_COREAUDIO
    ca_input_backend->ca_open_input();
#endif
#if SSS_HAVE_ALSA
    //alsa_input_backend->init_alsa_in();
#endif
  }
  void start_output_backend() {

#if SSS_HAVE_COREAUDIO
    for (auto i : open_devices)
      ca_backend->start(i);
#endif
#if SSS_HAVE_ALSA
	alsa_backend->start_alsa_output();
	std::jthread pcm_thread(pcm_write_cb, this->alsa_backend);
	pcm_thread.detach();
#endif
  }
  void start_input_backend() {
#if SSS_HAVE_COREAUDIO
    ca_input_backend->start_input();
#endif
  }

  void pause_output_backend() {
#if SSS_HAVE_COREAUDIO
    ca_backend->stop();
    // TODO: cleans up nullptr nodes in the ecs and removes
    // ecs_gc();
#endif
  }
  void pause_input_backend() {
#if SSS_HAVE_COREAUDIO
    ca_input_backend->stop_input();
#endif
  }
  void list_devices() {
#if SSS_HAVE_COREAUDIO
    ca_backend->list_devices();
#endif
  }

  // void pause_input_backend() { ca_input_backend->stop(); }
};
