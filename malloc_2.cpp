#include <unistd.h>
#include <cstring>
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
void* _split(void* freeBlock,size_t size){
    MallocMetadata* allocation = (MallocMetadata*)(freeBlock+size);
}
void* smalloc(size_t size){
      if(size==0 || size> 1e8){
        return nullptr;
    }
    MallocMetadata* to_find=head;
    MallocMetadata* temp= head;
    while(to_find!=nullptr){
        if(to_find->is_free && size<=to_find->size){
            if(to_find->size-size-sizeof(MallocMetadata)>128){
                return _split(to_find,size);
            }
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
    return (MallocMetadata*)allocation+1;
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
    num_of_free_blocks++;
    num_of_free_bytes+=to_update->size;
    

}

void* scalloc(size_t num, size_t size){
    if((num==0 || size==0) || size * num > 1e8){
        return nullptr;
    }
   void* allocation =  smalloc(size*num);
    if(allocation==NULL){
        return NULL;
    }
    std::memset(allocation,0,size*num);
    return allocation;
}

void* srealloc(void* oldp, size_t size){
    if(size==0 || size>1e8){
        return NULL;
    }
    if(oldp==NULL){
        return smalloc(size);
    }
    MallocMetadata* to_update=(MallocMetadata*)oldp-1;
    if(to_update->size>=size){
        return oldp;
    }
    void* allocation =  smalloc(size);
    if(allocation==NULL){
        return NULL;
    }
    memmove(allocation,oldp,to_update->size);
    sfree(oldp);
    return allocation;
}
size_t _num_free_blocks(){
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

size_t _size_meta_data(){
    return sizeof(MallocMetadata);
}

size_t _num_meta_data_bytes(){
    return (num_of_allocs*_size_meta_data());
}


