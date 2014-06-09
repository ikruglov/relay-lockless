#ifndef __CLEANUP_H__
#define __CLEANUP_H__

#include "list.h"

void init_cleanup(size_t num_of_threads, list_item_t* head);
void update_offset(size_t thread_id, list_item_t* offset);
list_item_t* shrink_list(list_item_t* head, size_t* deleted);

#endif
