# Reporte de Programa 1 - Monitor de procesos activos en /proc

## 1. Portada

- Materia: Sistemas Operativos II
- Actividad: Programa 1 (monitoreo de procesos)
- Alumno: Dante Castelán Carpinteyro
- Fecha: 19 de agosto de 2026
- Lenguaje: C (gcc 13.3.0, Linux)

## 2. Objetivo

Desarrollar un monitor de procesos en C que lea continuamente el directorio `/proc`, detecte procesos nuevos y desaparecidos, y muestre los PIDs activos en tiempo real usando una lista enlazada. El programa debe reflejar los cambios del sistema operativo sin reiniciar, mostrando en cada ciclo qué procesos están corriendo.

## 3. Instrucciones de la actividad

> Realizar un programa que monitoree los procesos en ejecución en el sistema. El programa debe leer el directorio `/proc`, identificar los PIDs de los procesos activos, y mantener actualizada la lista de procesos mostrando cuáles aparecieron, cuáles siguen activos y cuáles desaparecieron.

## 4. Requisitos y entorno

- Sistema operativo: Linux (requiere el sistema de archivos `/proc`).
- Compilador: gcc.
- Librerías: solo las del estándar y POSIX (`stdio.h`, `stdlib.h`, `dirent.h`, `ctype.h`, `unistd.h`, `sys/types.h`). Sin dependencias externas.

Compilación:

```bash
cd programs
gcc -O2 -Wall -o program-1 program-1.c
```

## 5. Implementación realizada

El programa se estructura en dos partes: una **lista enlazada** para almacenar los PIDs conocidos, y un **bucle continuo** que lee `/proc` en cada ciclo para detectar cambios.

### 5.1 Estructura de datos: lista enlazada de PIDs

Cada nodo de la lista guarda un PID y un flag `visto` que indica si el proceso apareció en el ciclo actual:

```c
typedef struct Nodo {
    int pid;
    int visto;              /* 1 = apareció en /proc este ciclo */
    struct Nodo *siguiente;
} Nodo;
```

El flag `visto` es la clave del algoritmo: al inicio de cada ciclo se ponen todos en 0, y los que reaparecen en `/proc` se marcan en 1. Los que quedan en 0 al final del ciclo ya no existen y se eliminan.

### 5.2 Funciones auxiliares de la lista

**`es_numero`** — verifica si el nombre de una entrada de `/proc` es un PID (solo dígitos):

```c
int es_numero(const char *nombre) {
    int i;
    if (nombre[0] == '\0')
        return 0;
    for (i = 0; nombre[i] != '\0'; i++) {
        if (!isdigit((unsigned char)nombre[i]))
            return 0;
    }
    return 1;
}
```

En `/proc` hay entradas como `self`, `net`, `bus` que no son PIDs. Solo las que son puramente numéricas corresponden a procesos reales.

**`buscar_pid`** — busca un PID en la lista y devuelve el nodo si existe:

```c
Nodo *buscar_pid(Nodo *lista, int pid) {
    Nodo *actual = lista;
    while (actual != NULL) {
        if (actual->pid == pid)
            return actual;
        actual = actual->siguiente;
    }
    return NULL;
}
```

**`agregar`** — agrega un PID nuevo al final de la lista:

```c
void agregar(Nodo **lista, int pid) {
    Nodo *nuevo = malloc(sizeof(Nodo));
    if (nuevo == NULL) {
        perror("Error al reservar memoria");
        return;
    }
    nuevo->pid = pid;
    nuevo->visto = 1;       /* se marca visto porque acaba de aparecer */
    nuevo->siguiente = NULL;

    if (*lista == NULL) {
        *lista = nuevo;
        return;
    }
    Nodo *actual = *lista;
    while (actual->siguiente != NULL)
        actual = actual->siguiente;
    actual->siguiente = nuevo;
}
```

### 5.3 Patrón de ciclo de vida: marcar, detectar, eliminar

El patrón funciona en tres pasos por cada ciclo del bucle:

**Paso 1 — Marcar todos como "no vistos":**

```c
void marcar_no_vistos(Nodo *lista) {
    Nodo *actual = lista;
    while (actual != NULL) {
        actual->visto = 0;
        actual = actual->siguiente;
    }
}
```

**Paso 2 — Recorrer `/proc` y marcar los que aparecen:**

```c
while ((entrada = readdir(directorio)) != NULL) {
    if (es_numero(entrada->d_name)) {
        pid = atoi(entrada->d_name);
        nodo = buscar_pid(lista, pid);
        if (nodo != NULL)
            nodo->visto = 1;       /* ya existia, sigue activo */
        else
            agregar(&lista, pid);   /* es nuevo */
    }
}
```

**Paso 3 — Eliminar los que no aparecieron:**

