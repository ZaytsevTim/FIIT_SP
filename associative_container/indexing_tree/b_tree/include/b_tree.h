#ifndef SYS_PROG_B_TREE_H
#define SYS_PROG_B_TREE_H

#include <iterator>
#include <utility>
#include <stdexcept>
#include <vector>
#include <boost/container/static_vector.hpp>
#include <stack>
#include <pp_allocator.h>
#include <associative_container.h>
#include <initializer_list>

// Шаблон B-дерева: tkey - тип ключа, tvalue - тип значения, compare - компаратор, t - степень дерева
template <typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5>
class B_tree final : private compare
{
public:
    using tree_data_type = std::pair<tkey, tvalue>;
    using tree_data_type_const = std::pair<const tkey, tvalue>;
    using value_type = tree_data_type_const;

private:
    // Минимальное и максимальное количество ключей в узле (зависит от степени t)
    static constexpr const size_t minimum_keys_in_node = t - 1;
    static constexpr const size_t maximum_keys_in_node = 2 * t - 1;

    // Сравнивает два ключа через компаратор
    inline bool compare_keys(const tkey& lhs, const tkey& rhs) const;
    // Сравнивает две пары (по ключам)
    inline bool compare_pairs(const tree_data_type& lhs, const tree_data_type& rhs) const;

    // Структура узла B-дерева
    struct btree_node
    {
        // Массив ключей (размер на стеке, +1 для временного переполнения)
        boost::container::static_vector<tree_data_type, maximum_keys_in_node + 1> _keys;
        // Массив указателей на детей (размер на стеке, +2 для временного переполнения)
        boost::container::static_vector<btree_node*, maximum_keys_in_node + 2> _pointers;
        btree_node() noexcept;
    };

    pp_allocator<value_type> _allocator;  // Аллокатор для узлов
    btree_node* _root;                    // Корень дерева
    size_t _size;                         // Количество элементов

    // Вспомогательные методы
    pp_allocator<value_type> get_allocator() const noexcept;
    btree_node* create_node();                         // Создать узел
    void destroy_node(btree_node* node) noexcept;      // Уничтожить узел
    void clear_node(btree_node* node) noexcept;        // Очистить поддерево
    bool is_leaf(const btree_node* node) const noexcept; // Проверка: лист? (нет детей)
    
    // Шаг 1: найти позицию ключа в узле (возвращает индекс, found=true если найден)
    size_t find_key_index(const btree_node* node, const tkey& key, bool& found) const;
    
    // Шаг 2: разбить переполненного ребёнка (когда у него 2t ключей)
    void split_child(btree_node* parent, size_t child_index);
    
    // Шаг 3: вставить ключ в непереполненный узел
    void insert_non_full(btree_node* node, tree_data_type&& data, btree_node** out_node, size_t* out_index, bool& inserted);
    
    // Шаг 4: удалить ключ (внутренний метод)
    bool erase_internal(btree_node* node, const tkey& key);
    
    // Шаг 5: объединить двух детей (когда оба имеют минимальное количество ключей)
    void merge_children(btree_node* node, size_t index);
    
    // Шаг 6: получить предшественника (максимум в левом поддереве)
    tree_data_type get_predecessor(btree_node* node, size_t index);
    
    // Шаг 7: получить последователя (минимум в правом поддереве)
    tree_data_type get_successor(btree_node* node, size_t index);

public:

    // КОНСТРУКТОРЫ

    // 1. Конструктор по умолчанию: создаёт пустое дерево
    explicit B_tree(const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    // 2. Конструктор с аллокатором
    explicit B_tree(pp_allocator<value_type> alloc, const compare& comp = compare());

    // 3. Конструктор из диапазона итераторов
    template<input_iterator_for_pair<tkey, tvalue> iterator>
    explicit B_tree(iterator begin, iterator end, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    // 4. Конструктор из initializer_list
    B_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    // ПРАВИЛО ПЯТИ

    // 5. Конструктор копирования: глубокое копирование дерева
    B_tree(const B_tree& other);

    // 6. Конструктор перемещения: забирает ресурсы у другого дерева
    B_tree(B_tree&& other) noexcept;

    // 7. Оператор присваивания копированием
    B_tree& operator=(const B_tree& other);

    // 8. Оператор присваивания перемещением
    B_tree& operator=(B_tree&& other) noexcept;

    // 9. Деструктор: очищает всё дерево
    ~B_tree() noexcept;

    // ИТЕРАТОРЫ

    class btree_iterator;
    class btree_reverse_iterator;
    class btree_const_iterator;
    class btree_const_reverse_iterator;

    // Обычный итератор (прямой обход)
    class btree_iterator final
    {
        std::stack<std::pair<btree_node**, size_t>> _path;  // Стек: (указатель на узел, индекс в родителе)
        size_t _index;  // Индекс текущего ключа в узле

    public:
        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_const_iterator;
        friend class btree_const_reverse_iterator;

        reference operator*() const noexcept;      // Шаг: получить текущий ключ
        pointer operator->() const noexcept;       // Шаг: доступ к полям ключа

        self& operator++();   // Шаг: перейти к следующему ключу
        self operator++(int); // Шаг: постфиксный ++
        self& operator--();   // Шаг: перейти к предыдущему ключу
        self operator--(int); // Шаг: постфиксный --

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;                     // Глубина в дереве
        size_t current_node_keys_count() const noexcept;   // Количество ключей в текущем узле
        bool is_terminate_node() const noexcept;           // Дошли ли до конца
        size_t index() const noexcept;                     // Индекс текущего ключа

        explicit btree_iterator(const std::stack<std::pair<btree_node**, size_t>>& path = std::stack<std::pair<btree_node**, size_t>>(), size_t index = 0);
    };

    // Константный итератор (только для чтения)
    class btree_const_iterator final
    {
        std::stack<std::pair<btree_node* const*, size_t>> _path;
        size_t _index;

    public:
        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_const_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_iterator;
        friend class btree_const_reverse_iterator;

        btree_const_iterator(const btree_iterator& it) noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);
        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_const_iterator(const std::stack<std::pair<btree_node* const*, size_t>>& path = std::stack<std::pair<btree_node* const*, size_t>>(), size_t index = 0);
    };

    // Обратный итератор
    class btree_reverse_iterator final
    {
        std::stack<std::pair<btree_node**, size_t>> _path;
        size_t _index;

    public:
        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_reverse_iterator;

        friend class B_tree;
        friend class btree_iterator;
        friend class btree_const_iterator;
        friend class btree_const_reverse_iterator;

        btree_reverse_iterator(const btree_iterator& it) noexcept;
        operator btree_iterator() const noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);
        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_reverse_iterator(const std::stack<std::pair<btree_node**, size_t>>& path = std::stack<std::pair<btree_node**, size_t>>(), size_t index = 0);
    };

