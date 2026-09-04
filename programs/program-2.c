#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

#define ANCHO_BARRA 70
#define TAM_RUTA 1024
#define MAX_NOMBRE 256

/* Un binario encontrado en el disco */
typedef struct
{
    char ruta[TAM_RUTA];     /* Ruta completa al binario */
    char nombre[MAX_NOMBRE]; /* Nombre del archivo */
    off_t tamano;            /* Tamaño en bytes (st_size) */
} Archivo;

/* Un binario ya cargado dentro del bloque de memoria */
typedef struct
{
    char nombre[MAX_NOMBRE];
    off_t tamano;
    size_t offset; /* Desfase dentro del bloque */
} Cargado;

static const char *DIRECTORIOS[] = {
    "/usr/bin",
    "/usr/sbin",
    "/usr/local/bin",
    "/usr/local/sbin",
    "/usr/libexec",
    "/usr/lib",
    "/opt",
    NULL};

static void uso(const char *prog)
{
    fprintf(stderr,
            "Uso: %s [modo]\n"
            "  modo 1 (por defecto): tope = RAM fisica del sistema\n"
            "  modo 2              : tope = RAM fisica + swap\n"
            "  modo 3              : sin tope (lo maximo que malloc devuelva)\n",
            prog);
}

/* Total de memoria RAM fisica del sistema */
static size_t physical_ram(void)
{
    long paginas = sysconf(_SC_PHYS_PAGES);
    long tam_pagina = sysconf(_SC_PAGE_SIZE);

    if (paginas < 1 || tam_pagina < 1)
        return 0;

    return (size_t)paginas * (size_t)tam_pagina;
}

/* Total de RAM fisica + espacio de swap */
static size_t ram_and_swap(void)
{
    struct sysinfo info;

    if (sysinfo(&info) != 0)
        return 0;

    return (size_t)info.totalram + (size_t)info.totalswap;
}

/*
 * Verifica si malloc puede otorgar un bloque de 'tamano' bytes.
 * malloc rechaza peticiones que exceden el limite de memoria
 * comprometida, asi que basta con probar la llamada.  La utilidad
 * real del bloque se confirma mas adelante cuando se copian los
 * binarios dentro de el (eso "toca" cada pagina).
 */
static int se_puede(size_t tamano)
{
    void *p;

    if (tamano == 0)
        return 0;

    p = malloc(tamano);
    if (p == NULL)
        return 0;

    free(p);
    return 1;
}

/*
 * Encuentra el tamaño más grande (<= tope) que malloc puede
 * reservar y que además sea utilizable.  Si tope es SIZE_MAX
 * primero duplica 1, 2, 4, ... hasta que falle para acotar el
 * intervalo y luego hace búsqueda binaria.
 */
static size_t buscar_maximo(size_t tope)
{
    size_t lo, hi, mid;

    if (tope == SIZE_MAX)
    {
        size_t n = 1;

        while (1)
        {
            if (se_puede(n))
            {
                if (n > SIZE_MAX / 2)
                {
                    lo = n;
                    hi = SIZE_MAX;
                    break;
                }
                n *= 2;
            }
            else
            {
                lo = n / 2;
                hi = n;
                break;
            }
        }
    }
    else
    {
        lo = 0;
        hi = tope;
    }

    while (lo < hi)
    {
        mid = lo + (hi - lo + 1) / 2;

        if (se_puede(mid))
            lo = mid;
        else
            hi = mid - 1;
    }

    return lo;
}

/* Ordena los archivos por tamaño, de mayor a menor */
static int comparar_desc(const void *a, const void *b)
{
    off_t ta = ((const Archivo *)a)->tamano;
    off_t tb = ((const Archivo *)b)->tamano;

    if (ta > tb)
        return -1;
    if (ta < tb)
        return 1;
    return 0;
}

/*
 * Dibuja en pantalla (ASCII) el estado actual del bloque de memoria:
 * recuadro con datos, barra de mapa de memoria donde cada programa
 * es un segmento proporcional a su tamaño, y leyenda de programas.
 */
