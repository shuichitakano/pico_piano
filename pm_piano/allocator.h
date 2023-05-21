/*
 * author : Shuichi TAKANO
 * since  : Sat Jun 01 2019 10:17:46
 */
#ifndef EEC7A464_A134_158E_9B58_A1883B48ECDC
#define EEC7A464_A134_158E_9B58_A1883B48ECDC

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>

namespace physical_modeling_piano
{

class SimpleLinearAllocator
{
    uint8_t* p_{};
    uint8_t* tail_{};

public:
    SimpleLinearAllocator() = default;
    SimpleLinearAllocator(void* p, size_t size);
    void* allocate(size_t size, size_t align = 4);
    explicit operator bool() const { return p_; }
};

class PoolAllocator
{
    std::vector<uint32_t> buffer_;
    size_t unitSize_{};
    uint32_t freeTop_{};

public:
    PoolAllocator() = default;
    PoolAllocator(size_t unitSize, size_t n) { initialize(unitSize, n); }
    void initialize(size_t unitSize, size_t n);
    void* allocate();
    void free(void* p);

    size_t getUnitSize() const { return unitSize_; }

protected:
    uint32_t* getUnit(size_t i);
};

} // namespace physical_modeling_piano

#endif /* EEC7A464_A134_158E_9B58_A1883B48ECDC */
