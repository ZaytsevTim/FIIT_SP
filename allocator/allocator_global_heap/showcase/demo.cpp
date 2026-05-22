#include <iostream>
#include <cstring>
#include <allocator_global_heap.h>

int main() {
    try {
        
        allocator_global_heap resource;
        
        // 1. int
        std::cout << "1. int:\n";
        void* int_mem = resource.allocate(sizeof(int));
        int* i = static_cast<int*>(int_mem);
        *i = 42;
        std::cout << "   int value: " << *i << std::endl;
        resource.deallocate(int_mem, sizeof(int));
        
        // 2. double
        std::cout << "\n2. double:\n";
        void* double_mem = resource.allocate(sizeof(double));
        double* d = static_cast<double*>(double_mem);
        *d = 3.14159;
        std::cout << "   double value: " << *d << std::endl;
        resource.deallocate(double_mem, sizeof(double));
        
        // 3. float array
        std::cout << "\n3. float array (5 elements):\n";
        void* arr_mem = resource.allocate(sizeof(float) * 5);
        float* arr = static_cast<float*>(arr_mem);
        std::cout << "   values: ";
        for (int j = 0; j < 5; ++j) {
            arr[j] = j * 1.1f;
            std::cout << arr[j] << " ";
        }
        std::cout << std::endl;
        resource.deallocate(arr_mem, sizeof(float) * 5);
        
        // 4. string
        std::cout << "\n4. string:\n";
        void* str_mem = resource.allocate(20);
        char* s = static_cast<char*>(str_mem);
        std::strcpy(s, "Hello, 806!");
        std::cout << "   string: " << s << std::endl;
        resource.deallocate(str_mem, 20);
         
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}