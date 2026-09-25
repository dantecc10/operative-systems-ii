#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>

typedef struct Nodo
{
    int pid;
    int visto;
    struct Nodo *siguiente;
} Nodo;

/* Verifica si una cadena contiene solamente números */
int es_numero(const char *nombre)
{
    int i;

    if (nombre[0] == '\0')
        return 0;

    for (i = 0; nombre[i] != '\0'; i++)
    {
        if (!isdigit((unsigned char)nombre[i]))
            return 0;
    }

    return 1;
}

/* Busca si un PID ya está en la lista */
Nodo *buscar_pid(Nodo *lista, int pid)
{
    Nodo *actual = lista;

    while (actual != NULL)
    {
        if (actual->pid == pid)
            return actual;

        actual = actual->siguiente;
    }

    return NULL;
}

/* Agrega un PID a la lista */
void agregar(Nodo **lista, int pid)
{
    Nodo *nuevo;
    Nodo *actual;

    nuevo = malloc(sizeof(Nodo));

    if (nuevo == NULL)
    {
        perror("Error al reservar memoria");
        return;
    }

    nuevo->pid = pid;
    nuevo->visto = 1;
    nuevo->siguiente = NULL;

    if (*lista == NULL)
    {
        *lista = nuevo;
        return;
    }

    actual = *lista;

    while (actual->siguiente != NULL)
    {
        actual = actual->siguiente;
    }

    actual->siguiente = nuevo;
}

/* Marca todos los nodos como NO vistos */
void marcar_no_vistos(Nodo *lista)
{
    Nodo *actual = lista;

    while (actual != NULL)
    {
        actual->visto = 0;
        actual = actual->siguiente;
    }
}

/* Elimina los procesos que ya no existen */
void eliminar_no_vistos(Nodo **lista)
{
    Nodo *actual = *lista;
    Nodo *anterior = NULL;
    Nodo *temp;

    while (actual != NULL)
    { // Es decir, que existe un nodo actual, y que existe la lista, y que aún no se termina de recorrerla

        if (actual->visto == 0)
        { // Si el nodo no fue visto, se elimina

            /* El nodo es el primero */
            if (anterior == NULL)
            {
                *lista = actual->siguiente; // Simplemente colocamos actual al siguiente nodo para recorrer el inicio de la lista, y así el actual queda fuera de la lista
                temp = actual;              // almacenamos provisionalmente el nodo actual
                actual = actual->siguiente; // se actualiza el nodo actual al siguiente
                free(temp);                 // se libera la memoria del nodo retirado de la lista (era el primero), se deja limpia la memoria
            }

            /* El nodo está en medio o al final */
            else
            {
                anterior->siguiente = actual->siguiente; // se recorre la lista para saltar el nodo actual
                temp = actual;                           // almacenamos provisionalmente el nodo actual
                actual = actual->siguiente;              // se actualiza el nodo actual al siguiente
                free(temp);                              // se libera la memoria del nodo retirado de la lista
            }
        }
        else
        {
            anterior = actual;          // este nodo fue visto (por lo tanto existe), así que recorremos
            actual = actual->siguiente; // se actualiza el nodo actual al siguiente
        }
    }
}

/* Imprime la lista */
void imprimir_lista(Nodo *lista)
{
    Nodo *actual = lista;

    printf("IDs de procesos actuales:\n");

    while (actual != NULL)
    {
        printf("%d\n", actual->pid);
        actual = actual->siguiente;
    }

    printf("-------------------------\n");
}

/* Libera toda la lista */
void liberar_lista(Nodo *lista)
{
    Nodo *temp;

    while (lista != NULL)
    {
        temp = lista;
        lista = lista->siguiente;
        free(temp);
    }
}

int main()
{
    DIR *directorio;
    struct dirent *entrada;
    Nodo *lista = NULL;
    Nodo *nodo;
    int pid;

    directorio = opendir("/proc");

    if (directorio == NULL)
    {
        perror("No se pudo abrir /proc");
        return 1;
    }

    /* Monitorear continuamente */
    while (1)
    {

        /*
         * Volver al principio de /proc
         * antes de realizar otro recorrido.
         */
        rewinddir(directorio);

        /*
         * Suponemos inicialmente que
         * todos los procesos desaparecieron.
         */
        marcar_no_vistos(lista);

        /*
         * Recorrer nuevamente /proc.
         */
        while ((entrada = readdir(directorio)) != NULL)
        {

            if (es_numero(entrada->d_name))
            {

                pid = atoi(entrada->d_name);

                /*
                 * Buscar si el PID ya estaba
                 * en nuestra lista.
                 */
                nodo = buscar_pid(lista, pid);

                if (nodo != NULL)
                {

                    /*
                     * Ya existía.
                     * Lo marcamos como encontrado.
                     */
                    nodo->visto = 1;
                }
                else
                {

                    /*
                     * No existía.
                     * Es un proceso nuevo.
                     */
                    agregar(&lista, pid);
                }
            }
        }

        /*
         * Eliminar los procesos que
         * ya no aparecieron en /proc.
         */
        eliminar_no_vistos(&lista);

        /*
         * Mostrar la lista actualizada.
         */
        imprimir_lista(lista);

        /*
         * Esperar 1 segundo antes
         * de volver a revisar.
         */
        sleep(5);
    }

    /*
     * Estas líneas nunca se alcanzan
     * con while(1), pero conceptualmente
     * serían necesarias para una salida
     * controlada.
     */
    closedir(directorio);
    liberar_lista(lista);

    return 0;
}
