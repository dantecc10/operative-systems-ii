# Reporte de Programa 2 - Administración de la memoria: bloque máximo con malloc y carga de binarios

## 1. Portada

- Materia: Sistemas Operativos II
- Actividad: Programa 2 (gestión de la memoria)
- Alumno: Dante Castelán Carpinteyro
- Fecha: 19 de agosto de 2026
- Lenguaje: C (gcc 13.3.0, Linux)

## 2. Objetivo

Desarrollar un programa en C que:

1. Reserve con `malloc` el **bloque de memoria más grande** que el sistema permita utilizar, de acuerdo a un modo elegible por el usuario.
2. Recorra el disco duro con `opendir`, `readdir` y `stat` para localizar los binarios de los programas instalados.
3. Copie (cargue) esos binarios dentro del bloque reservado, ocupándolo poco a poco hasta que ya no quepa ninguno.
4. Muestre **dinámicamente en la salida, con caracteres ASCII**, qué programas van siendo "lanzados" (cargados en memoria) y cómo se va llenando el bloque.

## 3. Instrucciones de la actividad

> Haz un programa en C que obtenga el bloque de memoria RAM más grande que se permita o pueda utilizar con `malloc`, todo lo que se pueda. Luego hay que ir al disco duro y usando `opendir`, `readdir`, `stat` copiar los binarios de algunos programas en el bloque de memoria reservado, ver cuánto ocupa y subirlo. Hacer esto varias veces hasta que el bloque se llene. Que dinámicamente se vayan viendo en la salida con ASCII los programas que han sido lanzados.

## 4. Requisitos y entorno

- Sistema operativo: Linux (probado en Ubuntu 24.04).
- Compilador: gcc (C11/POSIX).
- Memoria física del equipo de prueba: `16009359360` bytes ≈ **14.91 GiB**.
- Espacio de swap del equipo de prueba: ≈ 45 GiB.
- Librerías: solo las del estándar y POSIX (`stdio.h`, `stdlib.h`, `string.h`, `stdint.h`, `dirent.h`, `sys/stat.h`, `sys/sysinfo.h`, `sys/types.h`, `unistd.h`). Sin dependencias externas.

Compilación:

```bash
cd programs
gcc -O2 -Wall -Wextra -o program-2 program-2.c
```

## 5. Implementación realizada

El programa tiene tres grandes etapas, y cada una se explica a continuación con su código relevante.

### 5.1 Calcular el bloque de memoria más grande que `malloc` puede otorgar

Primero se obtienen los límites del sistema:

- **RAM física**: `sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE)`.
- **RAM + swap**: estructura `sysinfo` (campos `totalram` y `totalswap`).

Según el modo elegido en la línea de comandos (`1`, `2` o `3`) se fija un tope:

```c
switch (modo) {
    case 1:  tope = physical_ram();      break;   /* por defecto: RAM física */
    case 2:  tope = ram_and_swap();      break;   /* RAM + swap              */
    case 3:  tope = 1143525669; 	 break;   /* Hardcodeo un valor máximo */
    default: tope = SIZE_MAX;          break;   /* sin tope                */
}
```

Con ese tope se hace una **búsqueda binaria** sobre el tamaño. La idea es que `malloc` es una función monótona: si un tamaño cabe, cualquiera menor también cabe; si uno no cabe, ninguno mayor cabrá. Por eso podemos "acorralar" el máximo muy rápido:

```c
static int se_puede(size_t tamano) {
    void *p;

    if (tamano == 0)
        return 0;

    p = malloc(tamano);
    if (p == NULL)
        return 0;

    free(p);
    return 1;
}

static size_t buscar_maximo(size_t tope) {
    size_t lo, hi, mid;

    /* Sin tope: primero duplicar 1, 2, 4... hasta fallar */
    if (tope == SIZE_MAX) {
        size_t n = 1;
        while (1) {
            if (se_puede(n)) {
                if (n > SIZE_MAX / 2) { lo = n; hi = SIZE_MAX; break; }
                n *= 2;
            } else {
                lo = n / 2; hi = n; break;
            }
        }
    } else {
        lo = 0;
        hi = tope;
    }

    /* Búsqueda binaria: halla el mayor tamaño que se puede reservar */
    while (lo < hi) {
        mid = lo + (hi - lo + 1) / 2;
        if (se_puede(mid))
            lo = mid;
        else
            hi = mid - 1;
    }

    return lo;
}
```

