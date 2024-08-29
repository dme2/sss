#include "sss_file.hpp"
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// TODOS
//  [x] midi parsing
//  [] midi playback
//  [] midi streams
//  [] fix message data format

enum msg_type {
  NOTE_ON,
  NOTE_OFF,
  VELOCITY,
};

struct midi_message {
  std::byte channel;
  std::byte command;
  std::byte data1;
  std::byte data2;
  msg_type msg_type;

  enum msg {
    NORMAL = 0,
    TEMPO_CHAGE = 252,
    LOOP_START,
    LOOP_END,
    END_OF_TRACK
  };
};

struct midi_file_data {
  std::vector<midi_message> messages;
  std::vector<float> tick_times;
  int track_count;
  int resolution;
};

class SSS_MIDI_Parser {
public:
  SSS_MIDI_Parser(){};

  void skip_bytes(SSS_File *f, int n) {
    for (int i = 0; i < n; i++) {
      f->read_uint8();
    }
  }
  void discard_data(SSS_File *midi_file) {
    auto size = midi_file->read_int32_variable();
    std::cout << "discard size :" << size << std::endl;
    midi_file->discard_bytes(size);
  }

  int32_t read_tempo(SSS_File *midi_file) {
    auto size = midi_file->read_int32_variable();
    if (size != 3) {
      return -1; // err
    }

    auto b1 = midi_file->read_uint8();
    auto b2 = midi_file->read_uint8();
    auto b3 = midi_file->read_uint8();

    return (b1 << 16) | (b2 << 8) | b3;
  }

  // processes midi files into a series of midi events
  std::optional<midi_file_data> parse_midi(std::string file) {
    midi_file_data midi_data;
    auto midi_file = new SSS_File(file);
    std::string chunk_type = midi_file->read_string(4);
    if (chunk_type.compare("MThd")) {
      std::cout << "err on chunk type\n" << chunk_type.length() << std::endl;
      if (chunk_type == "MThd")
        std::cout << chunk_type << std::endl;
      return {}; // err
    }

    auto size = midi_file->read_int32_le();
    if (size != 6) {
      std::cout << size;
      std::cout << "err on chunk size\n";
      return {}; // err
    }

    auto fmt = midi_file->read_int16_le();
    if (fmt != 0) {
      std::cout << "err on chunk fmt\n";
      return {}; // err
    }

    auto track_count = midi_file->read_int16_be();
    auto resolution = midi_file->read_int16_be();

    midi_data.track_count = track_count;
    midi_data.resolution = resolution;

    std::vector<std::vector<midi_message>> msgs;
    std::vector<std::vector<int32_t>> ticks;

    for (auto i = 0; i < track_count; i++) {
      auto res = parse_track(midi_file);
      auto m = std::get<0>(res);
      auto t = std::get<1>(res);
      msgs.push_back(m);
      ticks.push_back(t);
    }

    auto merged = combine_tracks(midi_file, msgs, ticks, resolution);
    midi_data.messages = std::get<0>(merged);
    midi_data.tick_times = std::get<1>(merged);

    return midi_data;
  }

