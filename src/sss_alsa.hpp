#include <alsa/asoundlib.h>
#include <alsa/global.h>
#include <cstdlib>
#include <string>
// #include "sss_backend.hpp"

#include "sss_alsa_input.hpp"

class AlsaBackend;

void async_callback(snd_async_handler_t *handler);

class AlsaBackend {
public:
  snd_pcm_t *handle;
  snd_pcm_chmap_t *chmap;
  int chmap_size;
  int channel_count;
  snd_pcm_uframes_t offset;
  snd_pcm_access_t access;
  snd_pcm_uframes_t buffer_frame_size;
  int sample_buffer_size;
  int8_t *sample_buffer;
  // int poll_fd_count;
  // int poll_fd_count_with_extra;
  // struct pollfd *poll_fds;
  snd_pcm_uframes_t period_size;
  int write_frame_count;
  int frame_count; // for output?
  bool paused;
  std::string out_device;
  std::string in_device;
  int sample_rate;
  snd_pcm_format_t format;
  double sw_latency;
  int bytes_per_frame;
  int bytes_per_sample;
  SSS_Backend<float> *sss_backend;

  AlsaBackend(int sample_rate, int channels, int bytes_per_frame,
              int frame_count)
      : sample_rate(sample_rate), channel_count(channels),
        frame_count(frame_count), bytes_per_frame(bytes_per_frame) {
    this->out_device = "default";
  }

  /*
  void async_callback(snd_async_handler_t *handler) {
      sss_backend->mixer->sample_output_nodes();
      snd_pcm_t *pcm_handle = snd_async_handler_get_pcm(handler);
      AlsaBackend* data =
  (AlsaBackend*)snd_async_handler_get_callback_private(handler); auto avail =
  snd_pcm_avail_update(pcm_handle); float* buffer = new float[avail];
      sss_backend->get(avail, &buffer);
      while (avail >= period_size) {
          snd_pcm_writei(pcm_handle, buffer, period_size);
          avail = snd_pcm_avail_update(pcm_handle);
      }
  }
  */

  snd_pcm_format_t get_format(SSS_FMT fmt) {
    switch (fmt) {
    case SSS_FMT_S8:
      return SND_PCM_FORMAT_S8;
    case SSS_FMT_U8:
      return SND_PCM_FORMAT_U8;
    case SSS_FMT_S16:
      return SND_PCM_FORMAT_S16_LE;
    case SSS_FMT_U16:
      return SND_PCM_FORMAT_U16_LE;
    case SSS_FMT_S32:
      return SND_PCM_FORMAT_S32_LE;
    case SSS_FMT_U32:
      return SND_PCM_FORMAT_U32_LE;
    case SSS_FMT_FL32:
      return SND_PCM_FORMAT_FLOAT_LE;
    case SSS_FMT_FL64:
      return SND_PCM_FORMAT_FLOAT64_LE;
    }
    return SND_PCM_FORMAT_UNKNOWN;
  }

  // creates a new backend
  AlsaBackend *init_alsa_out(std::string out_device = "default") {
    int err;
    err = snd_pcm_open(&this->handle, out_device.c_str(),
                       SND_PCM_STREAM_PLAYBACK, 0);

    if (err < 0) {
      std::cout << "err on pcm open";
      EXIT_FAILURE;
    }

    // setup hw_params
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_alloca(&hw_params);

    if ((err = snd_pcm_hw_params_any(this->handle, hw_params)) < 0) {
      EXIT_FAILURE;
    }

    bool is_interleaved = true;

    if (snd_pcm_hw_params_set_access(this->handle, hw_params,
                                     SND_PCM_ACCESS_RW_INTERLEAVED) >= 0)
      is_interleaved = true;

    this->format = SND_PCM_FORMAT_FLOAT_LE;
    snd_pcm_hw_params_set_format(this->handle, hw_params, this->format);

    unsigned int periods = 2; // ?
    unsigned int sr = (unsigned int)this->sample_rate;

    int dir = 0; // ?
    snd_pcm_uframes_t samples_per_period = (snd_pcm_uframes_t)frame_count;

    snd_pcm_hw_params_set_rate_near(this->handle, hw_params, &sr, nullptr);
    snd_pcm_hw_params_set_channels(this->handle, hw_params,
                                   (unsigned int)this->channel_count);

    snd_pcm_hw_params_set_periods_near(this->handle, hw_params, &periods, &dir);
    snd_pcm_hw_params_set_period_size_near(this->handle, hw_params,
                                           &samples_per_period, &dir);
    snd_pcm_hw_params(this->handle, hw_params);

    snd_pcm_uframes_t frames = 0;
    int latency;

    snd_pcm_hw_params_get_period_size(hw_params, &frames, &dir);
    snd_pcm_hw_params_get_periods(hw_params, &periods, &dir);

    latency = (int)frames * ((int)periods - 1);

    snd_pcm_sw_params_t *sw_params;
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_uframes_t boundary;

    snd_pcm_sw_params_current(this->handle, sw_params);
    snd_pcm_sw_params_get_boundary(sw_params, &boundary);
    snd_pcm_sw_params_set_silence_threshold(this->handle, sw_params, 0);
    snd_pcm_sw_params_set_silence_size(this->handle, sw_params, boundary);
    snd_pcm_sw_params_set_start_threshold(this->handle, sw_params,
                                          samples_per_period);
    snd_pcm_sw_params_set_stop_threshold(this->handle, sw_params, boundary);
    snd_pcm_sw_params(this->handle, sw_params);

    snd_async_handler_t *callback_handler;

    snd_async_add_pcm_handler(&callback_handler, this->handle, async_callback,
                              this);

    return this;
  }

  int xrun_recovery(snd_pcm_t *handle, int err) {
    if (err == -EPIPE) {
      err = snd_pcm_prepare(handle);
      if (err > 0) {
        // TODO: underflow
        exit(1);
      } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN) {
          // spin until suspend flag is released
        }
        if (err < 0)
          err = snd_pcm_prepare(handle);
        if (err >= 0) {
          // underflow callback
          exit(1);
        }
      }
    }
    return err;
  }
};

void async_callback(snd_async_handler_t *handler) {
  AlsaBackend *data =
      (AlsaBackend *)snd_async_handler_get_callback_private(handler);
  auto sss_backend = data->sss_backend;
  sss_backend->mixer->sample_output_nodes();
  snd_pcm_t *pcm_handle = snd_async_handler_get_pcm(handler);
  auto avail = snd_pcm_avail_update(pcm_handle);
  float *buffer = new float[avail];
  sss_backend->get(avail, &buffer);
  auto period_size = data->period_size;
  while (avail >= period_size) {
    snd_pcm_writei(pcm_handle, buffer, period_size);
    avail = snd_pcm_avail_update(pcm_handle);
  }
}
