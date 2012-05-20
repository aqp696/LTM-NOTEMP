#ifndef _LTM_ALLOCATOR_H
#define _LTM_ALLOCATOR_H

#include <cstdlib>
#include <stddef.h>

#include <new>
#include <bits/functexcept.h>
#include <pthread.h>

#include "pool.h"

class shm_Allocator_base {
public:
    static pthread_mutex_t * sema;
    static pool * memory_pool;
};

template<typename _Tp>
class shm_Allocator : public shm_Allocator_base {
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef _Tp* pointer;
    typedef const _Tp* const_pointer;
    typedef _Tp& reference;
    typedef const _Tp& const_reference;
    typedef _Tp value_type;

    template<typename _Tp1>
    struct rebind {
        typedef shm_Allocator<_Tp1> other;
    };

    shm_Allocator() throw () {
    }

    shm_Allocator(const shm_Allocator&) throw () {
    }

    template<typename _Tp1>
    shm_Allocator(const shm_Allocator<_Tp1>&) throw () {
    }

    ~shm_Allocator() throw () {
    }

    pointer
    address(reference __x) const {
        return &__x;
    }

    const_pointer
    address(const_reference __x) const {
        return &__x;
    }

    // NB: __n is permitted to be 0.  The C++ standard says nothing
    // about what the return value is when __n == 0.

    pointer
    allocate(size_type __n, const void* = 0) {
        if (__builtin_expect(__n > this->max_size(), false))
            std::__throw_bad_alloc();
        //{printf("out of memory\n"); exit(1);}

        pthread_mutex_lock(sema);

        pointer po = static_cast<_Tp*> (memory_pool->allocate(__n * sizeof (_Tp)));
        if (!po) 
            fprintf(stderr, "shm_Allocator: agotada la memoria en shm\n");

        //	memory_pool->dump();

        pthread_mutex_unlock(sema);

        return po;
    }

    // __p is not permitted to be a null pointer.

    void
    deallocate(pointer __p, size_type) {
        pthread_mutex_lock(sema);
        memory_pool->deallocate((char*) __p);
        pthread_mutex_unlock(sema);
    }

    size_type
    max_size() const throw () {
        return size_t(-1) / sizeof (_Tp);
    }

    // _GLIBCXX_RESOLVE_LIB_DEFECTS
    // 402. wrong new expression in [some_] allocator::construct

    void
    construct(pointer __p, const _Tp& __val) {
        ::new(__p) _Tp(__val);
    }

    void
    destroy(pointer __p) {
        __p->~_Tp();
    }
};

template<typename _Tp>
inline bool
operator==(const shm_Allocator<_Tp>&, const shm_Allocator<_Tp>&) {
    return true;
}

template<typename _Tp>
inline bool
operator!=(const shm_Allocator<_Tp>&, const shm_Allocator<_Tp>&) {
    return false;
}

#endif /* _LTM_ALLOCATOR_H */
