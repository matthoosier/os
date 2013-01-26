#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>

#include <muos/elf.h>

static void checkCode (const char description[], int code) {
    if (code < 0) {
        perror(description);
        exit(1);
    }
}

int main (int argc, char * argv[]) {

    struct stat         stat;
    int                 fd;
    int                 code;
    const uint8_t     * base;
    const uint8_t     * string_table_base;
    const Elf32_Ehdr  * hdr;
    const Elf32_Shdr  * string_section_hdr;
    unsigned int        i;

    if (argc != 2) {
        printf("Usage: %s <file>\n", argv[0]);
        exit(1);
    }

    fd = open(argv[1], O_RDONLY);
    checkCode("open", fd);

    code = fstat(fd, &stat);
    checkCode("fstat", code);

    base = mmap(NULL, stat.st_size, PROT_READ, MAP_FILE | MAP_SHARED, fd, 0);

    if (base == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    if (stat.st_size < sizeof(Elf32_Ehdr)) {
        printf("%s too small (expected at least %lu bytes for header)\n", argv[1], sizeof(Elf32_Ehdr));
        exit(1);
    }

    hdr = (Elf32_Ehdr *)base;

    printf("ident[0]: %c\n", hdr->e_ident[0]);
    printf("ident[1]: %c\n", hdr->e_ident[1]);
    printf("ident[2]: %c\n", hdr->e_ident[2]);
    printf("ident[3]: %c\n", hdr->e_ident[3]);

    printf("entry: 0x%08x\n", hdr->e_entry);
    printf("Executable? %s (%d)\n", hdr->e_type == ET_EXEC ? "yes" : "no", hdr->e_type);
    printf("ARM? %s (%d)\n", hdr->e_machine == EM_ARM ? "yes" : "no", hdr->e_machine);

    string_section_hdr = (const Elf32_Shdr *)&base[hdr->e_shoff + hdr->e_shstrndx * hdr->e_shentsize];
    string_table_base = &base[string_section_hdr->sh_offset];

    for (i = 0; i < hdr->e_shnum; i++) {
        const Elf32_Shdr * shdr;

        shdr = ((const Elf32_Shdr *)&base[hdr->e_shoff + i * hdr->e_shentsize]);
        printf("Section %d: %s\n", i, &string_table_base[shdr->sh_name]);
    }
}
