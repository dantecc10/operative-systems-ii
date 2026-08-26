#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TOTAL_FRAMES 16
#define MAX_PROCESOS 10
#define MAX_NOMBRE 30

/* Colores ANSI */
#define RESET   "\033[0m"
#define ROJO    "\033[31m"
#define VERDE   "\033[32m"
#define AMARILLO "\033[33m"
#define GRIS    "\033[90m"

/* Estructura que representa un proceso */
typedef struct {
    int pid;
    char nombre[MAX_NOMBRE];
    int bloques_total;
    int bloques_cargados;
} Proceso;

/* Estado completo del sistema de paginacion */
typedef struct {
    int marcos[TOTAL_FRAMES];    /* -1 = libre, otherwise = PID */
    Proceso procesos[MAX_PROCESOS];
    int num_procesos;
} Sistema;

/* ----------------------------------------------------------------
 *  Inicializacion
 * ---------------------------------------------------------------- */
void sistema_init(Sistema *s) {
    int i;
    for (i = 0; i < TOTAL_FRAMES; i++)
        s->marcos[i] = -1;
    s->num_procesos = 0;
}

/* ----------------------------------------------------------------
 *  Asignacion de memoria (first-fit)
 *  Recorre los procesos y asigna marcos libres a los que tengan
 *  bloques pendientes.
 * ---------------------------------------------------------------- */
void asignar_memoria(Sistema *s) {
    int j, i;
    for (j = 0; j < s->num_procesos; j++) {
        Proceso *p = &s->procesos[j];
        while (p->bloques_cargados < p->bloques_total) {
            int libre = -1;
            for (i = 0; i < TOTAL_FRAMES; i++) {
                if (s->marcos[i] == -1) {
                    libre = i;
                    break;
                }
            }
            if (libre == -1)
                break; /* memoria fisica llena */
            s->marcos[libre] = p->pid;
            p->bloques_cargados++;
        }
    }
}

/* ----------------------------------------------------------------
 *  Buscar proceso por PID ( retorna indice o -1 )
 * ---------------------------------------------------------------- */
int buscar_proceso(const Sistema *s, int pid) {
    int i;
    for (i = 0; i < s->num_procesos; i++) {
        if (s->procesos[i].pid == pid)
            return i;
    }
    return -1;
}

/* ----------------------------------------------------------------
 *  Mostrar estado del sistema
 * ---------------------------------------------------------------- */
void mostrar_estado(const Sistema *s) {
    int i;
    printf("\n");
    printf("============================================================\n");
    printf("  SIMULADOR DE ADMINISTRACION DE MEMORIA Y PAGINACION\n");
    printf("============================================================\n\n");

    /* Memoria fisica */
    printf("Memoria Fisica (Marcos de Pagina: %d):\n[ ", TOTAL_FRAMES);
    for (i = 0; i < TOTAL_FRAMES; i++) {
        if (s->marcos[i] == -1)
            printf(GRIS "." RESET " ");
        else
            printf(VERDE "P%d" RESET " ", s->marcos[i]);
    }
    printf("]\n\n");

    /* Tabla de procesos */
    printf("Cola de Procesos Activos:\n");
    printf("---------------------------------------------------------------------------\n");
    printf("%-5s | %-15s | %-8s | %-8s | %s\n",
           "PID", "Nombre", "Bloques", "Cargados", "Estado");
    printf("---------------------------------------------------------------------------\n");

    if (s->num_procesos == 0) {
        printf("                     (No hay procesos activos)                     \n");
    } else {
        for (i = 0; i < s->num_procesos; i++) {
            const Proceso *p = &s->procesos[i];
            const char *estado;

            if (p->bloques_cargados >= p->bloques_total)
                estado = VERDE "EN MEMORIA (COMPLETO)" RESET;
            else if (p->bloques_cargados > 0)
                estado = AMARILLO "PARCIALMENTE CARGADO" RESET;
            else
                estado = ROJO "EN ESPERA (SWAP)" RESET;

            printf("%-5d | %-15s | %-8d | %-8d | %s\n",
                   p->pid, p->nombre, p->bloques_total,
                   p->bloques_cargados, estado);
        }
    }
    printf("---------------------------------------------------------------------------\n\n");
}

