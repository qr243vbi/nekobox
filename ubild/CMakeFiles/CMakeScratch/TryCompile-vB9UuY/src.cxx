#include <atomic>
#include <cstdint>

int main(int, char **)
{
    volatile std::atomic<char>    size_1;
    volatile std::atomic<short>   size_2;
    volatile std::atomic<int>     size_4;
    volatile std::atomic<int64_t> size_8;

    ++size_1;
    ++size_2;
    ++size_4;
    ++size_8;

    (void)size_1.load(std::memory_order_relaxed);
    (void)size_2.load(std::memory_order_relaxed);
    (void)size_4.load(std::memory_order_relaxed);
    (void)size_8.load(std::memory_order_relaxed);

    return 0;
}
