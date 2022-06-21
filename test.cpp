#include <unistd.h>
#include <stdio.h>
void* smalloc(size_t size);
void sfree(void* p);
int main(){
    int* arr =(int*)smalloc(8);
    arr[0]=5;
    arr[1]=4;
    printf("arr: %d, %d\n",arr[0],arr[1]);
    //sfree(arr);

    int* arr2 = (int*)smalloc(8);
    arr2[0]=6;
    arr2[1]=7;

    int* arr3 = (int*)smalloc(8);
    sfree(arr);
    sfree(arr3);
    sfree(arr2);
    return 0;

}