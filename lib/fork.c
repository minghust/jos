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
	addr = ROUNDDOWN(addr, PGSIZE);
	pte_t pte = uvpt[(uint32_t)addr / PGSIZE];

	if(!(err & FEC_WR) || !(pte & PTE_COW))
		panic("not a write or cow page, parent pgfault error!");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	uint32_t perm = PTE_W | PTE_U | PTE_P;
	if(sys_page_alloc(0, PFTEMP, perm) < 0)
		panic("allocate a new page error!"); // allocate a new page

	memcpy((void *)PFTEMP, addr, PGSIZE); // copy the data from the old page to the new page

	if(sys_page_map(0, (void *)PFTEMP, 0, addr, perm) < 0)
		panic("move the new page to the old page's addr error!");

	if(sys_page_unmap(0, PFTEMP) < 0)
		panic("unmap the temp page error!");
}

//
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
	int r;

	// LAB 4: Your code here.
	pte_t pte = uvpt[pn];
	void * addr = (void *)(pn * PGSIZE);
	if(pte & PTE_COW || pte & PTE_W)
	{
		// first map the parent addr to child addr
		// then map the parent addr to its addr
		r = sys_page_map(0, addr, envid, addr, PTE_COW | PTE_U | PTE_P);
		if(!r && !(pte & PTE_COW))
			r = sys_page_map(0, addr, 0, addr, PTE_COW | PTE_U | PTE_P);
	}
	else
	{
		r = sys_page_map(0, addr, envid, addr, PTE_U | PTE_P);
	}
	return r;
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
	// LAB 4: Your code here.
	// panic("fork not implemented");
	set_pgfault_handler(pgfault);
	envid_t childid = sys_exofork();
	if(childid < 0)
	{
		panic("fork error!");
	}
	else if(childid == 0) // in child env
	{
		thisenv = &envs[ENVX(sys_getenvid())];
	}

	// in parent env
	unsigned pn = 0;
	for(; pn < USTACKTOP / PGSIZE; pn++)
	{
		if((uvpd[PDX(pn*PGSIZE)] & PTE_P) && (uvpt[pn] & PTE_P))
			duppage(childid, pn);
	}

	if(sys_page_alloc(childid, (void *)(UXSTACKTOP-PGSIZE), PTE_W | PTE_U | PTE_P) < 0){
		panic("allocate exception stack for child error!");
	}
	
	if(sys_env_set_pgfault_upcall(childid, thisenv->env_pgfault_upcall) < 0){
		panic("child set pgfault upcall error!");
	}	

	if(sys_env_set_status(childid, ENV_RUNNABLE) < 0){
		panic("set child runnable error!");
	}

	return childid;

}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
