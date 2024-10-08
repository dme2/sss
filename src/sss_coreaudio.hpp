#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreAudioTypes/CoreAudioTypes.h>
#include <cstring>
#include <iostream>
#include <string>

// #include "sss_backend.hpp"
#include "sss_coreaudio_input.hpp"

// static const int INPUT_ELEMENT = 1;
// static const int OUTPUT_ELEMENT = 0;

template <typename T> class CoreAudioBackend {
public:
  size_t num_frames;
  std::size_t num_bytes;
  SSS_Backend<T> *sss_backend;
  AudioStreamBasicDescription format;
  AudioUnit audio_unit;
  AudioDeviceID device_id;
  float volume;
  double hardware_latency;
  bool stopped{true};
  SSS_FMT fmt;
  std::vector<AudioDeviceID> avail_devices;
  std::vector<AudioDeviceID> active_devices;
  AudioObjectPropertyAddress avail_property_address;
  bool run_midi{false};

  // TODO: params class
  size_t sample_rate;
  uint8_t bits_per_frame;
  uint8_t channels;
  size_t bytes_per_frame;

  using fn_type = std::function<std::size_t(SSS_Node<T> *, std::size_t)>;
  CoreAudioBackend(int sample_rate, uint8_t bits_per_sample, uint8_t channels,
                   size_t bytes_per_frame, size_t num_frames, SSS_FMT fmt)
      : sample_rate(sample_rate), bits_per_frame(bits_per_sample),
        channels(channels), bytes_per_frame(bytes_per_frame),
        num_frames(num_frames), device_id(kAudioObjectUnknown), fmt(fmt) {
    format.mFormatID = kAudioFormatLinearPCM;
    format.mSampleRate = sample_rate;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
    format.mBitsPerChannel = bits_per_sample; // 32
    format.mChannelsPerFrame = channels;
    format.mFramesPerPacket = 1;
    format.mBytesPerPacket = bytes_per_frame; // 8
    format.mBytesPerFrame = bytes_per_frame;  // 8
    format.mReserved = 0;
  }

  OSStatus render(size_t n_frames, AudioBufferList *io_data,
                  const AudioTimeStamp *output_time_stamp,
                  AudioDeviceID device_id) {
    AudioBuffer &buff = io_data->mBuffers[0];
    T *audio_data = (T *)buff.mData;
    sss_backend->get(n_frames, &audio_data, std::to_string(device_id));
    return noErr;
  }

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
    auto ca_data = (CoreAudioBackend *)user_data;
    auto n_bytes = io_data->mBuffers[0].mDataByteSize;
    auto chans = io_data->mBuffers[0].mNumberChannels;
    auto n_frames = n_bytes / 8; // TODO!!
    ca_data->sss_backend->stage_out_nodes(std::to_string(inDevice), n_frames);
    // ca_data->sss_backend->mixer->sample_output_nodes_ecs();
    ca_data->sss_backend->mixer->tick_mixer();
    ca_data->render(n_frames, io_data, output_time_stamp, inDevice);
    return noErr;
  }

  // check if node has a valid device, correct channel setup, etc..
  bool validate_node(SSS_Node<T> *node) { return true; }

  void list_devices() {
    if (avail_devices.size() == 0)
      get_avail_devices();
    std::cout << "Listing CoreAudio devices\n";
    std::cout << avail_devices.size() << std::endl;
    for (const auto &dev : avail_devices) {
      CFStringRef device_name = NULL;
      UInt32 data_size = sizeof(device_name);
      avail_property_address.mSelector = kAudioDevicePropertyDeviceNameCFString;
      // avail_property_address.mSelector = kAudioDevicePropertyDeviceName;

      OSStatus status = AudioObjectGetPropertyData(
          dev, &avail_property_address, 0, NULL, &data_size, &device_name);

      if (status == kAudioHardwareNoError) {
        char name[128];
        CFStringGetCString(device_name, name, sizeof(name),
                           kCFStringEncodingUTF8);
        std::cout << "Device Name: " << name << "\nDevice id: " << dev
                  << std::endl;
        CFRelease(device_name);
      }
    }
  }

  bool get_avail_devices() {
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
      return false;
    }

    UInt32 deviceCount = data_size / sizeof(AudioDeviceID);

    avail_devices = std::vector<AudioDeviceID>(deviceCount);

    OSStatus avail_res = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &avail_property_address, 0, 0, &data_size,
        avail_devices.data());

    if (avail_res != kAudioHardwareNoError) {
      std::cerr << "Error getting audio devices." << std::endl;
      return false;
    }

    return true;
  }

  bool ca_open_device(std::string out_id = "73") {
    device_id = std::stoi(out_id);
    UInt32 size = sizeof(device_id);
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain};

    if (std::stoi(out_id) == 1) {
      OSStatus res =
          AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress,
                                     0, NULL, &size, &device_id);

      if (res != noErr) {
        std::cout << "could not set up default device\n";
        return false;
      }
      std::cout << "default device opened\n";
    }

    OSStatus status;

    // set buffer size
    UInt32 numFrames = (UInt32)num_frames;
    propertyAddress.mElement = kAudioDevicePropertyBufferFrameSize;

    AudioObjectPropertyAddress buffSizeAddress = {
        kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};

    status = AudioObjectSetPropertyData(device_id, &buffSizeAddress, 0, nullptr,
                                        sizeof(UInt32), &numFrames);

    if (status != noErr) {
      std::cout << "Error setting up buffer size! " << status << std::endl;
    }

    AudioObjectPropertyAddress sampleRateAddress = {
        kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};

    // Set the new sample rate
    Float64 sampleRate = (Float64)sample_rate;
    status = AudioObjectSetPropertyData(device_id, &sampleRateAddress, 0, NULL,
                                        sizeof(Float64), &sampleRate);

    if (status != noErr) {
      std::cout << "Error setting up sample rate! " << status << std::endl;
    }

    AudioStreamBasicDescription desiredFormat;
    desiredFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
    desiredFormat.mSampleRate = (float)sample_rate;

    AudioDeviceIOProcID ioProcID;

    // set up callback
    status = AudioDeviceCreateIOProcID(device_id, IOProc, this, &ioProcID);

    if (status != noErr) {
      // Handle error
      return -1;
    }

    AudioStreamBasicDescription fmt;
    UInt32 dataSize = sizeof(fmt);
    AudioObjectPropertyAddress fmt_address = {kAudioDevicePropertyStreamFormat,
                                              kAudioDevicePropertyScopeOutput};

    status = AudioObjectGetPropertyData(device_id, &fmt_address, 0, NULL,
                                        &dataSize, &fmt);
    if (status != noErr) {
      fprintf(stderr, "Error getting device stream format.\n");
      return -1;
    }

    // Print the format details
    printf("Sample Rate: %f\n", fmt.mSampleRate);
    // printf("Format ID: %u\n", fmt.mFormatID);
    // printf("Format Flags: %u\n", fmt.mFormatFlags);
    // printf("Bytes Per Packet: %u\n", fmt.mBytesPerPacket);
    // printf("Frames Per Packet: %u\n", fmt.mFramesPerPacket);
    // printf("Bytes Per Frame: %u\n", fmt.mBytesPerFrame);
    // printf("Channels Per Frame: %u\n", fmt.mChannelsPerFrame);
    // printf("Bits Per Channel: %u\n", fmt.mBitsPerChannel);

    return true;
  }

  double get_hardware_latency() {
    if (!audio_unit) {
      std::cout << "audio unit invalid!\n";
      return 0.0;
    }

    Float64 audio_unit_latency_sec = 0.0;
    UInt32 size = sizeof(audio_unit_latency_sec);
    OSStatus res = AudioUnitGetProperty(audio_unit, kAudioUnitProperty_Latency,
                                        kAudioUnitScope_Global, 0,
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
    res = AudioObjectGetPropertyData(device_id, &property_address, 0, NULL,
                                     &size, &device_latency_frames);
    if (res != noErr) {
      std::cout << "could not get audio latency\n";
      return 0.0;
    }

    return static_cast<double>((audio_unit_latency_sec * format.mSampleRate) +
                               device_latency_frames);
  }

  void start(uint32_t device_id = 73) {
    std::cout << "Starting CoreAudio output\n";
    OSStatus status = AudioDeviceStart(device_id, IOProc);

    if (status != noErr) {
      std::cout << "error starting\n";
      return;
    }
  }

  void stop() {
    if (stopped)
      return;
    AudioOutputUnitStop(audio_unit);
  }
};
