#include <cstddef>
#include "../include/allocator_buddies_system.h"
#include <memory>
#include <stdexcept>

// Константы из .h - используем их через allocator_buddies_system::
// parent_off, mode_off, power_off, mutex_off, allocator_metadata_size,
// occupied_block_metadata_size, min_k

// ДЕСТРУКТОР
allocator_buddies_system::~allocator_buddies_system()
{
    // Если память не выделена — выходим
    if (_trusted_memory == nullptr)
        return;
    
    // Превращаем void* в char* для байтовой арифметики
    char* base = reinterpret_cast<char*>(_trusted_memory);
    
    // Читаем родительский аллокатор по смещению parent_off
    auto* parent = *reinterpret_cast<std::pmr::memory_resource**>(base + parent_off);
    
    // Читаем максимальную степень двойки (размер области = 2^max_power)
    const unsigned char max_power = *reinterpret_cast<unsigned char*>(base + power_off);
    
    // Получаем указатель на мьютекс по смещению mutex_off
    auto* mtx = reinterpret_cast<std::mutex*>(base + mutex_off);
    
    // Явно вызываем деструктор мьютекса (placement new требует этого!)
    std::destroy_at(mtx);
    
    // Вычисляем размер управляемой области: 2^max_power
    const size_t managed = static_cast<size_t>(1) << max_power;
    
    // Возвращаем память родителю
    parent->deallocate(_trusted_memory, allocator_metadata_size + managed);
    
    // Обнуляем указатель
    _trusted_memory = nullptr;
}

// КОНСТРУКТОР ПЕРЕМЕЩЕНИЯ
allocator_buddies_system::allocator_buddies_system(
    allocator_buddies_system &&other) noexcept
    : _trusted_memory(nullptr)
{
    // Если у other нет памяти — выходим
    if (other._trusted_memory == nullptr)
        return;
    
    // Получаем байтовый указатель на память other
    char* other_base = reinterpret_cast<char*>(other._trusted_memory);
    
    // Получаем мьютекс other по смещению mutex_off
    auto* other_mtx = reinterpret_cast<std::mutex*>(other_base + mutex_off);
    
    // Блокируем мьютекс other (чтобы никто не мешал при перемещении)
    std::lock_guard<std::mutex> lock(*other_mtx);
    
    // Забираем память у other
    _trusted_memory = other._trusted_memory;
    
    // Обнуляем other (чтобы он не пытался освободить память при своём разрушении)
    other._trusted_memory = nullptr;
}

