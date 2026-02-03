/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <algorithm>
#include "dynarmic/interface/exclusive_monitor.h"
#include "dynarmic/common/spin_lock.h"

namespace Dynarmic {

// SpinLock implementation using GCC/Clang builtins for portability and No-JIT
void SpinLock::Lock() {
    while (__sync_lock_test_and_set(&storage, 1));
}

void SpinLock::Unlock() {
    __sync_lock_release(&storage);
}

ExclusiveMonitor::ExclusiveMonitor(size_t processor_count)
    : exclusive_addresses(processor_count, INVALID_EXCLUSIVE_ADDRESS), exclusive_values(processor_count) {}

size_t ExclusiveMonitor::GetProcessorCount() const {
    return exclusive_addresses.size();
}

void ExclusiveMonitor::Lock() {
    lock.Lock();
}

void ExclusiveMonitor::Unlock() {
    lock.Unlock();
}

bool ExclusiveMonitor::CheckAndClear(size_t processor_id, VAddr address) {
    const VAddr masked_address = address & RESERVATION_GRANULE_MASK;

    Lock();
    if (exclusive_addresses[processor_id] != masked_address) {
        Unlock();
        return false;
    }

    for (VAddr& other_address : exclusive_addresses) {
        if (other_address == masked_address) {
            other_address = INVALID_EXCLUSIVE_ADDRESS;
        }
    }
    return true;
}

void ExclusiveMonitor::Clear() {
    Lock();
    std::fill(exclusive_addresses.begin(), exclusive_addresses.end(), INVALID_EXCLUSIVE_ADDRESS);
    Unlock();
}

void ExclusiveMonitor::ClearProcessor(size_t processor_id) {
    Lock();
    exclusive_addresses[processor_id] = INVALID_EXCLUSIVE_ADDRESS;
    Unlock();
}

} // namespace Dynarmic

// SoundTouch stub
extern "C" {
    unsigned int detectCPUextensions(void) {
        return 0; // No extensions
    }
}
