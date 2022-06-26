#include <unistd.h>
#include <cstring>
#include <iostream>
#include <math.h>
#include <sys/mman.h>

struct MallocMetadata
{
    size_t size;
    bool is_free;
    MallocMetadata *next;
    MallocMetadata *prev;
    MallocMetadata* next_free;
    MallocMetadata* prev_free;
};

MallocMetadata dummy={0, false, nullptr, nullptr};
MallocMetadata dummy_second = {0,false,nullptr,nullptr};
MallocMetadata mmap_dummy={0,false, nullptr, nullptr};
int num_of_allocs = 0;
int num_of_free_blocks = 0;
int num_of_free_bytes = 0;
int num_of_total_allocated_bytes = 0;
int num_of_meta_data_bytes = 0;
class mmap_List{
    MallocMetadata* head;
    MallocMetadata* last;
public:
    mmap_List(): head(&mmap_dummy),last(&mmap_dummy){};
    void insert(MallocMetadata* to_update){
        MallocMetadata* temp=head;
        last->next = to_update;
        to_update->prev = last;
        to_update->next=nullptr;
        last=to_update;
    }
    void remove(MallocMetadata* to_remove){
        // remove from freeList
        to_remove->prev->next = to_remove->next;
        if(to_remove->next!=nullptr){
            to_remove->next->prev=to_remove->prev;
        }
        to_remove->next = nullptr;
        to_remove->prev = nullptr;

    }
    //need to create List merge for free blocks and split for blocks
    friend void* smalloc(size_t size);
    friend MallocMetadata* _merge(MallocMetadata* to_update);
};
class List{
    MallocMetadata* head;
    MallocMetadata* last;
public:
    List(): head(&dummy),last(&dummy){};
    void insertFree(MallocMetadata* to_update){
        MallocMetadata* temp=head;
        while (temp->next_free != nullptr && temp->next_free->size <= to_update->size)
        {
            if(temp->next_free->size==to_update->size && temp->next_free-to_update>0){
                break;
            }
            temp = temp->next_free;
        }
        if (temp->next_free != nullptr)
        {
            temp->next_free->prev_free = to_update;
        }
        to_update->next_free = temp->next_free;
        temp->next_free = to_update;
        to_update->prev_free = temp;

        num_of_free_blocks++;
        num_of_free_bytes += to_update->size;
    }
    void insert(MallocMetadata* to_update){
        MallocMetadata* temp=head;
        last->next = to_update;
        to_update->prev = last;
        to_update->next=nullptr;
        last=to_update;
        to_update->next_free=nullptr;
        to_update->prev_free=nullptr;
        num_of_allocs++;
        num_of_total_allocated_bytes+=to_update->size;


    }
    void removeFree(MallocMetadata* to_remove){
        // remove from freeList

        to_remove->prev_free->next_free = to_remove->next_free;
        if(to_remove->next_free!=nullptr){
            to_remove->next_free->prev_free=to_remove->prev_free;
        }
        to_remove->next_free = nullptr;
        to_remove->prev_free = nullptr;

        num_of_free_blocks--;
        num_of_free_bytes -= to_remove->size;
    }
    void remove(MallocMetadata* to_remove){
        if(to_remove->next==nullptr){
            last= to_remove->prev;
        }

        to_remove->prev->next = to_remove->next;
        if(to_remove->next!=nullptr){
            to_remove->next->prev=to_remove->prev;
        }

        to_remove->next= nullptr;
        to_remove->prev = nullptr;
        to_remove->next_free= nullptr;
        to_remove->prev_free= nullptr;
        num_of_allocs--;
        num_of_total_allocated_bytes-=to_remove->size;


    }
    void insertFrom(MallocMetadata* to_insert, MallocMetadata* from){
        MallocMetadata* tmp = from->next;
        to_insert->prev=from;
        to_insert->next = tmp;
        if(from->next!=nullptr){
            from->next->prev=to_insert;
        }else{
            last=to_insert;
        }
        from->next = to_insert;
        num_of_allocs++;
        num_of_total_allocated_bytes+=to_insert->size;

    }
    friend void* smalloc(size_t size);
    friend MallocMetadata* _merge(MallocMetadata* to_update);
};
List allocates_List =List();
mmap_List mmap_list = mmap_List();

