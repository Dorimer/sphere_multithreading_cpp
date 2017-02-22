#ifndef INNER_POINTER
#define INNER_POINTER

#include <cstddef>

class InnerPointer {

public:
    InnerPointer(void* _pointer, size_t _size, InnerPointer* _next)
        : pointer(_pointer)
        , size(_size)
        , next(_next) {}

    void* pointer;
    size_t size;
    InnerPointer* next;

    bool isempty() const;
};

#endif //INNER_POINTER
