// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#ifdef __cplusplus
#include <cassert>
#include <cstdlib>
#else
#include <assert.h>
#include <stdlib.h>
#endif

#include "common/common_funcs.h"

#ifdef ASSERT
#undef ASSERT
#endif

#ifdef __cplusplus
#define ASSERT(_a_)                                                                                \
    ((_a_) ? (void)0 : (void)([] { assert(false); }()))
#else
#define ASSERT(_a_)                                                                                \
    ((_a_) ? (void)0 : (void)assert(0))
#endif

#ifdef ASSERT_MSG
#undef ASSERT_MSG
#endif
#define ASSERT_MSG(_a_, ...) ASSERT(_a_)

#ifdef UNREACHABLE
#undef UNREACHABLE
#endif
#define UNREACHABLE() ASSERT(false)

#ifdef UNREACHABLE_MSG
#undef UNREACHABLE_MSG
#endif
#define UNREACHABLE_MSG(...) ASSERT(false)

#ifdef _DEBUG
#define DEBUG_ASSERT(_a_) ASSERT(_a_)
#define DEBUG_ASSERT_MSG(_a_, ...) ASSERT_MSG(_a_, __VA_ARGS__)
#else
#define DEBUG_ASSERT(_a_)
#define DEBUG_ASSERT_MSG(_a_, ...)
#endif

#define UNIMPLEMENTED() ASSERT(false)
#define UNIMPLEMENTED_MSG(_a_, ...) ASSERT(false)
