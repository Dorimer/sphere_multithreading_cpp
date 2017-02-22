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
    void* base_ptr;
    int pointers_all;
    int pointers_empty;
    size_t base_size;
    size_t available_size;
    InnerPointer* innerspace_ptr;
    InnerPointer* inner_root_ptr;
    InnerPointer* inner_last_ptr;

public:
    Allocator(void* base, size_t size);

    /**
     * @param N size_t
     */
    Pointer alloc(size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     * @param N size_t
     */
    void realloc(Pointer& ptr, size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     */
    void free(Pointer& ptr);

    /**
     * TODO: semantics
     */
    void defrag();

    InnerPointer* inner_alloc(void* address, size_t size, InnerPointer* prev_inner, InnerPointer* next_inner);

    void inner_free(InnerPointer* ptr);

    /**
     * TODO: semantics
     */
    std::string dump() const { return ""; }
};

#endif // ALLOCATOR
