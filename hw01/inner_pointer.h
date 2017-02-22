#ifndef INNER_POINTER
#define INNER_POINTER

#include <cstddef>

//служебная структура, хранящая указатель на область памяти и представляющая собой элемент списка
class InnerPointer {
public:
    void* pointer; //указатель на область выделенной памяти
    size_t size; //размер участка выделенной памяти
    InnerPointer* next; //указатель следующую структуру
    InnerPointer(void* _pointer, size_t _size, InnerPointer* _next)
        : pointer(_pointer)
        , size(_size)
        , next(_next) {}
    //проверка, хранит ли структура указатель на память
    bool isempty() const;
};

#endif //INNER_POINTER
