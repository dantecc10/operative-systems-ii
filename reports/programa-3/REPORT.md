# Reporte de Programa 3 - Simulador de administración de memoria y paginación

## 1. Portada

- Materia: Sistemas Operativos II
- Actividad: Programa 3 (administración de memoria)
- Alumno: Dante Castelán Carpinteyro
- Fecha: 19 de agosto de 2026
- Lenguaje: C (gcc 13.3.0, Linux)

## 2. Objetivo

Desarrollar un simulador de administración de memoria con paginación en C que permita:

1. Visualizar la memoria física dividida en marcos de página.
2. Crear procesos que se dividen en bloques y se cargan en marcos libres.
3. Liberar procesos y reasignar los marcos liberados a otros procesos en espera.
4. Ejecutar una demostración automática que muestre todo el ciclo de vida.

## 3. Instrucciones de la actividad

> Desarrollar un simulador de administración de memoria y paginación que permita crear procesos, asignarles bloques de memoria, visualizar el estado de la memoria física y liberar procesos para reasignar memoria.

## 4. Requisitos y entorno

- Sistema operativo: Linux.
- Compilador: gcc.
- Librerías: solo las del estándar (`stdio.h`, `stdlib.h`, `string.h`, `unistd.h`). Sin dependencias externas.

Compilación:

```bash
cd programs
gcc -O2 -Wall -Wextra -o program-3 program-3.c
```

## 5. Implementación realizada

### 5.1 Estructura de datos: el sistema simulado

El núcleo del simulador es una estructura `Sistema` que contiene todo el estado del sistema de paginación:

```c
#define TOTAL_FRAMES 16
#define MAX_PROCESOS 10
#define MAX_NOMBRE 30

typedef struct {
    int pid;
    char nombre[MAX_NOMBRE];
    int bloques_total;
    int bloques_cargados;
} Proceso;

typedef struct {
    int marcos[TOTAL_FRAMES];    /* -1 = libre, otherwise = PID */
    Proceso procesos[MAX_PROCESOS];
    int num_procesos;
} Sistema;
```

**Explicación:** La memoria física se representa como un arreglo de 16 enteros. Cada posición es un "marco de página". Si el valor es `-1`, el marco está libre; si es un numero positivo, indica el PID del proceso que lo ocupa. La cola de procesos es un arreglo de hasta 10 procesos, cada uno con su PID, nombre, total de bloques requeridos y cuántos se han cargado realmente.

### 5.2 Inicialización del sistema

```c
void sistema_init(Sistema *s) {
    int i;
    for (i = 0; i < TOTAL_FRAMES; i++)
        s->marcos[i] = -1;
    s->num_procesos = 0;
}
```

**Explicación:** Al iniciar, todos los marcos se marcan como libres (`-1`) y no hay procesos en la cola. Esto es equivalente a lo que hace un SO cuando arranca: toda la memoria física está disponible.

### 5.3 Asignación de memoria (first-fit)

```c
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
```

**Explicación:** Esta es la función clave. Implementa el algoritmo **first-fit**: recorre la memoria física buscando el primer marco libre y lo asigna al primer proceso que tenga bloques pendientes. Si la memoria se llena, se detiene. Se llama después de cada operación (crear o liberar proceso) para mantener el estado consistente.

La diferencia con `best-fit` o `worst-fit` es que first-fit es mas rápido (se detiene en el primer hueco que encuentra) y en la práctica tiene un rendimiento comparable para cargas de trabajo típicas.

### 5.4 Búsqueda de procesos

```c
int buscar_proceso(const Sistema *s, int pid) {
    int i;
    for (i = 0; i < s->num_procesos; i++) {
        if (s->procesos[i].pid == pid)
            return i;
    }
    return -1;
}
```

**Explicación:** Búsqueda lineal por PID. Retorna el índice del proceso en el arreglo, o `-1` si no existe. Se usa para verificar PIDs duplicados al crear y para encontrar procesos al liberar.

### 5.5 Agregar proceso al sistema

```c
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
    return 0;
}
```

**Explicación:** Antes de agregar valida tres condiciones: que la cola no esté llena, que el PID no exista ya, y que los bloques sean válidos (1 a 16). Si todo está bien, registra el proceso con 0 bloques cargados y llama a `asignar_memoria` para intentar cargar sus bloques inmediatamente. El retorno indica el resultado: 0 = éxito, -1 = cola llena, -2 = PID duplicado, -3 = bloques inválidos.

### 5.6 Liberar proceso y reasignar memoria

```c
int liberar_proceso(Sistema *s, int pid) {
    int idx, i, j;
    char nombre_eliminado[MAX_NOMBRE];

    idx = buscar_proceso(s, pid);
    if (idx == -1)
        return -1;

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

    return 0;
}
```

**Explicación:** Este es el flujo inverso a la creación. Primero busca el proceso por PID. Luego recorre la memoria física y libera todos los marcos que le pertenecían (los marca como `-1`). Después elimina el proceso de la cola desplazando los elementos restantes hacia atrás. Finalmente llama a `asignar_memoria` para que los marcos liberados se asignen a otros procesos que estaban en espera (estados `EN ESPERA (SWAP)` o `PARCIALMENTE CARGADO`).

Este patrón es idéntico a como un SO real maneja la terminación de procesos: liberar marcos, actualizar tablas, y despertar procesos bloqueados que ahora pueden entrar en memoria.

### 5.7 Visualización con colores ANSI