    // Константный обратный итератор
    class btree_const_reverse_iterator final
    {
        std::stack<std::pair<btree_node* const*, size_t>> _path;
        size_t _index;

    public:
        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_const_reverse_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_const_iterator;
        friend class btree_iterator;

        btree_const_reverse_iterator(const btree_reverse_iterator& it) noexcept;
        operator btree_const_iterator() const noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);
        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_const_reverse_iterator(const std::stack<std::pair<btree_node* const*, size_t>>& path = std::stack<std::pair<btree_node* const*, size_t>>(), size_t index = 0);
    };

    friend class btree_iterator;
    friend class btree_const_iterator;
    friend class btree_reverse_iterator;
    friend class btree_const_reverse_iterator;

    // ДОСТУП К ЭЛЕМЕНТАМ

    // Шаг: получить значение по ключу (бросает out_of_range если нет)
    tvalue& at(const tkey&);
    const tvalue& at(const tkey&) const;

    // Шаг: получить значение по ключу (создаёт элемент если нет)
    tvalue& operator[](const tkey& key);
    tvalue& operator[](tkey&& key);

    // НАЧАЛЬНЫЕ ИТЕРАТОРЫ

    btree_iterator begin();   // Шаг: итератор на первый (самый левый) ключ
    btree_iterator end();     // Шаг: итератор на конец

    btree_const_iterator begin() const;
    btree_const_iterator end() const;

    btree_const_iterator cbegin() const;
    btree_const_iterator cend() const;

    btree_reverse_iterator rbegin();   // Шаг: обратный итератор на последний ключ
    btree_reverse_iterator rend();     // Шаг: обратный итератор на конец

    btree_const_reverse_iterator rbegin() const;
    btree_const_reverse_iterator rend() const;

    btree_const_reverse_iterator crbegin() const;
    btree_const_reverse_iterator crend() const;

    // ПОИСК

    size_t size() const noexcept;      // Шаг: количество элементов
    bool empty() const noexcept;       // Шаг: пусто ли дерево

    btree_iterator find(const tkey& key);           // Шаг: найти ключ (возвращает end() если нет)
    btree_const_iterator find(const tkey& key) const;

    btree_iterator lower_bound(const tkey& key);    // Шаг: первый элемент >= key
    btree_const_iterator lower_bound(const tkey& key) const;

    btree_iterator upper_bound(const tkey& key);    // Шаг: первый элемент > key
    btree_const_iterator upper_bound(const tkey& key) const;

    bool contains(const tkey& key) const;           // Шаг: проверка существования

    // МОДИФИКАЦИЯ

    void clear() noexcept;   // Шаг: удалить все элементы

    // Шаг: вставка пары (возвращает итератор и флаг успеха)
    std::pair<btree_iterator, bool> insert(const tree_data_type& data);
    std::pair<btree_iterator, bool> insert(tree_data_type&& data);

    // Шаг: конструирование и вставка
    template <typename ...Args>
    std::pair<btree_iterator, bool> emplace(Args&&... args);

    // Шаг: вставка или обновление значения
    btree_iterator insert_or_assign(const tree_data_type& data);
    btree_iterator insert_or_assign(tree_data_type&& data);

    template <typename ...Args>
    btree_iterator emplace_or_assign(Args&&... args);

    // Шаг: удаление по итератору
    btree_iterator erase(btree_iterator pos);
    btree_iterator erase(btree_const_iterator pos);

    // Шаг: удаление диапазона
    btree_iterator erase(btree_iterator beg, btree_iterator en);
    btree_iterator erase(btree_const_iterator beg, btree_const_iterator en);

    // Шаг: удаление по ключу (возвращает следующий итератор)
    btree_iterator erase(const tkey& key);
};

// CTAD: вывод типа из итератора
template<std::input_iterator iterator, comparator<typename std::iterator_traits<iterator>::value_type::first_type> compare = std::less<typename std::iterator_traits<iterator>::value_type::first_type>,
        std::size_t t = 5, typename U>
B_tree(iterator begin, iterator end, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> B_tree<typename std::iterator_traits<iterator>::value_type::first_type, typename std::iterator_traits<iterator>::value_type::second_type, compare, t>;

// CTAD: вывод типа из initializer_list
template<typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5, typename U>
B_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> B_tree<tkey, tvalue, compare, t>;

// РЕАЛИЗАЦИЯ МЕТОДОВ

// Сравнение пар (сравнивает ключи)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::compare_pairs(const B_tree::tree_data_type &lhs,
                                                     const B_tree::tree_data_type &rhs) const
{
    return compare_keys(lhs.first, rhs.first);
}

// Сравнение ключей через компаратор
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::compare_keys(const tkey &lhs, const tkey &rhs) const
{
    return compare::operator()(lhs, rhs);
}

// Конструктор узла B-дерева: очищает векторы
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_node::btree_node() noexcept
{
    _keys.clear();
    _pointers.clear();
}

// Возвращает копию аллокатора
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
pp_allocator<typename B_tree<tkey, tvalue, compare, t>::value_type> B_tree<tkey, tvalue, compare, t>::get_allocator() const noexcept
{
    return _allocator;
}

// Создание нового узла через аллокатор (placement new)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_node* B_tree<tkey, tvalue, compare, t>::create_node()
{
    return _allocator.template new_object<btree_node>();
}

// Уничтожение узла через аллокатор (явный вызов деструктора + освобождение памяти)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::destroy_node(btree_node* node) noexcept
{
    if (node == nullptr) {
        return;
    }
    _allocator.template delete_object(node);
}

// Рекурсивное удаление всего поддерева
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::clear_node(btree_node* node) noexcept
{
    if (node == nullptr) {
        return;
    }
    // Шаг 1: рекурсивно очищаем всех детей
    for (auto child : node->_pointers) {
        clear_node(child);
    }
    // Шаг 2: уничтожаем текущий узел
    destroy_node(node);
}