void* _split(void* freeBlock,size_t size){
    MallocMetadata* to_update = (MallocMetadata*)((char*)freeBlock+size+sizeof(MallocMetadata));
    to_update->is_free=true;
    to_update->size = ((MallocMetadata*)freeBlock)->size - size-sizeof(MallocMetadata);
    if(((MallocMetadata*)freeBlock)->is_free){
        allocates_List.removeFree((MallocMetadata*)freeBlock);
    }
    allocates_List.insertFree(to_update);
    // update stats as usual;
    allocates_List.insertFrom((MallocMetadata*)to_update,(MallocMetadata*)freeBlock);

    int orig_size = ((MallocMetadata*)freeBlock)->size;
    ((MallocMetadata*)freeBlock)->size=size;
    ((MallocMetadata*)freeBlock)->is_free=false;
    num_of_total_allocated_bytes-=orig_size-size;
    return freeBlock;

}

void *smalloc(size_t size)
{
    if (size == 0 || size > 1e8)
    {
        return nullptr;
    }
    if(size%8!=0){
        size=size+(8-size%8);
    }
    if(size>1024*128){
        void* allocation = mmap(nullptr, size+sizeof(MallocMetadata),PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE,-1,0);
        if(allocation==MAP_FAILED){
            perror("mmap");
            exit(1);
        }
        MallocMetadata *new_allocation = (MallocMetadata *)allocation;
        new_allocation->size = size;
        new_allocation->is_free = false;
        new_allocation->next = nullptr;
        new_allocation->prev = nullptr;
        mmap_list.insert(new_allocation);
        num_of_allocs++;
        num_of_total_allocated_bytes+=size;

        //need to ask if when munmapping an area do we need to lower by 1 the num_of_allocs???
        return (MallocMetadata *)allocation + 1;
    }

    MallocMetadata* wilderness_block = allocates_List.last;
    //std::cout<<"sbrk "<< sbrk(0)<< std::endl;

    MallocMetadata *to_find = allocates_List.head;
    MallocMetadata *temp = allocates_List.head;
    while (to_find != nullptr)
    {
        if (size <= to_find->size)
        {
            int extraSpace = to_find->size-size-sizeof(MallocMetadata);
            if(extraSpace >=128 ){
                return _split(to_find,size);
            }
            allocates_List.removeFree(to_find);
            // need to add all the global stats

            to_find->is_free=false;
            return to_find + 1;
        }
        temp = to_find;
        to_find = to_find->next_free;
    }

    if(wilderness_block->is_free==true){
        allocates_List.removeFree(wilderness_block);
        int original_size=wilderness_block->size;
        sbrk(size-original_size);
        wilderness_block->size=size;
        wilderness_block->is_free=false;

        //expand the wilderness_block
        //should we increase the num_of_allocs in this case????????????PIAZAAAAAAAAAAA
        num_of_total_allocated_bytes+=size-original_size;
        return wilderness_block+1;
    }
    void *allocation = sbrk(sizeof(MallocMetadata) + size);
    if (*(int *)allocation == -1)
    {
        return nullptr;
    }

    MallocMetadata *new_allocation = (MallocMetadata *)allocation;
    new_allocation->size = size;
    new_allocation->is_free = false;
    new_allocation->next = nullptr;
    new_allocation->prev = nullptr;

    allocates_List.insert(new_allocation);
    return (MallocMetadata *)allocation + 1;
    // update stats as usual
}

