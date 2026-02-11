#pragma once

#define MICROPROFILE_DEFINE(...)
#define MICROPROFILE_DECLARE(...)
#define MICROPROFILE_DEFINE_GPU(...)
#define MICROPROFILE_DECLARE_GPU(...)
#define MICROPROFILE_SCOPE(...)
#define MICROPROFILE_SCOPEI(...)
#define MICROPROFILE_SCOPEGPU(...)
#define MICROPROFILE_SCOPEGPUI(...)
#define MICROPROFILE_META_CPU(...)
#define MICROPROFILE_META_GPU(...)
#define MICROPROFILE_FORCEENABLECPUGROUP(...)
#define MICROPROFILE_FORCEDISABLECPUGROUP(...)
#define MICROPROFILE_FORCEENABLEGPUGROUP(...)
#define MICROPROFILE_FORCEDISABLEGPUGROUP(...)
#define MICROPROFILE_SCOPE_TOKEN(...)

#define MicroProfileGetTime(...) 0.f
#define MicroProfileOnThreadCreate(...)
#define MicroProfileFlip()
#define MicroProfileSetAggregateFrames(...)
#define MicroProfileGetAggregateFrames() 0
#define MicroProfileGetCurrentAggregateFrames() 0
#define MicroProfileTogglePause()
#define MicroProfileToggleAllGroups()
#define MicroProfileDumpTimers()
#define MicroProfileShutdown()
#define MicroProfileSetForceEnable(...)
#define MicroProfileGetForceEnable() false
#define MicroProfileSetEnableAllGroups(...)
#define MicroProfileEnableCategory(...)
#define MicroProfileDisableCategory(...)
#define MicroProfileGetEnableAllGroups() false
#define MicroProfileSetForceMetaCounters(...)
#define MicroProfileGetForceMetaCounters() 0
#define MicroProfileEnableMetaCounter(...)
#define MicroProfileDisableMetaCounter(...)
#define MicroProfileDumpFile(...)
#define MicroProfileWebServerPort() ((uint32_t)-1)

#ifndef MP_RGB
#define MP_RGB(r, g, b) ((r) << 16 | (g) << 8 | (b) << 0)
#endif

typedef uint64_t MicroProfileToken;
