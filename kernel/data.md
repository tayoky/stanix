---
title: data structures
---
Stanix's kernel provide helpers for various data structures.

## vectors
A **vector** (`vector_t` in `<kernel/vector.h>`) is a list of element continous in memory.
A vector can be initialized using `init_vector` and freed using `free_vector`.
New elements can be added using `vector_push_back`, poped using `vector_pop_back` or accessed from their indexes using `vector_at` or the `data` field.
The `count` field precise the number of elements in the vector.
While the size of a vector is dynamic the size of an element cannot change.

## hashmaps
An **hashmap** (`hashmap_t` in `<kernel/hashmap.h>`) is map that store a list of element-key pair by their hash.
An hashmap can be initialized using `init_hashmap` and freed using `free_hashmap`.
A new entry can be added using `hashmap_add`, get from a key using `hashmap_get` and removed using `hashmap_remove`.
To iterate over each pair use `hashmap_foreach`.
The hashmap implementation of Stanix only use `long` key.

## linked lists
A **linked list** (`list_t` in `<kernel/list.h>`) is a list of element made using pointer.
Each element must contain the `list_node_t` somewhere in it's struct.
A list can be initalized using `init_list` and freed using `destroy_list`.
A new element can be added at the end using `list_append`, added after a specific element (`NULL` mean at the start) using `list_add_after` and removed using `list_remove`.
To iterate over each element use the `foreach` macro.