/* ----------------------------------------------------------------
 *  Agregar proceso al sistema
 * ---------------------------------------------------------------- */
int agregar_proceso(Sistema *s, int pid, const char *nombre, int bloques) {
    Proceso *p;

    if (s->num_procesos >= MAX_PROCESOS)
        return -1; /* cola llena */

    if (buscar_proceso(s, pid) != -1)
        return -2; /* PID duplicado */

    if (bloques <= 0 || bloques > TOTAL_FRAMES)
        return -3; /* bloques invalidos */

    p = &s->procesos[s->num_procesos];
    p->pid = pid;
    strncpy(p->nombre, nombre, MAX_NOMBRE - 1);
    p->nombre[MAX_NOMBRE - 1] = '\0';
    p->bloques_total = bloques;
    p->bloques_cargados = 0;
    s->num_procesos++;

    asignar_memoria(s);
    return 0; /* exito */
}

/* ----------------------------------------------------------------
 *  Liberar proceso (libera marcos y reasigna)
 * ---------------------------------------------------------------- */
int liberar_proceso(Sistema *s, int pid) {
    int idx, i, j;
    char nombre_eliminado[MAX_NOMBRE];

    idx = buscar_proceso(s, pid);
    if (idx == -1)
        return -1; /* no encontrado */

    /* Copiar nombre antes de eliminar */
    strncpy(nombre_eliminado, s->procesos[idx].nombre, MAX_NOMBRE - 1);
    nombre_eliminado[MAX_NOMBRE - 1] = '\0';

    /* Liberar marcos en memoria fisica */
    for (i = 0; i < TOTAL_FRAMES; i++) {
        if (s->marcos[i] == pid)
            s->marcos[i] = -1;
    }

    /* Eliminar de la cola (desplazar) */
    for (j = idx; j < s->num_procesos - 1; j++)
        s->procesos[j] = s->procesos[j + 1];
    s->num_procesos--;

    /* Reasignar bloques pendientes a otros procesos */
    asignar_memoria(s);

    printf("\n[-] Proceso '%s' (PID %d) finalizado y memoria liberada.\n",
           nombre_eliminado, pid);
    return 0;
}

/* ----------------------------------------------------------------
 *  Modo interactivo: crear proceso(s)
 * ---------------------------------------------------------------- */
void modo_crear(Sistema *s) {
    int pid, bloques;
    char nombre[MAX_NOMBRE];
    char linea[128];
    char respuesta;

    do {
        if (s->num_procesos >= MAX_PROCESOS) {
            printf("\n[!] Cola de procesos llena (maximo %d).\n", MAX_PROCESOS);
            return;
        }

        printf("\n--- CREAR NUEVO PROCESO ---\n");

        printf("PID: ");
        if (fgets(linea, sizeof(linea), stdin) == NULL) return;
        pid = atoi(linea);
        if (pid <= 0) {
            printf("[!] PID invalido.\n");
            continue;
        }

        if (buscar_proceso(s, pid) != -1) {
            printf("[!] Ya existe un proceso con PID %d.\n", pid);
            continue;
        }

        printf("Nombre: ");
        if (fgets(nombre, sizeof(nombre), stdin) == NULL) return;
        nombre[strcspn(nombre, "\n")] = '\0';

        printf("Bloques (1-%d): ", TOTAL_FRAMES);
        if (fgets(linea, sizeof(linea), stdin) == NULL) return;
        bloques = atoi(linea);

        switch (agregar_proceso(s, pid, nombre, bloques)) {
            case -1: printf("[!] Cola llena.\n"); break;
            case -2: printf("[!] PID duplicado.\n"); break;
            case -3: printf("[!] Bloques invalidos.\n"); break;
            default:
                printf("[+] Proceso '%s' agregado.\n", nombre);
                mostrar_estado(s);
                break;
        }

        if (s->num_procesos >= MAX_PROCESOS) {
            printf("\n[!] Cola de procesos llena (maximo %d).\n", MAX_PROCESOS);
            return;
        }

        printf("\nAgregar otro proceso? (s/n): ");
        if (fgets(linea, sizeof(linea), stdin) == NULL) return;
        respuesta = linea[0];

    } while (respuesta == 's' || respuesta == 'S');
}

