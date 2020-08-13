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

#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#ifndef DMITIGR_GENICAM_DAHENG_GX_HPP
#define DMITIGR_GENICAM_DAHENG_GX_HPP

namespace dmitigr::genicam::daheng::gx {

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
