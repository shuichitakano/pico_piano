/*
 * author : Shuichi TAKANO
 * since  : Sun Apr 21 2019 1:7:21
 */
#ifndef EB296768_9134_14C0_1065_A859B7225127
#define EB296768_9134_14C0_1065_A859B7225127

#include "fixed.h"
#include <algorithm>
#include <array>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

namespace physical_modeling_piano
{

template <class T = float>
class DelayState
{
    size_t mask_   = 0;
    size_t cursor_ = 0;
    T* buffer_{};

public:
    DelayState() = default;
    DelayState(T* buffer, size_t size) { attachBuffer(buffer, size); }

    void attachBuffer(T* buffer, size_t size)
    {
        buffer_ = buffer;
        mask_   = size - 1;
        assert(((size & (size - 1)) == 0) && "size must be pow of 2");
    }

    template <size_t N>
    DelayState(std::array<T, N>& buffer)
        : DelayState(buffer.data(), N)
    {
    }

    inline T update(const T& in, size_t delay)
    {
        if (delay)
        {
            auto r = buffer_[(cursor_ - delay) & mask_];

            buffer_[cursor_] = in;
            cursor_          = (cursor_ + 1) & mask_;
            return r;
        }
        else
        {
            return in;
        }
    }

    void clear(size_t delay)
    {
        //        printf("delay = %zd/%zd\n", delay, mask_);
        assert(delay <= mask_);
        memset(buffer_, 0, sizeof(T) * (delay + 1));
        cursor_ = delay;
    }
};

template <size_t Size>
class Delay
{
    std::array<float, Size> buffer_{};
    DelayState<> state_{buffer_.data(), Size};

    size_t delay_{0};

public:
    void initialize(int delay)
    {
        delay_ = delay;
        state_.clear(std::max(0, delay));
    }
    float update(float in) { return state_.update(in, delay_); }
};

inline size_t
computeDelayBufferSize(size_t delay)
{
    delay += 1;
    if (delay == 1)
    {
        return 1;
    }
    int lz = __builtin_clz((uint32_t)delay - 1);
    return 1 << (32 - lz);
}

} // namespace physical_modeling_piano

#endif /* EB296768_9134_14C0_1065_A859B7225127 */
