// -*- C++ -*-
// Copyright (C) 2021 Dmitry Igrishin
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

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <chrono>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#ifndef DMITIGR_GENICAM_DAHENG_GX_HPP
#define DMITIGR_GENICAM_DAHENG_GX_HPP

namespace dmitigr::genicam::daheng::gx {

// -----------------------------------------------------------------------------
// Exceptions
// -----------------------------------------------------------------------------

/// A category of runtime errors.
class Error_category final : public std::error_category {
public:
  /// @returns The literal `dmitigr_genicam_daheng_gx_error`.
  const char* name() const noexcept override
  {
    return "dmitigr_genicam_daheng_gx_error";
  }

  /// @returns The string that describes the error condition denoted by `ev`.
  std::string message(const int ev) const override
  {
    return std::string{name()}.append(" ").append(std::to_string(ev));
  }
};

/// The instance of type Error_category.
inline Error_category error_category;

/// An exception.
class Exception final : public std::system_error {
public:
  /// The constructor.
  explicit Exception(const int ev)
    : system_error{ev, error_category}
  {}

  /// @overload
  Exception(const int ev, const std::string& what)
    : system_error{ev, error_category, what}
  {}
};

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
    throw Exception{s, "GXGetLastError()"};

  // Asking for string.
  if (const auto s = GXGetLastError(&code, str.data(), &str_size);
    s != GX_STATUS_SUCCESS)
    throw Exception{s, "GXGetLastError()"};

  return {code, str};
}

inline void throw_if_last_error()
{
  const auto [code, str] = get_last_error();
  if (code != GX_STATUS_SUCCESS)
    throw Exception{code, str};
}

template<typename F, typename ... Types>
inline auto call(F&& f, Types&& ... args)
{
  auto result = f(std::forward<Types>(args)...);
  throw_if_last_error();
  return result;
}

/**
 * Convenient wrapper around GX_OPEN_PARAM
 *
 * Possible values of GX_ACCESS_MODE are:
 *   -# GX_ACCESS_READONLY;
 *   -# GX_ACCESS_CONTROL;
 *   -# GX_ACCESS_EXCLUSIVE.
 */
class Open_param final {
public:
  /// Default-constructible. (Constructs an instance with empty content.)
  Open_param() = default;

  /**
   * The generic constructor.
   *
   * Please note, that this class is also provides convenient named constructors.
   *
   * @param content Could be and IP address, a serial number and so on.
   * @param om Device open mode. Possible values are:
   *   -# GX_OPEN_SN;
   *   -# GX_OPEN_IP;
   *   -# GX_OPEN_MAC;
   *   -# GX_OPEN_INDEX;
   *   -# GX_OPEN_USERID.
   * @param am Access mode. (See Open_param for possible values.)
   *
   * @see by_sn(), by_ip(), by_mac(), by_index(), by_userid().
   */
  Open_param(std::string content, const GX_OPEN_MODE om, const GX_ACCESS_MODE am)
    : content_{std::move(content)}
    , handle_{content_.data(), om, am}
  {};

  /// Copy-constructible.
  Open_param(const Open_param& rhs)
    : content_{rhs.content_}
    , handle_{content_.data(), rhs.handle_.openMode, rhs.handle_.accessMode}
  {}

  /// Copy-assignable.
  Open_param& operator=(const Open_param& rhs)
  {
    if (this != &rhs) {
      Open_param tmp{rhs};
      swap(tmp);
    }
    return *this;
  }

  /// Move-constructible.
  Open_param(Open_param&& rhs) noexcept
  {
    Open_param new_this;
    rhs.swap(new_this); // reset rhs to the default state
    swap(new_this);
  }

  /// Move-assignable.
  Open_param& operator=(Open_param&& rhs) noexcept
  {
    if (this != &rhs) {
      Open_param tmp{rhs};
      swap(tmp);
    }
    return *this;
  }

  /// Swaps `*this` and `other`.
  void swap(Open_param& other) noexcept
  {
    using std::swap;
    swap(content_, other.content_);
    handle_.pszContent = content_.data();
    other.handle_.pszContent = other.content_.data();
    swap(handle_.openMode, other.handle_.openMode);
    swap(handle_.accessMode, other.handle_.accessMode);
  }

