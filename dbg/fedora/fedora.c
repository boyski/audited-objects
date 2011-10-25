/*
 * Build: gcc -g -W -Wall -fpic -D_REENTRANT -shared -o fedora.so fedora.c
 *
 * Run:   LD_PRELOAD=./fedora.so uname
 *
 */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <elf.h>
#include <sys/types.h>

#if defined(linux)
#include <bits/wordsize.h>
#elif defined(sun)
#include <sys/machelf.h>
#endif

#if !defined(__WORDSIZE)
#if __LP64__
#define __WORDSIZE	64
#else
#define __WORDSIZE	32
#endif
#endif

#define LD_PRELOAD    "LD_PRELOAD"

static void fedora_init(void) __attribute__ ((constructor));	// gcc only
static void fedora_fini(void) __attribute__ ((destructor));	// gcc only

#if __WORDSIZE == 64
#define ELFADDR 0x400000L
#define ELF_EHDR Elf64_Ehdr
#define ELF_PHDR Elf64_Phdr
#define ELF_DYN Elf64_Dyn
#define ELF_ADDR Elf64_Addr
#define ELF_REL Elf64_Rela
#define ELF_SYM Elf64_Sym
#define ELF_R_SYM ELF64_R_SYM
#else
#define ELFADDR 0x08048000
#define ELF_EHDR Elf32_Ehdr
#define ELF_PHDR Elf32_Phdr
#define ELF_DYN Elf32_Dyn
#define ELF_ADDR Elf32_Addr
#define ELF_REL Elf32_Rel
#define ELF_SYM Elf32_Sym
#define ELF_R_SYM ELF32_R_SYM
#endif

static pthread_t(*pthread_self_real) (void);
static pthread_t tidopener = (pthread_t) - 1;

typedef struct {
    ELF_ADDR strtab;
    ELF_ADDR symtab;
    ELF_ADDR init;
    ELF_ADDR rel_plt;
    int rel_plt_size;
} elf_local_struct;

static elf_local_struct *elf_local_new(void);
static ELF_ADDR elf_local_find_symbol(elf_local_struct *, char *, int);
static ELF_ADDR elf_local_find_symbol_got(elf_local_struct *, char *);

ELF_ADDR origclose[2];
ELF_ADDR origopen[2];
ELF_ADDR origopen64[2];

static int added_ld_preload;

// Request the address of the "real" (system-supplied-and-documented)
// function of the specified name.
static const void *
_get_real(const char *fcn)
{
    void *fcnptr;

    fcnptr = dlsym(RTLD_NEXT, fcn);
    // Better to die now if symbols not found than to segfault later.
    if (!fcnptr) {
	fprintf(stderr, "Error: %s: %s\n", fcn, dlerror());
    }
    return fcnptr;
}

static int
x__close(int fd)
{
    static int (*next) (int);
    const char *funcname = "close";
    pthread_t tid;

    if (!next) {
	next = _get_real(funcname);
    }

    tid = pthread_self_real();
    if (tid == tidopener) {
	fprintf(stderr, "= close(%d)\n", fd);
    }

    return next (fd);
}

static int
x__open(const char *path, int oflag, ...)
{
    static int (*next) (const char *, int, ...);
    mode_t mode = 0;

    if (!next) {
	next = _get_real("open");
    }

    if (oflag & O_CREAT) {
	va_list ap;

	va_start(ap, oflag);
	mode = va_arg(ap, int);	/* NOT mode_t */

	va_end(ap);
    } else {
	mode = 0;
    }

    fprintf(stderr, "= x__open(%s, %d, 0%o)\n", path, oflag, (unsigned)mode);
    return next (path, oflag, mode);
}

static int
x__open64(const char *path, int oflag, ...)
{
    static int (*next) (const char *, int, ...);
    mode_t mode = 0;

    if (!next) {
	next = _get_real("open64");
    }

    if (oflag & O_CREAT) {
	va_list ap;

	va_start(ap, oflag);
	mode = va_arg(ap, int);	/* NOT mode_t */

	va_end(ap);
    } else {
	mode = 0;
    }

    fprintf(stderr, "= x__open64(%s, %d, 0%o)\n", path, oflag, (unsigned)mode);
    return next (path, oflag, mode);
}

static void
intercept(elf_local_struct * elf, char *fn, ELF_ADDR save[], ELF_ADDR new)
{
    if (!(save[0] = elf_local_find_symbol_got(elf, fn))) {
	fprintf(stderr, "= elf_local_find_symbol failed (%s)\n", fn);
    } else {
	save[1] = *(ELF_ADDR *) save[0];
	*(ELF_ADDR *) save[0] = new;
    }
}

static void
restoreptr(ELF_ADDR saved[])
{
    if (saved[0]) {
	*(ELF_ADDR *) saved[0] = saved[1];
    }
}

static void
fedora_init(void)
{
    elf_local_struct *elf;
    char *ld_preload;

    pthread_self_real = _get_real("pthread_self");
    tidopener = pthread_self_real();

    if (!(elf = elf_local_new())) {
	fprintf(stderr, "= elf_local_new failed!\n");
	exit(1);
    }

    intercept(elf, "close", origclose, (ELF_ADDR) x__close);
    intercept(elf, "open", origopen, (ELF_ADDR) x__open);
    intercept(elf, "open64", origopen64, (ELF_ADDR) x__open64);

    // TODO - this breaks if LD_PRELOAD exists and lists other libs.
    ld_preload = getenv(LD_PRELOAD);
    if (!ld_preload || !strstr(ld_preload, FEDORA)) {
	setenv(LD_PRELOAD, FEDORA, 1);
	added_ld_preload = 1;
	fprintf(stderr, "= %05ld [%p]: Explicitly loading %s ...\n",
		(long)getpid(), (void *)tidopener, FEDORA);
    } else {
	fprintf(stderr, "= %05ld [%p]: Implicitly loading %s ...\n",
		(long)getpid(), (void *)tidopener, FEDORA);
    }
    //sleep(2); /* REMOVEME - for testing */
}