**Explicación de la idea clave:** solo llamar a `malloc` para "probar" es suficiente en Linux, porque con el overcommit por defecto (`vm.overcommit_memory = 0`) el propio `malloc` rechaza peticiones que exceden el límite de memoria comprometida (RAM + swap). La *utilidad real* del bloque se confirma después: cuando copiamos los binarios dentro de él estamos escribiendo en cada página, es decir, "tocando" la memoria; si el sistema no pudiera respaldarla, el programa fallaría ahí.

El resultado se retiene con un `malloc` definitivo:

```c
bloque_tam = buscar_maximo(tope);
memoria = (unsigned char *)malloc(bloque_tam);
```

En el equipo de prueba el modo 1 reservó **14.91 GiB (16009359360 bytes)**, justo toda la RAM física.

### 5.2 Recorrer el disco: `opendir`, `readdir` y `stat`

Para encontrar los binarios se recorren varios directorios del sistema. Por cada entrada se arma la ruta completa, se llama a `stat()` para conocer su tipo y tamaño, y si es un archivo regular (`S_ISREG`) con tamaño mayor a cero se guarda en un arreglo dinámico:

```c
for (i = 0; DIRECTORIOS[i] != NULL; i++) {
    DIR *dir = opendir(DIRECTORIOS[i]);
    struct dirent *entrada;

    if (dir == NULL)
        continue;

    while ((entrada = readdir(dir)) != NULL) {
        char ruta[TAM_RUTA];
        struct stat st;

        if (strcmp(entrada->d_name, ".") == 0 ||
            strcmp(entrada->d_name, "..") == 0)
            continue;

        snprintf(ruta, sizeof(ruta), "%s/%s", DIRECTORIOS[i], entrada->d_name);

        if (stat(ruta, &st) != 0)
            continue;

        if (!S_ISREG(st.st_mode) || st.st_size <= 0)
            continue;

        /* guardar ruta, nombre y st.st_size en el arreglo 'archivos' */
    }
    closedir(dir);
}
```

Los directorios explorados son: `/usr/bin`, `/usr/sbin`, `/usr/local/bin`, `/usr/local/sbin`, `/usr/libexec`, `/usr/lib` y `/opt`. Solos en `/usr/bin` no habría binarios suficientes para llenar un bloque de ~15 GiB; con todos ellos se acumulan ≈ 28 GiB de archivos, que sí alcanzan para llenarlo. En el equipo de prueba se encontraron **3364 binarios**.

### 5.3 Ordenar de mayor a menor y cargar hasta llenar

Para que el bloque quede lo más lleno posible se ordenan los archivos por tamaño de **mayor a menor** con `qsort`. De esta forma los binarios grandes se cargan primero y los pequeños van "rellenando" el hueco que queda al final:

```c
static int comparar_desc(const void *a, const void *b) {
    off_t ta = ((const Archivo *)a)->tamano;
    off_t tb = ((const Archivo *)b)->tamano;
    if (ta > tb) return -1;
    if (ta < tb) return 1;
    return 0;
}
```

La carga en sí copia el contenido del binario dentro del bloque en el desfase actual (`offset`) y registra nombre, tamaño y posición:

```c
for (i = 0; i < n_archivos; i++) {
    FILE *f;
    size_t leido;

    if (archivos[i].tamano > (off_t)(bloque_tam - usado))
        continue;                     /* no cabe: se salta */

    f = fopen(archivos[i].ruta, "rb");
    if (f == NULL)
        continue;

    leido = fread(memoria + usado, 1, (size_t)archivos[i].tamano, f);
    fclose(f);

    if (leido != (size_t)archivos[i].tamano)
        continue;                     /* lectura incompleta */

    cargados[n_cargados].nombre = archivos[i].nombre;
    cargados[n_cargados].tamano  = archivos[i].tamano;
    cargados[n_cargados].offset  = usado;

    usado += (size_t)archivos[i].tamano;
    n_cargados++;

    dibujar(bloque_tam, usado, cargados, n_cargados, archivos[i].ruta);
    usleep(150000);                   /* pausa para apreciar la animación */
}
```

El ciclo termina cuando ya no queda ningún binario que quepa en el espacio restante. Se valida la lectura comparando el retorno de `fread` con el tamaño esperado para no registrar binarios incompletos.

### 5.4 Visualización ASCII dinámica

