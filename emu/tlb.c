#include "emu/cpu.h"
#include "emu/tlb.h"

struct tlb *tlb_new(struct mem *mem) {
    struct tlb *tlb = malloc(TLB_SIZE * sizeof(struct tlb_entry));
    tlb->mem = mem;
    tlb_flush(tlb);
    return tlb;
}

void tlb_flush(struct tlb *tlb) {
    for (unsigned i = 0; i < TLB_SIZE; i++)
        tlb->entries[i] = (struct tlb_entry) {.page = 1, .page_if_writable = 1};
}

void tlb_free(struct tlb *tlb) {
    free(tlb);
}

bool __tlb_read_cross_page(struct tlb *tlb, addr_t addr, char *value, unsigned size) {
    char *ptr1 = __tlb_read_ptr(tlb, addr);
    char *ptr2 = __tlb_read_ptr(tlb, (PAGE(addr) + 1) << PAGE_BITS);
    if (ptr1 == NULL || ptr2 == NULL)
        return false;
    size_t part1 = PAGE_SIZE - OFFSET(addr);
    assert(part1 < size);
    memcpy(value, ptr1, part1);
    memcpy(value + part1, ptr2, size - part1);
    return true;
}

bool __tlb_write_cross_page(struct tlb *tlb, addr_t addr, const char *value, unsigned size) {
    char *ptr1 = __tlb_write_ptr(tlb, addr);
    char *ptr2 = __tlb_write_ptr(tlb, (PAGE(addr) + 1) << PAGE_BITS);
    if (ptr1 == NULL || ptr2 == NULL)
        return false;
    size_t part1 = PAGE_SIZE - OFFSET(addr);
    assert(part1 < size);
    memcpy(ptr1, value, part1);
    memcpy(ptr2, value + part1, size - part1);
    return true;
}

void *tlb_handle_miss(struct tlb *tlb, addr_t addr, int type) {
    char *ptr = mem_ptr(tlb->mem, addr & 0xfffff000, type);
    if (ptr == NULL)
        return NULL;

    struct tlb_entry *tlb_ent = &tlb->entries[TLB_INDEX(addr)];
    tlb_ent->page = TLB_PAGE(addr);
    if (type == MEM_WRITE)
        tlb_ent->page_if_writable = tlb_ent->page;
    else
        // 1 is not a valid page so this won't look like a hit
        tlb_ent->page_if_writable = TLB_PAGE_EMPTY;
    tlb_ent->data_minus_addr = (uintptr_t) ptr - TLB_PAGE(addr);
    return (void *) (tlb_ent->data_minus_addr + addr);
}
