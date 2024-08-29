#include "../sss.hpp"

int main() {
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
  auto midi_node =
      new SSS_Node<float>(MIDI_OUT, "midi_out", "default", &midi_data.value());
  return 0;
}