// Проверка: является ли узел листом (нет детей)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::is_leaf(const btree_node* node) const noexcept
{
    return node == nullptr || node->_pointers.empty();
}

// Функция поиска позиции ключа в узле
// Шаг 1: проходим по ключам, пока они меньше искомого
// Шаг 2: если нашли точное совпадение - found = true
// Шаг 3: возвращаем индекс (куда вставлять или где найден)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::find_key_index(const btree_node* node, const tkey& key, bool& found) const
{
    size_t i = 0;
    while (i < node->_keys.size() && compare_keys(node->_keys[i].first, key)) {
        ++i;
    }
    found = (i < node->_keys.size() && !compare_keys(key, node->_keys[i].first) && !compare_keys(node->_keys[i].first, key));
    return i;
}

// Разбиение переполненного ребёнка (когда у него 2t ключей)
// Шаг 1: берём переполненного ребёнка full
// Шаг 2: создаём правого брата right
// Шаг 3: средний ключ (индекс t) поднимаем в родителя
// Шаг 4: перемещаем правую половину ключей из full в right
// Шаг 5: перемещаем правую половину детей из full в right
// Шаг 6: вставляем средний ключ в родителя
// Шаг 7: вставляем right в список детей родителя
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::split_child(btree_node* parent, size_t child_index)
{
    btree_node* full = parent->_pointers[child_index];
    btree_node* right = create_node();
    tree_data_type middle = std::move(full->_keys[t]);
    
    for (size_t i = t + 1; i < full->_keys.size(); ++i) {
        right->_keys.push_back(std::move(full->_keys[i]));
    }
    full->_keys.erase(full->_keys.begin() + static_cast<ptrdiff_t>(t), full->_keys.end());
    
    if (!full->_pointers.empty()) {
        for (size_t i = t + 1; i < full->_pointers.size(); ++i) {
            right->_pointers.push_back(full->_pointers[i]);
        }
        full->_pointers.erase(full->_pointers.begin() + static_cast<ptrdiff_t>(t + 1), full->_pointers.end());
    }
    
    parent->_keys.insert(parent->_keys.begin() + static_cast<ptrdiff_t>(child_index), std::move(middle));
    parent->_pointers.insert(parent->_pointers.begin() + static_cast<ptrdiff_t>(child_index + 1), right);
}

// Вставка в непереполненный узел (рекурсивная)
// Шаг 1: ищем позицию для вставки
// Шаг 2: если ключ уже есть - обновляем значение и выходим
// Шаг 3: если узел лист - вставляем ключ
// Шаг 4: иначе рекурсивно вставляем в ребёнка
// Шаг 5: если ребёнок переполнился - разбиваем его
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::insert_non_full(btree_node* node, tree_data_type&& data, btree_node** out_node, size_t* out_index, bool& inserted)
{
    bool found = false;
    size_t idx = find_key_index(node, data.first, found);
    
    if (found) {
        *out_node = node;
        *out_index = idx;
        inserted = false;
        return;
    }
    
    if (is_leaf(node)) {
        node->_keys.insert(node->_keys.begin() + static_cast<ptrdiff_t>(idx), std::move(data));
        *out_node = node;
        *out_index = idx;
        inserted = true;
        return;
    }
    
    insert_non_full(node->_pointers[idx], std::move(data), out_node, out_index, inserted);
    
    if (node->_pointers[idx]->_keys.size() == maximum_keys_in_node + 1) {
        split_child(node, idx);
    }
}

// Получение предшественника (максимальный ключ в левом поддереве)
// Шаг 1: идём в левого ребёнка (индекс index)
// Шаг 2: затем всё время вправо (последний ребёнок)
// Шаг 3: возвращаем последний ключ
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::tree_data_type B_tree<tkey, tvalue, compare, t>::get_predecessor(btree_node* node, size_t index)
{
    btree_node* cur = node->_pointers[index];
    while (!is_leaf(cur)) {
        cur = cur->_pointers.back();
    }
    return cur->_keys.back();
}

// Получение последователя (минимальный ключ в правом поддереве)
// Шаг 1: идём в правого ребёнка (индекс index+1)
// Шаг 2: затем всё время влево (первый ребёнок)
// Шаг 3: возвращаем первый ключ
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::tree_data_type B_tree<tkey, tvalue, compare, t>::get_successor(btree_node* node, size_t index)
{
    btree_node* cur = node->_pointers[index + 1];
    while (!is_leaf(cur)) {
        cur = cur->_pointers.front();
    }
    return cur->_keys.front();
}

// Объединение двух детей (когда оба имеют минимальное количество ключей)
// Шаг 1: берём левого и правого ребёнка
// Шаг 2: добавляем ключ из родителя в левый узел
// Шаг 3: добавляем все ключи из правого узла в левый
// Шаг 4: добавляем всех детей правого узла в левый
// Шаг 5: удаляем ключ из родителя и правый узел
// Шаг 6: уничтожаем правый узел
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::merge_children(btree_node* node, size_t index)
{
    btree_node* left = node->_pointers[index];
    btree_node* right = node->_pointers[index + 1];
    
    left->_keys.push_back(std::move(node->_keys[index]));
    
    for (size_t i = 0; i < right->_keys.size(); ++i) {
        left->_keys.push_back(std::move(right->_keys[i]));
    }
    
    if (!right->_pointers.empty()) {
        for (size_t i = 0; i < right->_pointers.size(); ++i) {
            left->_pointers.push_back(right->_pointers[i]);
        }
    }
    
    node->_keys.erase(node->_keys.begin() + static_cast<ptrdiff_t>(index));
    node->_pointers.erase(node->_pointers.begin() + static_cast<ptrdiff_t>(index + 1));
    
    destroy_node(right);
}

