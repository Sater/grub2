/* linux.c - boot Linux */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2004,2005,2007,2009,2010  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/elf.h>
#include <grub/elfload.h>
#include <grub/loader.h>
#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/mips/relocator.h>
#include <grub/memory.h>
#include <grub/i18n.h>

/* For frequencies.  */
#include <grub/pci.h>
#include <grub/machine/time.h>

static grub_dl_t my_mod;

static int loaded;

static grub_size_t linux_size;

static struct grub_relocator *relocator;
static grub_uint8_t *playground;
static grub_addr_t target_addr, entry_addr;
static int linux_argc;
static grub_off_t argv_off, envp_off;
static grub_off_t rd_addr_arg_off, rd_size_arg_off;
static int initrd_loaded = 0;

static grub_err_t
grub_linux_boot (void)
{
  struct grub_relocator32_state state;

  /* Boot the kernel.  */
  state.gpr[1] = entry_addr;
  state.gpr[4] = linux_argc;
  state.gpr[5] = target_addr + argv_off;
  state.gpr[6] = target_addr + envp_off;
  state.jumpreg = 1;
  grub_relocator32_boot (relocator, state);

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_linux_unload (void)
{
  grub_relocator_unload (relocator);
  grub_dl_unref (my_mod);

  loaded = 0;

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_linux_load32 (grub_elf_t elf, void **extra_mem, grub_size_t extra_size)
{
  Elf32_Addr base;
  int extraoff;
  grub_err_t err;

  /* Linux's entry point incorrectly contains a virtual address.  */
  entry_addr = elf->ehdr.ehdr32.e_entry;

  linux_size = grub_elf32_size (elf, &base);
  if (linux_size == 0)
    return grub_errno;
  target_addr = base;
  /* Pad it; the kernel scribbles over memory beyond its load address.  */
  linux_size += 0x100000;
  linux_size = ALIGN_UP (base + linux_size, 4) - base;
  extraoff = linux_size;
  linux_size += extra_size;

  relocator = grub_relocator_new ();
  if (!relocator)
    return grub_errno;

  {
    grub_relocator_chunk_t ch;
    err = grub_relocator_alloc_chunk_addr (relocator, &ch,
					   target_addr & 0x1fffffff,
					   linux_size);
    if (err)
      return err;
    playground = get_virtual_current_address (ch);
  }

  *extra_mem = playground + extraoff;

  /* Now load the segments into the area we claimed.  */
  auto grub_err_t offset_phdr (Elf32_Phdr *phdr, grub_addr_t *addr, int *do_load);
  grub_err_t offset_phdr (Elf32_Phdr *phdr, grub_addr_t *addr, int *do_load)
    {
      if (phdr->p_type != PT_LOAD)
	{
	  *do_load = 0;
	  return 0;
	}
      *do_load = 1;

      /* Linux's program headers incorrectly contain virtual addresses.
       * Translate those to physical, and offset to the area we claimed.  */
      *addr = (grub_addr_t) (phdr->p_paddr - base + playground);
      return 0;
    }
  return grub_elf32_load (elf, offset_phdr, 0, 0);
}

static grub_err_t
grub_linux_load64 (grub_elf_t elf, void **extra_mem, grub_size_t extra_size)
{
  Elf64_Addr base;
  int extraoff;
  grub_err_t err;

  /* Linux's entry point incorrectly contains a virtual address.  */
  entry_addr = elf->ehdr.ehdr64.e_entry;

  linux_size = grub_elf64_size (elf, &base);
  if (linux_size == 0)
    return grub_errno;
  target_addr = base;
  /* Pad it; the kernel scribbles over memory beyond its load address.  */
  linux_size += 0x100000;
  linux_size = ALIGN_UP (base + linux_size, 4) - base;
  extraoff = linux_size;
  linux_size += extra_size;

  relocator = grub_relocator_new ();
  if (!relocator)
    return grub_errno;

  {
    grub_relocator_chunk_t ch;
    err = grub_relocator_alloc_chunk_addr (relocator, &ch,
					   target_addr & 0x1fffffff,
					   linux_size);
    if (err)
      return err;
    playground = get_virtual_current_address (ch);
  }

  *extra_mem = playground + extraoff;

  /* Now load the segments into the area we claimed.  */
  auto grub_err_t offset_phdr (Elf64_Phdr *phdr, grub_addr_t *addr, int *do_load);
  grub_err_t offset_phdr (Elf64_Phdr *phdr, grub_addr_t *addr, int *do_load)
    {
      if (phdr->p_type != PT_LOAD)
	{
	  *do_load = 0;
	  return 0;
	}
      *do_load = 1;
      /* Linux's program headers incorrectly contain virtual addresses.
       * Translate those to physical, and offset to the area we claimed.  */
      *addr = (grub_addr_t) (phdr->p_paddr - base + playground);
      return 0;
    }
  return grub_elf64_load (elf, offset_phdr, 0, 0);
}

static grub_err_t
grub_cmd_linux (grub_command_t cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
  grub_elf_t elf = 0;
  int i;
  int size;
  void *extra = NULL;
  grub_uint32_t *linux_argv, *linux_envp;
  char *linux_args, *linux_envs;
  grub_err_t err;

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "no kernel specified");

  elf = grub_elf_open (argv[0]);
  if (! elf)
    return grub_errno;

  if (elf->ehdr.ehdr32.e_type != ET_EXEC)
    {
      grub_elf_close (elf);
      return grub_error (GRUB_ERR_UNKNOWN_OS,
			 "this ELF file is not of the right type\n");
    }

  /* Release the previously used memory.  */
  grub_loader_unset ();
  loaded = 0;

  /* For arguments.  */
  linux_argc = argc;
  /* Main arguments.  */
  size = (linux_argc) * sizeof (grub_uint32_t); 
  /* Initrd address and size.  */
  size += 2 * sizeof (grub_uint32_t); 
  /* NULL terminator.  */
  size += sizeof (grub_uint32_t); 

  /* First argument is always "a0".  */
  size += ALIGN_UP (sizeof ("a0"), 4);
  /* Normal arguments.  */
  for (i = 1; i < argc; i++)
    size += ALIGN_UP (grub_strlen (argv[i]) + 1, 4);
  
  /* rd arguments.  */
  size += ALIGN_UP (sizeof ("rd_start=0xXXXXXXXXXXXXXXXX"), 4);
  size += ALIGN_UP (sizeof ("rd_size=0xXXXXXXXXXXXXXXXX"), 4);

  /* For the environment.  */
  size += sizeof (grub_uint32_t);
  size += 4 * sizeof (grub_uint32_t);
  size += ALIGN_UP (sizeof ("memsize=XXXXXXXXXXXXXXXXXXXX"), 4)
    + ALIGN_UP (sizeof ("highmemsize=XXXXXXXXXXXXXXXXXXXX"), 4)
    + ALIGN_UP (sizeof ("busclock=XXXXXXXXXX"), 4)
    + ALIGN_UP (sizeof ("cpuclock=XXXXXXXXXX"), 4);

  if (grub_elf_is_elf32 (elf))
    err = grub_linux_load32 (elf, &extra, size);
  else
  if (grub_elf_is_elf64 (elf))
    err = grub_linux_load64 (elf, &extra, size);
  else
    err = grub_error (GRUB_ERR_BAD_FILE_TYPE, "unknown ELF class");

  grub_elf_close (elf);

  if (err)
    return err;

  linux_argv = extra;
  argv_off = (grub_uint8_t *) linux_argv - (grub_uint8_t *) playground;
  extra = linux_argv + (linux_argc + 1 + 2);
  linux_args = extra;

  grub_memcpy (linux_args, "a0", sizeof ("a0"));
  *linux_argv = (grub_uint8_t *) linux_args - (grub_uint8_t *) playground
    + target_addr;
  linux_argv++;
  linux_args += ALIGN_UP (sizeof ("a0"), 4);

  for (i = 1; i < argc; i++)
    {
      grub_memcpy (linux_args, argv[i], grub_strlen (argv[i]) + 1);
      *linux_argv = (grub_uint8_t *) linux_args - (grub_uint8_t *) playground
	+ target_addr;
      linux_argv++;
      linux_args += ALIGN_UP (grub_strlen (argv[i]) + 1, 4);
    }

  /* Reserve space for rd arguments.  */
  rd_addr_arg_off = (grub_uint8_t *) linux_args - (grub_uint8_t *) playground;
  linux_args += ALIGN_UP (sizeof ("rd_start=0xXXXXXXXXXXXXXXXX"), 4);
  *linux_argv = 0;
  linux_argv++;

  rd_size_arg_off = (grub_uint8_t *) linux_args - (grub_uint8_t *) playground;
  linux_args += ALIGN_UP (sizeof ("rd_size=0xXXXXXXXXXXXXXXXX"), 4);
  *linux_argv = 0;
  linux_argv++;

  *linux_argv = 0;

  extra = linux_args;

  linux_envp = extra;
  envp_off = (grub_uint8_t *) linux_envp - (grub_uint8_t *) playground;
  linux_envs = (char *) (linux_envp + 5);
  grub_snprintf (linux_envs, sizeof ("memsize=XXXXXXXXXXXXXXXXXXXX"),
		 "memsize=%lld",
		 (unsigned long long) grub_mmap_get_lower () >> 20);
  linux_envp[0] = (grub_uint8_t *) linux_envs - (grub_uint8_t *) playground
    + target_addr;
  linux_envs += ALIGN_UP (grub_strlen (linux_envs) + 1, 4);
  grub_snprintf (linux_envs, sizeof ("highmemsize=XXXXXXXXXXXXXXXXXXXX"),
		 "highmemsize=%lld",
		 (unsigned long long) grub_mmap_get_upper () >> 20);
  linux_envp[1] = (grub_uint8_t *) linux_envs - (grub_uint8_t *) playground
    + target_addr;
  linux_envs += ALIGN_UP (grub_strlen (linux_envs) + 1, 4);

  grub_snprintf (linux_envs, sizeof ("busclock=XXXXXXXXXX"),
		 "busclock=%d", grub_arch_busclock);
  linux_envp[2] = (grub_uint8_t *) linux_envs - (grub_uint8_t *) playground
    + target_addr;
  linux_envs += ALIGN_UP (grub_strlen (linux_envs) + 1, 4);
  grub_snprintf (linux_envs, sizeof ("cpuclock=XXXXXXXXXX"),
		 "cpuclock=%d", grub_arch_cpuclock);
  linux_envp[3] = (grub_uint8_t *) linux_envs - (grub_uint8_t *) playground
    + target_addr;
  linux_envs += ALIGN_UP (grub_strlen (linux_envs) + 1, 4);


  linux_envp[4] = 0;

  grub_loader_set (grub_linux_boot, grub_linux_unload, 1);
  initrd_loaded = 0;
  loaded = 1;
  grub_dl_ref (my_mod);

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_initrd (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char *argv[])
{
  grub_file_t file = 0;
  grub_ssize_t size;
  void *initrd_src;
  grub_addr_t initrd_dest;
  grub_err_t err;

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "no initrd specified");

  if (!loaded)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "you need to load Linux first.");

  if (initrd_loaded)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "only one initrd can be loaded.");

  grub_file_filter_disable_compression ();
  file = grub_file_open (argv[0]);
  if (! file)
    return grub_errno;

  size = grub_file_size (file);

  {
    grub_relocator_chunk_t ch;

    err = grub_relocator_alloc_chunk_align (relocator, &ch,
					    target_addr + linux_size + 0x10000,
					    (0xffffffff - size) + 1,
					    size, 0x10000,
					    GRUB_RELOCATOR_PREFERENCE_NONE);

    if (err)
      {
	grub_file_close (file);
	return err;
      }
    initrd_src = get_virtual_current_address (ch);
    initrd_dest = get_physical_target_address (ch) | 0x80000000;
  }

  if (grub_file_read (file, initrd_src, size) != size)
    {
      grub_error (GRUB_ERR_FILE_READ_ERROR, "couldn't read file");
      grub_file_close (file);

      return grub_errno;
    }

  grub_snprintf ((char *) playground + rd_addr_arg_off,
		 sizeof ("rd_start=0xXXXXXXXXXXXXXXXX"), "rd_start=0x%llx",
		(unsigned long long) initrd_dest);
  ((grub_uint32_t *) (playground + argv_off))[linux_argc]
    = target_addr + rd_addr_arg_off;
  linux_argc++;

  grub_snprintf ((char *) playground + rd_size_arg_off,
		sizeof ("rd_size=0xXXXXXXXXXXXXXXXXX"), "rd_size=0x%llx",
		(unsigned long long) size);
  ((grub_uint32_t *) (playground + argv_off))[linux_argc]
    = target_addr + rd_size_arg_off;
  linux_argc++;

  initrd_loaded = 1;

  grub_file_close (file);

  return GRUB_ERR_NONE;
}

static grub_command_t cmd_linux, cmd_initrd;

GRUB_MOD_INIT(linux)
{
  cmd_linux = grub_register_command ("linux", grub_cmd_linux,
				     0, N_("Load Linux."));
  cmd_initrd = grub_register_command ("initrd", grub_cmd_initrd,
				      0, N_("Load initrd."));
  my_mod = mod;
}

GRUB_MOD_FINI(linux)
{
  grub_unregister_command (cmd_linux);
  grub_unregister_command (cmd_initrd);
}
