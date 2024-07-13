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
  snd_pcm_uframes_t period_size;
  bool pause;
  std::string out_device;
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
      : sample_rate(sample_rate),
        channels(channels),
        period_size(frame_count),
        bytes_per_frame(bytes_per_frame) {}

  snd_pcm_t *init_alsa_out(std::string out_device = "plughw:0,0") {
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
      printf(
          "Broken configuration for playback: no configurations available: "
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

    std::cout << sample_rate << std::endl;
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

    // TODO:  seems like we can't manually set the period size to
    //	      anything lower than 4800? 2400 causes a ton of underruns
    //        but 4800 seems like an unnecessarily large buffer
    //
    //        maybe try increasing the  sample rate and see what happns?
    //
    // err = snd_pcm_hw_params_set_period_size(handle, params, , dir);
    // if (err < 0) {
    //       printf("Unable to set period size for playback: %s\n",
    //       snd_strerror(err));
    //      return err;
    //   }

    std::cout << "period size: " << period_size << std::endl;

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

  void alsa_pause(snd_pcm_t *handle) {
    if (!pause) {
      snd_pcm_pause(handle, 1);
      pause = true;
    }
  }

  void alsa_resume(snd_pcm_t *handle) {
    if (pause) {
      snd_pcm_pause(handle, 0);
      pause = false;
    }
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
        if (err < 0) err = snd_pcm_prepare(handle);
        if (err >= 0) {
          // underflow callback
          exit(1);
        }
      }
    }
    return err;
  }

  void list_devices() {
    int status;
    int card = -1;
    snd_ctl_t *ctl_handle;
    snd_ctl_card_info_t *card_info;
    snd_pcm_info_t *pcm_info;

    snd_ctl_card_info_alloca(&card_info);
    snd_pcm_info_alloca(&pcm_info);

    while (snd_card_next(&card) >= 0 && card >= 0) {
      char card_name[32];
      sprintf(card_name, "hw:%d", card);

      status = snd_ctl_open(&ctl_handle, card_name, 0);
      if (status < 0) {
        fprintf(stderr, "Error opening control for card %d: %s\n", card,
                snd_strerror(status));
        continue;
      }

      status = snd_ctl_card_info(ctl_handle, card_info);
      if (status < 0) {
        fprintf(stderr, "Error getting card info for card %d: %s\n", card,
                snd_strerror(status));
        snd_ctl_close(ctl_handle);
        continue;
      }

      printf("Card %d: %s [%s]\n", card, snd_ctl_card_info_get_id(card_info),
             snd_ctl_card_info_get_name(card_info));

      int device = -1;
      while (snd_ctl_pcm_next_device(ctl_handle, &device) >= 0 && device >= 0) {
        snd_pcm_info_set_device(pcm_info, device);
        snd_pcm_info_set_subdevice(pcm_info, 0);
        snd_pcm_info_set_stream(pcm_info, SND_PCM_STREAM_PLAYBACK);

        status = snd_ctl_pcm_info(ctl_handle, pcm_info);
        if (status >= 0) {
          printf("  Playback Device %d: %s [%s]\n", device,
                 snd_pcm_info_get_id(pcm_info),
                 snd_pcm_info_get_name(pcm_info));
        }

        snd_pcm_info_set_stream(pcm_info, SND_PCM_STREAM_CAPTURE);
        status = snd_ctl_pcm_info(ctl_handle, pcm_info);
        if (status >= 0) {
          printf("  Capture Device %d: %s [%s]\n", device,
                 snd_pcm_info_get_id(pcm_info),
                 snd_pcm_info_get_name(pcm_info));
        }
      }

      snd_ctl_close(ctl_handle);
    }
  }
};