```c
printf("Memoria Fisica (Marcos de Pagina: %d):\n[ ", TOTAL_FRAMES);
for (i = 0; i < TOTAL_FRAMES; i++) {
    if (s->marcos[i] == -1)
        printf(GRIS "." RESET " ");
    else
        printf(VERDE "P%d" RESET " ", s->marcos[i]);
}
printf("]\n\n");
```

**Explicación:** Se usan secuencias de escape ANSI para colorear la salida:
- Verde para marcos ocupados (`P100`, `P200`, etc.).
- Gris para marcos libres (`.`).
- Rojo para procesos en espera.
- Amarillo para procesos parcialmente cargados.

Esto hace que la visualización sea inmediatamente intuitiva: se puede ver de un vistazo cuánta memoria está ocupada y por cual proceso.

### 5.8 Modo de ejecución por argumento

```c
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
        default:
            uso(argv[0]);
            return 1;
    }

    return 0;
}
```

**Explicación:** En lugar de un menú interactivo con `scanf`, el programa recibe la opción como argumento de línea de comandos. Esto permite tanto el uso interactivo (`./program-3 1` y luego ingresar datos) como el uso automatizado.

### 5.9 Modo automático (demostración)

El modo automático ejecuta una secuencia predefinida que muestra:

1. Creación de procesos con asignación inmediata de memoria.
2. Agregación de más procesos hasta llenar la memoria.
3. Liberación de un proceso y reasignación automática.
4. Creación de un proceso grande que excede la memoria disponible.
5. Liberación de todos los procesos para demostrar la limpieza completa.

Cada paso incluye una pausa (`usleep`) para que el usuario pueda observar los cambios en la pantalla.

## 6. Ejecución

```bash
cd programs
gcc -O2 -Wall -Wextra -o program-3 program-3.c

# Modo automático
./program-3 3

# Crear proceso (interactivo)
./program-3 1

# Liberar proceso (interactivo)
./program-3 2
```

## 7. Cumplimiento del enunciado

- Simula memoria fisica con marcos de página: cumplido (arreglo de 16 enteros).
- Permite crear procesos que se dividen en bloques: cumplido (`agregar_proceso`).
- Asigna bloques a marcos libres (first-fit): cumplido (`asignar_memoria`).
- Muestra el estado de la memoria y los procesos: cumplido (`mostrar_estado` con colores ANSI).
- Permite liberar procesos y reasignar memoria: cumplido (`liberar_proceso`).
- Visualización dinámica del estado: cumplido (se muestra antes y después de cada operación).
- Modo automático para demostración: cumplido (`modo_auto`).

## 8. Conclusión

Se demuestran los conceptos fundamentales de paginación en sistemas operativos: la memoria física se divide en marcos de tamaño fijo, los procesos se dividen en bloques (páginas) que se cargan en marcos disponibles, y cuando un proceso termina sus marcos se liberan para ser reasignados.

Lo más interesante de esta implementación es el patrón de "asignar después de cada operación". En un SO real, la asignación de páginas es más compleja (involucra tablas de página, TLB, swaps, etc.), pero el principio básico es el mismo: buscar marcos libres y asignarlos a procesos que los necesiten. El algoritmo first-fit es suficiente para esta simulación y es el mismo que usan algunos sistemas reales por su simplicidad y velocidad.

La visualización con colores ANSI hace evidente el estado del sistema en cada momento: se puede ver instantaneamente cuantos marcos están ocupados, por cuáles procesos, y cuáles procesos están esperando memoria.

## 9. Anexo A - Comandos usados

```bash
cd programs
gcc -O2 -Wall -Wextra -o program-3 program-3.c
./program-3 3    # demo automatica
./program-3 1    # crear proceso
./program-3 2    # liberar proceso
```

## 10. Bitácora de prompts

Prompts usados para llegar al resultado final:

| # | Prompt textual | ¿Sirve? | LLM / Agente |
|:-:|:-:|:-:|:-:|
| 1 | **"Cómo representar la memoria física en C para simular paginación?"** — Conduce al arreglo `int marcos[TOTAL_FRAMES]` donde `-1` es libre y cualquier otro valor es el PID. | Sí | Gemini |
| 2 | **"Cómo implementar la asignación first-fit en C?"** — Conduce a la función `asignar_memoria` que recorre la memoria buscando el primer marco libre. | Sí | Gemini |
| 3 | **"Cómo manejar la liberación de un proceso y reasignar sus marcos a otros?"** — Conduce a `liberar_proceso` que marca marcos como libres, elimina el proceso de la cola, y llama a `asignar_memoria` para reasignar. | Sí | Gemini |
| 4 | **"Cómo mostrar el estado de la memoria con colores en terminal?"** — Conduce al uso de secuencias ANSI (`\033[32m` para verde, `\033[31m` para rojo, etc.). | Sí | Gemini |
| 5 | **"Cómo hacer que el programa reciba la opcion por argumento en vez de un menú?"** — Conduce al uso de `argv[1]` con `atoi` y un `switch` en `main`. | Sí | Gemini |
| 6 | **"Cómo crear una demostración automática que muestre todo el ciclo de vida?"** — Conduce a `modo_auto` que crea procesos, muestra estado, libera, y repite con pausas. | Sí | Gemini |
| 7 | **"Cómo evitar problemas con scanf y el buffer de entrada?"** — Conduce al uso de `fgets` + `strtol` en lugar de `scanf` directo. | Sí | Gemini |
| 8 | **"Cómo validar PIDs duplicados y bloques inválidos antes de agregar un proceso?"** — Conduce a las validaciones en `agregar_proceso` que retornan codigos de error especificos. | Sí | Gemini |