Después de cargar cada binario se limpia la pantalla (secuencia de escape `\033[2J\033[H`) y se redibuja el estado completo:

- Un recuadro con el tamaño total del bloque, los bytes ocupados, el porcentaje y cuántos programas se han lanzado.
- Una línea `>>> Lanzando: <ruta> <<<` con el último binario cargado.
- Una **barra de mapa de memoria** de 70 caracteres donde cada programa es un segmento proporcional a su tamaño (se rellena alternando los caracteres `# = @ % * ~ + O` y se escribe el nombre del programa centrado si el segmento es lo bastante ancho); el espacio libre se dibuja con puntos `.`.
- Una leyenda con todos los programas lanzados: número, nombre, tamaño, desfase dentro del bloque y porcentaje que ocupa.

```c
static void dibujar(size_t bloque_tam, size_t usado,
                    const Cargado *cargados, size_t n,
                    const char *ultimo) {
    char barra[ANCHO_BARRA + 1];
    const char *relleno = "#=@%*~+O";
    size_t pos = 0;
    size_t i;

    printf("\033[2J\033[H");                       /* limpiar pantalla */

    printf("|  Tamanio del bloque : %zu bytes (%5.2f%%)\n",
           bloque_tam, 100.0 * (double)usado / (double)bloque_tam);
    /* ... encabezado y ">>> Lanzando ... <<<" ... */

    memset(barra, '.', ANCHO_BARRA);               /* primero todo libre */
    barra[ANCHO_BARRA] = '\0';

    for (i = 0; i < n && pos < ANCHO_BARRA; i++) {
        size_t w = (size_t)((double)cargados[i].tamano *
                            (double)ANCHO_BARRA / (double)bloque_tam);
        if (w == 0)
            w = 1;
        if (pos + w > ANCHO_BARRA)
            w = ANCHO_BARRA - pos;

        memset(barra + pos, relleno[i % strlen(relleno)], w);

        if (strlen(cargados[i].nombre) + 2 <= w) {  /* centrar el nombre */
            size_t ini = pos + (w - strlen(cargados[i].nombre)) / 2;
            memcpy(barra + ini, cargados[i].nombre, strlen(cargados[i].nombre));
        }
        pos += w;
    }

    printf("\n  Memoria: [%s] %5.2f%% ocupada\n", barra,
           100.0 * (double)usado / (double)bloque_tam);
    /* ... leyenda de programas lanzados ... */
}
```

El ancho de cada segmento se calcula con una regla de tres: `tamano_programa * 70 / tamano_bloque`. Como los tamaños pueden ser muy distintos (hay binarios de megabytes y de cientos de megabytes), los nombres solo caben centrados en los segmentos suficientemente anchos; en los angostos solo se ve el carácter de relleno.

### 5.5 Modos de ejecución

El programa recibe un argumento numérico opcional:

```bash
./program-2        # modo 1 (por defecto): tope = RAM física (~14.91 GiB aquí)
./program-2 2      # modo 2: RAM + swap (~57 GiB aquí)
./program-2 3	   # modo 3: tope hardcodeado
./program-2 4      # modo 4: sin tope, lo máximo que malloc devuelva
```

- **Modo 1 (por defecto):** busca el bloque más grande *utilizable* dentro de la RAM física real. Es seguro: al tocar la memoria no se excede la RAM y no hay riesgo de thrashing ni OOM.
- **Modo 2:** permite usar también el swap; el bloque puede ser mucho mayor, pero llenarlo entero implica pasar gigabytes por el swap y el sistema puede volverse muy lento.
- **Modo 3:** hardcodeamos un tope de bytes para la RAM a reservar. Útil para hacer pruebas y verificar el funcionamiento del programa.
- **Modo 4:** sin tope; es el caso extremo y solo tiene sentido si el sistema tiene overcommit que permita respaldar la memoria.

## 6. Ejecución

```bash
cd programs
gcc -O2 -Wall -Wextra -o program-2 program-2.c
./program-2
```

## 7. Corridas y evidencias

### Evidencia 1 - Reserva del bloque máximo de memoria

Al arrancar, el programa calcula y reserva el bloque más grande posible. En el equipo de prueba el modo 1 reservó **14.91 GiB (16009359360 bytes)**.

![Reserva del bloque máximo con malloc](assets/img/exec1.png)

### Evidencia 2 - Comienzo de la carga de binarios

