#include <unistd.h>

void* smalloc(size_t size){
    if(size==0 || size> 1e8){
        return nullptr;
    }
    void* allocation= sbrk(size);
    if(*(int*)allocation==-1){
        return nullptr;
    }
    return allocation;
}