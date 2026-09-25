#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ext2fs/ext2_fs.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <dispositivo_o_imagen>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s /dev/sdb1\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *device = argv[1];

    // 1. Abrir el dispositivo en modo solo lectura
    int fd = open(device, O_RDONLY);
    if (fd < 0) {
        perror("Error al abrir el dispositivo (asegúrate de ejecutar con sudo/permisos)");
        return EXIT_FAILURE;
    }

    // 2. Mover el puntero de archivo al offset 1024 (donde inicia el superbloque)
    if (lseek(fd, 1024, SEEK_SET) < 0) {
        perror("Error al hacer lseek al offset 1024");
        close(fd);
        return EXIT_FAILURE;
    }

    // 3. Leer la estructura del superbloque
    struct ext2_super_block sb;
    ssize_t bytes_read = read(fd, &sb, sizeof(struct ext2_super_block));
    if (bytes_read != sizeof(struct ext2_super_block)) {
        perror("Error al leer el superbloque");
        close(fd);
        return EXIT_FAILURE;
    }

    close(fd);

    // 4. Validar el número mágico (Magic Number) de ext2/ext3/ext4
    if (sb.s_magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Error: El sistema de archivos no es un ext2/ext3/ext4 válido (Magic: 0x%X).\n", sb.s_magic);
        return EXIT_FAILURE;
    }

    // 5. Calcular el tamaño de bloque: 1024 << s_log_block_size
    unsigned int block_size = 1024 << sb.s_log_block_size;

    // 6. Mostrar la información leída
    printf("--- Información del Superbloque ---\n");
    printf("Magic Number       : 0x%X (OK)\n", sb.s_magic);
    printf("Total de Inodos    : %u\n", sb.s_inodes_count);
    printf("Total de Bloques   : %u\n", sb.s_blocks_count);
    printf("Bloques libres     : %u\n", sb.s_free_blocks_count);
    printf("Inodos libres      : %u\n", sb.s_free_inodes_count);
    printf("Tamaño de bloque   : %u bytes\n", block_size);
    printf("Nombre del volumen : %s\n", sb.s_volume_name[0] ? sb.s_volume_name : "(Sin nombre)");

    return EXIT_SUCCESS;
}
