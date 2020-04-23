// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "spinlock.h"
#include "rand.h"

int get_freelist_size();
struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int alloc_history[1000];
int hislen = 0;
extern char end[]; // first address after kernel loaded from ELF file

// Initialize free list of physical pages.
void
kinit(void)
{
  char *p;

  initlock(&kmem.lock, "kmem");
  p = (char*)PGROUNDUP((uint)end);
  for(; p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE)
    kfree(p);
}

void remove_from_history(int pgno) {
  int i, j;
  for(i=0, j=0; i<hislen; i++) {
    if(alloc_history[i] != pgno) {
      alloc_history[j++] = alloc_history[i];
    }
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || (uint)v >= PHYSTOP) 
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  remove_from_history((int)r);
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  
  r = kmem.freelist;
  int ind = 0;
  
  int size = get_freelist_size();
  if(size == 0)
    return (char*)r;
  int x = xv6_rand();
  int y = x % size;
 
  struct run *prev, *temp;
  prev = kmem.freelist;
  temp = kmem.freelist;

  while (ind != y) {
	  prev = temp;
  	temp = temp->next;
	  ind++;
  }

  r = temp;
  if (ind != 0) {
    if (prev && temp) {
        prev->next = temp->next;
    }
  }

  if(r) {
    if (ind == 0) {// r is at the head
    	kmem.freelist = r->next;
    }
    alloc_history[hislen++] = (int)r;
  }
  release(&kmem.lock);
  return (char*)r;
}

int get_freelist_size() {
	struct run *temp;
	int size = 0;
	temp = kmem.freelist;
	while (temp) {
		temp = temp->next;
		size++;
	}
	return size;
}

int
dump_allocated(int *arr, int n)
{
  if(n > hislen) {
    //Dont have enough history
    return -1;
  }
  int i = hislen-1, j = 0;
  while(j < n) {
    arr[j++] = alloc_history[i--];
  }
  return 0;
}