static void
fedora_fini(void)
{
    restoreptr(origclose);
    restoreptr(origopen);
    restoreptr(origopen64);

    if (added_ld_preload) {
	unsetenv(LD_PRELOAD);
	fprintf(stderr, "= %05d: [%p] Explicitly unloading %s ...\n",
		(int)getpid(), (void *)tidopener, FEDORA);
    } else {
	fprintf(stderr, "= %05d: [%p] Implicitly unloading %s ...\n",
		(int)getpid(), (void *)tidopener, FEDORA);
    }
}

/* find the address of the ELF header */
static ELF_ADDR
find_elf_load(void)
{
    FILE *fp;
    unsigned long start, end;
    char perms[12];
    char junk[1024];
    int nmatch;
    ELF_EHDR *ehdr;

    if (0 == (fp = fopen("/proc/self/maps", "r"))) {
	fprintf(stderr, "Can't open /proc/self/maps. Assume ELF LOAD %p\n",
		(void *)ELFADDR);
	return ELFADDR;
    } else {
	while (4 == (nmatch = fscanf(fp, "%lx-%lx %10s %1000[^\n]",
				     &start, &end, perms, junk))) {
	    ehdr = (ELF_EHDR *) start;
	    if (end - start >= sizeof(ELF_EHDR) && perms[0] == 'r' &&
		ehdr->e_ident[0] == ELFMAG0 && ehdr->e_ident[1] == ELFMAG1 &&
		ehdr->e_ident[2] == ELFMAG2 && ehdr->e_ident[3] == ELFMAG3 &&
		ehdr->e_type == ET_EXEC) {
		//fprintf(stderr, "Found elf %lx\n", start);
		return start;
	    }
	}
    }
    fprintf(stderr, "Can't find ELF LOAD address, assuming %p\n",
	    (void *)ELFADDR);
    return ELFADDR;
}

static elf_local_struct *
elf_local_new(void)
{
    static elf_local_struct _elf_struct;
    elf_local_struct *elf;
    ELF_EHDR *ehdr;
    ELF_PHDR *phdr;
    ELF_DYN *dyn;

    // Initialize structure
    elf = &_elf_struct;
    memset(elf, 0, sizeof(elf_local_struct));

    ehdr = (ELF_EHDR *) find_elf_load();

    // Get program headers offset
    phdr = (ELF_PHDR *) ((char *)ehdr + ehdr->e_phoff);

    // Locate PT_DYNAMIC program header
    while (phdr->p_type != PT_DYNAMIC) {
	phdr++;
    }

    dyn = (ELF_DYN *) phdr->p_vaddr;
    do {
	//printf("dyn %d %d\n", dyn->d_tag, dyn->d_un.d_val);
	if (dyn->d_tag == DT_JMPREL && !elf->rel_plt) {
	    elf->rel_plt = dyn->d_un.d_ptr;
	} else if (dyn->d_tag == DT_PLTRELSZ && !elf->rel_plt_size) {
	    elf->rel_plt_size = dyn->d_un.d_val / sizeof(ELF_REL);
	} else if (dyn->d_tag == DT_INIT && !elf->init) {
	    elf->init = dyn->d_un.d_ptr;
	} else if (dyn->d_tag == DT_SYMTAB && !elf->symtab) {
	    elf->symtab = dyn->d_un.d_ptr;
	} else if (dyn->d_tag == DT_STRTAB && !elf->strtab) {
	    elf->strtab = dyn->d_un.d_ptr;
	}
	dyn++;
    } while (!elf->rel_plt || !elf->rel_plt_size || !elf->init || !elf->symtab
	     || !elf->strtab);

    return elf;
}

static ELF_ADDR
elf_local_find_symbol(elf_local_struct * elf, char *sym_name, int plt)
{
    ELF_SYM *sym;
    ELF_REL *rel;
    char *str;
    int i;
    ELF_ADDR symaddr;

    i = symaddr = 0;
    rel = (ELF_REL *) elf->rel_plt;
    sym = (ELF_SYM *) elf->symtab + (ELF_R_SYM(rel->r_info));
    while (i++ < elf->rel_plt_size) {
	str = (char *)(elf->strtab + sym->st_name);
	if (!strcmp(str, sym_name)) {
	    if (plt) {
		// scan .init section for the GOT entry address
		symaddr = elf->init;
		for (;; symaddr++) {
		    if (*(ELF_ADDR *) symaddr == rel->r_offset) {
			symaddr = symaddr - 2;
			break;
		    }
		}
	    } else {
		symaddr = rel->r_offset;
	    }
	    break;
	}
	rel++;
	sym = (ELF_SYM *) elf->symtab + (ELF_R_SYM(rel->r_info));
    }
    return symaddr;
}

static ELF_ADDR
elf_local_find_symbol_got(elf_local_struct * elf, char *sym_name)
{
    return elf_local_find_symbol(elf, sym_name, 0);
}

#if 0
static ELF_ADDR
elf_local_find_symbol_plt(elf_local_struct * elf, char *sym_name)
{
    return elf_local_find_symbol(elf, sym_name, 1);
}
#endif