// ОПЕРАТОР ПРИСВАИВАНИЯ ПЕРЕМЕЩЕНИЕМ
allocator_buddies_system &allocator_buddies_system::operator=(
    allocator_buddies_system &&other) noexcept
{
    // Защита от самоприсваивания (a = std::move(a))
    if (this == &other)
        return *this;
    
    // Пытаемся получить мьютекс текущего объекта
    std::mutex* this_mtx = nullptr;
    if (_trusted_memory != nullptr)
    {
        char* this_base = reinterpret_cast<char*>(_trusted_memory);
        this_mtx = reinterpret_cast<std::mutex*>(this_base + mutex_off);
    }
    
    // Пытаемся получить мьютекс other
    std::mutex* other_mtx = nullptr;
    if (other._trusted_memory != nullptr)
    {
        char* other_base = reinterpret_cast<char*>(other._trusted_memory);
        other_mtx = reinterpret_cast<std::mutex*>(other_base + mutex_off);
    }
    
    void* old_memory = nullptr;
    
    // Блокируем мьютексы в правильном порядке (чтобы избежать deadlock)
    if (this_mtx != nullptr && other_mtx != nullptr && this_mtx != other_mtx)
    {
        std::scoped_lock lock(*this_mtx, *other_mtx);  // блокируем оба
        old_memory = _trusted_memory;
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    else if (this_mtx != nullptr)
    {
        std::lock_guard<std::mutex> lock(*this_mtx);
        old_memory = _trusted_memory;
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    else if (other_mtx != nullptr)
    {
        std::lock_guard<std::mutex> lock(*other_mtx);
        old_memory = _trusted_memory;
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    else
    {
        old_memory = _trusted_memory;
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    
    // Если была старая память — освобождаем её
    if (old_memory != nullptr)
    {
        char* old_base = reinterpret_cast<char*>(old_memory);
        auto* parent = *reinterpret_cast<std::pmr::memory_resource**>(old_base + parent_off);
        const unsigned char max_power = *reinterpret_cast<unsigned char*>(old_base + power_off);
        auto* mtx = reinterpret_cast<std::mutex*>(old_base + mutex_off);
        std::destroy_at(mtx);
        const size_t managed = static_cast<size_t>(1) << max_power;
        parent->deallocate(old_memory, allocator_metadata_size + managed);
    }
    
    return *this;
}

// КОНСТРУКТОР С ПАРАМЕТРАМИ
allocator_buddies_system::allocator_buddies_system(
        size_t space_size_power_of_two,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    // Если родитель не указан — используем глобальный аллокатор по умолчанию
    if (parent_allocator == nullptr)
        parent_allocator = std::pmr::get_default_resource();
    
    // Округляем размер до степени двойки вверх
    size_t normalized = 1;
    while (normalized < space_size_power_of_two)
        normalized *= 2;
    
    // Проверяем, что размер не меньше минимального (обычно 16 байт)
    if (normalized < (static_cast<size_t>(1) << min_k))
        throw std::logic_error("too small memory region");
    
    // Выделяем память у родителя
    _trusted_memory = parent_allocator->allocate(allocator_metadata_size + normalized);
    
    char* base = reinterpret_cast<char*>(_trusted_memory);
    
    // Сохраняем родителя по смещению parent_off
    *reinterpret_cast<std::pmr::memory_resource**>(base + parent_off) = parent_allocator;
    
    // Сохраняем стратегию выделения по смещению mode_off
    *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(base + mode_off) = allocate_fit_mode;
    
    // Сохраняем максимальную степень двойки
    *reinterpret_cast<unsigned char*>(base + power_off) = 
        static_cast<unsigned char>(__detail::nearest_greater_k_of_2(normalized));
    
    // Создаём мьютекс через placement new в доверенной памяти
    new (base + mutex_off) std::mutex();
    
    // Создаём корневой блок (вся область свободна)
    char* first_block = base + allocator_metadata_size;
    auto* root_md = reinterpret_cast<block_metadata*>(first_block);
    root_md->occupied = false;              // блок свободен
    root_md->size = *reinterpret_cast<unsigned char*>(base + power_off);  // степень двойки
}

// ВЫДЕЛЕНИЕ ПАМЯТИ (do_allocate_sm)
[[nodiscard]] void *allocator_buddies_system::do_allocate_sm(size_t size)
{
    // Стандарт требует, чтобы allocate(0) возвращал ненулевой указатель
    if (size == 0)
        size = 1;
    
    char* base = reinterpret_cast<char*>(_trusted_memory);
    
    // Блокируем мьютекс на всё время выделения
    auto* mtx = reinterpret_cast<std::mutex*>(base + mutex_off);
    std::lock_guard<std::mutex> lock(*mtx);
    
    // Читаем текущую стратегию и максимальную степень двойки
    const auto mode = *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(base + mode_off);
    const unsigned char max_k = *reinterpret_cast<unsigned char*>(base + power_off);
    
    // Границы управляемой области
    char* region_begin = base + allocator_metadata_size;
    char* region_end = region_begin + (static_cast<size_t>(1) << max_k);
    
    // Вычисляем нужную степень двойки (округление вверх)
    const size_t required_total = size + occupied_block_metadata_size;
    size_t req_k = __detail::nearest_greater_k_of_2(required_total == 0 ? 1 : required_total);
    
    // Проверяем, что степень в допустимых пределах
    if (req_k < min_k)
        req_k = min_k;
    if (req_k > max_k)
        throw std::bad_alloc();
    
    char* chosen = nullptr;
    unsigned char chosen_k = 0;
    char* cursor = region_begin;
    
    // ПОИСК ПОДХОДЯЩЕГО БЛОКА
    // Проходим по всем блокам последовательно (от начала к концу)
    while (cursor < region_end)
    {
        auto* md = reinterpret_cast<block_metadata*>(cursor);
        const unsigned char cur_k = md->size;                     // степень текущего блока
        const size_t cur_sz = static_cast<size_t>(1) << cur_k;    // размер в байтах
        
        // Если блок свободен и подходит по размеру
        if (!md->occupied && cur_k >= req_k)
        {
            if (chosen == nullptr)
            {
                chosen = cursor;
                chosen_k = cur_k;
                // Для first_fit — выходим при первом же подходящем
                if (mode == allocator_with_fit_mode::fit_mode::first_fit)
                    break;
            }
            // Для best_fit — запоминаем самый маленький подходящий
            else if (mode == allocator_with_fit_mode::fit_mode::the_best_fit && cur_k < chosen_k)
            {
                chosen = cursor;
                chosen_k = cur_k;
            }
            // Для worst_fit — запоминаем самый большой подходящий
            else if (mode == allocator_with_fit_mode::fit_mode::the_worst_fit && cur_k > chosen_k)
            {
                chosen = cursor;
                chosen_k = cur_k;
            }
        }
        cursor += cur_sz;   // переходим к следующему блоку
    }
    
    // Если не нашли подходящий блок — памяти не хватает
    if (chosen == nullptr)
        throw std::bad_alloc();
    
    // ДЕЛЕНИЕ БЛОКА ПОПОЛАМ (если выбранный блок больше нужного)
    while (chosen_k > req_k)
    {
        const unsigned char split_k = static_cast<unsigned char>(chosen_k - 1);  // степень половинки
        const size_t half = static_cast<size_t>(1) << split_k;                   // размер половинки в байтах
        
        // Левый блок (остаётся на месте выбранного)
        auto* left = reinterpret_cast<block_metadata*>(chosen);
        left->occupied = false;
        left->size = split_k;
        
        // Правый блок (новый блок справа)
        char* right_ptr = chosen + half;
        auto* right = reinterpret_cast<block_metadata*>(right_ptr);
        right->occupied = false;
        right->size = split_k;
        
        chosen_k = split_k;   // выбранный блок теперь имеет половинный размер
    }
    
    // Помечаем выбранный блок как занятый
    auto* md = reinterpret_cast<block_metadata*>(chosen);
    md->occupied = true;
    
    // Возвращаем указатель на пользовательские данные (смещаемся на размер заголовка)
    return chosen + occupied_block_metadata_size;
}

// ОСВОБОЖДЕНИЕ ПАМЯТИ (do_deallocate_sm)
void allocator_buddies_system::do_deallocate_sm(void *at)
{
    // Освобождение nullptr — ничего не делаем
    if (at == nullptr)
        return;
    
    char* base = reinterpret_cast<char*>(_trusted_memory);
    
    // Блокируем мьютекс
    auto* mtx = reinterpret_cast<std::mutex*>(base + mutex_off);
    std::lock_guard<std::mutex> lock(*mtx);
    
    const unsigned char max_k = *reinterpret_cast<unsigned char*>(base + power_off);
    char* region_begin = base + allocator_metadata_size;
    char* region_end = region_begin + (static_cast<size_t>(1) << max_k);
    
    // ПРОВЕРКА: указатель в пределах доверенной области?
    char* payload = reinterpret_cast<char*>(at);
    if (payload < region_begin + occupied_block_metadata_size || payload >= region_end)
        throw std::invalid_argument("allocator_buddies_system::do_deallocate_sm: pointer out of bounds");
    
    // Вычисляем указатель на начало блока
    char* block = payload - occupied_block_metadata_size;
    
    // ПОИСК БЛОКА В ОБЛАСТИ (обходим все блоки)
    block_metadata* md = nullptr;
    bool found = false;
    char* cursor = region_begin;
    while (cursor < region_end)
    {
        auto* cur_md = reinterpret_cast<block_metadata*>(cursor);
        const size_t cur_block_size = static_cast<size_t>(1) << cur_md->size;
        if (cursor == block)
        {
            md = cur_md;
            found = true;
            break;
        }
        cursor += cur_block_size;
    }
    
    // Если блок не найден — ошибка
    if (!found || md == nullptr)
        throw std::invalid_argument("allocator_buddies_system::do_deallocate_sm: block not found");
    
    // Проверка выравнивания блока
    const size_t rel = static_cast<size_t>(block - region_begin);
    const size_t block_size = static_cast<size_t>(1) << md->size;
    if (rel % block_size != 0)
        throw std::invalid_argument("allocator_buddies_system::do_deallocate_sm: invalid block alignment");
    
    // Проверка: блок должен быть занят
    if (!md->occupied)
        throw std::invalid_argument("allocator_buddies_system::do_deallocate_sm: block already free");
    
    // Помечаем блок как свободный
    md->occupied = false;
    
    // ОБЪЕДИНЕНИЕ С ДВОЙНИКОМ (Buddy Coalescing)
    unsigned char cur_k = md->size;
    size_t merge_rel = rel;
    
    // Пытаемся объединяться, пока не достигнем максимального размера
    while (cur_k < max_k)
    {
        const size_t cur_sz = static_cast<size_t>(1) << cur_k;          // текущий размер в байтах
        const size_t buddy_rel = merge_rel ^ cur_sz;                    // КЛЮЧЕВАЯ ФОРМУЛА: XOR для поиска двойника!
        
        char* buddy_ptr = region_begin + buddy_rel;
        auto* buddy_md = reinterpret_cast<block_metadata*>(buddy_ptr);
        
        // Если двойник занят или другого размера — останавливаемся
        if (buddy_md->occupied || buddy_md->size != cur_k)
            break;
        
        // Если двойник находится левее, обновляем указатель на объединённый блок
        if (buddy_rel < merge_rel)
        {
            merge_rel = buddy_rel;
            block = buddy_ptr;
            md = buddy_md;
        }
        
        // Увеличиваем размер (объединяем)
        cur_k = static_cast<unsigned char>(cur_k + 1);
        
        // Обновляем метаданные объединённого блока
        md = reinterpret_cast<block_metadata*>(region_begin + merge_rel);
        md->occupied = false;
        md->size = cur_k;
    }
}

// КОНСТРУКТОР КОПИРОВАНИЯ
allocator_buddies_system::allocator_buddies_system(const allocator_buddies_system &other)
{
    // Если у other нет памяти — копируем пустой объект
    if (other._trusted_memory == nullptr)
    {
        _trusted_memory = nullptr;
        return;
    }
    
    char* other_base = reinterpret_cast<char*>(other._trusted_memory);
    
    // Блокируем мьютекс источника
    auto* other_mtx = reinterpret_cast<std::mutex*>(other_base + mutex_off);
    std::lock_guard<std::mutex> lock(*other_mtx);
    
    // Читаем данные из источника
    auto* parent = *reinterpret_cast<std::pmr::memory_resource**>(other_base + parent_off);
    auto mode = *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(other_base + mode_off);
    auto max_k = *reinterpret_cast<unsigned char*>(other_base + power_off);
    
    if (parent == nullptr)
        parent = std::pmr::get_default_resource();
    
    // Выделяем новую память такого же размера
    const size_t managed = static_cast<size_t>(1) << max_k;
    _trusted_memory = parent->allocate(allocator_metadata_size + managed);
    
    char* base = reinterpret_cast<char*>(_trusted_memory);
    
    // Копируем служебные данные
    *reinterpret_cast<std::pmr::memory_resource**>(base + parent_off) = parent;
    *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(base + mode_off) = mode;
    *reinterpret_cast<unsigned char*>(base + power_off) = max_k;
    
    // Создаём новый мьютекс
    new (base + mutex_off) std::mutex();
    
    // Создаём корневой блок (вся область свободна)
    char* first_block = base + allocator_metadata_size;
    auto* root_md = reinterpret_cast<block_metadata*>(first_block);
    root_md->occupied = false;
    root_md->size = max_k;
}

// ОПЕРАТОР ПРИСВАИВАНИЯ КОПИРОВАНИЕМ (copy-and-swap)
allocator_buddies_system &allocator_buddies_system::operator=(const allocator_buddies_system &other)
{
    // Защита от самоприсваивания
    if (this == &other)
        return *this;
    
    // Copy-and-swap: создаём временную копию и перемещаем её
    allocator_buddies_system tmp(other);
    *this = std::move(tmp);
    return *this;
}

// СРАВНЕНИЕ АЛЛОКАТОРОВ (do_is_equal)
bool allocator_buddies_system::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    // Два аллокатора совместимы, если они одного типа
    return dynamic_cast<const allocator_buddies_system*>(&other) != nullptr;
}

// СМЕНА СТРАТЕГИИ ВЫДЕЛЕНИЯ
inline void allocator_buddies_system::set_fit_mode(allocator_with_fit_mode::fit_mode mode)
{
    char* base = reinterpret_cast<char*>(_trusted_memory);
    auto* mtx = reinterpret_cast<std::mutex*>(base + mutex_off);
    std::lock_guard<std::mutex> lock(*mtx);
    
    // Записываем новую стратегию по смещению mode_off
    *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(base + mode_off) = mode;
}

// ПОЛУЧЕНИЕ ИНФОРМАЦИИ О БЛОКАХ (ДЛЯ ТЕСТОВ) — ВНЕШНИЙ МЕТОД
std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info() const noexcept
{
    if (_trusted_memory == nullptr)
        return {};
    
    char* base = reinterpret_cast<char*>(_trusted_memory);
    auto* mtx = reinterpret_cast<std::mutex*>(base + mutex_off);
    std::lock_guard<std::mutex> lock(*mtx);
    
    return get_blocks_info_inner();
}

// ПОЛУЧЕНИЕ ИНФОРМАЦИИ О БЛОКАХ — ВНУТРЕННИЙ МЕТОД
std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> result;
    if (_trusted_memory == nullptr)
        return result;
    
    char* base = reinterpret_cast<char*>(_trusted_memory);
    const unsigned char max_k = *reinterpret_cast<unsigned char*>(base + power_off);
    char* region_begin = base + allocator_metadata_size;
    char* region_end = region_begin + (static_cast<size_t>(1) << max_k);
    
    // Проходим по всем блокам последовательно
    char* cursor = region_begin;
    while (cursor < region_end)
    {
        auto* md = reinterpret_cast<block_metadata*>(cursor);
        const size_t block_size = static_cast<size_t>(1) << md->size;   // размер блока в байтах
        result.push_back({block_size, md->occupied});                    // размер + занят/свободен
        cursor += block_size;
    }
    return result;
}

// НАЧАЛО ИТЕРАТОРА (первый блок в области)
allocator_buddies_system::buddy_iterator allocator_buddies_system::begin() const noexcept
{
    if (_trusted_memory == nullptr)
        return buddy_iterator();
    return buddy_iterator(reinterpret_cast<char*>(_trusted_memory) + allocator_metadata_size);
}

// КОНЕЦ ИТЕРАТОРА (nullptr)
allocator_buddies_system::buddy_iterator allocator_buddies_system::end() const noexcept
{
    return buddy_iterator();
}

// СРАВНЕНИЕ ИТЕРАТОРОВ (==)
bool allocator_buddies_system::buddy_iterator::operator==(const buddy_iterator &other) const noexcept
{
    return _block == other._block;
}

// СРАВНЕНИЕ ИТЕРАТОРОВ (!=)
bool allocator_buddies_system::buddy_iterator::operator!=(const buddy_iterator &other) const noexcept
{
    return !(*this == other);
}

// ПЕРЕХОД К СЛЕДУЮЩЕМУ БЛОКУ (префиксный инкремент)
allocator_buddies_system::buddy_iterator &allocator_buddies_system::buddy_iterator::operator++() & noexcept
{
    if (_block == nullptr)
        return *this;
    
    // Читаем размер текущего блока
    auto* md = reinterpret_cast<block_metadata*>(_block);
    const size_t block_size = static_cast<size_t>(1) << md->size;
    
    // Перемещаемся в начало следующего блока
    _block = reinterpret_cast<char*>(_block) + block_size;
    
    return *this;
}

// ПЕРЕХОД К СЛЕДУЮЩЕМУ БЛОКУ (постфиксный инкремент)
allocator_buddies_system::buddy_iterator allocator_buddies_system::buddy_iterator::operator++(int n)
{
    (void)n;   // подавляем warning о неиспользуемом параметре
    buddy_iterator tmp(*this);
    ++(*this);
    return tmp;
}

// РАЗМЕР ТЕКУЩЕГО БЛОКА
size_t allocator_buddies_system::buddy_iterator::size() const noexcept
{
    if (_block == nullptr)
        return 0;
    auto* md = reinterpret_cast<block_metadata*>(_block);
    return static_cast<size_t>(1) << md->size;
}

// ЗАНЯТ ЛИ ТЕКУЩИЙ БЛОК?
bool allocator_buddies_system::buddy_iterator::occupied() const noexcept
{
    if (_block == nullptr)
        return false;
    auto* md = reinterpret_cast<block_metadata*>(_block);
    return md->occupied;
}

// ПОЛУЧЕНИЕ УКАЗАТЕЛЯ НА ДАННЫЕ БЛОКА
void *allocator_buddies_system::buddy_iterator::operator*() const noexcept
{
    if (_block == nullptr)
        return nullptr;
    
    auto* md = reinterpret_cast<block_metadata*>(_block);
    
    // Если блок занят — возвращаем указатель на пользовательские данные
    if (md->occupied)
        return reinterpret_cast<char*>(_block) + occupied_block_metadata_size;
    
    // Если блок свободен — возвращаем указатель на начало блока
    return _block;
}

// КОНСТРУКТОР ИТЕРАТОРА ПО УМОЛЧАНИЮ
allocator_buddies_system::buddy_iterator::buddy_iterator()
{
    _block = nullptr;
}

// КОНСТРУКТОР ИТЕРАТОРА ОТ УКАЗАТЕЛЯ НА НАЧАЛО ОБЛАСТИ
allocator_buddies_system::buddy_iterator::buddy_iterator(void *start)
{
    _block = start;
}