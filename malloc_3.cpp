#include <unistd.h>
#include <cstring>
#include <iostream>
#include <math.h>
struct MallocMetadata
{
    size_t size;
    bool is_free;
    MallocMetadata *next;
    MallocMetadata *prev;
};

MallocMetadata dummy={0, false, nullptr, nullptr};
int num_of_allocs = 0;
int num_of_free_blocks = 0;
int num_of_free_bytes = 0;
int num_of_total_allocated_bytes = 0;
int num_of_meta_data_bytes = 0;
class SortedList{
    MallocMetadata* head;
    public:
    SortedList(): head(&dummy){};
    void insert(MallocMetadata* to_update){
    MallocMetadata* temp=head;
    while (temp->next != nullptr && temp->next->size <= to_update->size)
    {  
        if(temp->next->size==to_update->size && temp->next-to_update>0){
            break;
        }
        temp = temp->next;
    }
    if (temp->next != nullptr)
    {
        temp->next->prev = to_update;
    }
    to_update->next = temp->next;
    temp->next = to_update;
    to_update->prev = temp;

    num_of_free_blocks++;
    num_of_free_bytes += to_update->size;
    }
    void remove(MallocMetadata* to_remove){
    // remove from freeList
            
            to_remove->prev->next = to_remove->next;
            if(to_remove->next!=nullptr){
                to_remove->next->prev=to_remove->prev;
            }
            to_remove->next = nullptr;
            to_remove->prev = nullptr;

            num_of_free_blocks--;
            num_of_free_bytes -= to_remove->size;
    }
    friend void* smalloc(size_t size);
    friend MallocMetadata* _merge(MallocMetadata* to_update);
};

SortedList freeList = SortedList();

void* _split(void* freeBlock,size_t size){
    MallocMetadata* to_update = (MallocMetadata*)(freeBlock+size);
    to_update->is_free=true;
    to_update->size = ((MallocMetadata*)freeBlock)->size - size-sizeof(MallocMetadata);
    freeList.remove((MallocMetadata*)freeBlock);
    freeList.insert(to_update);
    
    // update stats as usual;
    
    
    ((MallocMetadata*)freeBlock)->size=size;
    ((MallocMetadata*)freeBlock)->is_free=false;
    ((MallocMetadata*)freeBlock)->next=nullptr;
    ((MallocMetadata*)freeBlock)->prev=nullptr;

}

void *smalloc(size_t size)
{
    double temp_num=size/8;
    size=ceil(temp_num)*8;
    MallocMetadata* wilderness_block = (MallocMetadata*)sbrk(0);
    std::cout<<"sbrk "<< sbrk(0)<< std::endl;
    if (size == 0 || size > 1e8)
    {
        return nullptr;
    }
    MallocMetadata *to_find = freeList.head;
    MallocMetadata *temp = freeList.head;
    while (to_find != nullptr)
    {
        if (size <= to_find->size)
        {   
            int extraSpace = to_find->size-size-sizeof(MallocMetadata);
             if(extraSpace >128 && extraSpace < 1e8 ){
                return _split(to_find,size);
            }
            freeList.remove(to_find);
            // need to add all the global stats

            to_find->is_free=false;
            return to_find + 1;
        }
        temp = to_find;
        to_find = to_find->next;
    }
    
    if(wilderness_block->is_free==true){
        freeList.remove(wilderness_block);
        int original_size=wilderness_block->size;
        sbrk(size-original_size);
        wilderness_block->size=size;
        wilderness_block->is_free=false;

        //expand the wilderness_block
        //should we increase the num_of_allocs in this case????????????PIAZAAAAAAAAAAA
        num_of_total_allocated_bytes+=size-original_size;
        return wilderness_block;
    } 
    void *allocation = sbrk(sizeof(MallocMetadata) + size);
    if (*(int *)allocation == -1)
    {
        return nullptr;
    }
    num_of_allocs++;
    num_of_total_allocated_bytes += size;

    MallocMetadata *new_allocation = (MallocMetadata *)allocation;
    new_allocation->size = size;
    new_allocation->is_free = false;
    new_allocation->next = nullptr;
    new_allocation->prev = nullptr;
    return (MallocMetadata *)allocation + 1;
    // update stats as usual
}

MallocMetadata* _merge(MallocMetadata* to_update){
    MallocMetadata* left_merge=nullptr, *right_merge=nullptr;
    MallocMetadata* head = freeList.head;
    while(head!=nullptr){
        if((void*)head+sizeof(MallocMetadata)+head->size==to_update){
                left_merge=head;
        }
        if((void*)to_update+sizeof(MallocMetadata)+to_update->size==head){
            right_merge=head;
        }
        head=head->next;
    }

    if(left_merge && right_merge){
        int new_size = left_merge->size + right_merge->size+to_update->size + 2*sizeof(MallocMetadata);
        freeList.remove(left_merge);
        freeList.remove(right_merge);
        left_merge->size=new_size;
        return left_merge;
    }
    else if (left_merge){
        int new_size = left_merge->size + to_update->size + sizeof(MallocMetadata);
        freeList.remove(left_merge);
         left_merge->size=new_size;
        return left_merge;
    }
    else if (right_merge){
        int new_size = right_merge->size + to_update->size + sizeof(MallocMetadata);
        freeList.remove(right_merge);
        to_update->size=new_size;
        return to_update;
    }
    else{
        return to_update;
    }
}
void sfree(void *p)
{
    if (p == nullptr)
    {
        return;
    }
    MallocMetadata *to_update = (MallocMetadata *)p - 1;
    if (to_update->is_free)
    {
        return;
    }
    to_update = _merge(to_update);
    to_update->is_free = true;
    freeList.insert(to_update);

    // update stats as usual;

}

void *scalloc(size_t num, size_t size)
{
    if ((num == 0 || size == 0) || size * num > 1e8)
    {
        return nullptr;
    }
    void *allocation = smalloc(size * num);
    if (allocation == NULL)
    {
        return NULL;
    }
    std::memset(allocation, 0, size * num);
    return allocation;
}

void *srealloc(void *oldp, size_t size)
{
    if (size == 0 || size > 1e8)
    {
        return NULL;
    }
    if (oldp == NULL)
    {
        return smalloc(size);
    }
    MallocMetadata *to_update = (MallocMetadata *)oldp - 1;
    if (to_update->size >= size)
    {
        return oldp;
    }
    void *allocation = smalloc(size);
    if (allocation == NULL)
    {
        return NULL;
    }
    memmove(allocation, oldp, to_update->size);
    sfree(oldp);
    return allocation;
}
size_t _num_free_blocks()
{
    return num_of_free_blocks;
}

size_t _num_free_bytes()
{
    return num_of_free_bytes;
}

size_t _num_allocated_blocks()
{
    return num_of_allocs;
}

size_t _num_allocated_bytes()
{
    return num_of_total_allocated_bytes;
}

size_t _size_meta_data()
{
    return sizeof(MallocMetadata);
}

size_t _num_meta_data_bytes()
{
    return (num_of_allocs * _size_meta_data());
}
