// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"
#include <cstring>

#if _MSC_VER
#include <intrin.h>
#endif

namespace Common {

#if _MSC_VER

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u8* pointer, u8 value, u8 expected) {
    const u8 result =
        _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(pointer), value, expected);
    return result == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u16* pointer, u16 value, u16 expected) {
    const u16 result =
        _InterlockedCompareExchange16(reinterpret_cast<volatile short*>(pointer), value, expected);
    return result == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u32* pointer, u32 value, u32 expected) {
    const u32 result =
        _InterlockedCompareExchange(reinterpret_cast<volatile long*>(pointer), value, expected);
    return result == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u64* pointer, u64 value, u64 expected) {
    const u64 result = _InterlockedCompareExchange64(reinterpret_cast<volatile __int64*>(pointer),
                                                     value, expected);
    return result == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u64* pointer, u128 value, u128 expected) {
    return _InterlockedCompareExchange128(reinterpret_cast<volatile __int64*>(pointer), value[1],
                                          value[0],
                                          reinterpret_cast<__int64*>(expected.data())) != 0;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u8* pointer, u8 value, u8 expected,
                                               u8& actual) {
    actual =
        _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(pointer), value, expected);
    return actual == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u16* pointer, u16 value, u16 expected,
                                               u16& actual) {
    actual =
        _InterlockedCompareExchange16(reinterpret_cast<volatile short*>(pointer), value, expected);
    return actual == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u32* pointer, u32 value, u32 expected,
                                               u32& actual) {
    actual =
        _InterlockedCompareExchange(reinterpret_cast<volatile long*>(pointer), value, expected);
    return actual == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u64* pointer, u64 value, u64 expected,
                                               u64& actual) {
    actual = _InterlockedCompareExchange64(reinterpret_cast<volatile __int64*>(pointer), value,
                                           expected);
    return actual == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u64* pointer, u128 value, u128 expected,
                                               u128& actual) {
    const bool result =
        _InterlockedCompareExchange128(reinterpret_cast<volatile __int64*>(pointer), value[1],
                                       value[0], reinterpret_cast<__int64*>(expected.data())) != 0;
    actual = expected;
    return result;
}

[[nodiscard]] inline u128 AtomicLoad128(volatile u64* pointer) {
    u128 result{};
    _InterlockedCompareExchange128(reinterpret_cast<volatile __int64*>(pointer), result[1],
                                   result[0], reinterpret_cast<__int64*>(result.data()));
    return result;
}

#else

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u8* pointer, u8 value, u8 expected) {
    return __sync_bool_compare_and_swap(pointer, expected, value);
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u16* pointer, u16 value, u16 expected) {
    return __sync_bool_compare_and_swap(pointer, expected, value);
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u32* pointer, u32 value, u32 expected) {
    return __sync_bool_compare_and_swap(pointer, expected, value);
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u64* pointer, u64 value, u64 expected) {
    return __sync_bool_compare_and_swap(pointer, expected, value);
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u64* pointer, u128 value, u128 expected) {
#if defined(__aarch64__)
    u64 res_lo, res_hi;
    int status;
    __asm__ __volatile__(
        "1: ldxp %0, %1, [%3]\n"
        "cmp %0, %4\n"
        "ccmp %1, %5, #0, eq\n"
        "b.ne 2f\n"
        "stxp %w2, %6, %7, [%3]\n"
        "cbnz %w2, 1b\n"
        "mov %w2, #0\n"
        "b 3f\n"
        "2: clrex\n"
        "mov %w2, #1\n"
        "3:\n"
        : "=&r"(res_lo), "=&r"(res_hi), "=&r"(status)
        : "r"(pointer), "r"(expected[0]), "r"(expected[1]), "r"(value[0]), "r"(value[1])
        : "cc", "memory"
    );
    return status == 0;
#elif defined(__x86_64__)
    bool success;
    __asm__ __volatile__(
        "lock; cmpxchg16b %1\n"
        "setz %0"
        : "=q"(success), "+m"(*pointer), "+a"(expected[0]), "+d"(expected[1])
        : "b"(value[0]), "c"(value[1])
        : "cc", "memory"
    );
    return success;
#else
    // Fallback: This is NOT atomic!
    if (pointer[0] == expected[0] && pointer[1] == expected[1]) {
        pointer[0] = value[0];
        pointer[1] = value[1];
        return true;
    }
    return false;
#endif
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u8* pointer, u8 value, u8 expected,
                                               u8& actual) {
    actual = __sync_val_compare_and_swap(pointer, expected, value);
    return actual == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u16* pointer, u16 value, u16 expected,
                                               u16& actual) {
    actual = __sync_val_compare_and_swap(pointer, expected, value);
    return actual == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u32* pointer, u32 value, u32 expected,
                                               u32& actual) {
    actual = __sync_val_compare_and_swap(pointer, expected, value);
    return actual == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u64* pointer, u64 value, u64 expected,
                                               u64& actual) {
    actual = __sync_val_compare_and_swap(pointer, expected, value);
    return actual == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u64* pointer, u128 value, u128 expected,
                                               u128& actual) {
#if defined(__aarch64__)
    u64 res_lo, res_hi;
    int status;
    __asm__ __volatile__(
        "1: ldxp %0, %1, [%3]\n"
        "cmp %0, %4\n"
        "ccmp %1, %5, #0, eq\n"
        "b.ne 2f\n"
        "stxp %w2, %6, %7, [%3]\n"
        "cbnz %w2, 1b\n"
        "mov %w2, #0\n"
        "b 3f\n"
        "2: clrex\n"
        "mov %w2, #1\n"
        "3:\n"
        : "=&r"(res_lo), "=&r"(res_hi), "=&r"(status)
        : "r"(pointer), "r"(expected[0]), "r"(expected[1]), "r"(value[0]), "r"(value[1])
        : "cc", "memory"
    );
    actual[0] = res_lo;
    actual[1] = res_hi;
    return status == 0;
#elif defined(__x86_64__)
    bool success;
    __asm__ __volatile__(
        "lock; cmpxchg16b %1\n"
        "setz %0"
        : "=q"(success), "+m"(*pointer), "+a"(expected[0]), "+d"(expected[1])
        : "b"(value[0]), "c"(value[1])
        : "cc", "memory"
    );
    actual[0] = expected[0];
    actual[1] = expected[1];
    return success;
#else
    actual[0] = pointer[0];
    actual[1] = pointer[1];
    if (actual[0] == expected[0] && actual[1] == expected[1]) {
        pointer[0] = value[0];
        pointer[1] = value[1];
        return true;
    }
    return false;
#endif
}

[[nodiscard]] inline u128 AtomicLoad128(volatile u64* pointer) {
#if defined(__aarch64__)
    u64 res_lo, res_hi;
    __asm__ __volatile__(
        "ldxp %0, %1, [%2]\n"
        "clrex\n"
        : "=&r"(res_lo), "=&r"(res_hi)
        : "r"(pointer)
        : "cc", "memory"
    );
    return {res_lo, res_hi};
#elif defined(__x86_64__)
    u128 result;
    // cmpxchg16b with same value to perform atomic load
    __asm__ __volatile__(
        "lock; cmpxchg16b %0"
        : "+m"(*pointer), "=a"(result[0]), "=d"(result[1])
        : "a"(0), "d"(0), "b"(0), "c"(0)
        : "cc", "memory"
    );
    return result;
#else
    return {pointer[0], pointer[1]};
#endif
}

#endif

} // namespace Common