static void dibujar(size_t bloque_tam, size_t usado,
                    const Cargado *cargados, size_t n,
                    const char *ultimo)
{
    char barra[ANCHO_BARRA + 1];
    const char *relleno = "#=@%*~+O";
    size_t pos = 0;
    size_t i;

    /* Limpiar pantalla y volver al inicio (efecto dinamico) */
    printf("\033[2J\033[H");

    printf("+-------------------------------------------------------------------------------+\n");
    printf("|  BLOQUE DE MEMORIA RESERVADO CON MALLOC\n");
    printf("|  Tamanio del bloque : %12.2f GiB   (%zu bytes)\n",
           (double)bloque_tam / (1024.0 * 1024.0 * 1024.0), bloque_tam);
    printf("|  Ocupado            : %12.2f GiB   (%zu bytes, %5.2f%%)\n",
           (double)usado / (1024.0 * 1024.0 * 1024.0), usado,
           100.0 * (double)usado / (double)bloque_tam);
    printf("|  Programas lanzados : %zu\n", n);
    printf("+-------------------------------------------------------------------------------+\n");
    printf("\n>>> Lanzando: %s <<<\n\n", ultimo);

    /* Barra de mapa de memoria */
    memset(barra, '.', ANCHO_BARRA);
    barra[ANCHO_BARRA] = '\0';

    for (i = 0; i < n && pos < ANCHO_BARRA; i++)
    {
        size_t w = (size_t)((double)cargados[i].tamano *
                            (double)ANCHO_BARRA / (double)bloque_tam);
        size_t l;

        if (w == 0)
            w = 1;
        if (pos + w > ANCHO_BARRA)
            w = ANCHO_BARRA - pos;

        memset(barra + pos, relleno[i % strlen(relleno)], w);

        /* Si el segmento es ancho, centrar el nombre del programa */
        l = strlen(cargados[i].nombre);
        if (l + 2 <= w)
        {
            size_t ini = pos + (w - l) / 2;
            memcpy(barra + ini, cargados[i].nombre, l);
        }

        pos += w;
    }

    printf("\n  Memoria: [%s] %5.2f%% ocupada\n", barra,
           100.0 * (double)usado / (double)bloque_tam);

    /* Leyenda de programas lanzados */
    printf("\n  Programas lanzados:\n");
    for (i = 0; i < n; i++)
    {
        printf("   %3zu) %-28s %12lld B   offset %12zu   (%5.2f%%)\n",
               i + 1, cargados[i].nombre, (long long)cargados[i].tamano,
               cargados[i].offset,
               100.0 * (double)cargados[i].tamano / (double)bloque_tam);
    }

    printf("   ------------------------------------------------------------\n");
    printf("   Espacio libre: %zu bytes (%.2f%%)\n",
           bloque_tam - usado,
           100.0 * (double)(bloque_tam - usado) / (double)bloque_tam);

    fflush(stdout);
}

