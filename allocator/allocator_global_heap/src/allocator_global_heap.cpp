#include "../include/allocator_global_heap.h"
#include <new>

allocator_global_heap::allocator_global_heap() = default;

[[nodiscard]] void *allocator_global_heap::do_allocate_sm(size_t size)
{
    std::lock_guard<std::mutex> const lock(_mutex);  // Потокобезопасность
    return ::operator new(size);  // Выделение из глобальной кучи
}

void allocator_global_heap::do_deallocate_sm(void *at)
{
    std::lock_guard<std::mutex> const lock(_mutex);
    ::operator delete(at);  // Освобождение в глобальную кучу
}

allocator_global_heap::~allocator_global_heap() = default;

allocator_global_heap::allocator_global_heap(const allocator_global_heap &other) {}
// Мьютекс не копируется, других полей нет

allocator_global_heap &allocator_global_heap::operator=(const allocator_global_heap &other)
{
    return *this;  // Ничего не присваиваем
}

bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    // Аллокаторы одного типа совместимы
    return dynamic_cast<const allocator_global_heap *>(&other) != nullptr;
}

allocator_global_heap::allocator_global_heap(allocator_global_heap &&other) noexcept {}

allocator_global_heap &allocator_global_heap::operator=(allocator_global_heap &&other) noexcept
{
    return *this;
}