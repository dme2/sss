#include "../sss.hpp"

#if SSS_HAVE_COREAUDIO
std::string device_id = "73";
#endif
#if SSS_HAVE_ALSA
std::string device_id = "plughw:0,0";
#endif

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

  auto sss_handle = new SSS<float>(512, 2, 48000, SSS_FMT_S32, false, 4, 1);
  sss_handle->set_mixer_fn(mixer_fn);

  auto midi_handle = new SSS_MIDI_Parser();
  auto midi_data = midi_handle->parse_midi(
      "/Users/daveevans/projects/sss/src/examples/Twinkle.mid");
  if (midi_data.has_value()) {
    std::cout << "parsed succussfully!\n";
  }
  auto md = midi_data.value().messages;
  auto mt = midi_data.value().tick_times;
  for (auto message : md) {
    std::cout << (int)message.command << std::endl;
  }

  for (auto time : mt) {
    std::cout << time << std::endl;
  }

  for (int i = 0; i < mt.size(); i++) {
    std::cout << (int)md[i].command << " " << (int)md[i].data1 << " "
              << (int)md[i].data2 << " " << mt[i] << std::endl;
  }

  auto midi_node =
      new SSS_Node<float>(MIDI_OUT, "midi_out", device_id, &midi_data.value());

  midi_node->midi_handle->midi_file = &midi_data.value();
  auto synth = new SSS_Synth<float>();
  midi_node->setup_midi_synth(synth);
  sss_handle->register_mixer_node_ecs(midi_node);
  sss_handle->start_output_backend();
  std::this_thread::sleep_for(std::chrono::seconds(5));
  return 0;
}
