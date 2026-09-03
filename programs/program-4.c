#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>

/* Colores ANSI */
#define RESET    "\033[0m"
#define ROJO     "\033[31m"
#define VERDE    "\033[32m"
#define AMARILLO "\033[33m"
#define AZUL     "\033[34m"
#define MAGENTA  "\033[35m"
#define CIAN     "\033[36m"
#define GRIS     "\033[90m"

/* Verifica si una cadena es solo numeros */
int es_numero(const char *s) {
    int i;
    if (s[0] == '\0') return 0;
    for (i = 0; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i]))
            return 0;
    }
    return 1;
}

/* Convierte bytes a formato legible (KB, MB, GB) */
void formato_tamano(unsigned long bytes, char *buf, size_t bufsz) {
    if (bytes >= 1073741824UL)
        snprintf(buf, bufsz, "%.2f GB", (double)bytes / 1073741824.0);
    else if (bytes >= 1048576UL)
        snprintf(buf, bufsz, "%.2f MB", (double)bytes / 1048576.0);
    else if (bytes >= 1024UL)
        snprintf(buf, bufsz, "%.2f KB", (double)bytes / 1024.0);
    else
        snprintf(buf, bufsz, "%lu B", bytes);
}

/* Muestra la informacion basica de un proceso */
void mostrar_info_proceso(const char *pid) {
    char ruta[256];
    FILE *f;
    char linea[512];
    long paginas_total, paginas_residentes;
    long pagina_sz;

    pagina_sz = sysconf(_SC_PAGESIZE);
    if (pagina_sz < 1) pagina_sz = 4096;

    /* Leer statm */
    snprintf(ruta, sizeof(ruta), "/proc/%s/statm", pid);
    f = fopen(ruta, "r");
    if (f) {
        if (fgets(linea, sizeof(linea), f)) {
            sscanf(linea, "%ld %ld", &paginas_total, &paginas_residentes);
            printf(CIAN "\n--- Informacion del Proceso PID %s ---\n" RESET, pid);
            printf("Tamano de pagina del sistema: %ld bytes (%.1f KB)\n",
                   pagina_sz, (double)pagina_sz / 1024.0);
            printf("Tamanio total: %ld paginas (%lu bytes)\n",
                   paginas_total, (unsigned long)(paginas_total * pagina_sz));
            printf("Residente:     %ld paginas (%lu bytes)\n",
                   paginas_residentes, (unsigned long)(paginas_residentes * pagina_sz));
        }
        fclose(f);
    }

    /* Leer status (nombre del proceso) */
    snprintf(ruta, sizeof(ruta), "/proc/%s/status", pid);
    f = fopen(ruta, "r");
    if (f) {
        while (fgets(linea, sizeof(linea), f)) {
            if (strncmp(linea, "Name:", 5) == 0) {
                linea[strcspn(linea, "\n")] = '\0';
                printf("Proceso:       %s\n", linea + 6);
                break;
            }
        }
        fclose(f);
    }
}

