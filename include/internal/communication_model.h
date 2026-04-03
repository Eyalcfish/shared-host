#ifndef COMMUNICATION_MODEL_H
#define COMMUNICATION_MODEL_H

#include <stdatomic.h>

#include <stddef.h>

typedef struct communication_model {
    size_t capacity;    
    atomic_int  owned;
    atomic_int  has_data;
    atomic_long  peeking;
} communication_model;

#endif /* COMMUNICATION_MODEL_H */