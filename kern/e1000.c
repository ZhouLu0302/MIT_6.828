#include <kern/e1000.h>
#include <kern/pmap.h>

// LAB 6: Your driver code here
// Add by Zhou

volatile uint32_t *bar_va;

int pci_e1000_attach(struct pci_func *pcif)
{
    pci_func_enable(pcif);
    bar_va = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

    cprintf("e1000: bar0 %x, size0 %x\n", pcif->reg_base[0], pcif->reg_size[0]);
    cprintf("e1000: status %x\n", bar_va[2]);

    return 1;
}