// Внутренний метод удаления ключа (рекурсивный)
// СЛУЧАЙ 1: ключ найден
//   1А: в листе - просто удаляем
//   1Б: во внутреннем узле
//       1Б(i): если у левого ребёнка >= t ключей - берём предшественника
//       1Б(ii): если у правого ребёнка >= t ключей - берём последователя
//       1Б(iii): иначе объединяем детей и удаляем
// СЛУЧАЙ 2: ключ не найден
//   2А: если у ребёнка минимальное количество ключей - перебалансируем
//       2А(i): берём ключ у левого брата
//       2А(ii): берём ключ у правого брата
//       2А(iii): объединяем с братом
//   2Б: рекурсивно удаляем из ребёнка
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::erase_internal(btree_node* node, const tkey& key)
{
    bool found = false;
    size_t idx = find_key_index(node, key, found);
    
    if (found) {
        if (is_leaf(node)) {
            node->_keys.erase(node->_keys.begin() + static_cast<ptrdiff_t>(idx));
            --_size;
            return true;
        }
        
        btree_node* left = node->_pointers[idx];
        btree_node* right = node->_pointers[idx + 1];
        
        if (left->_keys.size() >= t) {
            tree_data_type pred = get_predecessor(node, idx);
            node->_keys[idx] = pred;
            return erase_internal(left, pred.first);
        }
        
        if (right->_keys.size() >= t) {
            tree_data_type succ = get_successor(node, idx);
            node->_keys[idx] = succ;
            return erase_internal(right, succ.first);
        }
        
        merge_children(node, idx);
        return erase_internal(left, key);
    }
    
    if (is_leaf(node)) {
        return false;
    }
    
    btree_node* child = node->_pointers[idx];
    
    if (child->_keys.size() == minimum_keys_in_node) {
        if (idx > 0 && node->_pointers[idx - 1]->_keys.size() >= t) {
            btree_node* left = node->_pointers[idx - 1];
            child->_keys.insert(child->_keys.begin(), std::move(node->_keys[idx - 1]));
            node->_keys[idx - 1] = std::move(left->_keys.back());
            left->_keys.pop_back();
            if (!left->_pointers.empty()) {
                child->_pointers.insert(child->_pointers.begin(), left->_pointers.back());
                left->_pointers.pop_back();
            }
        }
        else if (idx + 1 < node->_pointers.size() && node->_pointers[idx + 1]->_keys.size() >= t) {
            btree_node* right = node->_pointers[idx + 1];
            child->_keys.push_back(std::move(node->_keys[idx]));
            node->_keys[idx] = std::move(right->_keys.front());
            right->_keys.erase(right->_keys.begin());
            if (!right->_pointers.empty()) {
                child->_pointers.push_back(right->_pointers.front());
                right->_pointers.erase(right->_pointers.begin());
            }
        }
        else {
            if (idx + 1 < node->_pointers.size()) {
                merge_children(node, idx);
            } else {
                merge_children(node, idx - 1);
                child = node->_pointers[idx - 1];
            }
        }
    }
    
    return erase_internal(child, key);
}

// Конструктор по умолчанию: создаёт пустое дерево
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        const compare& cmp,
        pp_allocator<value_type> alloc)
{
    compare::operator=(cmp);
    _allocator = alloc;
    _root = nullptr;
    _size = 0;
}

// Конструктор с аллокатором
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        pp_allocator<value_type> alloc,
        const compare& comp)
{
    compare::operator=(comp);
    _allocator = alloc;
    _root = nullptr;
    _size = 0;
}

// Конструктор из диапазона итераторов: последовательно вставляет элементы
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<input_iterator_for_pair<tkey, tvalue> iterator>
B_tree<tkey, tvalue, compare, t>::B_tree(
        iterator begin,
        iterator end,
        const compare& cmp,
        pp_allocator<value_type> alloc)
{
    compare::operator=(cmp);
    _allocator = alloc;
    _root = nullptr;
    _size = 0;
    for (auto it = begin; it != end; ++it) {
        emplace(it->first, it->second);
    }
}

// Конструктор из initializer_list
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        std::initializer_list<std::pair<tkey, tvalue>> data,
        const compare& cmp,
        pp_allocator<value_type> alloc)
{
    compare::operator=(cmp);
    _allocator = alloc;
    _root = nullptr;
    _size = 0;
    for (auto const& item : data) {
        emplace(item.first, item.second);
    }
}

// Деструктор: очищает всё дерево
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::~B_tree() noexcept
{
    clear();
}

// Конструктор копирования: глубокое копирование дерева
// Шаг 1: копируем компаратор и аллокатор
// Шаг 2: создаём новый корень
// Шаг 3: DFS обход для копирования всех узлов
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(const B_tree& other)
{
    compare::operator=(static_cast<const compare&>(other));
    _allocator = other._allocator.select_on_container_copy_construction();
    _root = nullptr;
    _size = other._size;
    if (other._root == nullptr) {
        return;
    }
    _root = create_node();
    std::stack<std::pair<const btree_node*, btree_node*>> st;
    st.push(std::make_pair(other._root, _root));
    while (!st.empty()) {
        auto current = st.top();
        st.pop();
        const btree_node* src = current.first;
        btree_node* dst = current.second;
        dst->_keys = src->_keys;
        if (!src->_pointers.empty()) {
            for (size_t i = 0; i < src->_pointers.size(); ++i) {
                dst->_pointers.push_back(nullptr);
                dst->_pointers[i] = create_node();
                st.push(std::make_pair(src->_pointers[i], dst->_pointers[i]));
            }
        }
    }
}

// Оператор присваивания копированием (copy-and-swap)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>& B_tree<tkey, tvalue, compare, t>::operator=(const B_tree& other)
{
    if (this == &other) {
        return *this;
    }
    clear();
    compare::operator=(static_cast<const compare&>(other));
    _allocator = other._allocator;
    _root = nullptr;
    _size = other._size;
    if (other._root == nullptr) {
        return *this;
    }
    _root = create_node();
    std::stack<std::pair<const btree_node*, btree_node*>> st;
    st.push(std::make_pair(other._root, _root));
    while (!st.empty()) {
        auto current = st.top();
        st.pop();
        const btree_node* src = current.first;
        btree_node* dst = current.second;
        dst->_keys = src->_keys;
        if (!src->_pointers.empty()) {
            for (size_t i = 0; i < src->_pointers.size(); ++i) {
                dst->_pointers.push_back(nullptr);
                dst->_pointers[i] = create_node();
                st.push(std::make_pair(src->_pointers[i], dst->_pointers[i]));
            }
        }
    }
    return *this;
}

// Конструктор перемещения: забираем ресурсы у другого дерева
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(B_tree&& other) noexcept
{
    compare::operator=(std::move(static_cast<compare&>(other)));
    _allocator = std::move(other._allocator);
    _root = other._root;
    _size = other._size;
    other._root = nullptr;
    other._size = 0;
}