  /**
   * @param ip A serial number of the device to open.
   * @param am An access mode of the device to open.
   *
   * @par Requires
   * `!sn.empty()`.
   */
  static Open_param by_sn(const std::string& sn, const GX_ACCESS_MODE am)
  {
    if (sn.empty())
      throw std::invalid_argument("invalid camera serial number");
    return {sn, GX_OPEN_SN, am};
  }

  /**
   * @param ip An IP address of the device to open.
   * @param am An access mode of the device to open.
   *
   * @par Requires
   * `!ip.empty()`.
   */
  static Open_param by_ip(const std::string& ip, const GX_ACCESS_MODE am)
  {
    if (ip.empty())
      throw std::invalid_argument("invalid camera IP address");
    return {ip, GX_OPEN_IP, am};
  }

  /**
   * @param mac A MAC address of the device to open.
   * @param am An access mode of the device to open.
   *
   * @par Requires
   * `!mac.empty()`.
   */
  static Open_param by_mac(const std::string& mac, const GX_ACCESS_MODE am)
  {
    if (mac.empty())
      throw std::invalid_argument("invalid camera MAC address");
    return {mac, GX_OPEN_MAC, am};
  }

  /**
   * @param index An index of the device to open. (Indexes starts from 1.)
   * @param am An access mode of the device to open.
   *
   * @par Requires
   * `index > 0`.
   */
  static Open_param by_index(const int index, const GX_ACCESS_MODE am)
  {
    if (!(index > 0))
      throw std::invalid_argument("invalid camera index");
    return {std::to_string(index), GX_OPEN_INDEX, am};
  }

  /**
   * @param userid An user ID of the device to open.
   * @param am An access mode of the device to open.
   *
   * @par Requires
   * `!userid.empty()`.
   */
  static Open_param by_userid(const std::string& userid, const GX_ACCESS_MODE am)
  {
    if (userid.empty())
      throw std::invalid_argument("invalid camera user ID");
    return {userid, GX_OPEN_USERID, am};
  }

  /// @returns A device index if `open_mode() == GX_OPEN_INDEX`, or `0` otherwise.
  std::uint32_t index() const noexcept
  {
    return open_mode() == GX_OPEN_INDEX ?
      static_cast<std::uint32_t>(std::stoul(content_)) : 0u;
  }

  /// @returns An underlying content (which could be an SN, IP, MAC, index or user ID).
  const std::string& content() const noexcept
  {
    return content_;
  }

  /// @returns An open mode.
  GX_OPEN_MODE open_mode() const noexcept
  {
    return static_cast<GX_OPEN_MODE>(handle_.openMode);
  }

  /// @returns An access mode.
  GX_ACCESS_MODE access_mode() const noexcept
  {
    return static_cast<GX_ACCESS_MODE>(handle_.accessMode);
  }

private:
  friend class Device;
  std::string content_;
  mutable GX_OPEN_PARAM handle_{};
};

// -----------------------------------------------------------------------------
// Free functions
// -----------------------------------------------------------------------------

/// @returns The number of devices enumerated in a subnet.
inline std::uint32_t update_device_list(const std::chrono::milliseconds timeout)
{
  std::uint32_t camera_count{};
  call(GXUpdateDeviceList, &camera_count, static_cast<std::uint32_t>(timeout.count()));
  return camera_count;
}

/// @returns The number of devices enumerated in an entire network.
inline std::uint32_t update_all_device_list(const std::chrono::milliseconds timeout)
{
  std::uint32_t camera_count{};
  call(GXUpdateAllDeviceList, &camera_count, static_cast<std::uint32_t>(timeout.count()));
  return camera_count;
}

// -----------------------------------------------------------------------------
// Class Library
// -----------------------------------------------------------------------------

/// Represents an underlying resourses of the library.
class Library final {
public:
  /// Similar to close().
  ~Library()
  {
    if (!is_refer_)
      return;
    else if (refs_ == 1)
      GXCloseLib();

    refs_--;
  }

  /**
   * The constructor.
   *
   * @param auto_open If `true` then open() will be called.
   *
   * @see open().
   */
  explicit Library(const bool auto_open)
  {
    if (auto_open)
      open();
  }

