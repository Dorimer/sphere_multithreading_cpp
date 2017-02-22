#include "allocator_pointer.h"

void* Pointer::get() const {
    return inner->pointer;
}