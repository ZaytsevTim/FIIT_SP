#ifndef MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_BUDDIES_SYSTEM_H
#define MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_BUDDIES_SYSTEM_H

#include <pp_allocator.h>
#include <allocator_test_utils.h>
#include <allocator_with_fit_mode.h>
#include <mutex>
#include <cmath>
#include <cstddef>
#include <vector>

// Вспомогательные функции для выравнивания и вычисления степени двойки
namespace __detail
{
    // Выравнивание значения вверх до заданного alignment
    constexpr size_t align_up(size_t value, size_t alignment) noexcept
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    // Находит степень двойки (k), такую что 2^k >= size
    constexpr size_t nearest_greater_k_of_2(size_t size) noexcept
    {
        int ones_counter = 0, index = -1;
        constexpr const size_t o = 1;

        for (int i = sizeof(size_t) * 8 - 1; i >= 0; --i)
        {
            if (size & (o << i))
            {
                if (ones_counter == 0) index = i;
                ++ones_counter;
            }
        }

        return ones_counter <= 1 ? index : index + 1;
    }
}

// Аллокатор  (Buddy System)
// Память делится на блоки, размеры которых — степени двойки
class allocator_buddies_system final:
    public smart_mem_resource,        // Полиморфный аллокатор
    public allocator_test_utils,      // Для тестов (get_blocks_info)
    public allocator_with_fit_mode    // Для смены стратегии выделения
{

private:

    // Метаданные блока: занят/свободен + размер (степень двойки)
    struct block_metadata
    {
        bool occupied : 1;      // 1 бит: занят?
        unsigned char size : 7; // 7 бит: степень двойки (2^size байт)
    };

    void *_trusted_memory;  // Единственное поле — указатель на доверенную область

    // Смещения в доверенной памяти (с учётом выравнивания)
    static constexpr const size_t parent_off = 0;
    static constexpr const size_t mode_off = __detail::align_up(parent_off + sizeof(std::pmr::memory_resource*), alignof(fit_mode));
    static constexpr const size_t power_off = __detail::align_up(mode_off + sizeof(fit_mode), alignof(unsigned char));
    static constexpr const size_t mutex_off = __detail::align_up(power_off + sizeof(unsigned char), alignof(std::mutex));
    static constexpr const size_t allocator_metadata_size = __detail::align_up(mutex_off + sizeof(std::mutex), alignof(std::max_align_t));
    static constexpr const size_t occupied_block_metadata_size = __detail::align_up(sizeof(block_metadata), alignof(void*));
    static constexpr const size_t free_block_metadata_size = sizeof(block_metadata);
    static constexpr const size_t min_k = __detail::nearest_greater_k_of_2(sizeof(allocator_dbg_helper::block_pointer_t) + 1);

public:
    // Конструктор: space_size_power_of_two — размер (будет округлён до степени двойки)
    explicit allocator_buddies_system(
            size_t space_size_power_of_two,
            std::pmr::memory_resource *parent_allocator = nullptr,
            allocator_with_fit_mode::fit_mode allocate_fit_mode = allocator_with_fit_mode::fit_mode::first_fit);

    allocator_buddies_system(allocator_buddies_system const &other);
    allocator_buddies_system &operator=(allocator_buddies_system const &other);
    allocator_buddies_system(allocator_buddies_system &&other) noexcept;
    allocator_buddies_system &operator=(allocator_buddies_system &&other) noexcept;
    ~allocator_buddies_system() override;

private:
    // Основные методы аллокатора
    [[nodiscard]] void *do_allocate_sm(size_t size) override;
    void do_deallocate_sm(void *at) override;
    
    // Проверка совместимости аллокаторов (по типу)
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;
    
    // Смена стратегии выделения
    inline void set_fit_mode(allocator_with_fit_mode::fit_mode mode) override;

    // Для тестов: получить информацию о всех блоках
    std::vector<allocator_test_utils::block_info> get_blocks_info() const noexcept override;

private:
    std::vector<allocator_test_utils::block_info> get_blocks_info_inner() const override;

    // Итератор по блокам в доверенной области
    class buddy_iterator
    {
        void* _block;  // Текущий блок

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = void*;
        using reference = void*&;
        using pointer = void**;
        using difference_type = ptrdiff_t;

        bool operator==(const buddy_iterator&) const noexcept;
        bool operator!=(const buddy_iterator&) const noexcept;
        buddy_iterator& operator++() & noexcept;   // Переход к следующему блоку
        buddy_iterator operator++(int n);
        
        size_t size() const noexcept;       // Размер текущего блока
        bool occupied() const noexcept;     // Занят ли блок?
        void* operator*() const noexcept;   // Для занятых — указатель на данные, для свободных — на заголовок

        buddy_iterator();
        buddy_iterator(void* start);
    };

    friend class buddy_iterator;

    buddy_iterator begin() const noexcept;
    buddy_iterator end() const noexcept;
};

#endif