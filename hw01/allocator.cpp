#include "allocator.h"
#include "allocator_error.h"
#include "allocator_pointer.h"
#include "inner_pointer.h"
#include <cstddef>
#include <cstring>

Allocator::Allocator(void* base, size_t size) {
    base_ptr = base;
    //указатель на служебную часть в начале указывает на конец области памяти, доступной аллокатору
    innerspace_ptr = static_cast<InnerPointer*>(base) + size / sizeof(InnerPointer);
    pointers_all = 0;
    pointers_empty = 0;
    available_size = reinterpret_cast<char *>(innerspace_ptr) - static_cast<char *>(base_ptr);
    inner_root_ptr = nullptr;
    inner_last_ptr = nullptr;
}

Pointer Allocator::alloc(size_t N) {
    //проверить наличие необходимого количества памяти
    if (available_size >= N) {
        InnerPointer *list_ptr = inner_root_ptr, *prev_ptr = nullptr, *node_ptr;
        if (inner_root_ptr == nullptr) {
            //если список выделенных участков пуст, то выделим память в самом начале
            node_ptr = inner_alloc(base_ptr, N, nullptr, nullptr);
        } else {
            if (static_cast<char*>(inner_root_ptr->pointer) - static_cast<char*>(base_ptr) >= N) {
                //если в начале доступной памяти есть кусок нужного размера, выделим память в начале
                node_ptr = inner_alloc(base_ptr, N, nullptr, inner_root_ptr);
            } else {
                //ищем достаточно большой участок свободного места
                do {
                    prev_ptr = list_ptr;
                    list_ptr = list_ptr->next;
                } while (list_ptr != nullptr && N > static_cast<char*>(list_ptr->pointer) - (static_cast<char*>(prev_ptr->pointer) + prev_ptr->size));
                //выделяем место найденном свободном участке
                node_ptr = inner_alloc(static_cast<char*>(prev_ptr->pointer) + prev_ptr->size, N, prev_ptr, list_ptr);
            }
        }
        if (node_ptr == nullptr) {
            //в случае неудачи выбросить исключение
            if (available_size < N + sizeof(InnerPointer)) {
                //нет памяти, чтобы выделить N байт и одновременно расширить служебную область
                throw AllocError(AllocErrorType::NoMemory, "Out of memory");
            } else {
                //не найден достаточно большой свободный участок
                throw AllocError(AllocErrorType::NoMemory, "Cannot alloc whole block. Memory needs defragmentation.");
            }
        }
        //уменьшаем размер доступной памяти
        available_size -= N;
        return Pointer(node_ptr);
    }
    throw AllocError(AllocErrorType::NoMemory, "Out of memory");
}

InnerPointer* Allocator::inner_alloc(void* address, size_t size, InnerPointer* prev_inner, InnerPointer* next_inner) {
    InnerPointer* ptr; //указатель на структуру в служеной памяти
    if (pointers_empty > 0) {
        //если есть свободные ячейки в служеной памяти, найдем одну из них
        for (ptr = innerspace_ptr; (ptr < innerspace_ptr + pointers_all) && !ptr->isempty(); ptr++)
            ;
    } else {
        ptr = innerspace_ptr + pointers_all;
    }
    if (ptr >= innerspace_ptr + pointers_all) {
        //если нет свободной ячейки, расширим область служебной памяти
        if ((inner_last_ptr != nullptr &&
            (reinterpret_cast<char*>(innerspace_ptr - 1) - static_cast<char*>(inner_last_ptr->pointer)) < inner_last_ptr->size)
            || (inner_last_ptr == nullptr && (innerspace_ptr - 1) < base_ptr)) {
            //если памяти не хватает, вернем nullptr
            return nullptr;
        } else {
            //расширяем область служебной памяти
            //двигаем указатель на начало служебной памяти
            innerspace_ptr--;
            //уменьшаем размер доступной памяти
            available_size -= sizeof(InnerPointer);
            //увеличиваем количество ячеек
            pointers_all++;
            pointers_empty++;
            ptr = innerspace_ptr;
        }
    }
    //записываем служебную структуру по определенному адресу
    *ptr = InnerPointer(address, size, next_inner);
    pointers_empty--;
    if (prev_inner == nullptr) {
        //структура становится головой списка, если список был пуст (то есть ничего не было выделено)
        inner_root_ptr = ptr;
    } else {
        //иначе структура встраивается в середину списка
        prev_inner->next = ptr;
    }
    if (next_inner == nullptr) {
        //обновим указатель на хвост списка, если это необходимо
        inner_last_ptr = ptr;
        if ((reinterpret_cast<char*>(innerspace_ptr) - static_cast<char*>(inner_last_ptr->pointer)) < inner_last_ptr->size) {
            //если при выделении памяти нарушены границы между блоками - вернуть все на место,
            // освободив ячейку в служебной области
            inner_free(ptr);
            return nullptr;
        }
    }
    return ptr;
}

