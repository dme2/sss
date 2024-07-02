// #include "sss_coreaudio.hpp"
#include "sss.hpp"
#include <algorithm>
#include <chrono>
#include <thread>

// TODOs
// [x] nodes should be associated with a particular device ?
// [x] fix coreaudio input
// [x] improve file handling
// [x] thread pool for node functions
// [] set node channel data from deviceid for input and output
//   -> [x] fix callback so that we register one for each open device
// [] fix multithread node logic
// [] get alsa working
// [] cleanup unused code

// [] maybe change mixer data structures/logic. i.e. fix the node
//    indexing mess
//
// [] add_device(int device_id)
//   -> [] make_aggregate(int device_id)
//

// this file is mostly just a placeholder for various tests and examples

struct fn_data {
  double pitch{440.0};
  double phase{0.0};
  double volume{0.2};
};

int32_t prep_sample(double sample) {
  double range = (double)INT_MAX - (double)INT_MIN;
  double val = sample * range / 2.0;
  return (int32_t)val;
}

std::size_t input_fn(SSS_Node<float> *node, std::size_t num_bytes) {
  node->file->write_out_bytes(node->temp_buffer, node->buff_size);
  return 0;
}

std::size_t gen_sine1(SSS_Node<float> *node, std::size_t num_samples) {
  // double seconds_offset_ = *seconds_offset;
  auto sine_data = (fn_data *)node->fn_data;
  auto chans = node->channels;
  double float_sample_rate = 48000.0; // TODO
  double seconds_per_frame = 1.0 / float_sample_rate;
  double phaseStep = (2.0 * M_PI * sine_data->pitch) / float_sample_rate;
  // std::size_t frame_count = num_samples * ;
  //  float *out = new float[frame_count];
  //  node->temp_buffer = new float[frame_count];
  std::size_t frame_count = fmax(num_samples, node->node_queue->get_capacity());
  // std::cout << "node:" << node->device_id << " "
  //          << "frame_count : " << frame_count << std::endl;
  // if (frame_count <= 1)
  // return frame_count;
  bool enq;
  for (int frame = 0; frame < frame_count / chans; frame++) {
    auto sample = sin(sine_data->phase) * sine_data->volume;
    for (int i = 0; i < chans; i++) {
      // node->temp_buffer[(frame * chans) + i] = sample;
      enq = node->node_queue->enqueue(sample);
      // TODO: the thread which calls this should catch if
      // there are already too many samples in the queue
      if (!enq)
        return frame_count;
    }

    sine_data->phase += phaseStep;
  }

  while (sine_data->phase >= 2.0 * M_PI) {
    sine_data->phase -= 2.0 * M_PI;
  }

  return frame_count;
}

/*
fn_data *fd = new fn_data();

std::size_t gen_sine2(int32_t *buff, std::size_t num_frames) {
  // double seconds_offset_ = *seconds_offset;
  std::cout << num_frames << std::endl;
  auto sine_data = fd;
  double float_sample_rate = 48000.0;
  double seconds_per_frame = 1.0 / float_sample_rate;
  int frame_count = num_frames;
  double phaseStep = (2.0 * M_PI * sine_data->pitch) / float_sample_rate;
  int32_t *out = new int32_t[frame_count * 2];
  for (int frame = 0; frame < frame_count; frame++) {
    auto sample = prep_sample(sin(sine_data->phase) * sine_data->volume);
    for (int i = 0; i < 2; i++) {
      // buff[(frame * 2) + i] = sample;
      out[(frame * 2) + i] = sample;
    }
    sine_data->phase += phaseStep;
  }

  while (sine_data->phase >= 2.0 * M_PI) {
    sine_data->phase -= 2.0 * M_PI;
  }

  memcpy(buff, out, sizeof(int32_t) * frame_count * 2);
  return frame_count;
}
*/

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

std::size_t file_fn(SSS_Node<float> *n, std::size_t num_samples) {
  auto res = n->file->get_buffer(num_samples * n->channels);
  auto f32_res = convert_s16_to_f32(res, num_samples);
  n->temp_buffer = f32_res;
  return num_samples;
}

int main() {
  using fn_type = std::function<std::size_t(SSS_Node<float> *, std::size_t)>;

  // auto sss_handle = new SSS<float>(512, 2, 44100, SSS_FMT_S32);
  auto sss_handle = new SSS<float>(512, 2, 48000, SSS_FMT_S32);
  sss_handle->set_mixer_fn(mixer_fn);

  fn_type fn = gen_sine1;
  fn_type i_fn = input_fn;
  fn_type f_fn = file_fn;
  fn_data *fn_d1 = new fn_data();
  fn_data *fn_d2 = new fn_data();
  fn_d2->pitch = 328.0;

  // auto node1 = new SSS_Node<float>(OUTPUT, fn, 2, 1024, "default", fn_d1);
  // auto node2 = new SSS_Node<float>(OUTPUT, fn, 2, 1024, "default2", fn_d2);
  auto node3 = new SSS_Node<float>(FILE_OUT, f_fn, 2, 1024, 73, "output2.raw");
  auto node4 =
      new SSS_Node<float>(FILE_INPUT, i_fn, 2, 1024, 73, "output_test");

  // node1->next = node2;
  // sss_handle->register_mixer_node(node1);
  // sss_handle->register_mixer_node(node2);
  // sss_handle->register_mixer_node(node3);
  // sss_handle->register_mixer_node(node4);
  sss_handle->init_input_backend();
  sss_handle->init_output_backend();
  //   sss_handle->list_devices();
  sss_handle->start_output_backend();
  // sss_handle->start_input_backend();
  std::this_thread::sleep_for(std::chrono::seconds(8));
  sss_handle->pause_output_backend();
  std::cout << "paused!\n";
  return 0;
}
