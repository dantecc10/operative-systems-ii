# Sistemas Operativos II

Mtro. Rafael de la Rosa Flores

1. Gestión de la memoria
  - Manejo de memoria con particiones fijas
  - Manejo de memoria con particiones variables
  - Memoria virtual en sistemas operativos centralizados
2. Gestión de archivos
  - Criterios de implementación de sistemas de archivos
  - Tamaño de bloques
  - Manejo de bloques libres ocupados
  - Caso de estudio: Linux, Windows
  - Sistema de archivos de red (NFS)
3. Entrada y salida
  - Características de los dispositivos de entrada y salida
  - Dispositivos de bloque y caracter
4. Multiprocesadores y multicomputadoras
  - Virtualización
  - Sistemas operativos distribuidos
  - Concurrencia entre procesos
    - RPC
    - RMI

## Criterios de evaluación
- Programas
- Exámenes (2-3)
- Proyecto
- Investigaciones
- Exposiciones

## Reporte
- No técnico, sino que explique cómo funciona el programa
- Debe explicar con nuestras palabras el código o algoritmo que hicimos

## Administración de la memoria

Administrador de la memoria
- Intercambio y paginación
  - Memoria central, memoria secundaria
- No existe el intercambio y paginación
  - Sólo en memoria principal


### Monoprogramación
Un sólo proceso en memoria y el proceso ocupa toda la memoria

- Programa del usuario, SO en ROM
- SO en RAM, programa del usuario
- Manejadores de dispositivo, programas del usuario, SO en ROM.

"TSR" - "Termina y permanece reciente"


### Multiprogramación
Como es sabido la monoprogramación desperdicia gran cantidad de memoria,es por este desperdicio de memoria que los diseñadores decidieron implementar los sistemas multiprogramación, en los cuales varios usuarios compiten al mismo tiempo por los recursos del sistema.

**Multiprogramación fija** (traducción y carga):
Aquí, para cada segmento o bloque de memoria (correspondiente a algún programa diferente) se tiene una cola de trabajo.

**Multiprogramación con partición fija (traducción y carga relocalizable)**:
Lista de trabajos

## Almacenamiento virtual
El almacenamiento virtual se asocia con la capacidad de direccionar un espacio de almacenamiento mucho mayor que el disponible en el almacenamiento primario del sistema.

- Real
  - un sólo usuario.
- Real
  - sistemas de multiprogramación en almacenamiento real
    - Multiprogramación en partición fija
      - Relocalizable
    - Multiprogramación en partición variable
- Virtual
  - Multiprogramación en almacenamiento virtual
    - Paginación
    - Segmentación
    - Paginación / segmentación

## Archivos

### Métodos de implantación de archivos
- Asignación adycente
- Asignación en forma de lista ligada
- Asignación mediante una lista ligada y un índice

- fat (file assignment table)

### Nodos índice

- Dirección en disco
  - Atributos
    - Bloque simplemente indirecto
    - Bloque doblemente indirecto
    - Bloque triplemente indirecto

El sistema NTFS contiene las direcciones asociados a los archivos de un disco. Los atributos se almacenan en la de root.

Para el caso de Linux, los discos manejan los nodos índice.

Windows: Tabla de asignación de archivos.
Linux: Nodos índice.

La clase pasada vimos en qué posición se encontraba el superbloque.
Ahora hay que leer el superbloque.


> _"Realizar un programa que lea y muestre el contenido del superbloque"._

> _"Realizar un programa que lea y muestre el contenido del directorio `root` sin usar `opendir()` ni `readdir()`"_
