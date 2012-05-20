#ifndef _POOL_H
#define _POOL_H

#include <cstdlib>
#include <cstdio>

class pool {

    enum pool_defaults {
        init_size = 0xfffff,
        min_size = 0xf
    };
public:

    pool(size_t size = init_size, char * origin = 0) : size_(size) {
        if (size_ - sizeof (block) < min_size) {
            printf("Initial pool size too small \n");
            std::exit(-1);
        }

        char * o = origin;
        if (!o)
            o = (char *) new char[size_];

        blocks_ = reinterpret_cast<block*> (o);
        blocks_->prev_ = 0;
        blocks_->next_ = 0;
        blocks_->free_ = 1;
        blocks_->size_ = size_ - sizeof (block);
        //		this->dump();
    };

    ~pool() {
    }

    void* allocate(size_t size) {
        if (size > size_ - sizeof (block))
            return 0;
        block *b = blocks_;
        while (1) {
            while (!b->free_) {
                if (!b->next_) return 0;
                b = b->next_;
            }
            if (b->size_ < size) {
                b = b->next_;
                if (b) continue;
                else
                    return 0;
            }
            break;
        }
        if (b->size_ - size < 2 * sizeof (block)) {
            b->free_ = 0;
            return reinterpret_cast<char *> (b) + sizeof (block);
        } else {
            block * new_block = (reinterpret_cast<block *> (reinterpret_cast<char *> (b)
                    + size + sizeof (block)));
            if (b->next_) b->next_->prev_ = new_block;
            new_block->next_ = b->next_;
            b->next_ = new_block;
            new_block->prev_ = b;
            b->free_ = 0;
            new_block->size_ = b->size_ - size - sizeof (block);
            b->size_ = size;
            new_block->free_ = 1;
            return reinterpret_cast<char *> (b) + sizeof (block);
        }

    }

    void deallocate(void *p, size_t = 0) {
        if (!p) return;
        block *b = reinterpret_cast<block *> (static_cast<char*> (p) - sizeof (block));
        if (b->prev_ && b->next_) {
            if (b->prev_->free_ && b->next_->free_) {
                b->prev_->size_ += b->size_ + b->next_->size_ + 2 * sizeof (block);
                b->prev_->next_ = b->next_->next_;
                if (b->next_->next_)b->next_->next_->prev_ = b->prev_;
                return;
            }
        }
        if (b->prev_) {
            if (b->prev_->free_) {
                b->prev_->size_ += b->size_ + sizeof (block);
                b->prev_->next_ = b->next_;
                if (b->next_) b->next_->prev_ = b->prev_;
                b->free_ = 1;
                return;
            }
        }
        if (b->next_) {
            if (b->next_->free_) {
                b->size_ += b->next_->size_ + sizeof (block);
                b->next_ = b->next_->next_;
                if (b->next_) b->next_->prev_ = b;
                b->free_ = 1;
                return;
            }
        }
        b->free_ = 1;
    }

    void dump() {
        using namespace std;
        block *b = blocks_;
        while (1) {
            printf("Size %zd, free %d, prev %p, next %p \n", b->size_, b->free_, b->prev_, b->next_);
            if (b->next_) b = b->next_;
            else break;
        }
    }
private:
    size_t size_;

    struct block {
        block *prev_;
        block *next_;
        size_t size_;
        int free_;

        block(block *prev, block *next, size_t size, int free) :
        prev_(prev), next_(next), size_(size), free_(free) {
        }

        ~block() {
        }
    };
    block * blocks_;
    //  pool(const pool &);
    //public:
    // pool& operator=(const pool&);

};


#endif