  std::tuple<std::vector<midi_message>, std::vector<int32_t>>
  parse_track(SSS_File *midi_file) {
    std::string chunk_type = midi_file->read_string(4);
    if (chunk_type.compare("MTrk")) {
      std::cout << "err on track type\n" << chunk_type.length() << std::endl;
      return {}; // err
    }

    auto size = midi_file->read_int32_le();

    std::vector<midi_message> msgs;
    std::vector<int32_t> ticks;

    int32_t tick = 0;
    uint8_t last_status = 0;

    bool exit = false;
    do {
      auto delta = midi_file->read_int32_variable();
      // std::cout << "delta " << delta << std::endl;
      auto first = midi_file->read_uint8();
      std::cout << "first: " << (int)first << std::endl;
      tick += delta;

      if ((first & 128) == 0) {
        auto command = last_status & 0xF0;
        if (command == 0xC0 || command == 0xD0) {
          midi_message msg;
          msg.channel = static_cast<std::byte>(last_status) & std::byte(0x0F);
          msg.command = static_cast<std::byte>(last_status) & std::byte(0xF0);
          msg.data1 = std::byte(0);
          msg.data2 = std::byte(0);

          msgs.push_back(msg);
          ticks.push_back(tick);
        } else {
          auto data2 = midi_file->read_uint8();
          midi_message msg;
          msg.channel = static_cast<std::byte>(last_status) & std::byte(0x0F);
          msg.command = static_cast<std::byte>(last_status) & std::byte(0xF0);
          msg.data1 = std::byte(0);
          msg.data2 = std::byte(data2);
          ticks.push_back(tick);
        }
        continue;
      }

      switch (first) {
      case 0xF0:
        discard_data(midi_file);
      case 0xF7:
        discard_data(midi_file);
      case 0xFF: {
        auto res = midi_file->read_uint8();
        if (res == 0x2F) {
          midi_file->read_uint8();
          // end of track
          ticks.push_back(tick);
          exit = true;
          break;
        } else if (res == 0x51) {
          ticks.push_back(tick);
        } else if (res == 0x58) {
          skip_bytes(midi_file, 5);
          break;
        } else if (res == 0x59) {
          skip_bytes(midi_file, 3);
          break;
        } else {
          discard_data(midi_file);
          break;
        }
      }
      default:
        auto command = first & 0xF0;
        if (command == 0xC0 || command == 0xD0) {
          midi_message msg;
          auto data1 = midi_file->read_uint8();
          msg.channel = static_cast<std::byte>(first) & std::byte(0x0F);
          msg.command = static_cast<std::byte>(first) & std::byte(0xF0);
          msg.data1 = std::byte(data1);
          msg.data2 = std::byte(0);

          msgs.push_back(msg);
          ticks.push_back(tick);
        } else {
          midi_message msg;
          auto data1 = midi_file->read_uint8();
          auto data2 = midi_file->read_uint8();
          msg.channel = static_cast<std::byte>(first) & std::byte(0x0F);
          msg.command = static_cast<std::byte>(first) & std::byte(0xF0);
          msg.data1 = std::byte(data1);
          msg.data2 = std::byte(data2);

          msgs.push_back(msg);
          ticks.push_back(tick);
        }
      }

      last_status = first;
    } while (!exit);

    return std::make_tuple(msgs, ticks);
  }

  std::tuple<std::vector<midi_message>, std::vector<float>>
  combine_tracks(SSS_File *midi_file,
                 std::vector<std::vector<midi_message>> &message_lists,
                 std::vector<std::vector<int32_t>> &tick_lists,
                 int32_t resolution) {
    std::vector<midi_message> msgs;
    std::vector<float> times;
    std::vector<size_t> indices(message_lists.size());

    int32_t curr_tick = 0;
    float curr_time = 0.0;

    float tempo = 120.0;
    bool exit = false;

    do {
      int32_t min_tick = INT_MAX;
      int32_t min_idx = -1;

      for (size_t ch = 0; ch < tick_lists.size(); ch++) {
        if (indices[ch] < tick_lists[ch].size()) {
          auto tick = tick_lists[ch][indices[ch]];
          if (tick < min_tick) {
            min_tick = tick;
            min_idx = (int32_t)ch;
          }
        }
      }

      if (min_idx == -1) {
        exit = true;
        continue;
      }

      auto next_tick = tick_lists[min_idx][indices[min_idx]];
      auto delta_tick = next_tick - curr_tick;
      float delta_time = 60.0 / ((float)resolution * tempo) * (float)delta_tick;

      curr_tick += delta_tick;
      curr_time += delta_time;

      auto message = message_lists[min_idx][indices[min_idx]];

      msgs.push_back(message);
      times.push_back(curr_time);

      indices[min_idx] += 1;

    } while (!exit);

    return std::make_tuple(msgs, times);
  }
};

// for midi nodes - handles playback and streaming io
// rendering should work as follows
// 1. a midi nodes callback is run
// 2. the callback runs send_midi function which delivers midi messages
//
class SSS_MIDI {
public:
  SSS_MIDI(){};

  midi_file_data *midi_file;
  size_t midi_file_idx;
  size_t midi_tick;

  void render(){};

  // launches a listener on another thread
  void listen(){};
};
