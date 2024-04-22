// #include "spsc_buffer.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <iostream>

template <typename T> class SSS_Buffer {
public:
  bool empty(void) { return _write_samples_avail == _size; }

  void clear_buffer() { memset(_data, 0, _size); }

  int get_avail() { return _write_samples_avail; }

  SSS_Buffer(std::size_t s) {
    _size = s;
    _read_ptr = 0;
    _write_ptr = 0;
    _data = new T[_size];
    _write_samples_avail = s;
  }

  int read_n(T *dest, int num_samples) {
    if (dest == nullptr || num_samples <= 0 || _write_samples_avail == _size) {
      return 0;
    }

    int read_samples_avail = _size - _write_samples_avail;

    if (num_samples > read_samples_avail) {
      num_samples = read_samples_avail;
    }

    for (std::size_t i = 0; i < num_samples; i++) {
      dest[i] = _data[_read_ptr++];
      if (_read_ptr == _size) {
        _read_ptr = 0;
      }
      _write_samples_avail += 1;
      // if (_read_ptr == _write_ptr)
      //  break;
    }

    return num_samples;
  }

  int write_n(T *src, int num_samples) {
    if (src == nullptr || num_samples <= 0 || _write_samples_avail == 0) {
      return 0;
    }

    if (num_samples > _write_samples_avail) {
      num_samples = _write_samples_avail;
    }

    for (std::size_t i = 0; i < num_samples; i++) {
      _data[_write_ptr++] = src[i];
      if (_write_ptr == _size) {
        _write_ptr = 0;
      }
      _write_samples_avail -= 1;
    }

    //_write_ptr = (_write_ptr + num_samples) % _size;
    //_write_samples_avail -= num_samples;

    return num_samples;
  }

  bool write(T src) {
    if (_write_samples_avail == 0) {
      return false;
    }

    _data[_write_ptr++] = src;

    if (_write_ptr == _size) {
      _write_ptr = 0;
    }
    _write_samples_avail -= 1;

    //_write_ptr = (_write_ptr + num_samples) % _size;
    //_write_samples_avail -= num_samples;

    return true;
  }

private:
  T *_data;
  int _size;
  int _read_ptr;
  int _write_ptr;
  int _write_samples_avail;
  ;
};
