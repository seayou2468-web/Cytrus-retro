#include <atomic>
#include <iostream>
#include <cstdint>

struct U128 { uint64_t lo, hi; };

int main() {
    std::atomic<U128> a;
    std::cout << a.is_lock_free() << std::endl;
    return 0;
}
