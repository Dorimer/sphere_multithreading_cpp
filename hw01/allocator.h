#ifndef ALLOCATOR
#define ALLOCATOR

#include "inner_pointer.h"
#include <cstddef>
#include <string>

// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Pointer;

/**
 * Wraps given memory area and provides defagmentation allocator interface on
 * the top of it.
 *
 *
 */
class Allocator {
private:
    void* base_ptr; //ссылка на начало бока памяти
    int pointers_all; //количество выделенных ячеек служебной памяти
    int pointers_empty; //количество пустых ячеек среди выделенных
    size_t available_size; //количество байт, доступных для записи
    InnerPointer* innerspace_ptr; //начало служебной области
    InnerPointer* inner_root_ptr; //указатель на голову списка указателей
    InnerPointer* inner_last_ptr; //указатель на хвост списка указателей

public:
    Allocator(void* base, size_t size);

    //выделение памяти
    Pointer alloc(size_t N);
    //изменение размера выделенной области
    void realloc(Pointer& ptr, size_t N);
    //освобождение выделенной памяти
    void free(Pointer& ptr);
    //дефрагментация памяти
    void defrag();
    //выделение служебной памяти и создание указателя внутри неё
    InnerPointer* inner_alloc(void* address, size_t size, InnerPointer* prev_inner, InnerPointer* next_inner);
    //удаление указателя и освобождение участка служебной памяти
    void inner_free(InnerPointer* ptr);
};

#endif // ALLOCATOR
