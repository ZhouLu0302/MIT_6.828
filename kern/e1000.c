#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>

// LAB 6: Your driver code here
// Add by Zhou

volatile void *bar_va;
#define E1000_REG(offset) (void *)(bar_va + offset)

/* Tx */
struct e1000_tdh *tdh;
struct e1000_tdt *tdt;
struct e1000_tx_desc tx_desc[TX_DESC_NUM];
char tx_buf_array[TX_DESC_NUM][TX_PKT_SIZE];


static void e1000_transmit_init(void);

int e1000_attachfn(struct pci_func *pcif)
{
    pci_func_enable(pcif);
    cprintf("e1000: bar0 %x, size0 %x\n", pcif->reg_base[0], pcif->reg_size[0]);
    
    bar_va = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

    uint32_t *status_reg = (uint32_t *)E1000_REG(E1000_STATUS);
    assert(*status_reg == 0x80080783);

    e1000_transmit_init();

    return 0;
}

static void e1000_transmit_init(void)
{
    int i = 0;

    for (i = 0; i < TX_DESC_NUM; i++) {
    
        tx_desc[i].addr = PADDR(tx_buf_array[i]);
        tx_desc[i].cmd = 0;
        tx_desc[i].status |= E1000_TXD_STAT_DD;
    }

    struct e1000_tdlen *tdlen = (struct e1000_tdlen *)E1000_REG(E1000_TDLEN);
    tdlen->len = TX_DESC_NUM;

    uint32_t *tdbal = (uint32_t *)E1000_REG(E1000_TDBAL);
    *tdbal = PADDR(tx_desc);

    uint32_t *tdbah = (uint32_t *)E1000_REG(E1000_TDBAH);
    *tdbah = 0;

    tdh = (struct e1000_tdh *)E1000_REG(E1000_TDH);
    tdh->tdh = 0;

    tdt = (struct e1000_tdt *)E1000_REG(E1000_TDT);
    tdt->tdt = 0;

    struct e1000_tctl *tctl = (struct e1000_tctl *)E1000_REG(E1000_TCTL);
    tctl->en =   1;
    tctl->psp =  1;
    tctl->ct = 0x10;
    tctl->cold = 0x40;

    struct e1000_tipg *tipg = (struct e1000_tipg *)E1000_REG(E1000_TIPG);
    tipg->ipgt = 10;
    tipg->ipgr1 = 4;
    tipg->ipgr2 = 6;
}

int e1000_transmit(void *data, size_t len)
{
    uint32_t current = tdt->tdt;

    if (!(tx_desc[current].status & E1000_TXD_STAT_DD)) {
    
        return -E_TRANSMIT_RETRY;
    }

    tx_desc[current].length = len;
    tx_desc[current].status &= ~E1000_TXD_STAT_DD;
    tx_desc[current].cmd |= (E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS);
    memcpy(tx_buf_array[current], data, len);
    uint32_t next = (current + 1) % TX_DESC_NUM;
    tdt->tdt = next;

    return 0;
}
