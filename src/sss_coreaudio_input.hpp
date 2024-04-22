#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreAudioTypes/CoreAudioTypes.h>
#include <cstring>
#include <fstream>
#include <iostream>

#include "sss_backend.hpp"

static const int INPUT_ELEMENT = 1;
static const int OUTPUT_ELEMENT = 0;

void set_ca_fmt_flags(AudioStreamBasicDescription &fmt, SSS_FMT sss_fmt) {
  if (is_float(sss_fmt)) {
    fmt.mFormatFlags = kLinearPCMFormatFlagIsFloat;
  } else {
    fmt.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
  }
}

// T = buffer_type
// S = buffer_size
template <typename T> class CoreAudioInputBackend {
public:
  SSS_Backend<T> *backend;
  size_t num_frames;
  AudioStreamBasicDescription input_format;
  AudioUnit input_audio_unit;
  AudioDeviceID input_device_id;
  float volume;
  double hardware_latency;
  bool stopped{true};
  SSS_FMT fmt;
  // static const int bb_size{S * 4};
  std::vector<AudioDeviceID> avail_devices;
  AudioObjectPropertyAddress avail_property_address;
  UInt32 avail_prop_size;

  // TODO: params class
  size_t sample_rate;
  uint8_t bits_per_frame;
  uint8_t channels;
  size_t bytes_per_frame;

  using fn_type = std::function<std::size_t(SSS_Node<T> *)>;
  CoreAudioInputBackend(int sample_rate, uint8_t bits_per_sample,
                        uint8_t channels, size_t bytes_per_frame,
                        size_t num_frames, SSS_FMT fmt)
      : sample_rate(sample_rate), bits_per_frame(bits_per_sample),
        channels(channels), bytes_per_frame(bytes_per_frame),
        num_frames(num_frames), input_device_id(kAudioObjectUnknown), fmt(fmt) {
    input_format.mFormatID = kAudioFormatLinearPCM;
    input_format.mSampleRate = sample_rate;
    input_format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
    input_format.mBitsPerChannel = bits_per_sample; // 32
    input_format.mChannelsPerFrame = channels;
    input_format.mFramesPerPacket = 1;
    input_format.mBytesPerPacket = bytes_per_frame; // 8
    input_format.mBytesPerFrame = bytes_per_frame;  // 8
    input_format.mReserved = 0;

    // TODO ?
    // backend = new SSS_Backend<T>(2, 480000, 1024, SSS_FMT_S32);
  }

  void list_devices() {
    std::cout << "Listing CoreAudio input devices\n";
    for (const auto &dev : avail_devices) {
      CFStringRef device_name = NULL;
      UInt32 data_size = sizeof(device_name);
      avail_property_address.mSelector = kAudioDevicePropertyDeviceNameCFString;
      OSStatus status = AudioObjectGetPropertyData(
          dev, &avail_property_address, 0, NULL, &data_size, &device_name);

      if (status == kAudioHardwareNoError) {
        char name[128];
        CFStringGetCString(device_name, name, sizeof(name),
                           kCFStringEncodingUTF8);
        std::cout << "Device Name: " << name << std::endl;
        CFRelease(device_name);
      }
    }
  }

  // void push_node(NodeType nt, fn_type fn, void *fn_data) {
  //  this->backend->mixer->new_node(OUTPUT, fn, channels, fn_data);
  // }

  static OSStatus IOProc(AudioDeviceID inDevice, const AudioTimeStamp *inNow,
                         const AudioBufferList *inInputData,
                         const AudioTimeStamp *inInputTime,
                         AudioBufferList *io_data,
                         const AudioTimeStamp *output_time_stamp,
                         void *user_data) {

    if (!user_data) {
      std::cout << "invalid user data\n";
      return -1;
    }
    auto ca_data = (CoreAudioInputBackend *)user_data;
    // std::cout << "starting sampling input\n";
    //  ca_data->sss_backend->mixer->sample_output_nodes();
    // std::cout << inInputData->mNumberBuffers << std::endl;
    auto n_bytes = inInputData->mBuffers[0].mDataByteSize;
    auto audio_data = (float *)inInputData->mBuffers[0].mData;
    // std::cout << audio_data[100] << " ";
    //     std::cout << n_bytes << std::endl;
    //        std::cout << "calling render " << n_frames << " " << n_bytes <<
    //        std::endl;
    //       ca_data->render(n_frames, io_data, output_time_stamp);
    //     std::cout << audio_data[100] << std::endl;
    ca_data->backend->handle_in(n_bytes, &audio_data);

    return noErr;
  }

  bool ca_open_input() {
    UInt32 size = sizeof(input_device_id);

    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain};

    OSStatus res =
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress,
                                   0, NULL, &size, &input_device_id);

    std::cout << "default device id: " << input_device_id << std::endl;
    if (res != noErr) {
      std::cout << "could not set up default device\n";
      return false;
    }

    input_device_id = 79;

    UInt32 data_size = 0;

    avail_property_address.mSelector = kAudioHardwarePropertyDevices;
    avail_property_address.mScope = kAudioObjectPropertyScopeGlobal;
    avail_property_address.mElement = kAudioObjectPropertyElementMain;
    // AudioObjectPropertyAddress property_address;
    //  Get id's for rest of the devices
    OSStatus status = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject, &avail_property_address, 0, 0, &data_size);

    if (status != kAudioHardwareNoError) {
      std::cerr << "Error getting audio devices data size." << std::endl;
      return 1;
    }

    UInt32 deviceCount = data_size / sizeof(AudioDeviceID);

    avail_devices = std::vector<AudioDeviceID>(deviceCount);

    OSStatus avail_res = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &avail_property_address, 0, 0, &data_size,
        avail_devices.data());

    if (avail_res != kAudioHardwareNoError) {
      std::cerr << "Error getting audio devices." << std::endl;
      return 1;
    }

    // set buffer size
    UInt32 numFrames = (UInt32)num_frames;
    propertyAddress.mElement = kAudioDevicePropertyBufferFrameSize;

    AudioObjectPropertyAddress buffSizeAddress = {
        kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};

    status = AudioObjectSetPropertyData(input_device_id, &buffSizeAddress, 0,
                                        nullptr, sizeof(UInt32), &numFrames);

    if (status != noErr) {
      std::cout << "Error setting up buffer size! " << status << std::endl;
    }

    AudioObjectPropertyAddress sampleRateAddress = {
        kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};

    // Set the new sample rate
    Float64 sampleRate = (Float64)sample_rate;
    status = AudioObjectSetPropertyData(input_device_id, &sampleRateAddress, 0,
                                        NULL, sizeof(Float64), &sampleRate);

    if (status != noErr) {
      std::cout << "Error setting up sample rate! " << status << std::endl;
    }
    // TODO:
    // the default mic on macbooks don't like being set to a desired fmt
    // probably should just pass in a default format
    /*
        AudioStreamBasicDescription desiredFormat;
        // desiredFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
        desiredFormat.mSampleRate = 44100.0;
        desiredFormat.mFormatFlags =
            kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
        desiredFormat.mBytesPerPacket = 2;
        desiredFormat.mFramesPerPacket = 1;
        desiredFormat.mBytesPerFrame = 2;
        desiredFormat.mChannelsPerFrame = 1;
        desiredFormat.mBitsPerChannel = 16;

        // desiredFormat.mChannelsPerFrame = 2;

        // Set the property address for the device's stream format
        AudioObjectPropertyAddress fmtAddress =
       {kAudioDevicePropertyStreamFormat, kAudioDevicePropertyScopeInput};

        // Apply the new format to the device
        status = AudioObjectSetPropertyData(input_device_id, &fmtAddress, 0,
       NULL, sizeof(AudioStreamBasicDescription), &desiredFormat);

        if (status != noErr) {
          fprintf(stderr, "Error setting audio format: %d\n", status);
          return 0;
        }
    */
    AudioDeviceIOProcID ioProcID;

    // set up callback
    status =
        AudioDeviceCreateIOProcID(input_device_id, IOProc, this, &ioProcID);

    if (status != noErr) {
      // Handle error
      return -1;
    }

    AudioStreamBasicDescription fmt;
    UInt32 dataSize = sizeof(fmt);
    AudioObjectPropertyAddress fmt_address = {kAudioDevicePropertyStreamFormat,
                                              kAudioDevicePropertyScopeInput};

    status = AudioObjectGetPropertyData(input_device_id, &fmt_address, 0, NULL,
                                        &dataSize, &fmt);
    if (status != noErr) {
      fprintf(stderr, "Error getting device stream format.\n");
      return -1;
    }

    // Print the format details
    printf("Sample Rate: %f\n", fmt.mSampleRate);
    printf("Format ID: %u\n", fmt.mFormatID);
    printf("Format Flags: %u\n", fmt.mFormatFlags);
    printf("Bytes Per Packet: %u\n", fmt.mBytesPerPacket);
    printf("Frames Per Packet: %u\n", fmt.mFramesPerPacket);
    printf("Bytes Per Frame: %u\n", fmt.mBytesPerFrame);
    printf("Channels Per Frame: %u\n", fmt.mChannelsPerFrame);
    printf("Bits Per Channel: %u\n", fmt.mBitsPerChannel);

    return true;
  }

  static OSStatus read_callback(void *user_data, AudioUnitRenderActionFlags *,
                                const AudioTimeStamp *output_time_stamp, UInt32,
                                UInt32 n_frames, AudioBufferList *io_data) {
    if (!user_data) {
      std::cout << "invalid user data\n";
      return -1;
    }
    auto ca_data = (CoreAudioInputBackend *)user_data;
    // std::cout << "starting sampling\n";
    // ca_data->backend->mixer->sample_output_nodes();
    // std::cout << "calling render\n";
    // ca_data->render(n_frames, io_data, output_time_stamp);
    std::cout << "READ \n";
    return noErr;
  }

  double get_hardware_latency() {
    if (!input_audio_unit) {
      std::cout << "audio unit invalid!\n";
      return 0.0;
    }

    Float64 audio_unit_latency_sec = 0.0;
    UInt32 size = sizeof(audio_unit_latency_sec);
    OSStatus res = AudioUnitGetProperty(
        input_audio_unit, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0,
        &audio_unit_latency_sec, &size);
    if (res != noErr) {
      std::cout << "error getting latency\n";
      return 0.0;
    }

    AudioObjectPropertyAddress property_address = {
        kAudioDevicePropertyLatency, kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain};

    UInt32 device_latency_frames = 0;
    size = sizeof(device_latency_frames);
    res = AudioObjectGetPropertyData(input_device_id, &property_address, 0,
                                     NULL, &size, &device_latency_frames);
    if (res != noErr) {
      std::cout << "could not get audio latency\n";
      return 0.0;
    }

    return static_cast<double>(
        (audio_unit_latency_sec * input_format.mSampleRate) +
        device_latency_frames);
  }

  double get_play_latency(const AudioTimeStamp *output_time_stamp) {
    if ((output_time_stamp->mFlags & kAudioTimeStampHostTimeValid) == 0)
      return 0;

    UInt64 output_time_ns =
        AudioConvertHostTimeToNanos(output_time_stamp->mHostTime);
    UInt64 now_ns = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());

    if (now_ns > output_time_ns)
      return 0;

    double delay_frames = static_cast<double>(1e-9 * (output_time_ns - now_ns) *
                                              input_format.mSampleRate);

    return (delay_frames + hardware_latency);
  }

  bool configure_input(int buffer_size) {
    AURenderCallbackStruct input;
    input.inputProc = read_callback;
    input.inputProcRefCon = this;
    OSStatus result = AudioUnitSetProperty(
        input_audio_unit, kAudioOutputUnitProperty_SetInputCallback,
        kAudioUnitScope_Output, 0, &input, sizeof(input));
    if (result != noErr) {
      std::cout << "AudioUnitSetProperty(kAudioUnitProperty_SetRenderCallback) "
                   "failed.";
      return false;
    }
    std::cout << "read callback setup\n";

    result = AudioUnitSetProperty(
        input_audio_unit, kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Output, 1, &input_format, sizeof(input_format));

    if (result != noErr) {
      std::cout
          << "AudioUnitSetProperty(kAudioUnitProperty_StreamFormat failed."
          << result << "\n";
      return false;
    }

    result = AudioUnitSetProperty(
        input_audio_unit, kAudioDevicePropertyBufferFrameSize,
        kAudioUnitScope_Output, 1, &buffer_size, sizeof(buffer_size));
    if (result != noErr) {
      std::cout << "AudioUnitSetProperty(kAudioDevicePropertyBufferFrameSize) "
                   "failed.";
      return false;
    }

    std::cout << "INPUT CONFIGURED!\n";

    return true;
  }

  void start_input() {
    std::cout << "Starting CoreAudio input\n";
    OSStatus status = AudioDeviceStart(input_device_id, IOProc);

    if (status != noErr) {
      std::cout << "error starting\n";
      return;
    }
  }

  void stop_input() {
    if (stopped)
      return;
    AudioOutputUnitStop(input_audio_unit);
  }
};
