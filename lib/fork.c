// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
    // Add by Zhou
    if (!((err & FEC_WR) && (uvpd[PDX(addr)] & PTE_P) \
        && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_COW)))
        panic("page cow check failed!\n");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
    // Add by Zhou
    addr = ROUNDDOWN(addr, PGSIZE);
    envid_t envid = sys_getenvid();

    if ((r = sys_page_alloc(envid, PFTEMP, PTE_P | PTE_W | PTE_U)))
        panic("sys_page_alloc failed!\n");

    memmove(PFTEMP, addr, PGSIZE);

    if ((r = sys_page_map(envid, PFTEMP, envid, addr, PTE_P | PTE_U | PTE_W)))
        panic("sys_page_map failed!\n");

    if ((r = sys_page_unmap(envid, PFTEMP)))
        panic("sys_page_unmap failed!\n");
}

//failed
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r = 0;

	// LAB 4: Your code here.
    // Add by Zhou
    envid_t myenvid = sys_getenvid();
    void *addr = (void *)(pn * PGSIZE);
    pte_t pte = uvpt[pn];
    int perm = PTE_U | PTE_P;

    if ((pte & PTE_W) || (pte & PTE_COW))
        perm |= PTE_COW;

    // map to envid VA
    if ((r = sys_page_map(myenvid, addr, envid, addr, perm)))
        return r;

    if (perm & PTE_COW) {
    
        if ((r = sys_page_map(myenvid, addr, myenvid, addr, perm)))
            return r;
    }

    return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
#if 1
	// LAB 4: Your code here.
    // Add by Zhou 
    envid_t envid;

    set_pgfault_handler(pgfault);

    envid = sys_exofork();

    if (envid < 0) {

        panic("sys_exofork failed!\n");
    }else if (0 == envid) {
    
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }

    uint8_t *addr = NULL;
    extern unsigned char end[];
    cprintf("end: 0x%08x, UTOP: 0x%08x\n", end, UTOP);
    int page_num = 0;
    for (addr = (uint8_t *)UTEXT; addr < (uint8_t *)end; addr += PGSIZE) {

        if (uvpd[PDX(addr)] & PTE_P)
            page_num++;
    
        if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) \
            && (uvpt[PGNUM(addr)] & PTE_U)) {
        
            duppage(envid, PGNUM(addr));
        }
    }
    cprintf("page_num: %d\n", page_num);
    duppage(envid, PGNUM(ROUNDDOWN(&addr, PGSIZE)));

    if (sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W))
        panic("sys_page_alloc for child exception failed!\n");

    extern void _pgfault_upcall();
    sys_env_set_pgfault_upcall(envid, _pgfault_upcall);

    if (sys_env_set_status(envid, ENV_RUNNABLE))
        panic("fork set child env status failed!\n");

    return envid;
#else
    extern void _pgfault_upcall(void);
    envid_t myenvid = sys_getenvid();
    envid_t envid;
    uint32_t i, j, pn;

    // set page fault handler
    set_pgfault_handler(pgfault);

    // create a child
    if ((envid = sys_exofork()) < 0) {
    
        return -1;
    }

    // child
    if (envid == 0) {
    
        thisenv = &envs[ENVX(sys_getenvid())];
        
        return envid;
    }

    // copy address space to child
    int page_num = 0;
    for (i = PDX(UTEXT); i < PDX(UXSTACKTOP); i++) {
    
        if (uvpd[i] & PTE_P) {
        
            for (j = 0; j < NPTENTRIES; j++) {
                page_num++;
                pn = PGNUM(PGADDR(i, j, 0));
                if (pn == PGNUM(UXSTACKTOP - PGSIZE)) {
                
                    break;
                }

                if (uvpt[pn] & PTE_P) {
                
                    duppage(envid, pn);
                }
            }
        }
    }
    cprintf("page_num: %d\n", page_num);

    if (sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_P | PTE_W) < 0) {
    
        return -1;
    }

    if (sys_page_map(envid, (void *)(UXSTACKTOP - PGSIZE), myenvid, PFTEMP, PTE_U | PTE_P | PTE_W) < 0) {
    
        return -1;
    }

    memmove((void *)(UXSTACKTOP - PGSIZE), PFTEMP, PGSIZE);

    if (sys_page_unmap(myenvid, PFTEMP) < 0) {
    
        return -1;
    }

    if (sys_env_set_pgfault_upcall(envid, _pgfault_upcall) < 0) {
    
        return -1;
    }

    if (sys_env_set_status(envid, ENV_RUNNABLE) < 0) {
    
        return -1;
    }

    return envid;

#endif
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
