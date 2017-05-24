#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()
struct segdesc gdt[NSEGS];

#ifndef NONE
  uint selectpage(struct pageselect* ps);
  void addpage(struct pageselect* ps, uint va);
  void removepage(struct pageselect* ps, uint va);
  void copyps(struct pageselect* pps, struct pageselect* cps);
  int initSwapFile(struct proc* p);
  int addToSwapMeta(struct proc* p, uint page);
  int removeFromSwapMeta(struct proc* p, uint page);
  int writePageToSwapFile(struct proc* p, uint page);
  int readPageFromSwapFile(struct proc* p, uint page);
  uint swapin(uint va);
  void swapout(uint va);
#endif

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpunum()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);

  // Map cpu, and curproc
  c->gdt[SEG_KCPU] = SEG(STA_W, &c->cpu, 8, 0);

  lgdt(c->gdt, sizeof(c->gdt));
  loadgs(SEG_KCPU << 3);

  // Initialize cpu-local storage.
  cpu = c;
  proc = 0;
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)p2v(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = v2p(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (p2v(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0)
      return 0;
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(v2p(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  pushcli();
  cpu->gdt[SEG_TSS] = SEG16(STS_T32A, &cpu->ts, sizeof(cpu->ts)-1, 0);
  cpu->gdt[SEG_TSS].s = 0;
  cpu->ts.ss0 = SEG_KDATA << 3;
  cpu->ts.esp0 = (uint)proc->kstack + KSTACKSIZE;
  ltr(SEG_TSS << 3);
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");
  lcr3(v2p(p->pgdir));  // switch to new address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, v2p(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, p2v(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(struct proc* p, pde_t* pgdir,uint oldsz, uint newsz)
{
  char *mem;
  uint a;
  #ifdef DEBUG
    int pid = proc->pid;
    if(p) pid=p->pid;
		cprintf("@in allocuvm: oldsz: %d, newsz: %d, pid: %d\n",oldsz,newsz,pid);
	#endif
  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;
  #ifndef NONE
  if(p && newsz > MAX_TOTAL_PAGES*PGSIZE){ // process trying to extend beyond MAX_TOTAL_PAGES.
    #ifdef DEBUG
      cprintf("trying to allocate more then MAX_TOTAL_PAGES for pid: %d",pid);
    #endif
    return 0;
  }
  #endif

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    // cprintf("&&&&&&&&  allocuvm: a = %d \n",a);
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(p, pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    mappages(pgdir, (char*)a, PGSIZE, v2p(mem), PTE_W|PTE_U);
    #ifndef NONE
      if(p)
        addpage(&p->ps, a);
    #endif
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(struct proc* p, pte_t* pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;
  #ifdef DEBUG
    int pid = proc->pid;
    if(p) pid=p->pid;
    cprintf("@in deallocuvm: oldsz %d, newsz %d, pid %d\n",pid,oldsz,newsz);
	#endif
  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a += (NPTENTRIES - 1) * PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = p2v(pa);
      kfree(v);
      #ifndef NONE
        if(p)
          removepage(&p->ps, a);
      #endif
      *pte = 0;
    }
    #ifndef NONE
      else if(p && *pte & PTE_PG){
        *pte = 0;
        removeFromSwapMeta(p,a);
      }
    #endif
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(struct proc* p, pte_t* pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(p, pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = p2v(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  #ifndef NONE
    if(p){
      clearps(&p->ps);
      clearswapmeta(&p->sm);
    }
  #endif
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(struct proc* p, pte_t* pgdir, char *uva)
{
  pte_t *pte;
  #ifdef DEBUG
    int pid = proc->pid;
    if(p) pid=p->pid;
    cprintf("@in clearpteu: uva %d:%d pid: %d\n",(uint)uva/PGSIZE,(uint)uva,pid);
  #endif
  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  #ifndef NONE
    if(p){
      if(!(*pte & PTE_P) && *pte & PTE_PG)
        swapin((uint)uva);
      removepage(&p->ps,(uint)uva);
    }
  #endif
  *pte &= ~PTE_U;

}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(struct proc* parent, struct proc* child)
{
  uint sz = parent->sz;
  pte_t *ppte;
  #ifndef NONE
    pte_t *cpte;
  #endif
  uint pa, i, flags;
  char *mem;

  #ifdef DEBUG
	   cprintf("@in copyuvm: parent: %d, child %d\n",parent->pid,child->pid);
  #endif

  if((child->pgdir = setupkvm()) == 0)
    return 0;
  // copy physical memory
  for(i = 0; i < sz; i += PGSIZE){
    if((ppte = walkpgdir(parent->pgdir, (void *) i, 0)) == 0)
      panic("in copyuvm: parent pte should exist");
    pa = PTE_ADDR(*ppte);
    flags = PTE_FLAGS(*ppte);
    if(*ppte & PTE_P){
      #ifdef DEBUG
  		  cprintf("@in copyuvm: copying page %d:%d from parent %d pysical memory to child %d pysical memory\n",i/PGSIZE,i,parent->pid,child->pid);
  		#endif
      if((mem = kalloc()) == 0)
        goto bad;
      memmove(mem, (char*)p2v(pa), PGSIZE);
      if(mappages(child->pgdir, (void*)i, PGSIZE, v2p(mem), flags) < 0)
        goto bad;
    }
    else if(!(*ppte & PTE_PG)){
      panic("in copyuvm: page not present");
    }
  }

  #ifndef NONE
    // copy selection data
    if(parent->pid > 2)
      copyps(&parent->ps,&child->ps);
    // copy swapFile and swap metadata;
    if(parent->pid > 2 && parent->swapFile){
      createSwapFile(child);
      for(int i = 0 ; i< MAX_PSYC_PAGES; i++){
        int va = parent->sm.swapmeta[i];
        if(va >= 0){
          #ifdef DEBUG
            cprintf("@in copyuvm: copying page %d:%d from parent %d swapFile to child %d swapFile\n",va/PGSIZE,va,parent->pid,child->pid);
          #endif
          if((mem = kalloc()) == 0)
            goto bad;
          if(readFromSwapFile(parent, mem, va*PGSIZE, PGSIZE) < 0)
            goto bad;
          if(writeToSwapFile(child, mem, i*PGSIZE, PGSIZE) < 0)
            goto bad;
          kfree(mem);
          // copy parent pte to child page dir
          if((ppte = walkpgdir(parent->pgdir, (void*)va, 0)) == 0)
            panic("in copyuvm: parent pte should exist");
          if((cpte = walkpgdir(child->pgdir, (void*)va, 1)) == 0)
            goto bad;
          if(*cpte & PTE_P)
            panic("remap");
          *cpte = *ppte;
        }
        child->sm.swapmeta[i] = parent->sm.swapmeta[i];
      }
      child->sm.count = parent->sm.count;
    }
  #endif

  return child->pgdir;

bad:
  freevm(child, child->pgdir);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)p2v(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

#ifndef NONE

  int
  initSwapFile(struct proc* p)
  {
    if(p->swapFile)
      return 0;
    for(int i = 0; i < MAX_PSYC_PAGES; i++)
      p->sm.swapmeta[i] = -1;
    p->sm.count=0;
    return createSwapFile(p);
  }


  int
  addToSwapMeta(struct proc* p, uint page){
    for(int i = 0 ; i< MAX_PSYC_PAGES; i++){
      if(p->sm.swapmeta[i] == -1){
        p->sm.swapmeta[i] = page;
        p->sm.count++;
        return i;
      }
    }
    return -1;
  }

  int
  removeFromSwapMeta(struct proc* p, uint page){
    for(int i = 0 ; i< MAX_PSYC_PAGES; i++){
      if(p->sm.swapmeta[i] == page){
        p->sm.swapmeta[i] = -1;
        p->sm.count--;
        return i;
      }
    }
    return -1;
  }

  int
  writePageToSwapFile(struct proc* p, uint page)
  {
    int i = addToSwapMeta(p,page);
    if(i < 0)
      return -1;
    return writeToSwapFile(p, (char*)page, i*PGSIZE, PGSIZE);
  }

  int
  readPageFromSwapFile(struct proc* p, uint page)
  {
    int i = removeFromSwapMeta(p,page);
    if(i<0)
      return -1;
    return readFromSwapFile(p, (char*)page, i*PGSIZE, PGSIZE);
  }

  uint
  swapin(uint va)
  {
    int flags;
    char* mem;
    pte_t* pte;

    va = PGROUNDDOWN(va);
    #ifdef DEBUG
			cprintf("@in swapin: va: %d:%d pid: %d\n",va/PGSIZE,va,proc->pid);
	   #endif
    if(va > proc->sz)
      panic("SWAPIN FAILED! trying to swapin none private page");
    if((pte = walkpgdir(proc->pgdir, (char*)va, 0)) == 0)
      panic("SWAPIN FAILED! trying to swapin a none exsiting page");
    if(*pte & PTE_P)
      panic("SWAPIN FAILED! trying to swapin a page already present");
    if(!(*pte & PTE_U))
      panic("SWAPIN FAILED! trying to swapin a none user page");
    if((mem = kalloc()) == 0)
      panic("SWAPIN FAILED! out of memory\n"); // if we  are changing panics later to cprintf  we need to remove page firstt.
    memset(mem, 0, PGSIZE);
    if(!proc->swapFile)
      panic("SWAPIN FAILED! process has no swapFile\n");
    flags = ((PTE_FLAGS(*pte) | PTE_P) & ~PTE_PG);
    *pte = v2p(mem) | flags;
    if(readPageFromSwapFile(proc, va) < 0)
      panic("SWAPIN FAILED! failed to read from swapFile\n");
    addpage(&proc->ps,va); //record that this page is being added to pysical memory
    return va;
  }


  void
  swapout(uint va)
  {
    uint flags;
    uint pa;
    char* mem;
    pte_t* pte;

    va = PGROUNDDOWN(va);
    #ifdef DEBUG
			cprintf("@in swapout: va: %d:%d pid: %d\n",va/PGSIZE,va,proc->pid);
	  #endif
    // if((uint)va > proc->sz)
    //   panic("SWAPOUT FAILED! trying to swapout none private page");
    if((pte = walkpgdir(proc->pgdir, (char*)va, 0)) == 0)
      panic("SWAPOUT FAILED! trying to swapout a none exsiting page");
    if(!(*pte & PTE_P))
      panic("SWAPOUT FAILED! trying to swapout a page none present page");
    if(!(*pte & PTE_U))
      panic("SWAPOUT FAILED! trying to swapout a none user page");
    pa = PTE_ADDR(*pte);
    if(pa == 0)
      panic("SWAPOUT FAILED! kfree");
    mem = p2v(pa);
    initSwapFile(proc);
    if(writePageToSwapFile(proc, va) < 0)
      panic("SWAPOUT FAILED! failed to write to swapFile\n");
    kfree(mem);
    //removepage(&proc->ps,va); //remove this page from pysical memory record
    flags = ((PTE_FLAGS(*pte) | PTE_PG) & ~PTE_P);
    *pte = pa | flags;
    lcr3(v2p(proc->pgdir)); // refresh TLB
    proc->pageoutCounter++; //counting number of swapouts taking place for task3
  }

  void clearswapmeta(struct swapMetaData* sm){
    for(int i=0; i< MAX_PSYC_PAGES; i++)
      sm->swapmeta[i] = -1;
    sm->count = 0;
  }


  int
  handle_pgflt(uint va)
  {
    #ifdef DEBUG
			cprintf("@in handle_pgflt va: %d:%d pid: %d\n",va/PGSIZE,va,proc->pid);
	  #endif
    proc->pagefaultsCounter++; // counting pagefaults for task3
    pte_t *pte = walkpgdir((pde_t*)proc->pgdir,(char*)va,0);
    if(!pte){
      cprintf("@in handle_pgflt pte = 0\n");
      return -1;
    }
    else if(*pte & PTE_P){
      cprintf("@in handle_pgflt page is present\n");
      return -1;
    }
    else if(!(*pte & PTE_PG)){
      cprintf("@in handle_pgflt page is not swaped out\n");
      return -1;
    }
    else if(!(*pte & PTE_U)){
      cprintf("@in handle_pgflt page in not allowed for user access\n");
      return -1;
    }

    #ifdef DEBUG
			cprintf("@in handle_pgflt swaping in va: %d:%d pid %d\n",va/PGSIZE,va,proc->pid);
	  #endif
    return (int)swapin(va); // swap it back in
  }

#endif


#ifdef LIFO

  uint
  selectpage(struct pageselect* ps)
  {
    if(!ps->top)
      panic("(LIFO) trying to select a page while the process has none.");
    ps->top--;
    uint va = ps->stack[ps->top];
    ps->stack[ps->top] = -1;
    #ifdef DEBUG
		  cprintf("@in selectpage: va: %d:%d selected. top: %d,  pid: %d\n",va/PGSIZE,va,ps->top,proc->pid);
	  #endif
    return va;
  }

  void
  addpage(struct pageselect* ps, uint va)
  {
    va = PGROUNDDOWN(va);
    #ifdef DEBUG
			cprintf("@in addpage: va %d:%d adding. top: %d, pid: %d\n",va/PGSIZE,va,ps->top,proc->pid);
	  #endif
    if(ps->top == MAX_PSYC_PAGES){
      #ifdef DEBUG
			   cprintf("@in addpage: va: %d:%d no room! swaping out another page. top: %d, pid: %d\n",va/PGSIZE,va,ps->top,proc->pid);
	    #endif
      swapout(selectpage(ps));
    }
    ps->stack[ps->top] = va;
    ps->top++;
    #ifdef DEBUG
      cprintf("@in addpage: va: %d:%d added! top %d, pid: %d\n",va/PGSIZE,va,ps->top,proc->pid);
    #endif
  }

  void
  removepage(struct pageselect* ps, uint va)
  {
    #ifdef DEBUG
  		cprintf("@in removepage: va: %d:%d removing. top: %d, pid: %d\n",va/PGSIZE,va,ps->top,proc->pid);
  	#endif
    va = PGROUNDDOWN(va);
    if(!ps->top){
      #ifdef DEBUG
        cprintf("@in removepage: va: %d:%d ps is empty! top: %d, pid: %d\n",va/PGSIZE,va,ps->top,proc->pid);
      #endif
      return;
    }
    int page = 0;
    for(; page <= ps->top; page++){
      if(ps->stack[page] == va)
        goto rearage;
    }
    return;
    rearage:
    for(;page < ps->top; page++)
      ps->stack[page] = ps->stack[page+1];
    ps->stack[ps->top] = -1;
    ps->top--;
  }

  void
  copyps(struct pageselect* pps, struct pageselect* cps)
  {
    for(int i = 0; i < MAX_PSYC_PAGES; i++)
      cps->stack[i] = pps->stack[i];
    cps->top = pps->top;
    #ifdef DEBUG
      cprintf("@in copyps: top: %d, pid: %d\n",pps->top,proc->pid);
    #endif
  }

  void
  clearps(struct pageselect* ps)
  {
    for(int i = 0; i<MAX_PSYC_PAGES;i++)
      ps->stack[i] = -1;
    ps->top = 0;
    #ifdef DEBUG
      cprintf("@in clearps: top: %d, pid: %d\n",ps->top,proc->pid);
    #endif
  }

#endif


// SCFIFO
#ifdef SCFIFO

  uint
  selectpage(struct pageselect* ps)
  {
    uint va;
    pte_t* pte;
    if(!ps->contains)
      panic("(SCFIFO) trying to select a page while the process has none.");
    for(;;){
      va = ps->queue[ps->out];
      #ifdef DEBUG
        cprintf("@in select: va: %d:%d dequeued. in: %d, out: %d, contains: %d, pid: %d\n",va/PGSIZE,va,ps->in,ps->out,ps->contains,proc->pid);
      #endif
      ps->queue[ps->out] = -1;
      ps->out = (ps->out + 1) % MAX_PSYC_PAGES;
      ps->contains--;
      if((pte = walkpgdir(proc->pgdir, (char*)va, 0)) == 0)
        panic("@in select: page sould be present");
      if(!(*pte & PTE_A))
        break;
      *pte &= ~PTE_A;
      lcr3(v2p(proc->pgdir)); // refresh TLB
      #ifdef DEBUG
        cprintf("@in select: va: %d:%d enqueued back. in: %d, out: %d, contains: %d, pid: %d\n",va/PGSIZE,va,ps->in,ps->out,ps->contains,proc->pid);
      #endif
      addpage(ps,va);
    }
    #ifdef DEBUG
		  cprintf("@in select: va: %d:%d selected. in: %d, out: %d, contains: %d, pid: %d\n",proc->pid,va/PGSIZE,va,ps->in,ps->out,ps->contains,proc->pid);
	  #endif
    return va;
  }

  void
  addpage(struct pageselect* ps, uint va)
  {
    va = PGROUNDDOWN(va);
    #ifdef DEBUG
			cprintf("@in addpage: va: %d:%d adding. in: %d, out: %d, contains: %d, pid: %d\n",va/PGSIZE,va,ps->in,ps->out,ps->contains,proc->pid);
	  #endif
    if(ps->contains == MAX_PSYC_PAGES){
      #ifdef DEBUG
			   cprintf("@in addpage: va: %d:%d no room! swaping out another page. in: %d, out: %d, contains: %d, pid: %d\n",va/PGSIZE,va,ps->in,ps->out,ps->contains,proc->pid);
	    #endif
      swapout(selectpage(ps));
    }
    ps->queue[ps->in] = va;
    ps->in = (ps->in + 1) % MAX_PSYC_PAGES;
    ps->contains++;
    #ifdef DEBUG
			cprintf("@in addpage: va: %d:%d added! in: %d, out: %d, contains: %d, pid: %d\n",proc->pid,va/PGSIZE,va,ps->in,ps->out,ps->contains,proc->pid);
	  #endif
  }

  void
  removepage(struct pageselect* ps, uint va)
  {
  #ifdef DEBUG
			cprintf("@in removepage: va: %d:%d removing. in: %d, out: %d, contains: %d, pid: %d\n",va/PGSIZE,va,ps->in,ps->out,ps->contains,proc->pid);
	#endif
    va = PGROUNDDOWN(va);
    if(!ps->contains){
      #ifdef DEBUG
        cprintf("@in removepage: va: %d:%d ps is empty! in: %d, out: %d, contains: %d, pid: %d\n",va/PGSIZE,va,ps->in,ps->out,ps->contains,proc->pid);
      #endif
      return;
    }
    int page = 0;
    for(; page < MAX_PSYC_PAGES; page++){
      if(ps->queue[page] == va)
        goto rearage;
    }
    return;
    rearage:
    if(page == ps->out){
      ps->queue[ps->out] = -1;
      ps->out = (ps->out + 1) % MAX_PSYC_PAGES;
      ps->contains--;
    }
    else{
      for(;page != ps->in; page = (page+1)%MAX_PSYC_PAGES)
        ps->queue[page] = ps->queue[(page+1)%MAX_PSYC_PAGES];
      ps->queue[ps->in] = -1;
      ps->in--;
      if(ps->in < 0)
        ps->in += MAX_PSYC_PAGES;
      ps->contains--;
    }
  }

  void
  copyps(struct pageselect* pps, struct pageselect* cps)
  {
    for(int i = 0; i < MAX_PSYC_PAGES; i++)
      cps->queue[i] = pps->queue[i];
      cps->in = pps->in;
      cps->out = pps->out;
      cps->contains = pps->contains;
      #ifdef DEBUG
        cprintf("@in copyps: contains: %d, in: %d, out: %d, pid: %d\n",pps->contains,pps->in,pps->out,proc->pid);
      #endif
  }

  void
  clearps(struct pageselect* ps)
  {
    for(int i = 0; i<MAX_PSYC_PAGES; i++)
      ps->queue[i] = -1;
    ps->in = 0;
    ps->out = 0;
    ps->contains = 0;
    #ifdef DEBUG
      cprintf("@in clearps: contains: %d, in: %d, out: %d, pid: %d\n",ps->contains,ps->in,ps->out,proc->pid);
    #endif
  }

#endif


// LAP
#ifdef LAP

  void
  accessupdate(){
    pte_t* pte;
    // if(!proc || proc->pid <= 2 || proc->state == UNUSED || proc->state == ZOMBIE)
    //   return;
    if(!proc || proc->pid <= 2)
      return;
    for(int page=0;page<MAX_TOTAL_PAGES; page++){
      if((proc->ps).accesses[page] < UINT_MAX){
        pte = walkpgdir(proc->pgdir, (char*)(page*PGSIZE), 0);
        if(*pte & PTE_A)
          (proc->ps).accesses[page]++;
        *pte = *pte & ~PTE_A;
        lcr3(v2p(proc->pgdir)); // refresh TLB
      }
    }
  }

  uint
  selectpage(struct pageselect* ps)
  {
    if(!ps->contains)
      panic("(LAP) trying to select a page while the process has none.");

    uint min = UINT_MAX;
    uint page = 0;
    for(int i = MAX_TOTAL_PAGES-1; i >=0; i--){
      if(ps->accesses[i] < min){
        min = ps->accesses[i];
        page = i;
      }
    }
    ps->accesses[page] = UINT_MAX;
    ps->contains--;
    #ifdef DEBUG
      cprintf("@in select: selected page %d:%d contains: %d, pid: %d\n",page,page*PGSIZE,ps->contains,proc->pid);
    #endif
    return page*PGSIZE;
  }

  void
  addpage(struct pageselect* ps, uint va)
  {
    va = PGROUNDDOWN(va);
    #ifdef DEBUG
			cprintf("@in addpage: va: %d:%d adding. contains: %d, pid: %d\n",va/PGSIZE,va,ps->contains,proc->pid);
	  #endif
    if(ps->contains == MAX_PSYC_PAGES){
      #ifdef DEBUG
			   cprintf("@in addpage: va: %d:%d no room! swaping out another page. contains: %d, pid: %d\n",va/PGSIZE,va,ps->contains,proc->pid);
	    #endif
      swapout(selectpage(ps));
    }
    ps->accesses[va/PGSIZE] = 0;
    ps->contains++;
    #ifdef DEBUG
			cprintf("@in addpage: va: %d:%d added! contains: %d, pid: %d\n",va/PGSIZE,va,ps->contains,proc->pid);
	  #endif
  }

  void
  removepage(struct pageselect* ps, uint va)
  {
    #ifdef DEBUG
  		cprintf("@in removepage: va:%d:%d removing. contains: %d, pid: %d\n",va/PGSIZE,va,ps->contains,proc->pid);
  	#endif
    va = PGROUNDDOWN(va);
    if(!ps->contains){
      #ifdef DEBUG
        cprintf("@in removepage: va: %d:%d ps is empty! contains: %d, pid: %d\n",va/PGSIZE,va,ps->contains,proc->pid);
      #endif
      return;
    }
    ps->accesses[va/PGSIZE] = UINT_MAX;
    ps->contains--;
  }

  void
  copyps(struct pageselect* pps, struct pageselect* cps)
  {
    for(int i = 0; i < MAX_PSYC_PAGES; i++)
      cps->accesses[i] = pps->accesses[i];
      cps->contains = pps->contains;
      #ifdef DEBUG
        cprintf("@in copyps: contains: %d, pid: %d\n",pps->contains,proc->pid);
      #endif
  }

  void
  clearps(struct pageselect* ps)
  {

    for(int i = 0; i<MAX_TOTAL_PAGES;i++)
      ps->accesses[i] = UINT_MAX;
    ps->contains = 0;
    #ifdef DEBUG
      cprintf("@in clearps: contains: %d, pid: %d\n",ps->contains,proc->pid);
    #endif
  }

#endif

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