/* ----------------------------------------------------------------
 *  Modo interactivo: liberar proceso
 * ---------------------------------------------------------------- */
void modo_liberar(Sistema *s) {
    int pid;
    char linea[128];

    if (s->num_procesos == 0) {
        printf("\n[!] No hay procesos activos.\n");
        return;
    }

    printf("\n--- FINALIZAR PROCESO ---\n");
    printf("PID a finalizar: ");
    if (fgets(linea, sizeof(linea), stdin) == NULL) return;
    pid = atoi(linea);

    if (liberar_proceso(s, pid) == -1)
        printf("[!] No se encontro proceso con PID %d.\n", pid);
}

/* ----------------------------------------------------------------
 *  Modo menu interactivo: agregar + liberar + ver estado
 * ---------------------------------------------------------------- */
static void menu_mostrar_opciones(void) {
    printf("\n1. Agregar proceso\n");
    printf("2. Liberar proceso\n");
    printf("3. Ver estado\n");
    printf("4. Salir\n");
    printf("Seleccione: ");
}

void modo_menu(Sistema *s) {
    int opcion;
    char linea[128];
    int pid, bloques;
    char nombre[MAX_NOMBRE];

    do {
        mostrar_estado(s);
        menu_mostrar_opciones();

        if (fgets(linea, sizeof(linea), stdin) == NULL) break;
        opcion = atoi(linea);

        switch (opcion) {
            case 1:
                /* Agregar proceso */
                if (s->num_procesos >= MAX_PROCESOS) {
                    printf("[!] Cola de procesos llena (maximo %d).\n", MAX_PROCESOS);
                    break;
                }

                printf("PID: ");
                if (fgets(linea, sizeof(linea), stdin) == NULL) return;
                pid = atoi(linea);
                if (pid <= 0) {
                    printf("[!] PID invalido.\n");
                    break;
                }

                if (buscar_proceso(s, pid) != -1) {
                    printf("[!] Ya existe un proceso con PID %d.\n", pid);
                    break;
                }

                printf("Nombre: ");
                if (fgets(nombre, sizeof(nombre), stdin) == NULL) return;
                nombre[strcspn(nombre, "\n")] = '\0';

                printf("Bloques (1-%d): ", TOTAL_FRAMES);
                if (fgets(linea, sizeof(linea), stdin) == NULL) return;
                bloques = atoi(linea);

                switch (agregar_proceso(s, pid, nombre, bloques)) {
                    case -1: printf("[!] Cola llena.\n"); break;
                    case -2: printf("[!] PID duplicado.\n"); break;
                    case -3: printf("[!] Bloques invalidos.\n"); break;
                    default: printf("[+] Proceso '%s' agregado.\n", nombre); break;
                }
                break;

            case 2:
                /* Liberar proceso */
                if (s->num_procesos == 0) {
                    printf("[!] No hay procesos activos.\n");
                    break;
                }

                printf("PID a finalizar: ");
                if (fgets(linea, sizeof(linea), stdin) == NULL) return;
                pid = atoi(linea);

                if (liberar_proceso(s, pid) == -1)
                    printf("[!] No se encontro proceso con PID %d.\n", pid);
                break;

            case 3:
                /* Ver estado (ya se muestra al inicio del ciclo) */
                break;

            case 4:
                printf("Saliendo del menu...\n");
                break;

            default:
                printf("[!] Opcion invalida.\n");
                break;
        }
    } while (opcion != 4);
}

