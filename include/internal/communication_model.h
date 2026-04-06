#ifndef COMMUNICATION_MODEL_H
#define COMMUNICATION_MODEL_H

#include <stdatomic.h>

#include <stddef.h>

typedef struct communication_model {
    size_t capacity;    
    atomic_long left_space;
} communication_model;

typedef struct communication_model_message {
    atomic_int  owned;
    atomic_int  has_data;
    size_t message_size;
} communication_model_message;

#endif /* COMMUNICATION_MODEL_H */