// Оператор присваивания перемещением
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>& B_tree<tkey, tvalue, compare, t>::operator=(B_tree&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    clear();
    compare::operator=(std::move(static_cast<compare&>(other)));
    _allocator = std::move(other._allocator);
    _root = other._root;
    _size = other._size;
    other._root = nullptr;
    other._size = 0;
    return *this;
}

// Конструктор итератора
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_iterator::btree_iterator(
        const std::stack<std::pair<btree_node**, size_t>>& path, size_t index)
{
    _path = path;
    _index = index;
}

// Разыменование итератора: возвращает текущий ключ
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator*() const noexcept
{
    btree_node* node = *(_path.top().first);
    return *reinterpret_cast<value_type*>(&node->_keys[_index]);
}

// Доступ через ->
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator->() const noexcept
{
    btree_node* node = *(_path.top().first);
    return reinterpret_cast<value_type*>(&node->_keys[_index]);
}

// Оператор ++ (переход к следующему ключу)
// Шаг 1: если есть правый ребёнок, идём в него и затем всё время влево
// Шаг 2: если есть следующий ключ в том же узле - берём его
// Шаг 3: иначе поднимаемся вверх, пока не найдём следующий ключ
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator&
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator++()
{
    if (_path.empty()) {
        return *this;
    }
    btree_node* node = *(_path.top().first);
    if (!node->_pointers.empty()) {
        size_t child_idx = _index + 1;
        btree_node** child_ptr = &node->_pointers[child_idx];
        _path.push(std::make_pair(child_ptr, child_idx));
        node = *child_ptr;
        while (!node->_pointers.empty()) {
            btree_node** next_ptr = &node->_pointers[0];
            _path.push(std::make_pair(next_ptr, 0));
            node = *next_ptr;
        }
        _index = 0;
        return *this;
    }
    if (_index + 1 < node->_keys.size()) {
        ++_index;
        return *this;
    }
    while (!_path.empty()) {
        size_t child_index = _path.top().second;
        _path.pop();
        if (_path.empty()) {
            _index = 0;
            return *this;
        }
        btree_node* parent = *(_path.top().first);
        if (child_index < parent->_keys.size()) {
            _index = child_index;
            return *this;
        }
    }
    return *this;
}

// Постфиксный ++
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator++(int)
{
    self tmp(*this);
    ++(*this);
    return tmp;
}

// Оператор -- (переход к предыдущему ключу)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator&
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator--()
{
    if (_path.empty()) {
        return *this;
    }
    btree_node* node = *(_path.top().first);
    if (!node->_pointers.empty()) {
        size_t child_idx = _index;
        btree_node** child_ptr = &node->_pointers[child_idx];
        _path.push(std::make_pair(child_ptr, child_idx));
        node = *child_ptr;
        while (!node->_pointers.empty()) {
            size_t last = node->_pointers.size() - 1;
            btree_node** next_ptr = &node->_pointers[last];
            _path.push(std::make_pair(next_ptr, last));
            node = *next_ptr;
        }
        _index = node->_keys.size() - 1;
        return *this;
    }
    if (_index > 0) {
        --_index;
        return *this;
    }
    while (!_path.empty()) {
        size_t child_index = _path.top().second;
        _path.pop();
        if (_path.empty()) {
            _index = 0;
            return *this;
        }
        if (child_index > 0) {
            _index = child_index - 1;
            return *this;
        }
    }
    return *this;
}

// Постфиксный --
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator--(int)
{
    self tmp(*this);
    --(*this);
    return tmp;
}

// Сравнение итераторов
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::operator==(const self& other) const noexcept
{
    if (_path.empty() || other._path.empty()) {
        return _path.empty() && other._path.empty();
    }
    return _path.top().first == other._path.top().first && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

// Глубина текущего итератора в дереве
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::depth() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return _path.size() - 1;
}

// Количество ключей в текущем узле
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return (*(_path.top().first))->_keys.size();
}

// Дошли ли до конца (итератор не валиден)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::is_terminate_node() const noexcept
{
    return _path.empty();
}

// Индекс текущего ключа в узле
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::index() const noexcept
{
    return _index;
}

// Конструктор константного итератора
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::btree_const_iterator(
        const std::stack<std::pair<btree_node* const*, size_t>>& path, size_t index)
{
    _path = path;
    _index = index;
}

// Преобразование из обычного итератора в константный
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::btree_const_iterator(
        const btree_iterator& it) noexcept
{
    if (it._path.empty()) {
        _index = 0;
        return;
    }
    std::stack<std::pair<btree_node**, size_t>> temp = it._path;
    std::stack<std::pair<btree_node* const*, size_t>> rebuilt;
    std::vector<std::pair<btree_node* const*, size_t>> order;
    while (!temp.empty()) {
        order.push_back(std::make_pair(const_cast<btree_node* const*>(temp.top().first), temp.top().second));
        temp.pop();
    }
    for (auto itv = order.rbegin(); itv != order.rend(); ++itv) {
        rebuilt.push(*itv);
    }
    _path = rebuilt;
    _index = it._index;
}