Se muestran los binarios encontrados en el disco (3364) y la pantalla empieza a dibujarse dinámicamente: recuadro con datos, barra de memoria y el mensaje `>>> Lanzando: <binario> <<<`.

![Inicio de la carga de binarios](assets/img/exec2.png)

### Evidencia 3 - El bloque se va llenando

Conforme avanzan las iteraciones, la barra muestra más segmentos (cada uno con su carácter ASCII y su nombre) y la leyenda va creciendo con los programas lanzados y su desfase dentro del bloque.

![El bloque de memoria se va llenando](assets/img/exec3.png)

### Evidencia 4 - Resumen final

Al final se imprime un resumen: bloque reservado, bytes ocupados, porcentaje, cuántos programas se lanzaron de los encontrados y el estado final (BLOQUE LLENO o "no caben más binarios").

![Resumen final](assets/img/exec4.png)

## 8. Cumplimiento del enunciado

- Obtiene el bloque de memoria RAM más grande que `malloc` permite usar: cumplido (búsqueda binaria según modo 1/2/3).
- Recorre el disco duro con `opendir`, `readdir` y `stat`: cumplido (7 directorios del sistema).
- Copia los binarios de los programas dentro del bloque reservado: cumplido (`fopen`/`fread` sobre el bloque).
- Ve cuánto ocupa y lo sube/carga: cumplido (muestra bytes ocupados y porcentaje).
- Lo repite hasta que el bloque se llena: cumplido (carga mientras quepa espacio; termina cuando ya no cabe ningún binario).
- Visualización dinámica ASCII de los programas lanzados: cumplido (barra de mapa de memoria, leyenda y `>>> Lanzando <<<`).

## 9. Bitácora de prompts

**Instrucción**: Registra todo lo que se pregunta a la IA para realizar cada una de las partes del programa.

| # | Promt textual | ¿Sirve? | LLM / Agente |
| --- | --- | --- | --- |
| 1 | "¿Cómo se usan `opendir`, `readdir` y `stat` en C?" | ✅ | Gemini | 
| 2 | "¿Qué variables del sistema puedo usar para determinar usando `C` estándar la RAM de mi laptop? Necesito conocer la física, la swap, la física y la swap"  | ✅  | Gemini |
| 3 | "Dime todas las rutas desde las cuales puedo copiar binarios. Imagino que son las de sistema como /bin, /usr/bin, etc. Es tos archivos tendrán que ser revisados y copiados con C, ¿usaremos fread?" | ✅ | Gemini |

**Nota**: Gemini es mi LLM favorito y con su capacidad de "recordar" (almacenar en memoria) información sobre mí, mi uso, mi perfil de estudio, ya me responde con contexto útil. Por ejemplo, siempre que pregunto sobre algo en mi computadora, orienta su respuesta a solucionar o diagnosticar en sistemas Linux, porque no uso Windows, y lo sabe.

- ¿Qué hace el programa, utiliza frases cortas?
- ¿Qué parte aún no se entiende al 100%?
- ¿Qué sucede en memoria/sistema?

## 10. Conclusión

El programa simula de forma muy cercana lo que hace un administrador de memoria en multiprogramación: un único bloque contiguo (la memoria principal) que se va repartiendo entre los programas que se cargan desde la memoria secundaria (el disco). La búsqueda binaria aprovecha que `malloc` es una función monótona y encuentra el máximo muy rápido, y el uso de `opendir`/`readdir`/`stat` permite leer el directorio de archivos del sistema tal como lo haría un cargador real.

La parte más interesante fue decidir cómo verificar que el bloque realmente se puede usar: en lugar de tocar cada página en cada prueba (muy lento cerca del máximo), se confía en el overcommit por defecto de Linux, que hace que `malloc` rechace peticiones imposibles, y la propia copia de los binarios confirma que la memoria es real. La visualización en ASCII hace evidente cómo los programas se van "lanzando" y cómo el espacio libre se reduce hasta que el bloque queda lleno.

## 10. Anexo - Comandos usados

```bash
# Compilar
cd programs
gcc -O2 -Wall -Wextra -o program-2 program-2.c

# Ejecutar (modo 1 por defecto: RAM física)
./program-2

# Otros modos
./program-2 2   # RAM + swap
./program-2 3	# tope hardcodeado (en bytes)
./program-2 4   # sin tope

# Ver memoria del sistema
free -h
cat /proc/sys/vm/overcommit_memory
```
