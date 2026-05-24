#include <stdlib.h>

#include "ft_elf.h"
#include "logging.h"
#include "libft.h"

int get_text_section_header(Elf64_Shdr **text_section_ptr, t_elf_info *elf_info)
{
	// find .shstrtab SHT_STRTAB section to locate the string table
	Elf64_Shdr *sections = elf_info->sections;
	int num_sections = elf_info->elf_header->e_shnum;
	
	for (int i = 0; i < num_sections; i++)
	{
		Elf64_Shdr *curr_section = &sections[i];
		if (curr_section->sh_type == SHT_STRTAB)
		{
			Elf32_Word sh_idx = curr_section->sh_name;
			Elf32_Off sh_offset = curr_section->sh_offset;
			
			// Elf32_Off sh_size = curr_section->sh_size;
			
			unsigned char *sect_name = elf_info->guest_file.contents + sh_offset + sh_idx;
			// once section name string table is located, find text section
			if (!ft_strcmp((const char *) sect_name, ".shstrtab"))
			{
				for (int j = 0; j < num_sections; j++)	
				{
					Elf64_Shdr *query_section = &sections[j];
					Elf32_Word query_sh_idx = query_section->sh_name;
					unsigned char *query_sect_name = elf_info->guest_file.contents + sh_offset + query_sh_idx;
					if (!ft_strcmp((const char *) query_sect_name, ".text"))
					{
						*text_section_ptr = query_section;
						return 0;
					}
				}
				
				error("get_text_section_header: .text section not found");
				return 1;
			}
		}
	}

	error("get_text_section_header: shstrtab section not found");
	return 1;
}

int get_first_x_segment_hdr(Elf64_Phdr **first_x_seg_ptr, t_elf_info *elf_info)
{
	Elf64_Phdr *segments = elf_info->segments;
	int ph_num = elf_info->elf_header->e_phnum;

	for (int i = 0; i < ph_num - 1; i++)
	{
		// check if next segment is a loadable one, because if its not, the offset which
		// we use to inject shellcode is irrelevant
		Elf64_Phdr *curr_segment = &segments[i];
		Elf64_Phdr *next_segment = &segments[i + 1];

		if (curr_segment->p_type == PT_LOAD && curr_segment->p_flags & PF_X && next_segment->p_type == PT_LOAD)
		{
			*first_x_seg_ptr = curr_segment;
			return 0;
		}
	}
	error("get_first_x_segment_hdr: no exploitable segment found");
	return 1;
}

int init_elf_info(t_elf_info *elf_info, t_file_info guest_file)
{
	elf_info->guest_file = guest_file;
	elf_info->elf_header = guest_file.contents;

	// check magic
	if (elf_info->elf_header->e_ident[0] != 0x7f ||
		elf_info->elf_header->e_ident[1] != 'E' ||	
		elf_info->elf_header->e_ident[2] != 'L' ||
		elf_info->elf_header->e_ident[3] != 'F' 
	)
	{
		error("init_elf_info: invalid magic");
		return 1;
	}

	// check 64 bit
	if (elf_info->elf_header->e_ident[4] != 0x2)
	{
		error("init_elf_info: invalid file class");
		return 1;
	}

	// check valid header offsets
	if ((long)elf_info->elf_header->e_phoff > guest_file.size || (long)elf_info->elf_header->e_shoff > guest_file.size)
	{
		error("init_elf_info: invalid header offsets");
		return 1;		
	}

	if ((long)elf_info->elf_header->e_phoff == 0 || (long)elf_info->elf_header->e_shoff == 0)
	{
		error("init_elf_info: no program header or no section header");
		return 1;
	}

	// fill up the rest of elf_info
	elf_info->segments = guest_file.contents + elf_info->elf_header->e_phoff;
	elf_info->sections = guest_file.contents + elf_info->elf_header->e_shoff;

	int ret = get_text_section_header(&elf_info->text_section, elf_info);
	if (ret)
		return ret;

	ret = get_first_x_segment_hdr(&elf_info->pt_load, elf_info);
	if (ret)
		return ret;


	return 0;
}