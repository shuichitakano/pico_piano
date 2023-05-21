/*
 * author : Shuichi TAKANO
 * since  : Sat Jun 01 2019 22:21:32
 */

#include "allocator.h"

namespace physical_modeling_piano
{

///////////////////////////////////
///////////////////////////////////
SimpleLinearAllocator::SimpleLinearAllocator(void* p, size_t size)
    : p_(static_cast<uint8_t*>(p))
    , tail_(p_ + size)
{
}

void*
SimpleLinearAllocator::allocate(size_t size, size_t align)
{
    auto ptr = reinterpret_cast<uintptr_t>(p_);
    auto m   = align - 1;
    ptr      = (ptr + m) & ~m;
    auto r   = reinterpret_cast<uint8_t*>(ptr);
    p_       = r + size;
    assert(p_ <= tail_);
    return r;
}

///////////////////////////////////
///////////////////////////////////

void
PoolAllocator::initialize(size_t unitSize, size_t n)
{
    buffer_.resize(unitSize * n);
    unitSize_ = unitSize;

    for (size_t i = 0; i < n - 1; ++i)
    {
        auto p = getUnit(i);
        *p     = i + 1;
    }
    *getUnit(n - 1) = 0xffffffff;
    freeTop_        = 0;
}

void*
PoolAllocator::allocate()
{
    if (freeTop_ == 0xffffffff)
    {
        return nullptr;
    }

    auto p   = getUnit(freeTop_);
    freeTop_ = *p;

    return p;
}

void
PoolAllocator::free(void* p)
{
    auto p0  = reinterpret_cast<char*>(buffer_.data());
    auto pp  = reinterpret_cast<char*>(p);
    auto idx = (pp - p0) / unitSize_;
    assert(pp - p0 <= buffer_.size());
    assert(pp >= p0);
    assert(p0 + idx * unitSize_ == p);
    *reinterpret_cast<uint32_t*>(p) = freeTop_;
    freeTop_                        = idx;
}

uint32_t*
PoolAllocator::getUnit(size_t i)
{
    return &buffer_[i * unitSize_ / sizeof(uint32_t)];
}

} // namespace physical_modeling_piano
