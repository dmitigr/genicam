// -*- C++ -*-
// Copyright (C) 2020 Dmitry Igrishin
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
// Dmitry Igrishin
// dmitigr@gmail.com

#include <GxIAPI.h>
#include <DxImageProc.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#ifndef DMITIGR_GENICAM_DAHENG_GX_HPP
#define DMITIGR_GENICAM_DAHENG_GX_HPP

namespace dmitigr::genicam::daheng::gx {

// -----------------------------------------------------------------------------
// Basics
// -----------------------------------------------------------------------------

inline std::pair<GX_STATUS, std::string> get_last_error()
{
  std::string str;
  GX_STATUS code{};

  // Asking for required string size.
  std::size_t str_size{};
  if (const auto s = GXGetLastError(&code, nullptr, &str_size);
    s == GX_STATUS_SUCCESS)
    str.resize(str_size);
  else
    throw std::runtime_error{"GXGetLastError()"};

  // Asking for string.
  if (const auto s = GXGetLastError(&code, str.data(), &str_size);
    s != GX_STATUS_SUCCESS)
    throw std::runtime_error{"GXGetLastError()"};

  return {code, str};
}

inline void throw_if_last_error()
{
  const auto [code, str] = get_last_error();
  if (code != GX_STATUS_SUCCESS)
    throw std::runtime_error{str};
}

template<typename F, typename ... Types>
inline auto call(F&& f, Types&& ... args)
{
  auto result = f(std::forward<Types>(args)...);
  throw_if_last_error();
  return result;
}

// -----------------------------------------------------------------------------
// Free functions
// -----------------------------------------------------------------------------

/// @returns The number of devices.
inline std::uint32_t update_device_list(const std::chrono::milliseconds timeout)
{
  std::uint32_t camera_count{};
  call(GXUpdateDeviceList, &camera_count, static_cast<std::uint32_t>(timeout.count()));
  return camera_count;
}

// -----------------------------------------------------------------------------
// Class Device
// -----------------------------------------------------------------------------

class Device final {
public:
  /// The destructor.
  ~Device()
  {
    if (handle_) {
      GXStreamOff(handle_);
      GXUnregisterCaptureCallback(handle_);
      GXCloseDevice(handle_);
    }
  }

  /// Non copy-constructible.
  Device(const Device&) = delete;
  /// Non copy-assignable.
  Device& operator=(const Device&) = delete;

  /// Move-constructible.
  Device(Device&&) = default;
  /// Move-assignable.
  Device& operator=(Device&&) = default;

  /// The constructor.
  explicit Device(GX_DEV_HANDLE handle = {})
    : handle_{handle}
  {}

  /// Opens the device by index, starting from 1.
  explicit Device(const std::uint32_t index)
  {
    call(GXOpenDeviceByIndex, index, &handle_);
  }

  /**
   * Opens the device by a specific unique identification,
   * such as: SN, IP, MAC, Index etc.
   */
  explicit Device(GX_OPEN_PARAM* const open_param)
  {
    call(GXOpenDevice, open_param, &handle_);
  }

  /// @returns The underlying handle.
  GX_DEV_HANDLE handle() noexcept
  {
    return handle_;
  }

  /// @overload
  const GX_DEV_HANDLE handle() const noexcept
  {
    return handle_;
  }

  /// @returns `true` if this object keeps handle, or `false` otherwise.
  explicit operator bool() const noexcept
  {
    return handle_;
  }

  /// @returns The released handle.
  GX_DEV_HANDLE release() noexcept
  {
    auto result = handle_;
    handle_ = {};
    return result;
  }

  /// Closes the device.
  void close()
  {
    GXStreamOff(handle_);
    GXUnregisterCaptureCallback(handle_);
    call(GXCloseDevice, handle_);
    handle_ = {};
  }

  /// @name Settings
  /// @{

  void set_trigger_mode(const GX_TRIGGER_MODE_ENTRY value)
  {
    set_enum(GX_ENUM_TRIGGER_MODE, value);
  }

  GX_TRIGGER_MODE_ENTRY trigger_mode() const
  {
    return static_cast<GX_TRIGGER_MODE_ENTRY>(get_enum(GX_ENUM_TRIGGER_MODE));
  }

  void set_trigger_source(const GX_TRIGGER_SOURCE_ENTRY value)
  {
    set_enum(GX_ENUM_TRIGGER_SOURCE, value);
  }

  GX_TRIGGER_SOURCE_ENTRY trigger_source() const
  {
    return static_cast<GX_TRIGGER_SOURCE_ENTRY>(get_enum(GX_ENUM_TRIGGER_SOURCE));
  }

  void set_pixel_format(const GX_PIXEL_FORMAT_ENTRY value)
  {
    set_enum(GX_ENUM_PIXEL_FORMAT, value);
  }

