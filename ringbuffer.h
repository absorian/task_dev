#ifndef TRINGBUFFER
#define TRINGBUFFER

#include <linux/module.h>
#include <linux/types.h>

struct tringbuffer {
    void* data;
    size_t capacity;

    size_t head;
    size_t tail;

    bool is_full;
};

struct tringbuffer tringbuffer_init(size_t size);

void tringbuffer_deinit(struct tringbuffer* rb);

size_t tringbuffer_available(struct tringbuffer* rb);

size_t tringbuffer_stored(struct tringbuffer* rb);

size_t tringbuffer_write(struct tringbuffer* rb, const void* data, size_t bytes);

size_t tringbuffer_read(struct tringbuffer* rb, void* data, size_t bytes);


#endif //TRINGBUFFER

