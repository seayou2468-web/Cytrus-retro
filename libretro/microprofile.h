#pragma once

#define MICROPROFILE_DEFINE(var, group, name, color)
#define MICROPROFILE_DECLARE(var)
#define MICROPROFILE_DEFINE_GPU(var, name, color)
#define MICROPROFILE_DECLARE_GPU(var)
#define MICROPROFILE_SCOPE(var)
#define MICROPROFILE_SCOPEI(group, name, color)
#define MICROPROFILE_SCOPEGPU(var)
#define MICROPROFILE_SCOPEGPUI(name, color)
#define MICROPROFILE_META_CPU(name, count)
#define MICROPROFILE_META_GPU(name, count)
#define MICROPROFILE_FORCEENABLECPUGROUP(s)
#define MICROPROFILE_FORCEDISABLECPUGROUP(s)
#define MICROPROFILE_FORCEENABLEGPUGROUP(s)
#define MICROPROFILE_FORCEDISABLEGPUGROUP(s)
#define MICROPROFILE_SCOPE_TOKEN(token)

#define MicroProfileGetTime(group, name) 0.f
#define MicroProfileOnThreadCreate(foo)
#define MicroProfileFlip()
#define MicroProfileSetAggregateFrames(a)
#define MicroProfileGetAggregateFrames() 0
#define MicroProfileGetCurrentAggregateFrames() 0
#define MicroProfileTogglePause()
#define MicroProfileToggleAllGroups()
#define MicroProfileDumpTimers()
#define MicroProfileShutdown()
#define MicroProfileSetForceEnable(a)
#define MicroProfileGetForceEnable() false
#define MicroProfileSetEnableAllGroups(a)
#define MicroProfileEnableCategory(a)
#define MicroProfileDisableCategory(a)
#define MicroProfileGetEnableAllGroups() false
#define MicroProfileSetForceMetaCounters(a)
#define MicroProfileGetForceMetaCounters() 0
#define MicroProfileEnableMetaCounter(c)
#define MicroProfileDisableMetaCounter(c)
#define MicroProfileDumpFile(html,csv)
#define MicroProfileWebServerPort() ((uint32_t)-1)

#ifndef MP_RGB
#define MP_RGB(r, g, b) ((r) << 16 | (g) << 8 | (b) << 0)
#endif
