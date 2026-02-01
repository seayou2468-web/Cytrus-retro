// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cassert>
#ifndef assert
#define assert ASSERT
#endif
#include <cstdlib>
#include "common/common_funcs.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"

// For asserts we'd like to keep all the junk executed when an assert happens away from the
// important code in the function. One way of doing this is to put all the relevant code inside a
// lambda and force the compiler to not inline it.

#ifdef ASSERT
#undef ASSERT
#endif
#define ASSERT(_a_)                                                                                \
    do                                                                                             \
        if (!(_a_)) [[unlikely]] {                                                                 \
            []() CITRA_NO_INLINE CITRA_NO_RETURN {                                                 \
                LOG_CRITICAL(Debug, "Assertion Failed!");                                          \
                Common::Log::Stop();                                                               \
                Crash();                                                                           \
                exit(1);                                                                           \
            }();                                                                                   \
        }                                                                                          \
    while (0)

#ifdef ASSERT_MSG
#undef ASSERT_MSG
#endif
#define ASSERT_MSG(_a_, ...)                                                                       \
    do                                                                                             \
        if (!(_a_)) [[unlikely]] {                                                                 \
            [&]() CITRA_NO_INLINE CITRA_NO_RETURN {                                                \
                LOG_CRITICAL(Debug, "Assertion Failed!\n" __VA_ARGS__);                            \
                Common::Log::Stop();                                                               \
                Crash();                                                                           \
                exit(1);                                                                           \
            }();                                                                                   \
        }                                                                                          \
    while (0)

#ifdef UNREACHABLE
#undef UNREACHABLE
#endif
#define UNREACHABLE()                                                                              \
    ([]() CITRA_NO_INLINE CITRA_NO_RETURN {                                                        \
        LOG_CRITICAL(Debug, "Unreachable code!");                                                  \
        Common::Log::Stop();                                                                       \
        Crash();                                                                                   \
        exit(1);                                                                                   \
    }())

#ifdef UNREACHABLE_MSG
#undef UNREACHABLE_MSG
#endif
#define UNREACHABLE_MSG(...)                                                                       \
    ([&]() CITRA_NO_INLINE CITRA_NO_RETURN {                                                       \
        LOG_CRITICAL(Debug, "Unreachable code!\n" __VA_ARGS__);                                    \
        Common::Log::Stop();                                                                       \
        Crash();                                                                                   \
        exit(1);                                                                                   \
    }())

#ifdef _DEBUG
#ifdef DEBUG_ASSERT
#undef DEBUG_ASSERT
#endif
#define DEBUG_ASSERT(_a_) ASSERT(_a_)
#ifdef DEBUG_ASSERT_MSG
#undef DEBUG_ASSERT_MSG
#endif
#define DEBUG_ASSERT_MSG(_a_, ...) ASSERT_MSG(_a_, __VA_ARGS__)
#else // not debug
#ifdef DEBUG_ASSERT
#undef DEBUG_ASSERT
#endif
#define DEBUG_ASSERT(_a_)
#ifdef DEBUG_ASSERT_MSG
#undef DEBUG_ASSERT_MSG
#endif
#define DEBUG_ASSERT_MSG(_a_, _desc_, ...)
#endif

#define UNIMPLEMENTED() LOG_CRITICAL(Debug, "Unimplemented code!")
#define UNIMPLEMENTED_MSG(_a_, ...) LOG_CRITICAL(Debug, _a_, __VA_ARGS__)
