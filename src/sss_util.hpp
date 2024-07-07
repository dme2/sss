#include <cstring>

enum SSS_FMT {
  SSS_FMT_S8,   // SND_PCM_FORMAT_S8_LE
  SSS_FMT_U8,   // SND_PCM_FORMAT_U8_LE
  SSS_FMT_S16,  // SND_PCM_FORMAT_S16_LE
  SSS_FMT_U16,  // SND_PCM_FORMAT_U16_LE
  SSS_FMT_S32,  // SND_PCM_FORMAT_S32_LE
  SSS_FMT_U32,  // SND_PCM_FORMAT_U32_LE
  SSS_FMT_FL32, // SND_PCM_FORMAT_FLOAT_LE
  SSS_FMT_FL64, // SND_PCM_FORMAT_FLOAT64_LE
  SSS_FMT_UNKNOWN
};

int fmt_to_bits(SSS_FMT fmt) {
  switch (fmt) {
  case SSS_FMT_S8:
    return 8;
  case SSS_FMT_U8:
    return 8;
  case SSS_FMT_S16:
    return 16;
  case SSS_FMT_U16:
    return 16;
  case SSS_FMT_S32:
    return 32;
  case SSS_FMT_U32:
    return 32;
  case SSS_FMT_FL32:
    return 32;
  case SSS_FMT_FL64:
    return 64;
  case SSS_FMT_UNKNOWN:
    return 0;
  }
  return 0;
}

int fmt_to_bytes(SSS_FMT fmt) { return fmt_to_bits(fmt) / 8; }

bool is_float(SSS_FMT fmt) {
  if (fmt == SSS_FMT_FL32 || fmt == SSS_FMT_FL64)
    return true;
  else
    return false;
}

bool is_signed(SSS_FMT fmt) { return true; }

unsigned char *float_to_uchar(float *buff, std::size_t n_floats) {
  unsigned char *res = new unsigned char[n_floats];
  std::memcpy(res, &buff, sizeof buff);
  return res;
}

// expects the source samples to be chars representing s16 stereo
int32_t *convert_s16_to_i32(std::vector<char> &samples, int num_samples) {
  auto samples_16 = reinterpret_cast<int16_t *>(samples.data());
  auto samples_32 = new int32_t[num_samples];
  for (int i = 0; i < num_samples; i += 1) {
    samples_32[i] = samples_16[i] << 8;
  }

  return samples_32;
}

float *convert_s16_to_f32(std::vector<char> &samples, int num_samples) {
  auto samples_16 = reinterpret_cast<int16_t *>(samples.data());
  // constexpr auto scale_factor = 1.0f / static_cast<float>(0x7fffffff);
  float *samples_f32 = new float[num_samples];
  for (int i = 0; i < num_samples; i += 1) {
    samples_f32[i] = (float)(samples_16[i] / 100000.0f); // TODO ????
  }
  return samples_f32;
}
