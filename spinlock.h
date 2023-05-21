/*
 * author : Shuichi TAKANO
 * since  : Sun May 01 2022 04:00:10
 */
#pragma once

#include <stdint.h>
#include "hardware/sync.h"

namespace util
{
    class SpinLock
    {
        uint32_t idx_;
        uint32_t irqState_{};

    public:
        SpinLock()
        {
            idx_ = spin_lock_claim_unused(true);
        }

        ~SpinLock()
        {
            spin_lock_unclaim(idx_);
        }

        __attribute__((always_inline)) auto *get()
        {
            return spin_lock_instance(idx_);
        }

        __attribute__((always_inline)) void lock()
        {
            irqState_ = spin_lock_blocking(get());
        }

        __attribute__((always_inline)) void unlock()
        {
            spin_unlock(get(), irqState_);
        }
    };
}