  /// Non copy-constructible.
  Library(const Library&) = delete;
  /// Non copy-assignable.
  Library& operator=(const Library&) = delete;
  /// Non move-constructible.
  Library(Library&&) = delete;
  /// Non move-assignable.
  Library& operator=(Library&&) = delete;

  /// @returns `true` if this instance refer to the underlying resources.
  bool is_refer() const noexcept
  {
    return is_refer_;
  }

  /// @returns `true` the underlying resources is open.
  bool is_open() const noexcept
  {
    return refs_ > 0;
  }

  /// @returns The current value of the reference counter.
  static int reference_count() noexcept
  {
    return refs_;
  }

  /**
   * Increments the reference counter; if its current value is `0` then
   * opens the underlying resources.
   *
   * @remarks Idempotent.
   */
  void open()
  {
    if (is_refer_)
      return;
    else if (!refs_)
      call(GXInitLib);

    is_refer_ = true;
    refs_++;

    if (!is_invariant_ok())
      throw std::logic_error{"bug"};
  }

  /**
   * Decrements the reference counter; if its current value is `1` then
   * closes the underlying resources.
   *
   * @warning Avoid to call this function from handlers registered by
   * `std::atexit`, `std::at_quick_exit` etc.
   *
   * @remarks Idempotent.
   */
  void close()
  {
    if (!is_refer_)
      return;
    else if (refs_ == 1)
      call(GXCloseLib);

    is_refer_ = false;
    refs_--;

    if (!is_invariant_ok())
      throw std::logic_error{"bug"};
  }

private:
  bool is_refer_{};
  inline static std::atomic_int refs_{};

  bool is_invariant_ok() const noexcept
  {
    return (!is_refer_ || refs_);
  }
};

// -----------------------------------------------------------------------------
// Struct Frame_data
// -----------------------------------------------------------------------------

struct Frame_data final {
  ~Frame_data()
  {
    std::free(data.pImgBuf);
  }

  Frame_data() = default;

  Frame_data(GX_FRAME_DATA data) noexcept
    : data{data}
  {}

  Frame_data(const Frame_data&) = delete;
  Frame_data& operator=(const Frame_data&) = delete;

  Frame_data(Frame_data&& rhs) noexcept
    : data{rhs.data}
  {
    rhs.data.pImgBuf = nullptr;
  }

  Frame_data& operator=(Frame_data&& rhs) noexcept
  {
    if (this != &rhs) {
      data = rhs.data;
      rhs.data.pImgBuf = nullptr;
    }
    return *this;
  }

  GX_FRAME_DATA data{};
};

// -----------------------------------------------------------------------------
// Class Device
// -----------------------------------------------------------------------------

class Device final {
public:
  /// The destructor.
  ~Device()
  {
    close_nothrow();
  }

  /// Non copy-constructible.
  Device(const Device&) = delete;
  /// Non copy-assignable.
  Device& operator=(const Device&) = delete;

  /// Move-constructible.
  Device(Device&& rhs) noexcept
    : handle_{rhs.handle_}
  {
    rhs.handle_ = {};
  }

  /// Move-assignable.
  Device& operator=(Device&& rhs) noexcept
  {
    if (this != &rhs) {
      Device tmp{std::move(rhs)};
      swap(tmp);
    }
    return *this;
  }

  /// The swap operation.
  void swap(Device& other) noexcept
  {
    std::swap(handle_, other.handle_);
  }

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