// Разыменование константного итератора
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator*() const noexcept
{
    btree_node* node = *(_path.top().first);
    return *reinterpret_cast<const value_type*>(&node->_keys[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator->() const noexcept
{
    btree_node* node = *(_path.top().first);
    return reinterpret_cast<const value_type*>(&node->_keys[_index]);
}

// Оператор ++ для константного итератора (аналогично обычному)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator++()
{
    if (_path.empty()) {
        return *this;
    }
    btree_node* node = *(_path.top().first);
    if (!node->_pointers.empty()) {
        size_t child_idx = _index + 1;
        btree_node* const* child_ptr = &node->_pointers[child_idx];
        _path.push(std::make_pair(child_ptr, child_idx));
        node = *child_ptr;
        while (!node->_pointers.empty()) {
            btree_node* const* next_ptr = &node->_pointers[0];
            _path.push(std::make_pair(next_ptr, 0));
            node = *next_ptr;
        }
        _index = 0;
        return *this;
    }
    if (_index + 1 < node->_keys.size()) {
        ++_index;
        return *this;
    }
    while (!_path.empty()) {
        size_t child_index = _path.top().second;
        _path.pop();
        if (_path.empty()) {
            _index = 0;
            return *this;
        }
        btree_node* parent = *(_path.top().first);
        if (child_index < parent->_keys.size()) {
            _index = child_index;
            return *this;
        }
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator++(int)
{
    self tmp(*this);
    ++(*this);
    return tmp;
}

// Оператор -- для константного итератора (аналогично обычному)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator--()
{
    if (_path.empty()) {
        return *this;
    }
    btree_node* node = *(_path.top().first);
    if (!node->_pointers.empty()) {
        size_t child_idx = _index;
        btree_node* const* child_ptr = &node->_pointers[child_idx];
        _path.push(std::make_pair(child_ptr, child_idx));
        node = *child_ptr;
        while (!node->_pointers.empty()) {
            size_t last = node->_pointers.size() - 1;
            btree_node* const* next_ptr = &node->_pointers[last];
            _path.push(std::make_pair(next_ptr, last));
            node = *next_ptr;
        }
        _index = node->_keys.size() - 1;
        return *this;
    }
    if (_index > 0) {
        --_index;
        return *this;
    }
    while (!_path.empty()) {
        size_t child_index = _path.top().second;
        _path.pop();
        if (_path.empty()) {
            _index = 0;
            return *this;
        }
        if (child_index > 0) {
            _index = child_index - 1;
            return *this;
        }
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator--(int)
{
    self tmp(*this);
    --(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator==(const self& other) const noexcept
{
    if (_path.empty() || other._path.empty()) {
        return _path.empty() && other._path.empty();
    }
    return _path.top().first == other._path.top().first && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::depth() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return (*(_path.top().first))->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::is_terminate_node() const noexcept
{
    return _path.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::index() const noexcept
{
    return _index;
}

// Конструктор обратного итератора
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::btree_reverse_iterator(
        const std::stack<std::pair<btree_node**, size_t>>& path, size_t index)
{
    _path = path;
    _index = index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::btree_reverse_iterator(
        const btree_iterator& it) noexcept
{
    _path = it._path;
    _index = it._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator B_tree<tkey, tvalue, compare, t>::btree_iterator() const noexcept
{
    return btree_iterator(_path, _index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator*() const noexcept
{
    btree_node* node = *(_path.top().first);
    return *reinterpret_cast<value_type*>(&node->_keys[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator->() const noexcept
{
    btree_node* node = *(_path.top().first);
    return reinterpret_cast<value_type*>(&node->_keys[_index]);
}

// ++ обратного итератора = -- обычного
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator++()
{
    btree_iterator it = static_cast<btree_iterator>(*this);
    --it;
    _path = it._path;
    _index = it._index;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator++(int)
{
    self tmp(*this);
    ++(*this);
    return tmp;
}

// -- обратного итератора = ++ обычного
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator--()
{
    btree_iterator it = static_cast<btree_iterator>(*this);
    ++it;
    _path = it._path;
    _index = it._index;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator--(int)
{
    self tmp(*this);
    --(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator==(const self& other) const noexcept
{
    if (_path.empty() || other._path.empty()) {
        return _path.empty() && other._path.empty();
    }
    return _path.top().first == other._path.top().first && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::depth() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return (*(_path.top().first))->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::is_terminate_node() const noexcept
{
    return _path.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::index() const noexcept
{
    return _index;
}

// Конструктор константного обратного итератора
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::btree_const_reverse_iterator(
        const std::stack<std::pair<btree_node* const*, size_t>>& path, size_t index)
{
    _path = path;
    _index = index;
}

// Преобразование из обратного итератора в константный обратный
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::btree_const_reverse_iterator(
        const btree_reverse_iterator& it) noexcept
{
    if (it._path.empty()) {
        _index = 0;
        return;
    }
    std::stack<std::pair<btree_node**, size_t>> temp = it._path;
    std::stack<std::pair<btree_node* const*, size_t>> rebuilt;
    std::vector<std::pair<btree_node* const*, size_t>> order;
    while (!temp.empty()) {
        order.push_back(std::make_pair(const_cast<btree_node* const*>(temp.top().first), temp.top().second));
        temp.pop();
    }
    for (auto itv = order.rbegin(); itv != order.rend(); ++itv) {
        rebuilt.push(*itv);
    }
    _path = rebuilt;
    _index = it._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator B_tree<tkey, tvalue, compare, t>::btree_const_iterator() const noexcept
{
    return btree_const_iterator(_path, _index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator*() const noexcept
{
    btree_node* node = *(_path.top().first);
    return *reinterpret_cast<const value_type*>(&node->_keys[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator->() const noexcept
{
    btree_node* node = *(_path.top().first);
    return reinterpret_cast<const value_type*>(&node->_keys[_index]);
}

// ++ константного обратного итератора = -- константного обычного
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator++()
{
    btree_const_iterator it = static_cast<btree_const_iterator>(*this);
    --it;
    _path = it._path;
    _index = it._index;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator++(int)
{
    self tmp(*this);
    ++(*this);
    return tmp;
}

// -- константного обратного итератора = ++ константного обычного
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator--()
{
    btree_const_iterator it = static_cast<btree_const_iterator>(*this);
    ++it;
    _path = it._path;
    _index = it._index;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator--(int)
{
    self tmp(*this);
    --(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator==(const self& other) const noexcept
{
    if (_path.empty() || other._path.empty()) {
        return _path.empty() && other._path.empty();
    }
    return _path.top().first == other._path.top().first && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::depth() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return (*(_path.top().first))->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::is_terminate_node() const noexcept
{
    return _path.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::index() const noexcept
{
    return _index;
}

// ДОСТУП К ЭЛЕМЕНТАМ

// at: доступ с проверкой границ (бросает out_of_range если нет ключа)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::at(const tkey& key)
{
    auto it = find(key);
    if (it == end()) {
        throw std::out_of_range("B_tree::at");
    }
    return it->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
const tvalue& B_tree<tkey, tvalue, compare, t>::at(const tkey& key) const
{
    auto it = find(key);
    if (it == end()) {
        throw std::out_of_range("B_tree::at");
    }
    return it->second;
}

// operator[]: доступ, создаёт элемент с значением по умолчанию если нет
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::operator[](const tkey& key)
{
    auto result = emplace(key, tvalue());
    return result.first->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::operator[](tkey&& key)
{
    auto result = emplace(std::move(key), tvalue());
    return result.first->second;
}

// НАЧАЛЬНЫЕ ИТЕРАТОРЫ

// begin(): самый левый ключ в дереве
// Шаг 1: идём от корня всё время по левым указателям
// Шаг 2: возвращаем итератор на первый ключ
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::begin()
{
    if (_root == nullptr) {
        return end();
    }
    std::stack<std::pair<btree_node**, size_t>> path;
    btree_node** cur_ptr = &_root;
    path.push(std::make_pair(cur_ptr, 0));
    btree_node* node = *cur_ptr;
    while (!is_leaf(node)) {
        btree_node** next_ptr = &node->_pointers[0];
        path.push(std::make_pair(next_ptr, 0));
        node = *next_ptr;
    }
    return btree_iterator(path, 0);
}

// end(): итератор на конец (пустой)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::end()
{
    return btree_iterator();
}

// Константный begin()
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::begin() const
{
    if (_root == nullptr) {
        return end();
    }
    std::stack<std::pair<btree_node* const*, size_t>> path;
    btree_node* const* cur_ptr = &_root;
    path.push(std::make_pair(cur_ptr, 0));
    btree_node* node = *cur_ptr;
    while (!is_leaf(node)) {
        btree_node* const* next_ptr = &node->_pointers[0];
        path.push(std::make_pair(next_ptr, 0));
        node = *next_ptr;
    }
    return btree_const_iterator(path, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::end() const
{
    return btree_const_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::cbegin() const
{
    return begin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::cend() const
{
    return end();
}

// rbegin(): обратный итератор на самый правый ключ
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator B_tree<tkey, tvalue, compare, t>::rbegin()
{
    if (_root == nullptr) {
        return rend();
    }
    std::stack<std::pair<btree_node**, size_t>> path;
    btree_node** cur_ptr = &_root;
    path.push(std::make_pair(cur_ptr, 0));
    btree_node* node = *cur_ptr;
    while (!is_leaf(node)) {
        size_t last = node->_pointers.size() - 1;
        btree_node** next_ptr = &node->_pointers[last];
        path.push(std::make_pair(next_ptr, last));
        node = *next_ptr;
    }
    return btree_reverse_iterator(path, node->_keys.size() - 1);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator B_tree<tkey, tvalue, compare, t>::rend()
{
    return btree_reverse_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::rbegin() const
{
    if (_root == nullptr) {
        return rend();
    }
    std::stack<std::pair<btree_node* const*, size_t>> path;
    btree_node* const* cur_ptr = &_root;
    path.push(std::make_pair(cur_ptr, 0));
    btree_node* node = *cur_ptr;
    while (!is_leaf(node)) {
        size_t last = node->_pointers.size() - 1;
        btree_node* const* next_ptr = &node->_pointers[last];
        path.push(std::make_pair(next_ptr, last));
        node = *next_ptr;
    }
    return btree_const_reverse_iterator(path, node->_keys.size() - 1);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::rend() const{
    return btree_const_reverse_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::crbegin() const
{
    return rbegin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::crend() const
{
    return rend();
}

// ПОИСК

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::size() const noexcept
{
    return _size;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::empty() const noexcept
{
    return _size == 0;
}

// find: поиск ключа, возвращает итератор (end() если не найден)
// Шаг 1: спускаемся по дереву, сохраняя путь
// Шаг 2: в каждом узле ищем позицию ключа
// Шаг 3: если нашли - возвращаем итератор
// Шаг 4: если дошли до листа и не нашли - end()
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::find(const tkey& key)
{
    if (_root == nullptr) {
        return end();
    }
    std::stack<std::pair<btree_node**, size_t>> path;
    btree_node** cur_ptr = &_root;
    path.push(std::make_pair(cur_ptr, 0));
    btree_node* node = *cur_ptr;
    while (true) {
        bool found = false;
        size_t idx = find_key_index(node, key, found);
        if (found) {
            return btree_iterator(path, idx);
        }
        if (is_leaf(node)) {
            return end();
        }
        btree_node** child_ptr = &node->_pointers[idx];
        path.push(std::make_pair(child_ptr, idx));
        node = *child_ptr;
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::find(const tkey& key) const
{
    if (_root == nullptr) {
        return end();
    }
    std::stack<std::pair<btree_node* const*, size_t>> path;
    btree_node* const* cur_ptr = &_root;
    path.push(std::make_pair(cur_ptr, 0));
    btree_node* node = *cur_ptr;
    while (true) {
        bool found = false;
        size_t idx = find_key_index(node, key, found);
        if (found) {
            return btree_const_iterator(path, idx);
        }
        if (is_leaf(node)) {
            return end();
        }
        btree_node* const* child_ptr = &node->_pointers[idx];
        path.push(std::make_pair(child_ptr, idx));
        node = *child_ptr;
    }
}

// lower_bound: первый элемент >= key
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key)
{
    if (_root == nullptr) {
        return end();
    }
    std::stack<std::pair<btree_node**, size_t>> path;
    std::stack<std::pair<btree_node**, size_t>> candidate;
    size_t candidate_index = 0;
    bool has_candidate = false;
    btree_node** cur_ptr = &_root;
    path.push(std::make_pair(cur_ptr, 0));
    btree_node* node = *cur_ptr;
    while (true) {
        bool found = false;
        size_t idx = find_key_index(node, key, found);
        if (idx < node->_keys.size()) {
            candidate = path;
            candidate_index = idx;
            has_candidate = true;
        }
        if (is_leaf(node)) {
            break;
        }
        btree_node** child_ptr = &node->_pointers[idx];
        path.push(std::make_pair(child_ptr, idx));
        node = *child_ptr;
    }
    if (!has_candidate) {
        return end();
    }
    return btree_iterator(candidate, candidate_index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key) const
{
    if (_root == nullptr) {
        return end();
    }
    std::stack<std::pair<btree_node* const*, size_t>> path;
    std::stack<std::pair<btree_node* const*, size_t>> candidate;
    size_t candidate_index = 0;
    bool has_candidate = false;
    btree_node* const* cur_ptr = &_root;
    path.push(std::make_pair(cur_ptr, 0));
    btree_node* node = *cur_ptr;
    while (true) {
        bool found = false;
        size_t idx = find_key_index(node, key, found);
        if (idx < node->_keys.size()) {
            candidate = path;
            candidate_index = idx;
            has_candidate = true;
        }
        if (is_leaf(node)) {
            break;
        }
        btree_node* const* child_ptr = &node->_pointers[idx];
        path.push(std::make_pair(child_ptr, idx));
        node = *child_ptr;
    }
    if (!has_candidate) {
        return end();
    }
    return btree_const_iterator(candidate, candidate_index);
}

// upper_bound: первый элемент > key
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key)
{
    if (_root == nullptr) {
        return end();
    }
    std::stack<std::pair<btree_node**, size_t>> path;
    std::stack<std::pair<btree_node**, size_t>> candidate;
    size_t candidate_index = 0;
    bool has_candidate = false;
    btree_node** cur_ptr = &_root;
    path.push(std::make_pair(cur_ptr, 0));
    btree_node* node = *cur_ptr;
    while (true) {
        size_t idx = 0;
        while (idx < node->_keys.size() && !compare_keys(key, node->_keys[idx].first)) {
            ++idx;
        }
        if (idx < node->_keys.size()) {
            candidate = path;
            candidate_index = idx;
            has_candidate = true;
        }
        if (is_leaf(node)) {
            break;
        }
        btree_node** child_ptr = &node->_pointers[idx];
        path.push(std::make_pair(child_ptr, idx));
        node = *child_ptr;
    }
    if (!has_candidate) {
        return end();
    }
    return btree_iterator(candidate, candidate_index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key) const
{
    if (_root == nullptr) {
        return end();
    }
    std::stack<std::pair<btree_node* const*, size_t>> path;
    std::stack<std::pair<btree_node* const*, size_t>> candidate;
    size_t candidate_index = 0;
    bool has_candidate = false;
    btree_node* const* cur_ptr = &_root;
    path.push(std::make_pair(cur_ptr, 0));
    btree_node* node = *cur_ptr;
    while (true) {
        size_t idx = 0;
        while (idx < node->_keys.size() && !compare_keys(key, node->_keys[idx].first)) {
            ++idx;
        }
        if (idx < node->_keys.size()) {
            candidate = path;
            candidate_index = idx;
            has_candidate = true;
        }
        if (is_leaf(node)) {
            break;
        }
        btree_node* const* child_ptr = &node->_pointers[idx];
        path.push(std::make_pair(child_ptr, idx));
        node = *child_ptr;
    }
    if (!has_candidate) {
        return end();
    }
    return btree_const_iterator(candidate, candidate_index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::contains(const tkey& key) const
{
    return find(key) != end();
}

// МОДИФИКАЦИЯ

// clear: удалить все элементы
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::clear() noexcept
{
    clear_node(_root);
    _root = nullptr;
    _size = 0;
}

// insert: вставка пары (копированием)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::insert(const tree_data_type& data)
{
    tree_data_type copy = data;
    return insert(std::move(copy));
}

// insert: вставка пары (перемещением)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::insert(tree_data_type&& data)
{
    return emplace(std::move(data.first), std::move(data.second));
}

// emplace: конструирование и вставка
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<typename... Args>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::emplace(Args&&... args)
{
    tree_data_type data(std::forward<Args>(args)...);
    tkey key_copy = data.first;
    auto existing = find(key_copy);
    if (existing != end()) {
        return std::make_pair(existing, false);
    }
    if (_root == nullptr) {
        _root = create_node();
        _root->_keys.push_back(std::move(data));
        ++_size;
        std::stack<std::pair<btree_node**, size_t>> path;
        path.push(std::make_pair(&_root, 0));
        return std::make_pair(btree_iterator(path, 0), true);
    }
    btree_node* out_node = nullptr;
    size_t out_index = 0;
    bool inserted = false;
    insert_non_full(_root, std::move(data), &out_node, &out_index, inserted);
    if (_root->_keys.size() == maximum_keys_in_node + 1) {
        btree_node* old_root = _root;
        _root = create_node();
        _root->_pointers.push_back(old_root);
        split_child(_root, 0);
    }
    if (inserted) {
        ++_size;
    }
    return std::make_pair(find(key_copy), inserted);
}

// insert_or_assign: вставка или обновление значения
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::insert_or_assign(const tree_data_type& data)
{
    auto result = emplace(data.first, data.second);
    if (!result.second) {
        result.first->second = data.second;
    }
    return result.first;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::insert_or_assign(tree_data_type&& data)
{
    auto result = emplace(data.first, std::move(data.second));
    if (!result.second) {
        result.first->second = std::move(data.second);
    }
    return result.first;
}

// emplace_or_assign: конструирование, вставка или обновление
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<typename... Args>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::emplace_or_assign(Args&&... args)
{
    tree_data_type data(std::forward<Args>(args)...);
    auto result = emplace(data.first, std::move(data.second));
    if (!result.second) {
        result.first->second = std::move(data.second);
    }
    return result.first;
}

// erase: удаление по итератору
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_iterator pos)
{
    if (pos == end()) {
        return end();
    }
    return erase(pos->first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_const_iterator pos)
{
    if (pos == end()) {
        return end();
    }
    return erase(pos->first);
}

// erase: удаление диапазона
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_iterator beg, btree_iterator en)
{
    auto it = beg;
    while (it != en) {
        it = erase(it);
    }
    return it;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_const_iterator beg, btree_const_iterator en)
{
    auto it = beg;
    while (it != en) {
        it = erase(it);
    }
    return it;
}

// erase: удаление по ключу
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(const tkey& key)
{
    if (_root == nullptr) {
        return end();
    }
    bool removed = erase_internal(_root, key);
    if (_root != nullptr && _root->_keys.empty()) {
        if (!_root->_pointers.empty()) {
            btree_node* old_root = _root;
            _root = _root->_pointers[0];
            destroy_node(old_root);
        } else {
            destroy_node(_root);
            _root = nullptr;
        }
    }
    if (!removed) {
        return end();
    }
    if (_root == nullptr) {
        return end();
    }
    return lower_bound(key);
}

// Внешние функции сравнения (для удобства использования)
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool compare_pairs(const typename B_tree<tkey, tvalue, compare, t>::tree_data_type &lhs,
                   const typename B_tree<tkey, tvalue, compare, t>::tree_data_type &rhs)
{
    return compare()(lhs.first, rhs.first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool compare_keys(const tkey &lhs, const tkey &rhs)
{
    return compare()(lhs, rhs);
}

#endif