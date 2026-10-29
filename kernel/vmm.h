#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PTE_PRESENT  1ULL
#define PTE_WRITE    2ULL
#define PTE_USER     4ULL
#define PTE_HUGE     0x80ULL
#define PTE_ADDR     0x000FFFFFFFFFF000ULL

typedef uint64_t *addrspace_t;  /* PML4 */

void        vmm_init(void);
addrspace_t vmm_kernel_as(void);
addrspace_t vmm_create_as(void);          /* копия kernel maps + пустое user */
void        vmm_destroy_as(addrspace_t as);
int         vmm_map_page(addrspace_t as, uint64_t virt, uint64_t phys, uint64_t flags);
int         vmm_map_range(addrspace_t as, uint64_t virt, uint64_t phys,
                          size_t pages, uint64_t flags);
void        vmm_switch(addrspace_t as);
addrspace_t vmm_current(void);

#ifdef __cplusplus
}
#endif

#endif
