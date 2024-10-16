#include <not_implemented.h>

#include "../include/allocator_global_heap.h"

allocator_global_heap::allocator_global_heap(logger *logger) //constructor
{
    _logger = logger;
}

allocator_global_heap::~allocator_global_heap() //destructor
{
    _logger = nullptr;
}

allocator_global_heap::allocator_global_heap(allocator_global_heap &&other) noexcept //move constructor
{
    _logger = other._logger;
    other._logger = nullptr;
}

allocator_global_heap &allocator_global_heap::operator=(allocator_global_heap &&other) noexcept //move assignment operator
{
        if (this != &other)
        {
            _logger = other._logger;
            other._logger = nullptr;
        }
    return *this;
}

[[nodiscard]] void *allocator_global_heap::allocate(
    size_t value_size,
    size_t values_count)
{
    try
    {
        return ::operator new(value_size*values_count);
    }
    catch (std::bad_alloc &ex)
    {
        //передаём логгеру
        throw ex;
    }
}

void allocator_global_heap::deallocate(void *at) //destructor
{
    ::operator delete (at);
}

inline logger *allocator_global_heap::get_logger() const
{
    return _logger;
}

inline std::string allocator_global_heap::get_typename() const noexcept
{
    return "allocator_global_heap";
}