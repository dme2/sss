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
  int resample{1};
  unsigned int buffer_time{500000};
  unsigned int period_time{100000};
  int period_event{0};
  int channels{2};

  snd_pcm_sframes_t buffer_size;

  std::string device_id;

  AlsaBackend(int sample_rate, int channels, int bytes_per_frame,
              int frame_count)
      : sample_rate(sample_rate), channels(channels), frame_count(frame_count),
        bytes_per_frame(bytes_per_frame) {
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

  // creates a new backend
  AlsaBackend *init_alsa_out(std::string out_device = "plughw:0,0") {
    std::cout << out_device << std::endl;
    int err;
    this->handle = NULL;
    err = snd_pcm_open(&this->handle, out_device.c_str(),
                       SND_PCM_STREAM_PLAYBACK, 0);

    if (err < 0) {
      std::cout << "err on pcm open\n";
      EXIT_FAILURE;
    }

    this->device_id = out_device;
    // setup hw_params
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_malloc(&hw_params);
    this->format = SND_PCM_FORMAT_FLOAT_LE;

    if (set_hw_params(this->handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED) <
        0)
      std::cout << "err on hw params\n";

    /*
if ((err = snd_pcm_hw_params_any(this->handle, hw_params)) < 0) {
  EXIT_FAILURE;
}

bool is_interleaved = true;

if (snd_pcm_hw_params_set_access(this->handle, hw_params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED) >= 0)
  is_interleaved = true;

snd_pcm_hw_params_set_format(this->handle, hw_params, this->format);

unsigned int periods = 2; // ?
unsigned int sr = (unsigned int)this->sample_rate;

int dir = 0; // ?
snd_pcm_uframes_t samples_per_period = (snd_pcm_uframes_t)512;

snd_pcm_hw_params_set_rate_near(this->handle, hw_params, &sr, nullptr);
snd_pcm_hw_params_set_channels(this->handle, hw_params,
                               (unsigned int)this->channel_count);

    unsigned int buffer_time = 500000;
    snd_pcm_hw_params_set_buffer_time_near(this->handle, hw_params,
&buffer_time, nullptr);


    snd_pcm_uframes_t size;
    snd_pcm_hw_params_get_buffer_size(hw_params, &size);

//snd_pcm_hw_params_set_periods_near(this->handle, hw_params, &periods, &dir);
//snd_pcm_hw_params_set_period_size_near(this->handle, hw_params,
 //                                      &samples_per_period, &dir);

snd_pcm_uframes_t frames = 0;
int latency;

snd_pcm_hw_params_get_period_size(hw_params, &frames, &dir);
//snd_pcm_hw_params_get_periods(hw_params, &periods, &dir);

snd_pcm_hw_params(this->handle, hw_params);
    std::cout << frames << " " << frame_count << " " <<periods << std::endl;
    this->period_size = frames;
latency = (int)frames * ((int)periods - 1);

snd_pcm_sw_params_t *sw_params;
snd_pcm_sw_params_malloc(&sw_params);
snd_pcm_uframes_t boundary;

snd_pcm_sw_params_current(this->handle, sw_params);
snd_pcm_sw_params_get_boundary(sw_params, &boundary);
snd_pcm_sw_params_set_silence_threshold(this->handle, sw_params, 0);
snd_pcm_sw_params_set_silence_size(this->handle, sw_params, boundary);
snd_pcm_sw_params_set_start_threshold(this->handle, sw_params,
                                      samples_per_period);
snd_pcm_sw_params_set_stop_threshold(this->handle, sw_params, boundary);
snd_pcm_sw_params(this->handle, sw_params);

//snd_async_handler_t *callback_handler;

//snd_async_add_pcm_handler(&callback_handler, this->handle, async_callback,
 //                         this);

*/

    snd_pcm_sw_params_t *sw_params;
    snd_pcm_sw_params_malloc(&sw_params);

    if (set_sw_params(this->handle, sw_params) < 0)
      std::cout << "err on sw params\n";

    std::cout << "alsa setup!\n";
    return this;
  }

  int set_hw_params(snd_pcm_t *handle, snd_pcm_hw_params_t *params,
                    snd_pcm_access_t access) {
    unsigned int rrate;
    snd_pcm_uframes_t size;
    int err, dir;

    /* choose all parameters */
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
      printf("Broken configuration for playback: no configurations available: "
             "%s\n",
             snd_strerror(err));
      return err;
    }
    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
    if (err < 0) {
      printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
      return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, params, access);
    if (err < 0) {
      printf("Access type not available for playback: %s\n", snd_strerror(err));
      return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_FLOAT_LE);
    if (err < 0) {
      printf("Sample format not available for playback: %s\n",
             snd_strerror(err));
      return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(handle, params, channels);
    if (err < 0) {
      printf("Channels count (%u) not available for playbacks: %s\n", channels,
             snd_strerror(err));
      return err;
    }
    /* set the stream rate */
    rrate = sample_rate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
    if (err < 0) {
      printf("Rate %uHz not available for playback: %s\n", sample_rate,
             snd_strerror(err));
      return err;
    }
    if (rrate != sample_rate) {
      printf("Rate doesn't match (requested %uHz, get %iHz)\n", sample_rate,
             err);
      return -EINVAL;
    }
    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time,
                                                 &dir);
    if (err < 0) {
      printf("Unable to set buffer time %u for playback: %s\n", buffer_time,
             snd_strerror(err));
      return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &size);
    if (err < 0) {
      printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
      return err;
    }
    buffer_size = size;

    printf("buffer size is %li\n", buffer_size);
    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time,
                                                 &dir);
    if (err < 0) {
      printf("Unable to set period time %u for playback: %s\n", period_time,
             snd_strerror(err));
      return err;
    }
    err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
    if (err < 0) {
      printf("Unable to get period size for playback: %s\n", snd_strerror(err));
      return err;
    }
    period_size = size;
    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
      printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
      return err;
    }
    return 0;
  }

  int set_sw_params(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams) {
    int err;

    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0) {
      printf("Unable to determine current swparams for playback: %s\n",
             snd_strerror(err));
      return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(
        handle, swparams, (buffer_size / period_size) * period_size);
    if (err < 0) {
      printf("Unable to set start threshold mode for playback: %s\n",
             snd_strerror(err));
      return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt
     * like style processing) */
    err = snd_pcm_sw_params_set_avail_min(
        handle, swparams, period_event ? buffer_size : period_size);
    if (err < 0) {
      printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
      return err;
    }
    /* enable period events when requested */
    if (period_event) {
      err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
      if (err < 0) {
        printf("Unable to set period event: %s\n", snd_strerror(err));
        return err;
      }
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
      printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
      return err;
    }
    return 0;
  }

  void start_alsa_output() {
    // snd_pcm_start(this->handle);
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
  std::cout << "cb\n";
  AlsaBackend *data =
      (AlsaBackend *)snd_async_handler_get_callback_private(handler);
  snd_pcm_t *pcm_handle = snd_async_handler_get_pcm(handler);
  auto avail = snd_pcm_avail_update(pcm_handle) / 8;
  auto sss_backend = data->sss_backend;
  auto period_size = data->period_size;
  while (avail >= period_size) {
    float *buffer = new float[avail * 2];
    sss_backend->stage_out_nodes(data->device_id, avail);
    sss_backend->mixer->sample_output_nodes_ecs();
    sss_backend->get(avail, &buffer, data->device_id);
    snd_pcm_writei(pcm_handle, buffer, period_size);
    avail = snd_pcm_avail_update(pcm_handle) / 8;
  }
}