```c
void eliminar_no_vistos(Nodo **lista) {
    Nodo *actual = *lista;
    Nodo *anterior = NULL;
    while (actual != NULL) {
        if (actual->visto == 0) {
            if (anterior == NULL)
                *lista = actual->siguiente;
            else
                anterior->siguiente = actual->siguiente;
            free(actual);
            actual = (anterior == NULL) ? *lista : anterior->siguiente;
        } else {
            anterior = actual;
            actual = actual->siguiente;
        }
    }
}
```

Este patrón es equivalente a un "ciclo de vida" de procesos: se asume que todos murieron, se leen los que existen, y se eliminan los que no aparecieron. Es el mismo principio que usa el sistema operativo para actualizar tablas de procesos.

### 5.4 Bucle principal

```c
int main() {
    DIR *directorio;
    struct dirent *entrada;
    Nodo *lista = NULL;

    directorio = opendir("/proc");

    while (1) {
        rewinddir(directorio);          /* volver al inicio de /proc */
        marcar_no_vistos(lista);        /* paso 1 */
        while ((entrada = readdir(directorio)) != NULL) {
            if (es_numero(entrada->d_name))
                /* paso 2: marcar o agregar */;
        }
        eliminar_no_vistos(&lista);     /* paso 3 */
        imprimir_lista(lista);
        sleep(1);                       /* esperar 1 segundo */
    }

    closedir(directorio);
    liberar_lista(lista);
    return 0;
}
```

La función `rewinddir` es fundamental: sin ella, `readdir` no volvería al principio del directorio en el siguiente ciclo. Cada iteración del bucle `while(1)` es un ciclo completo de monitoreo.

## 6. Ejecución

```bash
cd programs
gcc -O2 -Wall -o program-1 program-1.c
./program-1
```

## 7. Cumplimiento del enunciado

- Lee el directorio `/proc` con `opendir`/`readdir`: cumplido.
- Identifica PIDs de procesos activos: cumplido (filtra entradas numéricas).
- Detecta procesos nuevos: cumplido (`agregar` cuando el PID no está en la lista).
- Detecta procesos desaparecidos: cumplido (`eliminar_no_vistos`).
- Mantiene lista actualizada en cada ciclo: cumplido (`rewinddir` + `while(1)`).
- Muestra la lista de PIDs en pantalla: cumplido (`imprimir_lista`).

## 8. Conclusión

El programa demuestra cómo el directorio `/proc` es una ventana en tiempo real hacia los procesos del sistema. La estructura de lista enlazada con el flag `visto` es un patrón eficiente para detectar cambios sin tener que comparar dos listas completas: basta con asumir que todos murieron y se eliminan los que no reaparecen.

Lo más interesante de esta implementación es que no necesita guardar una "instantánea" anterior del sistema. En cada ciclo, el simple hecho de recorrer `/proc` y cruzar datos con la lista existente es suficiente para saber exactamente qué proceso nació y cuál murió. Es el mismo principio que usan herramientas como `ps`, `top` o `htop` internamente.

## 9. Anexo A - Comandos usados

```bash
cd programs
gcc -O2 -Wall -o program-1 program-1.c
./program-1
```

## 10. Bitácora de prompts

Prompts usados para llegar al resultado final:

| # | Prompt textual | ¿Sirve? | LLM / Agente |
|:-:|:-:|:-:|:-:|
| 1 | **"Cómo leer el directorio `/proc` en C para obtener solo los directorios numéricos?"** — Conduce a usar `opendir`/`readdir` y filtrar con `isdigit`. | Sí | Gemini |
| 2 | **"Cómo implementar una lista enlazada en C para guardar PIDs de procesos?"** — Conduce a la definición de `Nodo` con `pid`, `visto` y `siguiente`. | Sí | Gemini |
| 3 | **"Cómo detectar si un PID sigue activo en `/proc` después de un ciclo?"** — Conduce al patrón de marcar todos como "no vistos" antes de recorrer `/proc`. | Sí | Gemini |
| 4 | **"Cómo comparar la lista de PIDs anterior con la actual para saber cuáles aparecieron o desaparecieron?"** — Conduce a la función `eliminar_no_vistos` que borra los que no se marcaron. | Sí | Gemini |
| 5 | **"Cómo estructurar el bucle `while(1)` con `rewinddir` para volver a leer `/proc`?"** — Conduce a usar `rewinddir` antes de cada recorrido. | Sí | Gemini |
| 6 | **"Cómo usar `isdigit` para verificar si un nombre de entrada es numérico?"** — Conduce a la función `es_numero`. | Sí | Gemini |
| 7 | **"Cómo liberar memoria de una lista enlazada al salir del programa?"** — Conduce a `liberar_lista` con `free` en cada nodo.  | Sí | Gemini |