MallocMetadata* _merge(MallocMetadata* to_update){
    MallocMetadata* left_merge=to_update->prev, *right_merge=to_update->next;
    MallocMetadata* head = allocates_List.head;

    if(left_merge && left_merge->is_free &&  right_merge && right_merge->is_free){
        int new_size = left_merge->size + right_merge->size+to_update->size + 2*sizeof(MallocMetadata);
        allocates_List.removeFree(left_merge);
        allocates_List.removeFree(right_merge);
        allocates_List.remove(right_merge);
        allocates_List.remove(to_update);
        num_of_total_allocated_bytes+=new_size-left_merge->size;
        left_merge->size=new_size;
        return left_merge;
    }
    else if (left_merge && left_merge->is_free){
        int new_size = left_merge->size + to_update->size + sizeof(MallocMetadata);
        allocates_List.removeFree(left_merge);
        allocates_List.remove(to_update);
        num_of_total_allocated_bytes+=new_size-left_merge->size;
        left_merge->size=new_size;
        return left_merge;
    }
    else if (right_merge && right_merge->is_free){
        int new_size = right_merge->size + to_update->size + sizeof(MallocMetadata);
        allocates_List.removeFree(right_merge);
        allocates_List.remove(right_merge);
        num_of_total_allocated_bytes+=new_size-to_update->size;

        to_update->size=new_size;
        return to_update;
    }
        return to_update;
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
    if(to_update->size>=1024*128){
        mmap_list.remove(to_update);
        num_of_allocs--;//Check PIAZZAAAAAA
        num_of_total_allocated_bytes-=to_update->size;

        if(munmap(to_update,to_update->size+sizeof(MallocMetadata))<0){
            exit(1);
        }

        return;
    }
    MallocMetadata* tmp=to_update;
    to_update = _merge(to_update);
    to_update->is_free = true;
    allocates_List.insertFree(to_update);

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
    void* allocation=nullptr;
    //case a.
    MallocMetadata *to_update = (MallocMetadata *)oldp - 1;
    int bytes_to_copy = to_update->size;
    if (to_update->size >= size)
    {    int extraSpace =to_update->size-size-sizeof(MallocMetadata);
        if(extraSpace >=128 ){
            allocation = (MallocMetadata*)_split(to_update,size);
            allocates_List.removeFree(((MallocMetadata*)allocation)->next);
            to_update= _merge(((MallocMetadata*)allocation)->next);
            allocates_List.insertFree(to_update);
            return (MallocMetadata*)allocation+1;

        }
        return oldp;
    }
    //case b- lower address neighbor
    MallocMetadata* left_neighbor = to_update->prev;
    MallocMetadata* right_neighbor = to_update->next;
    if(left_neighbor && left_neighbor->is_free){
        if(left_neighbor->size+to_update->size+sizeof(MallocMetadata)>=size){
            //Merge left with to_update
            allocates_List.removeFree(left_neighbor);
            int orig_size = left_neighbor->size;
            left_neighbor->size = to_update->size + left_neighbor->size + sizeof(MallocMetadata);
            left_neighbor->is_free = false;
            allocates_List.remove(to_update);
            allocation = (MallocMetadata *) left_neighbor + 1;
            num_of_total_allocated_bytes+=left_neighbor->size-orig_size;
            if(left_neighbor->size-size>=128){
                allocation = (MallocMetadata*)(_split(left_neighbor,size));
                allocates_List.removeFree(((MallocMetadata*)allocation)->next);
                to_update= _merge(((MallocMetadata*)allocation)->next);
                allocates_List.insertFree(to_update);
                return (MallocMetadata*)allocation+1;

            }
            memmove(allocation, oldp, bytes_to_copy);
            return allocation;
        }
        else if(to_update->next==nullptr){
            //Merge left with to_update
            //enlarge(if needed) after merge(sbrk)
            allocates_List.removeFree(left_neighbor);
            int orig_size=left_neighbor->size;
            left_neighbor->size = to_update->size + left_neighbor->size + sizeof(MallocMetadata);
            left_neighbor->is_free = false;
            allocates_List.remove(to_update);
            sbrk(size-left_neighbor->size);
            left_neighbor->size=size;
            allocation = (MallocMetadata *) left_neighbor + 1;
            num_of_total_allocated_bytes+=left_neighbor->size-orig_size;
            memmove(allocation, oldp, bytes_to_copy);
            return allocation;
        }
    }
    if(to_update->next==nullptr){
        //Enlarge
        int orig_size=to_update->size;
        sbrk(size-to_update->size);
        to_update->size=size;
        allocation = (MallocMetadata *) to_update + 1;
        num_of_total_allocated_bytes+=to_update->size-orig_size;
        memmove(allocation, oldp, bytes_to_copy);
        return allocation;
    }
    if(right_neighbor && right_neighbor->is_free){
        if(right_neighbor->size+to_update->size+sizeof(MallocMetadata)>=size){
            //Merge with right neighbor
            int orig_size = right_neighbor->size;
            to_update->size = to_update->size + right_neighbor->size + sizeof(MallocMetadata);
            to_update->is_free = false;
            allocates_List.removeFree(right_neighbor);
            allocates_List.remove(right_neighbor);
            allocation = (MallocMetadata *) to_update + 1;
            num_of_total_allocated_bytes+=to_update->size-orig_size;
            if(to_update->size-size>=128){
                allocation = (MallocMetadata*)(_split(to_update,size));
                allocates_List.removeFree(((MallocMetadata*)allocation)->next);
                to_update= _merge(((MallocMetadata*)allocation)->next);
                allocates_List.insertFree(to_update);
                return (MallocMetadata*)allocation+1;
            }
            memmove(allocation, oldp, bytes_to_copy);
            return allocation;

        }
    }
    if(left_neighbor && left_neighbor->is_free && right_neighbor && right_neighbor->is_free){
        if(left_neighbor->size+right_neighbor->size+to_update->size+2*sizeof(MallocMetadata)>=size){
            //Merge all 3
            int orig_size = left_neighbor->size;
            allocates_List.removeFree(left_neighbor);

            left_neighbor->size = to_update->size + left_neighbor->size + right_neighbor->size+ 2*sizeof(MallocMetadata);
            left_neighbor->is_free = false;
            right_neighbor->is_free=false;
            allocates_List.remove(to_update);
            allocates_List.removeFree(right_neighbor);
            allocates_List.remove(right_neighbor);
            allocation = (MallocMetadata *) left_neighbor + 1;
            num_of_total_allocated_bytes+=left_neighbor->size-orig_size;
            if(left_neighbor->size-size>=128){
                allocation = (MallocMetadata*)(_split(left_neighbor,size));
                allocates_List.removeFree(((MallocMetadata*)allocation)->next);
                to_update= _merge(((MallocMetadata*)allocation)->next);
                allocates_List.insertFree(to_update);
                return (MallocMetadata*)allocation+1;
            }
            memmove(allocation, oldp, bytes_to_copy);
            return allocation;

        }
        else if(right_neighbor->next==nullptr){
            //Merge all 3
            //Enlarge wilderness
            int orig_size = left_neighbor->size;
            allocates_List.removeFree(left_neighbor);

            left_neighbor->size = to_update->size + left_neighbor->size + right_neighbor->size+ 2*sizeof(MallocMetadata);
            left_neighbor->is_free = false;
            right_neighbor->is_free=false;
            allocates_List.remove(to_update);
            allocates_List.removeFree(right_neighbor);
            allocates_List.remove(right_neighbor);
            sbrk(size-left_neighbor->size);
            left_neighbor->size=size;
            allocation = (MallocMetadata *) left_neighbor + 1;
            num_of_total_allocated_bytes+=left_neighbor->size-orig_size;
            memmove(allocation, oldp, bytes_to_copy);
            return allocation;

        }
    }
    if(right_neighbor && right_neighbor->is_free && right_neighbor->next==nullptr){
        //Merge with right
        //Enlarge
        int orig_size = to_update->size;
        to_update->size = to_update->size + right_neighbor->size + sizeof(MallocMetadata);
        to_update->is_free = false;
        allocates_List.removeFree(right_neighbor);
        allocates_List.remove(right_neighbor);
        sbrk(size-to_update->size);
        to_update->size=size;
        allocation = (MallocMetadata *) to_update + 1;
        num_of_total_allocated_bytes+=to_update->size-orig_size;
        memmove(allocation, oldp, bytes_to_copy);
        return allocation;

    }

    else {
        allocation = smalloc(size);
        if (allocation == NULL) {
            return NULL;
        }
        memmove(allocation, oldp, bytes_to_copy);
        sfree(oldp);
        return allocation;
    }

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