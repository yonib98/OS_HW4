#include <unistd.h>
struct MallocMetadata{
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};
int num_of_allocs=0;
MallocMetadata dummy={0,false,nullptr,nullptr};
MallocMetadata *head=&dummy;
void* smalloc(size_t size){
      if(size==0 || size> 1e8){
        return nullptr;
    }
    MallocMetadata* to_find=head;
    MallocMetadata* temp= head;
    while(to_find!=nullptr){
        if(to_find->is_free || size<=to_find->size){
            //no need to alloc new space
            to_find->is_free=false;
            //need to add all the global stats
            int sizeeee= sizeof(MallocMetadata);
            return to_find+1;
        }
        temp=to_find;
        to_find=to_find->next;
    }   
    void* allocation= sbrk(sizeof(MallocMetadata)+size);
    if(*(int*)allocation==-1){
        return nullptr;
    }
    MallocMetadata* new_allocation= (MallocMetadata*)allocation;
    new_allocation->size=size;
    new_allocation->is_free=false;
    new_allocation->next=nullptr;
    new_allocation->prev=temp;
    temp->next=new_allocation;
    return allocation+sizeof(MallocMetadata);
    //update stats as usual
}

void sfree(void* p){
    if(p==nullptr){
        return;
    }
    MallocMetadata* to_update=(MallocMetadata*)p-1;
    if(to_update->is_free){
        return;
    } 
    to_update->is_free=true;
    //update stats as usual;
}

void* scalloc(size_t num, size_t size){
    if((num==0 || size==0) || size * num > 1e8){
        return nullptr;
    }
    
}