int main(int argc, char **argv)
{
    int modo = 1; // 1; Comento la asignación del modo 1 para limitar el tamaño del bloque
    size_t tope;
    size_t bloque_tam;
    unsigned char *memoria;
    size_t i;

    Archivo *archivos = NULL;
    size_t n_archivos = 0;
    size_t cap_archivos = 0;

    Cargado *cargados = NULL;
    size_t n_cargados = 0;
    size_t cap_cargados = 0;

    size_t usado = 0;

    if (argc > 1)
    {
        modo = atoi(argv[1]);
        if (modo < 1 || modo > 3)
        {
            uso(argv[0]);
            return 1;
        }
    }

    switch (modo)
    {
    case 1:
        tope = physical_ram();
        break;
    case 2:
        tope = ram_and_swap();
        break;
    case 3:
        tope = 1143525669; // Aquí hardcodeo un valor (lo de 14 GB / 14)
        break;
    default:
        tope = SIZE_MAX;
        break;
    }

    printf("Calculando el bloque de memoria mas grande utilizable con malloc...\n");
    fflush(stdout);

    bloque_tam = buscar_maximo(tope);
    if (bloque_tam == 0)
    {
        fprintf(stderr, "No se pudo reservar memoria.\n");
        return 1;
    }

    memoria = (unsigned char *)malloc(bloque_tam);
    if (memoria == NULL)
    {
        perror("malloc");
        return 1;
    }

    printf("Bloque reservado: %.2f GiB (%zu bytes) en %p\n\n",
           (double)bloque_tam / (1024.0 * 1024.0 * 1024.0), bloque_tam,
           (void *)memoria);

    /*
     * Recorrer el disco con opendir/readdir y usar stat para saber
     * el tamaño de cada archivo regular.
     */
    for (i = 0; DIRECTORIOS[i] != NULL; i++)
    {
        DIR *dir = opendir(DIRECTORIOS[i]);
        struct dirent *entrada;

        if (dir == NULL)
            continue;

        while ((entrada = readdir(dir)) != NULL)
        {
            char ruta[TAM_RUTA];
            struct stat st;

            if (strcmp(entrada->d_name, ".") == 0 ||
                strcmp(entrada->d_name, "..") == 0)
                continue;

            snprintf(ruta, sizeof(ruta), "%s/%s",
                     DIRECTORIOS[i], entrada->d_name);

            if (stat(ruta, &st) != 0)
                continue;

            if (!S_ISREG(st.st_mode) || st.st_size <= 0)
                continue;

            if (n_archivos == cap_archivos)
            {
                size_t nueva_cap = cap_archivos ? cap_archivos * 2 : 1024;
                Archivo *tmp = (Archivo *)realloc(archivos,
                                                  nueva_cap * sizeof(Archivo));
                if (tmp == NULL)
                {
                    perror("realloc");
                    continue;
                }
                archivos = tmp;
                cap_archivos = nueva_cap;
            }

            strncpy(archivos[n_archivos].ruta, ruta, TAM_RUTA - 1);
            archivos[n_archivos].ruta[TAM_RUTA - 1] = '\0';
            strncpy(archivos[n_archivos].nombre, entrada->d_name,
                    MAX_NOMBRE - 1);
            archivos[n_archivos].nombre[MAX_NOMBRE - 1] = '\0';
            archivos[n_archivos].tamano = st.st_size;
            n_archivos++;
        }

        closedir(dir);
    }

    if (n_archivos == 0)
    {
        fprintf(stderr, "No se encontraron binarios en el disco.\n");
        free(memoria);
        return 1;
    }

    /* Ordenar de mayor a menor para llenar mejor el bloque */
    qsort(archivos, n_archivos, sizeof(Archivo), comparar_desc);

    printf("Binarios encontrados en el disco: %zu\n", n_archivos);
    printf("Cargando programas hasta llenar el bloque...\n\n");
    fflush(stdout);

    /* Cargar binarios mientras quepa espacio */
    for (i = 0; i < n_archivos; i++)
    {
        FILE *f;
        size_t leido;

        if (archivos[i].tamano > (off_t)(bloque_tam - usado))
            continue;

        f = fopen(archivos[i].ruta, "rb");
        if (f == NULL)
            continue;

        leido = fread(memoria + usado, 1, (size_t)archivos[i].tamano, f);
        fclose(f);

        if (leido != (size_t)archivos[i].tamano)
            continue;

        if (n_cargados == cap_cargados)
        {
            size_t nueva_cap = cap_cargados ? cap_cargados * 2 : 64;
            Cargado *tmp = (Cargado *)realloc(cargados,
                                              nueva_cap * sizeof(Cargado));
            if (tmp == NULL)
            {
                perror("realloc");
                break;
            }
            cargados = tmp;
            cap_cargados = nueva_cap;
        }

        strncpy(cargados[n_cargados].nombre, archivos[i].nombre,
                MAX_NOMBRE - 1);
        cargados[n_cargados].nombre[MAX_NOMBRE - 1] = '\0';
        cargados[n_cargados].tamano = archivos[i].tamano;
        cargados[n_cargados].offset = usado;

        usado += (size_t)archivos[i].tamano;
        n_cargados++;

        dibujar(bloque_tam, usado, cargados, n_cargados, archivos[i].ruta);
        usleep(150000);
    }

    /* Resumen final */
    printf("\n+-------------------------------------------------------------------------------+\n");
    printf("|  RESUMEN FINAL\n");
    printf("|  Bloque reservado : %zu bytes (%.2f GiB)\n", bloque_tam,
           (double)bloque_tam / (1024.0 * 1024.0 * 1024.0));
    printf("|  Ocupado          : %zu bytes (%.2f%%)\n", usado,
           100.0 * (double)usado / (double)bloque_tam);
    printf("|  Programas lanzados: %zu de %zu binarios encontrados\n",
           n_cargados, n_archivos);
    printf("|  Espacio libre    : %zu bytes (%.2f%%)\n",
           bloque_tam - usado,
           100.0 * (double)(bloque_tam - usado) / (double)bloque_tam);
    if (usado == bloque_tam)
        printf("|  Estado: BLOQUE LLENO\n");
    else
        printf("|  Estado: no caben mas binarios en el espacio restante\n");
    printf("+-------------------------------------------------------------------------------+\n");

    free(cargados);
    free(archivos);
    free(memoria);

    return 0;
}
