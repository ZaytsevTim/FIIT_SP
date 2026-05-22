#include <iostream>
#include <allocator_buddies_system.h>

int main()
{
    try
    {    
        allocator_buddies_system resource(4096, nullptr, allocator_with_fit_mode::fit_mode::first_fit);
        std::pmr::memory_resource &mem = resource;
        auto *fit_mode_iface = dynamic_cast<allocator_with_fit_mode*>(&resource);

        // 1. Массив int (first_fit)
        std::cout << "1. int[8] with first_fit:\n";
        int *first = reinterpret_cast<int*>(mem.allocate(sizeof(int) * 8));
        for (int i = 0; i < 8; ++i)
        {
            first[i] = i + 1;
            std::cout << "   " << first[i] << "\n";
        }

        // 2. Меняем на best_fit
        std::cout << "\n2. Switching to best_fit...\n";
        fit_mode_iface->set_fit_mode(allocator_with_fit_mode::fit_mode::the_best_fit);

        // 3. Массив double (best_fit)
        std::cout << "\n3. double[3] with best_fit:\n";
        double *second = reinterpret_cast<double*>(mem.allocate(sizeof(double) * 3));
        for (int i = 0; i < 3; ++i)
        {
            second[i] = 1.5 * i;
            std::cout << "   " << second[i] << "\n";
        }

        // 4. Освобождаем first (правильный размер!)
        mem.deallocate(first, sizeof(int) * 8);
        std::cout << "\n4. Deallocated int array\n";

        // 5. Меняем на worst_fit
        std::cout << "\n5. Switching to worst_fit...\n";
        fit_mode_iface->set_fit_mode(allocator_with_fit_mode::fit_mode::the_worst_fit);

        // 6. Массив char (worst_fit)
        std::cout << "\n6. char[12] with worst_fit:\n";
        char *third = reinterpret_cast<char*>(mem.allocate(sizeof(char) * 12));
        for (int i = 0; i < 12; ++i)
        {
            third[i] = static_cast<char>('a' + i);
            std::cout << "   " << third[i] << "\n";
        }

        // 7. Вывод результатов
        std::cout << "\n7. Results:\n";
        std::cout << "   double: ";
        for (int i = 0; i < 3; ++i)
        {
            std::cout << second[i] << (i == 2 ? "\n" : " ");
        }
        std::cout << "   char:   ";
        for (int i = 0; i < 12; ++i)
        {
            std::cout << third[i] << (i == 11 ? "\n" : " ");
        }

        // 8. Освобождаем (правильные размеры!)
        mem.deallocate(second, sizeof(double) * 3);
        mem.deallocate(third, sizeof(char) * 12);

    }
    catch (const std::exception &ex)
    {
        std::cerr << "Demo failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}