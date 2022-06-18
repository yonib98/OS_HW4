#include <unistd.h>
struct MallocMetadata{
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

int num_of_allocs=0;
int num_of_free_blocks=0;
int num_of_free_bytes=0;
int num_of_total_allocated_bytes=0;
int num_of_meta_data_bytes=0;


MallocMetadata dummy={0,false,nullptr,nullptr};
MallocMetadata *head=&dummy;
void* _smalloc(size_t size){
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
            num_of_free_blocks--;
            num_of_free_bytes-=to_find->size;
            return to_find+1;
        }
        temp=to_find;
        to_find=to_find->next;
    }   
    void* allocation= sbrk(sizeof(MallocMetadata)+size);
    if(*(int*)allocation==-1){
        return nullptr;
    }
    num_of_allocs++;
    num_of_total_allocated_bytes+=size;
    
    MallocMetadata* new_allocation= (MallocMetadata*)allocation;
    new_allocation->size=size;
    new_allocation->is_free=false;
    new_allocation->next=nullptr;
    new_allocation->prev=temp;
    temp->next=new_allocation;
    return allocation+sizeof(MallocMetadata);
    //update stats as usual
}

void _sfree(void* p){
    if(p==nullptr){
        return;
    }
    MallocMetadata* to_update=(MallocMetadata*)p-1;
    if(to_update->is_free){
        return;
    } 
    to_update->is_free=true;
    //update stats as usual;
    num_of_free_blocks++;
    num_of_free_bytes+=to_update->size;

}

void* scalloc(size_t num, size_t size){
    if((num==0 || size==0) || size * num > 1e8){
        return nullptr;
    }
}

size_t _num_free_block(){
    return num_of_free_blocks;
}

size_t _num_free_bytes(){
    return num_of_free_bytes;
}

size_t _num_allocated_blocks(){
    return num_of_allocs;
}

size_t _num_allocated_bytes(){
    return num_of_total_allocated_bytes;
}

size_t _num_meta_data_bytes(){
    return (num_of_allocs*_size_meta_data());
}

size_t _size_meta_data(){
    return sizeof(MallocMetadata);
}