/* ----------------------------------------------------------------
 *  Modo automatico (demostracion)
 * ---------------------------------------------------------------- */
void modo_auto(Sistema *s) {
    printf("\n>>> MODO AUTOMATICO - Demostracion de paginacion <<<\n");
    usleep(800000);

    /* Paso 1: crear procesos */
    printf("\n[1] Creando procesos...\n");
    agregar_proceso(s, 100, "navegador", 5);
    mostrar_estado(s);
    usleep(1200000);

    agregar_proceso(s, 200, "editor", 3);
    mostrar_estado(s);
    usleep(1200000);

    agregar_proceso(s, 300, "terminal", 4);
    mostrar_estado(s);
    usleep(1200000);

    /* Paso 2: crear mas procesos para llenar memoria */
    printf("\n[2] Agregando mas procesos...\n");
    agregar_proceso(s, 400, "servidor", 3);
    mostrar_estado(s);
    usleep(1200000);

    agregar_proceso(s, 500, "compilador", 2);
    mostrar_estado(s);
    usleep(1200000);

    /* Paso 3: liberar un proceso y ver reasignacion */
    printf("\n[3] Liberando proceso 'editor' (PID 200)...\n");
    liberar_proceso(s, 200);
    mostrar_estado(s);
    usleep(1200000);

    /* Paso 4: liberar otro y crear uno nuevo */
    printf("\n[4] Liberando 'terminal' (PID 300) y creando 'juego' (PID 600)...\n");
    liberar_proceso(s, 300);
    agregar_proceso(s, 600, "juego", 6);
    mostrar_estado(s);
    usleep(1200000);

    /* Paso 5: intentar crear mas de los que caben */
    printf("\n[5] Intentando crear proceso que excede la memoria disponible...\n");
    agregar_proceso(s, 700, "gigante", 15);
    mostrar_estado(s);
    usleep(1200000);

    /* Paso 6: limpiar todo */
    printf("\n[6] Liberando todos los procesos...\n");
    liberar_proceso(s, 100);
    liberar_proceso(s, 400);
    liberar_proceso(s, 500);
    liberar_proceso(s, 600);
    liberar_proceso(s, 700);
    mostrar_estado(s);

    printf(">>> DEMO COMPLETADA <<<\n");
}

/* ----------------------------------------------------------------
 *  Uso del programa
 * ---------------------------------------------------------------- */
void uso(const char *prog) {
    fprintf(stderr,
        "Uso: %s <opcion>\n\n"
        "Opciones:\n"
        "  1   Crear nuevo proceso (solicita PID, nombre, bloques)\n"
        "  2   Finalizar proceso (solicita PID)\n"
        "  3   Ejecutar demostracion automatica\n"
        "  4   Menu interactivo (agregar, liberar, ver estado)\n"
        "\n"
        "Ejemplo: %s 3\n",
        prog, prog);
}

/* ----------------------------------------------------------------
 *  Main
 * ---------------------------------------------------------------- */
int main(int argc, char **argv) {
    Sistema s;

    if (argc < 2) {
        uso(argv[0]);
        return 1;
    }

    sistema_init(&s);

    switch (atoi(argv[1])) {
        case 1:
            mostrar_estado(&s);
            modo_crear(&s);
            mostrar_estado(&s);
            break;
        case 2:
            mostrar_estado(&s);
            modo_liberar(&s);
            mostrar_estado(&s);
            break;
        case 3:
            modo_auto(&s);
            break;
        case 4:
            modo_menu(&s);
            break;
        default:
            uso(argv[0]);
            return 1;
    }

    return 0;
}
