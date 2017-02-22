#include "allocator.h"
#include "allocator_error.h"
#include "allocator_pointer.h"
#include "inner_pointer.h"
#include <cstddef>
#include <cstring>

Allocator::Allocator(void* base, size_t size) {
    base_ptr = base;
    base_size = size;
    innerspace_ptr = static_cast<InnerPointer*>(base) + size / sizeof(InnerPointer);
    pointers_all = 0;
    pointers_empty = 0;
    available_size = size;
    inner_root_ptr = nullptr;
    inner_last_ptr = nullptr;
}

Pointer Allocator::alloc(size_t N) {
    if (available_size >= N) {
        InnerPointer *list_ptr = inner_root_ptr, *prev_ptr = nullptr, *node_ptr;
        if (inner_root_ptr == nullptr) {
            node_ptr = inner_alloc(base_ptr, N, nullptr, nullptr);
        } else {
            if (static_cast<char*>(inner_root_ptr->pointer) - static_cast<char*>(base_ptr) > N) {
                node_ptr = inner_alloc(base_ptr, N, nullptr, inner_root_ptr);
            } else {
                do {
                    prev_ptr = list_ptr;
                    list_ptr = list_ptr->next;
                } while (list_ptr != nullptr && N > static_cast<char*>(list_ptr->pointer) - (static_cast<char*>(prev_ptr->pointer) + prev_ptr->size));
                node_ptr = inner_alloc(static_cast<char*>(prev_ptr->pointer) + prev_ptr->size, N, prev_ptr, list_ptr);
            }
        }
        if (node_ptr == nullptr) {
            if (available_size < N + sizeof(InnerPointer)) {
                throw AllocError(AllocErrorType::NoMemory, "Out of memory");
            } else {
                throw AllocError(AllocErrorType::NoMemory, "Cannot alloc whole block. Memory needs defragmentation.");
            }
        }
        available_size -= N;
        return Pointer(node_ptr);
    }
    throw AllocError(AllocErrorType::NoMemory, "Out of memory");
}

InnerPointer* Allocator::inner_alloc(void* address, size_t size, InnerPointer* prev_inner, InnerPointer* next_inner) {
    InnerPointer* ptr;
    if (pointers_empty > 0) {
        for (ptr = innerspace_ptr; (ptr < innerspace_ptr + pointers_all) && !ptr->isempty(); ptr++)
            ;
    } else {
        ptr = innerspace_ptr + pointers_all;
    }
    if (ptr >= innerspace_ptr + pointers_all) {
        if ((inner_last_ptr != nullptr && (reinterpret_cast<char*>(innerspace_ptr - 1) - static_cast<char*>(inner_last_ptr->pointer)) < inner_last_ptr->size) || (inner_last_ptr == nullptr && (innerspace_ptr - 1) < base_ptr)) {
            return nullptr;
        } else {
            innerspace_ptr--;
            available_size -= sizeof(InnerPointer);
            pointers_all++;
            pointers_empty++;
            ptr = innerspace_ptr;
        }
    }
    *ptr = InnerPointer(address, size, next_inner);
    pointers_empty--;
    if (prev_inner == nullptr) {
        inner_root_ptr = ptr;
    } else {
        prev_inner->next = ptr;
    }
    if (next_inner == nullptr) {
        inner_last_ptr = ptr;
        if ((reinterpret_cast<char*>(innerspace_ptr) - static_cast<char*>(inner_last_ptr->pointer)) < inner_last_ptr->size) {
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
        ptr = alloc(N);
        return;
    }
    if (ptr.inner->isempty())
        throw AllocError(AllocErrorType::InvalidFree, "Address already freed");
    inner = ptr.inner;
    if (inner->next == nullptr) {
        gap_size = reinterpret_cast<char*>(innerspace_ptr) - static_cast<char*>(inner->pointer);
    } else {
        gap_size = static_cast<char*>(inner->next->pointer) - static_cast<char*>(inner->pointer);
    }
    if (gap_size >= N) {
        available_size += N - inner->size;
        inner->size = N;
    } else {
        void* prev_data = inner->pointer;
        size_t prev_size = inner->size;
        free(ptr);
        ptr = alloc(N);
        std::memmove(ptr.inner->pointer, prev_data, prev_size);
        available_size += N - prev_size;
    }
}

void Allocator::free(Pointer& ptr) {
    InnerPointer* inner;
    if (ptr.inner == nullptr)
        throw AllocError(AllocErrorType::InvalidFree, "Invalid pointer");
    if (ptr.inner->isempty())
        throw AllocError(AllocErrorType::InvalidFree, "Address already freed");
    inner = ptr.inner;
    available_size += inner->size;
    inner_free(inner);
}

void Allocator::inner_free(InnerPointer* inner) {
    InnerPointer* prev_inner;
    if (inner_root_ptr != inner) {
        for (prev_inner = innerspace_ptr;
             prev_inner->next != inner; prev_inner++)
            ; //(prev_inner<(innerspace_ptr+pointers_all))&&
        prev_inner->next = inner->next;
    } else {
        inner_root_ptr = inner->next;
        prev_inner = inner_root_ptr;
    }
    if (inner_last_ptr == inner) {
        inner_last_ptr = prev_inner;
    }
    inner->pointer = nullptr;
    pointers_empty++;
    if (inner == innerspace_ptr) {
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
    if (inner_root_ptr == nullptr) {
        return;
    }
    if (inner_root_ptr->pointer != base_ptr) {
        inner_root_ptr->pointer = std::memmove(base_ptr, inner_root_ptr->pointer, inner_root_ptr->size);
    }
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
