#ifndef __FT_ELF__H__
#define __FT_ELF__H__


#include <stddef.h>
#include "types.h"
#include "io.h"
#include "enc.h"

#ifdef __APPLE__
#include <libelf.h>
#else
#include <elf.h>
#endif

typedef struct {
	u8     e_ident[16];         /* Magic number and other info */
	u16    e_type;              /* Object file type */
	u16    e_machine;           /* Architecture */
	u32    e_version;           /* Object file version */
	u64    e_entry;             /* Entry point virtual address */
	u64    e_phoff;             /* Program header table file offset */
	u64    e_shoff;             /* Section header table file offset */
	u32    e_flags;             /* Processor-specific flags */
	u16    e_ehsize;            /* ELF header size in bytes */
	u16    e_phentsize;         /* Program header table entry size */
	u16    e_phnum;             /* Program header table entry count */
	u16    e_shentsize;         /* Section header table entry size */
	u16    e_shnum;             /* Section header table entry count */
	u16    e_shstrndx;          /* Section header string table index */
} t_elf64_hdr;

typedef struct		s_elf_info
{
	t_file_info		guest_file; // guest file info
	Elf64_Ehdr		*elf_header; // ELF header
	Elf64_Phdr		*segments; // program headers of guest
	Elf64_Shdr		*sections; // guest headers of guest
	Elf64_Phdr		*pt_load; /* injected segment */ // program header of guest that describes a loadable, executable segment +1
	Elf64_Shdr		*text_section; // points to text section header
}					t_elf_info;


int init_elf_info(t_elf_info *elf_info, t_file_info guest_file);
int write_binary(unsigned char **stub_buffer, t_key *key, t_file_info *guest_file, t_elf_info *elf_info);

#endif  //!__FT_ELF__H__