void Allocator::realloc(Pointer& ptr, size_t N) {
    InnerPointer* inner;
    size_t gap_size = 0;
    if (ptr.inner == nullptr) {
        //если указатель пуст, то просто выделить память
        ptr = alloc(N);
        return;
    }
    //проверить, что указатель не был освобожден
    if (ptr.inner->isempty())
        throw AllocError(AllocErrorType::InvalidFree, "Address already freed");
    inner = ptr.inner;
    //определить размер свободной области впереди данного блока (включая размер самого блока)
    if (inner->next == nullptr) {
        gap_size = reinterpret_cast<char*>(innerspace_ptr) - static_cast<char*>(inner->pointer);
    } else {
        gap_size = static_cast<char*>(inner->next->pointer) - static_cast<char*>(inner->pointer);
    }
    if (gap_size >= N) {
        //если памяти после указателя достаточно, то просто увеличим размер блока
        available_size += N - inner->size;
        inner->size = N;
    } else {
        //иначе освободим текущую область и выделим другую
        void* data = inner->pointer;
        size_t size = inner->size;
        InnerPointer *next_inner = inner->next, *prev_inner;
        free(ptr);
        try {
            ptr = alloc(N);
            std::memmove(ptr.inner->pointer, data, size);
            available_size += N - size;
        }
        catch (AllocError &) {
            //в случае ошибки выделения, находим место в списке и возвращаем на место старую структуру
            for (prev_inner = innerspace_ptr; prev_inner->next != next_inner; prev_inner++)
                ;
            inner = inner_alloc(data,size,prev_inner,next_inner);
            ptr = Pointer(inner);
            throw;
        }
    }
}

void Allocator::free(Pointer& ptr) {
    InnerPointer* inner;
    //проверка, что указатель не пустой
    if (ptr.inner == nullptr)
        throw AllocError(AllocErrorType::InvalidFree, "Invalid pointer");
    if (ptr.inner->isempty())
        throw AllocError(AllocErrorType::InvalidFree, "Address already freed");
    //увеличим размер свободной памяти и очистим ячейку в служебной памяти
    inner = ptr.inner;
    available_size += inner->size;
    inner_free(inner);
}

void Allocator::inner_free(InnerPointer* inner) {
    //поиск элемента списка, предшествующего удаляемому и обновление указателей на голову и хвост списка
    InnerPointer* prev_inner;
    if (inner_root_ptr != inner) {
        for (prev_inner = innerspace_ptr;
             prev_inner->next != inner; prev_inner++)
            ;
        prev_inner->next = inner->next;
    } else {
        inner_root_ptr = inner->next;
        prev_inner = inner_root_ptr;
    }
    if (inner_last_ptr == inner) {
        inner_last_ptr = prev_inner;
    }
    //опустошение ячейки
    inner->pointer = nullptr;
    pointers_empty++;
    if (inner == innerspace_ptr) {
        //в случае, когда эта ячейка располагается в начале служебной области,
        // сокращаем служебную область до тех пор, пока не дойдем до непустой ячейки
        while ((pointers_all > 0) && inner->isempty()) {
            innerspace_ptr++;
            pointers_empty--;
            pointers_all--;
            available_size += sizeof(InnerPointer);
        }
    }
}

void Allocator::defrag() {
    InnerPointer *list_ptr = inner_root_ptr, *prev_ptr = nullptr;
    //проверим, что выделена хоть одна область
    if (inner_root_ptr == nullptr) {
        return;
    }
    //если в начале доступной памяти есть место, сдвинем первый выделенный блок
    if (inner_root_ptr->pointer != base_ptr) {
        inner_root_ptr->pointer = std::memmove(base_ptr, inner_root_ptr->pointer, inner_root_ptr->size);
    }
    //сдвигаем оставшиеся куски
    prev_ptr = inner_root_ptr;
    list_ptr = inner_root_ptr->next;
    while (list_ptr != nullptr) {
        if (static_cast<char*>(list_ptr->pointer) != static_cast<char*>(prev_ptr->pointer) + prev_ptr->size) {
            list_ptr->pointer = std::memmove(static_cast<char*>(prev_ptr->pointer) + prev_ptr->size, list_ptr->pointer,
                list_ptr->size);
        }
        prev_ptr = list_ptr;
        list_ptr = list_ptr->next;
    }
}