  /// @overload
  explicit Device(const Open_param& open_param)
    : Device{&open_param.handle_}
  {}

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
    call(GXStreamOff, handle_);
    call(GXUnregisterCaptureCallback, handle_);
    call(GXCloseDevice, handle_);
    handle_ = {};
  }

  /**
   * @brief Restores the device to the initial state and the device is powered
   * on again.
   *
   * After the call is completed, the host will lose its connection to the device.
   * And because the interface can be set only when the device is open, the device
   * will be closed in order to release the corresponding memory resources.
   */
  void reset()
  {
    /// Caution! Don't call close() after sending the reset, since GXStreamOff() will fail!
    call(GXSendCommand, handle_, GX_COMMAND_DEVICE_RESET);
    call(GXCloseDevice, handle_);
    handle_ = {};
  }

  /**
   * Closes the device.
   *
   * @returns `true` on success, or `false` otherwise.
   */
  bool close_nothrow() noexcept
  {
    if (handle_) {
      auto s = GXStreamOff(handle_);
      s |= GXUnregisterCaptureCallback(handle_);
      s |= GXCloseDevice(handle_);
      handle_ = {};
      return (s == GX_STATUS_SUCCESS);
    } else
      return true;
  }

  /// @name Device information
  /// @{

  bool is_device_link_throughput_limit_mode_implemented() const
  {
    return is_implemented(GX_ENUM_DEVICE_LINK_THROUGHPUT_LIMIT_MODE);
  }

  void set_device_link_throughput_limit_mode(const GX_DEVICE_LINK_THROUGHPUT_LIMIT_MODE_ENTRY value)
  {
    set_enum(GX_ENUM_DEVICE_LINK_THROUGHPUT_LIMIT_MODE, value);
  }

  auto device_link_throughput_limit_mode() const
  {
    return static_cast<GX_DEVICE_LINK_THROUGHPUT_LIMIT_MODE_ENTRY>(get_enum(GX_ENUM_DEVICE_LINK_THROUGHPUT_LIMIT_MODE));
  }

  bool is_timestamp_tick_frequency_implemented() const
  {
    return is_implemented(GX_INT_TIMESTAMP_TICK_FREQUENCY);
  }

  int timestamp_tick_frequency() const
  {
    return get_int(GX_INT_TIMESTAMP_TICK_FREQUENCY);
  }

  bool is_timestamp_latch_value_implemented() const
  {
    return is_implemented(GX_INT_TIMESTAMP_LATCH_VALUE);
  }

  int timestamp_latch_value() const
  {
    return get_int(GX_INT_TIMESTAMP_LATCH_VALUE);
  }

  bool is_latch_timestamp_implemented() const
  {
    return is_implemented(GX_COMMAND_TIMESTAMP_LATCH);
  }

  /**
   * Latchs the current timestamp value, that is, the time value taken from the
   * start of the device power-on to the call of the timestamp latch command. The
   * time value needs to be read through the `timestamp_latch_value()`.
   *
   * @see timestamp_latch_value().
   */
  void latch_timestamp()
  {
    call(GXSendCommand, handle_, GX_COMMAND_TIMESTAMP_LATCH);
  }

  bool is_reset_timestamp_implemented() const
  {
    return is_implemented(GX_COMMAND_TIMESTAMP_RESET);
  }

  /// Resets the timestamp counter and recount from zero.
  void reset_timestamp()
  {
    call(GXSendCommand, handle_, GX_COMMAND_TIMESTAMP_RESET);
  }

  bool is_latch_reset_timestamp_implemented() const
  {
    return is_implemented(GX_COMMAND_TIMESTAMP_LATCH_RESET);
  }

  /**
   * First latchs the current timestamp value, then resets the timestamp
   * counter and recounts from zero.
   */
  void latch_reset_timestamp()
  {
    call(GXSendCommand, handle_, GX_COMMAND_TIMESTAMP_LATCH_RESET);
  }

  /// @}

  /// @name Image format
  /// @{

  bool is_pixel_format_implemented() const
  {
    return is_implemented(GX_ENUM_PIXEL_FORMAT);
  }

  void set_pixel_format(const GX_PIXEL_FORMAT_ENTRY value)
  {
    set_enum(GX_ENUM_PIXEL_FORMAT, value);
  }

  GX_PIXEL_FORMAT_ENTRY pixel_format() const
  {
    return static_cast<GX_PIXEL_FORMAT_ENTRY>(get_enum(GX_ENUM_PIXEL_FORMAT));
  }

  /// @}

  /// @name Transport layer
  /// @{

  /// @returns The number of bytes transferred for each image or chunk on the stream channel.
  std::int64_t payload_size() const
  {
    return get_int(GX_INT_PAYLOAD_SIZE);
  }

  /// @}

  /// @name Acquisition trigger
  /// @{

  bool is_trigger_mode_implemented() const
  {
    return is_implemented(GX_ENUM_TRIGGER_MODE);
  }

  void set_trigger_mode(const GX_TRIGGER_MODE_ENTRY value)
  {
    set_enum(GX_ENUM_TRIGGER_MODE, value);
  }

  GX_TRIGGER_MODE_ENTRY trigger_mode() const
  {
    return static_cast<GX_TRIGGER_MODE_ENTRY>(get_enum(GX_ENUM_TRIGGER_MODE));
  }

  bool is_trigger_source_implemented() const
  {
    return is_implemented(GX_ENUM_TRIGGER_SOURCE);
  }

  void set_trigger_source(const GX_TRIGGER_SOURCE_ENTRY value)
  {
    set_enum(GX_ENUM_TRIGGER_SOURCE, value);
  }

  GX_TRIGGER_SOURCE_ENTRY trigger_source() const
  {
    return static_cast<GX_TRIGGER_SOURCE_ENTRY>(get_enum(GX_ENUM_TRIGGER_SOURCE));
  }

  bool is_external_trigger_switch_implemented() const
  {
    return is_implemented(GX_ENUM_TRIGGER_SWITCH);
  }

  void set_external_trigger_switch(const GX_TRIGGER_SWITCH_ENTRY value)
  {
    set_enum(GX_ENUM_TRIGGER_SWITCH, value);
  }

  GX_TRIGGER_SWITCH_ENTRY external_trigger_switch() const
  {
    return static_cast<GX_TRIGGER_SWITCH_ENTRY>(get_enum(GX_ENUM_TRIGGER_SWITCH));
  }

  bool is_trigger_filter_raising_implemented() const
  {
    return is_implemented(GX_FLOAT_TRIGGER_FILTER_RAISING);
  }

  void set_trigger_filter_raising(const double value)
  {
    set_float(GX_FLOAT_TRIGGER_FILTER_RAISING, value);
  }

  double trigger_filter_raising() const
  {
    return get_float(GX_FLOAT_TRIGGER_FILTER_RAISING);
  }

  std::pair<double, double> trigger_filter_raising_range() const
  {
    return get_float_range(GX_FLOAT_TRIGGER_FILTER_RAISING);
  }

  bool is_trigger_filter_falling_implemented() const
  {
    return is_implemented(GX_FLOAT_TRIGGER_FILTER_FALLING);
  }

  void set_trigger_filter_falling(const double value)
  {
    set_float(GX_FLOAT_TRIGGER_FILTER_FALLING, value);
  }

  double trigger_filter_falling() const
  {
    return get_float(GX_FLOAT_TRIGGER_FILTER_FALLING);
  }

  std::pair<double, double> trigger_filter_falling_range() const
  {
    return get_float_range(GX_FLOAT_TRIGGER_FILTER_FALLING);
  }

  bool is_trigger_delay_implemented() const
  {
    return is_implemented(GX_FLOAT_TRIGGER_DELAY);
  }

  void set_trigger_delay(const double value)
  {
    set_float(GX_FLOAT_TRIGGER_DELAY, value);
  }

  double trigger_delay() const
  {
    return get_float(GX_FLOAT_TRIGGER_DELAY);
  }

  std::pair<double, double> trigger_delay_range() const
  {
    return get_float_range(GX_FLOAT_TRIGGER_DELAY);
  }

  bool is_exposure_time_implemented() const
  {
    return is_implemented(GX_FLOAT_EXPOSURE_TIME);
  }

  void set_exposure_time(const double value)
  {
    set_float(GX_FLOAT_EXPOSURE_TIME, value);
  }

  double exposure_time() const
  {
    return get_float(GX_FLOAT_EXPOSURE_TIME);
  }

  std::pair<double, double> exposure_time_range() const
  {
    return get_float_range(GX_FLOAT_EXPOSURE_TIME);
  }

  bool is_exposure_delay_implemented() const
  {
    return is_implemented(GX_FLOAT_EXPOSURE_DELAY);
  }

  void set_exposure_delay(const double value)
  {
    set_float(GX_FLOAT_EXPOSURE_DELAY, value);
  }

  double exposure_delay() const
  {
    return get_float(GX_FLOAT_EXPOSURE_DELAY);
  }

  std::pair<double, double> exposure_delay_range() const
  {
    return get_float_range(GX_FLOAT_EXPOSURE_DELAY);
  }

  bool is_exposure_mode_implemented() const
  {
    return is_implemented(GX_ENUM_EXPOSURE_MODE);
  }

  void set_exposure_mode(const GX_EXPOSURE_MODE_ENTRY value)
  {
    set_enum(GX_ENUM_EXPOSURE_MODE, value);
  }

  GX_EXPOSURE_MODE_ENTRY exposure_mode() const
  {
    return static_cast<GX_EXPOSURE_MODE_ENTRY>(get_enum(GX_ENUM_EXPOSURE_MODE));
  }

  bool is_exposure_auto_implemented() const
  {
    return is_implemented(GX_ENUM_EXPOSURE_AUTO);
  }

  void set_exposure_auto(const GX_EXPOSURE_AUTO_ENTRY value)
  {
    set_enum(GX_ENUM_EXPOSURE_AUTO, value);
  }

  GX_EXPOSURE_AUTO_ENTRY exposure_auto() const
  {
    return static_cast<GX_EXPOSURE_AUTO_ENTRY>(get_enum(GX_ENUM_EXPOSURE_AUTO));
  }

  /// @}

  /// @name Analog controls
  /// @{

  bool is_gain_auto_implemented() const
  {
    return is_implemented(GX_ENUM_GAIN_AUTO);
  }

  void set_gain_auto(const GX_GAIN_AUTO_ENTRY value)
  {
    set_enum(GX_ENUM_GAIN_AUTO, value);
  }

  GX_GAIN_AUTO_ENTRY gain_auto() const
  {
    return static_cast<GX_GAIN_AUTO_ENTRY>(get_enum(GX_ENUM_GAIN_AUTO));
  }

  bool is_gain_implemented() const
  {
    return is_implemented(GX_FLOAT_GAIN);
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

  std::pair<double, double> gain_range(const GX_GAIN_SELECTOR_ENTRY channel) const
  {
    set_enum(GX_ENUM_GAIN_SELECTOR, channel);
    return get_float_range(GX_FLOAT_GAIN);
  }

  bool is_balance_ratio_implemented() const
  {
    return is_implemented(GX_FLOAT_BALANCE_RATIO);
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

  std::pair<double, double> balance_ratio_range(const GX_BALANCE_RATIO_SELECTOR_ENTRY channel) const
  {
    set_enum(GX_ENUM_BALANCE_RATIO_SELECTOR, channel);
    return get_float_range(GX_FLOAT_BALANCE_RATIO);
  }

  /// @}

  /// @name Flow layer (DataStream feature)

  bool is_stream_transfer_size_implemented() const
  {
    return is_implemented(GX_DS_INT_STREAM_TRANSFER_SIZE);
  }

  void set_stream_transfer_size(const std::int64_t value)
  {
    set_int(GX_DS_INT_STREAM_TRANSFER_SIZE, value);
  }

  std::int64_t stream_transfer_size()
  {
    return get_int(GX_DS_INT_STREAM_TRANSFER_SIZE);
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

  Frame_data capture(const std::chrono::milliseconds timeout)
  {
    Frame_data result;
    result.data.pImgBuf = std::malloc(static_cast<std::size_t>(payload_size()));
    call(GXGetImage, handle_, &result.data, static_cast<std::int32_t>(timeout.count()));
    return result;
  }

  void trigger_capture()
  {
    call(GXSendCommand, handle_, GX_COMMAND_TRIGGER_SOFTWARE);
  }

  void flush_queue()
  {
    call(GXFlushQueue, handle_);
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

  std::int64_t get_int(const GX_FEATURE_ID feature) const
  {
    std::int64_t result{};
    call(GXGetInt, handle_, feature, &result);
    return result;
  }

  void set_int(const GX_FEATURE_ID feature, const std::int64_t value)
  {
    call(GXSetInt, handle_, feature, value);
  }

  std::pair<double, double> get_float_range(const GX_FEATURE_ID feature) const
  {
    GX_FLOAT_RANGE result{};
    call(GXGetFloatRange, handle_, feature, &result);
    return {result.dMin, result.dMax};
  }

  bool is_implemented(const GX_FEATURE_ID feature) const
  {
    bool result{};
    call(GXIsImplemented, handle_, feature, &result);
    return result;
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
