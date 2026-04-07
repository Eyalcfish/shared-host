#ifndef COMMUNICATION_MODEL_H
#define COMMUNICATION_MODEL_H

#include <stdatomic.h>

#include <stddef.h>

typedef struct communication_model {
    size_t capacity;    
    atomic_long left_space;
    atomic_long waiting_for_message;
} communication_model;

typedef struct communication_model_message {
    atomic_int  owned;
    atomic_int  has_data;
    atomic_long message_size;
} communication_model_message;

#endif /* COMMUNICATION_MODEL_H */