/* Analiza y muestra el mapa de memoria de un proceso */
void analizar_mapa_memoria(const char *pid) {
    char ruta[256];
    FILE *f;
    char linea[1024];
    unsigned long dir_inicio, dir_fin, tamano, total = 0;
    int perm[4] = {0, 0, 0, 0}; /* r, w, x, p */
    int num_mapeos = 0;
    char nombre_arch[256];
    char tamano_str[32];
    unsigned long pagina_sz;

    pagina_sz = sysconf(_SC_PAGESIZE);
    if (pagina_sz < 1) pagina_sz = 4096;

    snprintf(ruta, sizeof(ruta), "/proc/%s/maps", pid);
    f = fopen(ruta, "r");
    if (f == NULL) {
        printf(ROJO "[!] No se pudo abrir %s\n" RESET, ruta);
        printf("    Asegurese de que el PID %s existe y tiene permisos.\n", pid);
        return;
    }

    printf(CIAN "\n--- Mapa de Memoria del Proceso PID %s ---\n" RESET, pid);
    printf("Pagina del sistema: %lu bytes (%.1f KB)\n\n", pagina_sz, (double)pagina_sz / 1024.0);

    printf(AMARILLO "%-18s %-18s %-8s %-6s %-10s %s\n" RESET,
           "Direccion Inicio", "Direccion Fin", "Tamano", "Perms", "Paginas", "Nombre/Tipo");
    printf(GRIS "---------------------------------------------------------------------------------------------------\n" RESET);

    while (fgets(linea, sizeof(linea), f)) {
        /* Parsear: inicio-fin perms offset dev inode [nombre] */
        nombre_arch[0] = '\0';

        /* Intentar leer con nombre */
        if (sscanf(linea, "%lx-%lx %*s %*s %*s %*s %255[^\n]",
                   &dir_inicio, &dir_fin, nombre_arch) < 2) {
            /* Intentar sin nombre */
            if (sscanf(linea, "%lx-%lx %*s", &dir_inicio, &dir_fin) < 2)
                continue;
            nombre_arch[0] = '\0';
        }

        tamano = dir_fin - dir_inicio;
        total += tamano;

        /* Extraer permisos */
        char perms[5] = "----";
        if (sscanf(linea, "%*s %4s", perms) >= 1) {
            perm[0] += (perms[0] == 'r');
            perm[1] += (perms[1] == 'w');
            perm[2] += (perms[2] == 'x');
            perm[3] += (perms[3] == 'p');
        }

        /* Calcular paginas de este mapeo */
        unsigned long num_paginas = tamano / pagina_sz;
        if (tamano % pagina_sz != 0) num_paginas++;

        formato_tamano(tamano, tamano_str, sizeof(tamano_str));

        /* Color segun tipo */
        const char *color = GRIS;
        const char *tipo = "";
        char nombre_limpio[256] = "";

        /* Limpiar nombre */
        {
            int j = 0, k = 0;
            while (nombre_arch[j] && j < 255) {
                if (nombre_arch[j] != ' ' && nombre_arch[j] != '\t' &&
                    nombre_arch[j] != '\n') {
                    nombre_limpio[k++] = nombre_arch[j];
                }
                j++;
            }
            nombre_limpio[k] = '\0';
        }

        if (strstr(nombre_limpio, "[heap]")) {
            color = VERDE;
            tipo = "[heap]";
        } else if (strstr(nombre_limpio, "[stack]")) {
            color = ROJO;
            tipo = "[stack]";
        } else if (strstr(nombre_limpio, "[vdso]")) {
            color = MAGENTA;
            tipo = "[vdso]";
        } else if (strstr(nombre_limpio, "[vvar]")) {
            color = MAGENTA;
            tipo = "[vvar]";
        } else if (strstr(nombre_limpio, ".so")) {
            color = CIAN;
            tipo = "libreria";
        } else if (strstr(nombre_limpio, ".a")) {
            color = CIAN;
            tipo = "estatica";
        } else if (nombre_limpio[0] != '\0') {
            color = AMARILLO;
            tipo = "ejecutable";
        } else {
            color = GRIS;
            tipo = "anonimo";
        }

        /* Imprime una linea del mapa de memoria */
        printf("%s", GRIS);
        printf("%016lx-%016lx ", dir_inicio, dir_fin);
        printf("%s%-8s ", color, perms);
        printf("%s%-10s ", AMARILLO, tamano_str);
        printf("%s%lu ", CIAN, num_paginas);
        printf("%s%-10s ", color, tipo);
        printf("%s", nombre_limpio[0] ? nombre_limpio : "(anonimo)");
        printf("%s\n", RESET);
    }

    fclose(f);

    /* Resumen */
    formato_tamano(total, tamano_str, sizeof(tamano_str));
    printf(GRIS "---------------------------------------------------------------------------------------------------\n" RESET);
    printf("\nResumen:\n");
    printf("  Total de mapeos:    %d\n", num_mapeos);
    printf("  Memoria total:      %s (%lu bytes)\n", tamano_str, total);
    printf("  Paginas del sistema: %lu bytes\n", pagina_sz);
    printf("  Memoria en paginas:  %lu paginas\n", total / pagina_sz);

    printf("\nPermisos acumulados:\n");
    printf("  Lectura (r):   %d mapeos\n", perm[0]);
    printf("  Escritura (w): %d mapeos\n", perm[1]);
    printf("  Ejecucion (x): %d mapeos\n", perm[2]);
    printf("  Privados (p):  %d mapeos\n", perm[3]);
}

/* Lista todos los procesos disponibles */
void listar_procesos(void) {
    DIR *dir;
    struct dirent *entrada;
    int count = 0;

    dir = opendir("/proc");
    if (dir == NULL) {
        perror("No se pudo abrir /proc");
        return;
    }

    printf(CIAN "\n--- Procesos Disponibles ---\n\n" RESET);
    printf(AMARILLO "%-8s %s\n" RESET, "PID", "Nombre");
    printf(GRIS "-----------------------------------\n" RESET);

    while ((entrada = readdir(dir)) != NULL) {
        if (es_numero(entrada->d_name)) {
            char ruta[512], nombre[256] = "?";
            FILE *f;

            snprintf(ruta, sizeof(ruta), "/proc/%s/comm", entrada->d_name);
            f = fopen(ruta, "r");
            if (f) {
                if (fgets(nombre, sizeof(nombre), f)) {
                    nombre[strcspn(nombre, "\n")] = '\0';
                }
                fclose(f);
            }

            printf("%-8s %s\n", entrada->d_name, nombre);
            count++;
        }
    }

    closedir(dir);
    printf(GRIS "-----------------------------------\n" RESET);
    printf("Total: %d procesos\n", count);
}

/* Uso del programa */
void uso(const char *prog) {
    fprintf(stderr,
        "Uso: %s <opcion> [PID]\n\n"
        "Opciones:\n"
        "  1 <PID>   Analizar mapa de memoria de un proceso\n"
        "  2         Listar procesos activos\n"
        "  3         Mostrar ayuda\n"
        "\n"
        "Ejemplo:\n"
        "  %s 1 1234    # Analizar proceso PID 1234\n"
        "  %s 2         # Ver todos los procesos\n",
        prog, prog, prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        uso(argv[0]);
        return 1;
    }

    switch (atoi(argv[1])) {
        case 1:
            if (argc < 3) {
                printf("[!] Falta el PID. Use: %s 1 <PID>\n", argv[0]);
                return 1;
            }
            if (!es_numero(argv[2])) {
                printf("[!] PID invalido: %s\n", argv[2]);
                return 1;
            }
            mostrar_info_proceso(argv[2]);
            analizar_mapa_memoria(argv[2]);
            break;

        case 2:
            listar_procesos();
            break;

        case 3:
            uso(argv[0]);
            break;

        default:
            uso(argv[0]);
            return 1;
    }

    return 0;
}
