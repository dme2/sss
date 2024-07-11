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

  snd_pcm_t *init_alsa_out(std::string out_device = "plughw:0,0") {
    std::cout << out_device << std::endl;
    snd_pcm_t *cur_handle = nullptr;
    int err;

    err = snd_pcm_open(&cur_handle, out_device.c_str(), SND_PCM_STREAM_PLAYBACK,
                       0);

    if (err < 0) {
      std::cout << "err on pcm open\n";
      EXIT_FAILURE;
    }

    // this->device_id = out_device;
    //  setup hw_params
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_malloc(&hw_params);
    this->format = SND_PCM_FORMAT_FLOAT_LE;

    if (set_hw_params(cur_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
      std::cout << "err on hw params\n";

    snd_pcm_sw_params_t *sw_params;
    snd_pcm_sw_params_malloc(&sw_params);

    if (set_sw_params(cur_handle, sw_params) < 0)
      std::cout << "err on sw params\n";

    std::cout << "alsa setup!\n";
    return cur_handle;
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

  void alsa_pause(snd_pcm_t* handle) {
	snd_pcm_pause(handle, 1);
  }

  void alsa_resume(snd_pcm_t* handle) {
	snd_pcm_pause(handle, 0);
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
