// Copyright 2017 DeepFabric, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <string>
#include <iostream>

namespace deepfabric
{
namespace logger
{

// use a prefx that does not clash with any predefined macros (e.g. win32 'ERROR')
enum level_t
{
    FATAL,
    ERROR,
    WARN,
    INFO,
    DEBUG,
    TRACE
};

bool enabled(level_t level);
FILE* output(level_t level);
void output(level_t level, FILE* out); // nullptr == /dev/null
void output_le(level_t level, FILE* out); // nullptr == /dev/null
void stack_trace(level_t level);
void stack_trace(level_t level, const std::exception_ptr& eptr);
std::ostream& stream(level_t level);

}
}

inline __attribute__ ((always_inline)) constexpr deepfabric::logger::level_t exception_stack_trace_level()
{
    return deepfabric::logger::DEBUG;
}

#define LOG_FORMATED(level, prefix, format, ...) \
  std::fprintf(::deepfabric::logger::output(level), "%s: %s:%u " format "\n", prefix, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_STREM(level, prefix) \
  ::deepfabric::logger::stream(level) << prefix << " " << __FILE__ << ":" << __LINE__ << " "

#define FRMT_FATAL(format, ...) LOG_FORMATED(::deepfabric::logger::FATAL, "FATAL", format, __VA_ARGS__)
#define FRMT_ERROR(format, ...) LOG_FORMATED(::deepfabric::logger::ERROR, "ERROR", format, __VA_ARGS__)
#define FRMT_WARN(format, ...) LOG_FORMATED(::deepfabric::logger::WARN, "WARN", format, __VA_ARGS__)
#define FRMT_INFO(format, ...) LOG_FORMATED(::deepfabric::logger::INFO, "INFO", format, __VA_ARGS__)
#define FRMT_DEBUG(format, ...) LOG_FORMATED(::deepfabric::logger::DEBUG, "DEBUG", format, __VA_ARGS__)
#define FRMT_TRACE(format, ...) LOG_FORMATED(::deepfabric::logger::TRACE, "TRACE", format, __VA_ARGS__)

#define STRM_FATAL() LOG_STREM(::deepfabric::logger::FATAL, "FATAL")
#define STRM_ERROR() LOG_STREM(::deepfabric::logger::ERROR, "ERROR")
#define STRM_WARN() LOG_STREM(::deepfabric::logger::WARN, "WARN")
#define STRM_INFO() LOG_STREM(::deepfabric::logger::INFO, "INFO")
#define STRM_DEBUG() LOG_STREM(::deepfabric::logger::DEBUG, "DEBUG")
#define STRM_TRACE() LOG_STREM(::deepfabric::logger::TRACE, "TRACE")

#define EXCEPTION() \
  LOG_FORMATED(exception_stack_trace_level(), "EXCEPTION", "@%s\nstack trace:", __FUNCTION__); \
  ::deepfabric::logger::stack_trace(exception_stack_trace_level(), std::current_exception());
#define STACK_TRACE() \
  LOG_FORMATED(exception_stack_trace_level(), "STACK_TRACE", "@%s\nstack trace:", __FUNCTION__); \
  ::deepfabric::logger::stack_trace(exception_stack_trace_level());