  GX_PIXEL_FORMAT_ENTRY pixel_format() const
  {
    return static_cast<GX_PIXEL_FORMAT_ENTRY>(get_enum(GX_ENUM_PIXEL_FORMAT));
  }

  void set_exposure_time(const double value)
  {
    set_float(GX_FLOAT_EXPOSURE_TIME, value);
  }

  double exposure_time() const
  {
    return get_float(GX_FLOAT_EXPOSURE_TIME);
  }

  void set_gain_auto(const GX_GAIN_AUTO_ENTRY value)
  {
    set_enum(GX_ENUM_GAIN_AUTO, value);
  }

  GX_GAIN_AUTO_ENTRY gain_auto() const
  {
    return static_cast<GX_GAIN_AUTO_ENTRY>(get_enum(GX_ENUM_GAIN_AUTO));
  }

  void set_gain(const GX_GAIN_SELECTOR_ENTRY channel, const double value)
  {
    set_enum(GX_ENUM_GAIN_SELECTOR, channel);
    set_float(GX_FLOAT_GAIN, value);
  }

  double gain(const GX_GAIN_SELECTOR_ENTRY channel) const
  {
    set_enum(GX_ENUM_GAIN_SELECTOR, channel);
    return get_float(GX_FLOAT_GAIN);
  }

  void set_balance_ratio(const GX_BALANCE_RATIO_SELECTOR_ENTRY channel, const double value)
  {
    set_enum(GX_ENUM_BALANCE_RATIO_SELECTOR, channel);
    set_float(GX_FLOAT_BALANCE_RATIO, value);
  }

  double balance_ratio(const GX_BALANCE_RATIO_SELECTOR_ENTRY channel) const
  {
    set_enum(GX_ENUM_BALANCE_RATIO_SELECTOR, channel);
    return get_float(GX_FLOAT_BALANCE_RATIO);
  }

  /// @}

  /// @name Control
  /// @{

  void register_capture_callback(GXCaptureCallBack callback, void* const data = {})
  {
    call(GXRegisterCaptureCallback, handle_, data, callback);
  }

  void unregister_capture_callback()
  {
    call(GXUnregisterCaptureCallback, handle_);
  }

  void set_capture_callback(GXCaptureCallBack callback, void* const data = {})
  {
    GXUnregisterCaptureCallback(handle_);
    register_capture_callback(callback, data);
  }

  void start_acquisition()
  {
    call(GXStreamOn, handle_);
  }

  void stop_acquisition()
  {
    call(GXStreamOff, handle_);
  }

  /// @}

private:
  GX_DEV_HANDLE handle_{};

  std::int64_t get_enum(const GX_FEATURE_ID feature) const
  {
    std::int64_t result{};
    call(GXGetEnum, handle_, feature, &result);
    return result;
  }

  void set_enum(const GX_FEATURE_ID feature, const std::int64_t value) const
  {
    call(GXSetEnum, handle_, feature, value);
  }

  double get_float(const GX_FEATURE_ID feature) const
  {
    double result{};
    call(GXGetFloat, handle_, feature, &result);
    return result;
  }

  void set_float(const GX_FEATURE_ID feature, const double value)
  {
    call(GXSetFloat, handle_, feature, value);
  }
};

namespace img {

inline void throw_if_error(const VxInt32 s)
{
  switch (s) {
  case DX_OK: return;
  case DX_PARAMETER_INVALID:
    throw std::runtime_error{"invalid input parameter"};
  case DX_PARAMETER_OUT_OF_BOUND:
    throw std::runtime_error{"the parameter is out of bound"};
  case DX_NOT_ENOUGH_SYSTEM_MEMORY:
    throw std::bad_alloc{};
  case DX_NOT_FIND_DEVICE:
    throw std::runtime_error{"no device found"};
  case DX_STATUS_NOT_SUPPORTED:
    throw std::runtime_error{"the format is not supported"};
  case DX_CPU_NOT_SUPPORT_ACCELERATE:
    throw std::runtime_error{"the CPU does not support acceleration"};
  default:
    throw std::runtime_error{"unknown error"};
  }
}

template<typename F, typename ... Types>
inline void call(F&& f, Types&& ... args)
{
  throw_if_error(f(std::forward<Types>(args)...));
}

inline std::unique_ptr<unsigned char[]> raw8_to_rgb24(
  void* const input,
  const std::uint32_t width,
  const std::uint32_t height,
  const DX_BAYER_CONVERT_TYPE conversion_type,
  const DX_PIXEL_COLOR_FILTER bayer_layout,
  const bool flip = false)
{
  std::unique_ptr<unsigned char[]> result{new unsigned char[width * height * 3]};
  call(DxRaw8toRGB24, input, result.get(), width, height, conversion_type, bayer_layout, flip);
  return result;
}

} // namespace img

} // namespace dmitigr::genicam::daheng::gx

#endif  // DMITIGR_GENICAM_DAHENG_GX_HPP
