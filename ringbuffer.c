#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/errno.h>

#include "ringbuffer.h"

MODULE_LICENSE("GPL v2");

#define TLOG_PREF "tringbuffer: "

static size_t write_raw(void* to, const void* __user from, size_t bytes) {
    if (!bytes) return 0; // is it really needed
    
    size_t nb = copy_from_user(to, from, bytes);
    if (nb != 0) {
        pr_err(TLOG_PREF "Failed copy_from_user (written %lu of %lu bytes)\n", bytes - nb, bytes);
    }
    return bytes - nb;
}

static size_t read_raw(void* __user to, const void* from, size_t bytes) {
    if (!bytes) return 0; // is it really needed
    
    size_t nb = copy_to_user(to, from, bytes); // can sleep, do not use spinlock
    if (nb != 0) {
        pr_err(TLOG_PREF "Failed copy_to_user (written %lu of %lu bytes)\n", bytes - nb, bytes);
    }
    return bytes - nb;
}

struct tringbuffer* tringbuffer_init(size_t size) {
    struct tringbuffer* rb = kmalloc(sizeof(struct tringbuffer), GFP_KERNEL);
    if (rb == NULL) return NULL;
    rb->data = kmalloc(size, GFP_USER);
    if (rb->data == NULL) {
        kfree(rb);
        return NULL;
    }

    rb->capacity = size;
    rb->head = 0;
    rb->tail = 0;
    rb->is_full = false;
    mutex_init(&rb->lock);
    return rb;
}

void tringbuffer_deinit(struct tringbuffer* rb) {
    kfree(rb->data);
    mutex_destroy(&rb->lock);
    kfree(rb);
}

static size_t tringbuffer__available(struct tringbuffer* rb) {
    if (rb->head < rb->tail) return rb->tail - rb->head;
    if (rb->head > rb->tail) return rb->capacity - rb->head + rb->tail;
    if (!rb->is_full) return rb->capacity;
    return 0;
}

size_t tringbuffer_available(struct tringbuffer* rb) {
    size_t ret;
    mutex_lock(&rb->lock);
    ret = tringbuffer__available(rb);
    mutex_unlock(&rb->lock);
    return ret;
}

size_t tringbuffer_stored(struct tringbuffer* rb) {
    size_t ret;
    mutex_lock(&rb->lock);
    ret = rb->capacity - tringbuffer__available(rb);
    mutex_unlock(&rb->lock);
    return ret;
}

size_t tringbuffer_capacity(struct tringbuffer* rb) {
    size_t ret;
    mutex_lock(&rb->lock);
    ret = rb->capacity;
    mutex_unlock(&rb->lock);
    return ret;
}


size_t tringbuffer_write(struct tringbuffer* rb, const void* data, size_t bytes) {
    mutex_lock(&rb->lock);
    pr_info(TLOG_PREF "Writing %lu bytes to rb: cap %lu, head %lu, tail %lu, full? %s\n", 
        bytes, rb->capacity, rb->head, rb->tail, rb->is_full ? "yes" : "no");
    if (rb->is_full) {
        mutex_unlock(&rb->lock);
        return 0;
    }

    size_t total = 0;
    if (rb->head >= rb->tail) {
        size_t wr = write_raw(rb->data + rb->head, data, min(rb->capacity - rb->head, bytes));
        rb->head = (rb->head + wr) % rb->capacity;
        total += wr;
    }

    size_t wr = write_raw(rb->data + rb->head, data + total, min(rb->tail - rb->head, bytes - total));
    rb->head += wr;
    total += wr;

    if (rb->tail == rb->head) rb->is_full = true;

    mutex_unlock(&rb->lock);
    return total;
}

size_t tringbuffer_read(struct tringbuffer* rb, void* data, size_t bytes) {
    mutex_lock(&rb->lock);
    pr_info(TLOG_PREF "Reading %lu bytes from rb: cap %lu, head %lu, tail %lu, full? %s\n", 
        bytes, rb->capacity, rb->head, rb->tail, rb->is_full ? "yes" : "no");
    if (!rb->is_full && rb->head == rb->tail) {
        mutex_unlock(&rb->lock);
        return 0;
    }

    size_t total = 0;
    if (rb->tail >= rb->head) {
        size_t rd = read_raw(data, rb->data + rb->tail, min(rb->capacity - rb->tail, bytes));
        rb->tail = (rb->tail + rd) % rb->capacity;
        total += rd;
    }

    size_t rd = read_raw(data + total, rb->data + rb->tail, min(rb->head - rb->tail, bytes - total));
    rb->tail += rd;
    total += rd;

    if (bytes) rb->is_full = false;

    mutex_unlock(&rb->lock);
    return total;
}
