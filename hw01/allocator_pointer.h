#ifndef ALLOCATOR_POINTER
#define ALLOCATOR_POINTER

#include "inner_pointer.h"

// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Allocator;

class Pointer {
public:
    InnerPointer* inner;

    Pointer()
        : inner(nullptr) {}

    Pointer(InnerPointer* _inner)
        : inner(_inner) {}

    void* get() const;
};

#endif //ALLOCATOR_POINTER
