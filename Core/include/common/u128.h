#pragma once

#include "common/common_types.h"

struct U128 {
    u64 lo;
    u64 hi;

    constexpr U128() : lo(0), hi(0) {}
    constexpr U128(u64 lo) : lo(lo), hi(0) {}
    constexpr U128(u64 lo, u64 hi) : lo(lo), hi(hi) {}

    static constexpr U128 FromU64(u64 val) { return U128(val, 0); }

    constexpr u64 ToU64() const { return lo; }

    constexpr U128 operator+(const U128& other) const {
        u64 res_lo = lo + other.lo;
        u64 res_hi = hi + other.hi + (res_lo < lo);
        return {res_lo, res_hi};
    }

    constexpr U128 operator-(const U128& other) const {
        u64 res_lo = lo - other.lo;
        u64 res_hi = hi - other.hi - (lo < other.lo);
        return {res_lo, res_hi};
    }

    constexpr U128 operator|(const U128& other) const { return {lo | other.lo, hi | other.hi}; }
    constexpr U128 operator&(const U128& other) const { return {lo & other.lo, hi & other.hi}; }
    constexpr U128 operator^(const U128& other) const { return {lo ^ other.lo, hi ^ other.hi}; }
    constexpr U128 operator~() const { return {~lo, ~hi}; }

    constexpr U128 operator<<(int shift) const {
        if (shift == 0) return *this;
        if (shift >= 128) return {0, 0};
        if (shift >= 64) return {0, lo << (shift - 64)};
        return {lo << shift, (hi << shift) | (lo >> (64 - shift))};
    }

    constexpr U128 operator>>(int shift) const {
        if (shift == 0) return *this;
        if (shift >= 128) return {0, 0};
        if (shift >= 64) return {hi >> (shift - 64), 0};
        return {(lo >> shift) | (hi << (64 - shift)), hi >> shift};
    }

    static constexpr U128 Multiply64(u64 a, u64 b) {
        u64 a_lo = (u32)a;
        u64 a_hi = a >> 32;
        u64 b_lo = (u32)b;
        u64 b_hi = b >> 32;

        u64 p0 = a_lo * b_lo;
        u64 p1 = a_lo * b_hi;
        u64 p2 = a_hi * b_lo;
        u64 p3 = a_hi * b_hi;

        u64 cy = (p0 >> 32) + (u32)p1 + (u32)p2;
        u64 res_lo = (p0 & 0xFFFFFFFF) | (cy << 32);
        u64 res_hi = p3 + (p1 >> 32) + (p2 >> 32) + (cy >> 32);

        return {res_lo, res_hi};
    }

    static constexpr U128 SignedMultiply64(s64 a, s64 b) {
        U128 res = Multiply64((u64)a, (u64)b);
        if (a < 0) res.hi -= (u64)b;
        if (b < 0) res.hi -= (u64)a;
        return res;
    }

    constexpr bool operator==(const U128& other) const { return lo == other.lo && hi == other.hi; }
    constexpr bool operator!=(const U128& other) const { return !(*this == other); }
    constexpr explicit operator bool() const { return lo != 0 || hi != 0; }
};
