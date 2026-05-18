#include <ctype.h>   /* Permite usar isspace(), tolower(), etc. */
#include <math.h>    /* Permite usar isfinite() para descartar valores NaN. */
#include <stdint.h>  /* Incluido por compatibilidad con plantillas que usan enteros de tamaño fijo. */
#include <stdio.h>   /* Permite usar FILE, fopen(), fgets(), printf(), fclose(), scanf(), etc. */
#include <stdlib.h>  /* Permite usar atof(), atoi(), strtod(), strtol(), qsort(), etc. */
#include <string.h>  /* Permite usar strlen(), strcmp(), strstr(), strchr(), strncpy(), etc. */

#ifdef _WIN32
#include <io.h>      /* Permite usar _findfirst(), _findnext() y _findclose() en Windows. */
#else
#include <dirent.h>  /* Permite recorrer carpetas con opendir(), readdir() y closedir() en Linux/macOS. */
#endif

/*
    Tarea2.c

    Procesamiento de Datos Biomecanicos - Guia 2

    Se sigue el esquema pedido por la Guia 2:

        1. cargar la carpeta con archivos CSV de marcha,
        2. listar los archivos disponibles,
        3. seleccionar un archivo por indice,
        4. analizar solamente el archivo seleccionado.

    Luego del paso de seleccion, el analisis es el mismo:
        - lectura de Sampling Frequency desde metadatos,
        - lectura de Angle_X, Linear_Acceleration_Z, Segmentation_output y Sync,
        - correccion de signo de Linear_Acceleration_Z,
        - deteccion de ventana Sync,
        - conteo de pasos por transiciones S3 -> S0,
        - calculo de metricas de marcha.
*/

#define MAX_LINEA 1024              /* Cantidad maxima de caracteres por linea del CSV. */
#define MAX_MUESTRAS 20000          /* Cantidad maxima de muestras numericas por archivo. */
#define MAX_ARCHIVOS 100            /* Cantidad maxima de CSV que se pueden listar desde la carpeta. */
#define MAX_TEXTO 256               /* Longitud maxima para nombres de archivo y campos auxiliares. */
#define MAX_RUTA 512                /* Longitud maxima para rutas completas de archivos. */
#define DISTANCIA_UTIL_10MWT_M 6.0  /* Distancia util del 10MWT usada para calcular velocidad de marcha. */

/*
    Estructura que representa un archivo disponible dentro de la carpeta.

    Se usa para poder listar la carpeta completa y luego seleccionar un archivo
    por indice, tal como solicita el enunciado.
*/
typedef struct {
    char nombre_fichero[MAX_TEXTO]; /* Nombre del CSV, por ejemplo: S01_gait_10MWT_01.csv. */
    char ruta_completa[MAX_RUTA];   /* Ruta completa hacia ese CSV. */
} ArchivoCSV;

/*
    Estructura que representa una muestra numerica del CSV.

    Cada fila del bloque numerico puede contener muchas columnas, pero para la
    tarea solo se conservan cuatro variables de interes.
*/
typedef struct {
    double angle_x;                 /* Senal Angle_X de la muestra. */
    double linear_acceleration_z;   /* Senal Linear_Acceleration_Z de la muestra. */
    int segmentation_output;        /* Estado de segmentacion usado para detectar pasos. */
    int sync;                       /* Senal Sync usada para delimitar la ventana util. */
} Datos;

/* ------------------------------------------------------------------------- */
/* Prototipos asociados a carga de carpeta y seleccion de archivo. */
int cargar_carpeta_csv_gait(const char carpeta[], ArchivoCSV archivos[], int max_archivos);
void imprimir_resumen_carpeta(const char carpeta[], int total_archivos);
void imprimir_lista_archivos(const ArchivoCSV archivos[], int total_archivos);
int seleccionar_indice_archivo(int total_archivos, int argc, char *argv[]);
void unir_ruta(char destino[], const char carpeta[], const char fichero[]);
int nombre_es_csv(const char nombre[]);
int nombre_es_gait_10mwt(const char nombre[]);
int contiene_texto_insensible(const char texto[], const char patron[]);
int comparar_archivos_por_nombre(const void *a, const void *b);

/* Prototipos asociados al analisis biomecanico de un archivo seleccionado. */
int analizar_archivo(const char archivo[]);
int cargar_metadata(const char archivo[], double *frecuencia_muestreo);
int cargar_datos(const char archivo[], Datos registros[], int max_registros, int *total);
void corregir_aceleracion(Datos registros[], int total);
double calcular_longitud_temporal(int total, double fs);
int buscar_indice_primera_sync(const Datos registros[], int total);
int buscar_indice_ultima_sync(const Datos registros[], int total);
int contar_transiciones_S3_S0(const Datos registros[], int inicio, int fin);
double calcular_velocidad_marcha(int muestras_sync, double fs);
double calcular_velocidad_pasos(int pasos, int muestras_pasos, double fs);
double calcular_longitud_zancada(double velocidad_marcha, double velocidad_pasos);
void imprimir_resultados(
    const char archivo[],
    int total,
    double fs,
    int inicio_sync,
    int fin_sync,
    int muestras_sync,
    int pasos,
    double velocidad_marcha,
    double velocidad_pasos,
    double longitud_zancada,
    double tiempo_total
);

/* Prototipos auxiliares de texto y parseo CSV. */
void quitar_salto_linea(char texto[]);
int linea_vacia(const char texto[]);
void copiar_texto(char destino[], const char origen[], size_t tamano);
void limpiar_espacios(char texto[]);
int indice_columna_csv(const char cabecera[], const char nombre_columna[]);
int obtener_campo_csv(const char linea[], int indice_campo, char destino[], size_t tamano);
int convertir_double(const char texto[], double *valor);
int convertir_int(const char texto[], int *valor);

/* ------------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    /*
        Flujo principal del programa.

        Uso recomendado:
            ./Tarea2_PDB_Aldo_Pastore data/raw/gait 0

        Donde:
            argv[1] = carpeta donde estan los CSV de marcha.
            argv[2] = indice del archivo que se desea analizar.

        Si no se pasa argv[2], el programa muestra la lista y pide el indice
        por teclado. Si no se escribe un indice valido, se usa 0 como respaldo.
    */

    /* Carpeta por defecto si se ejecuta desde la raiz del proyecto. */
    const char *carpeta = "data/raw/gait";

    /* Arreglo donde se guardara la lista de archivos encontrados. */
    ArchivoCSV archivos[MAX_ARCHIVOS];

    /* Cantidad real de archivos gait_10MWT encontrados en la carpeta. */
    int total_archivos = 0;

    /* Indice del archivo seleccionado por el usuario o por argumento. */
    int indice = 0;

    /* Si el usuario pasa una carpeta por consola, se reemplaza la ruta por defecto. */
    if (argc > 1) {
        carpeta = argv[1];
    }

    /* Se cargan los CSV de marcha disponibles en la carpeta indicada. */
    total_archivos = cargar_carpeta_csv_gait(carpeta, archivos, MAX_ARCHIVOS);

    /* Si no se encontro nada, se prueba una ruta alternativa comun desde dataAnalysis/gait. */
    if (total_archivos == 0 && argc <= 1) {
        carpeta = "../../data/raw/gait";
        total_archivos = cargar_carpeta_csv_gait(carpeta, archivos, MAX_ARCHIVOS);
    }

    /* Si despues de ambos intentos no hay archivos, no se puede continuar. */
    if (total_archivos == 0) {
        printf("No se encontraron archivos CSV gait_10MWT en la carpeta indicada.\n");
        printf("Carpeta evaluada: %s\n", carpeta);
        return 1;
    }

    /* Se muestra un resumen de la carpeta cargada. */
    imprimir_resumen_carpeta(carpeta, total_archivos);

    /* Se muestra la lista numerada de archivos disponibles. */
    imprimir_lista_archivos(archivos, total_archivos);

    /* Se obtiene el indice de seleccion desde argv[2] o desde teclado. */
    indice = seleccionar_indice_archivo(total_archivos, argc, argv);

    /* Si el indice queda fuera de rango, se detiene el programa con mensaje claro. */
    if (indice < 0 || indice >= total_archivos) {
        printf("Indice fuera de rango: %d. Debe estar entre 0 y %d.\n", indice, total_archivos - 1);
        return 1;
    }

    /* Se informa que archivo fue elegido para el analisis posterior. */
    printf("\nArchivo seleccionado: [%d] %s\n", indice, archivos[indice].nombre_fichero);
    printf("Ruta completa: %s\n\n", archivos[indice].ruta_completa);

    /* Se analiza solamente el archivo seleccionado. */
    return analizar_archivo(archivos[indice].ruta_completa);
}

/* ------------------------------------------------------------------------- */
int cargar_carpeta_csv_gait(const char carpeta[], ArchivoCSV archivos[], int max_archivos) {
    /*
        Recorre una carpeta y carga solamente los archivos CSV correspondientes
        al protocolo gait_10MWT.

        Esto evita mezclar archivos como subject_key.csv, stair_ascent o
        stair_descent, que no pertenecen al analisis central de la Guia 2.
    */

    /* Contador de archivos aceptados. */
    int total = 0;

#ifdef _WIN32
    /* Patron de busqueda para Windows: carpeta mas comodin CSV. */
    char patron[MAX_RUTA];

    /* Estructura usada por _findfirst() para devolver datos del archivo encontrado. */
    struct _finddata_t datos;

    /* Identificador de busqueda de Windows. */
    intptr_t busqueda;

    /* Se arma el patron con comodin. */
    unir_ruta(patron, carpeta, "*.csv");

    /* Se inicia la busqueda de archivos. */
    busqueda = _findfirst(patron, &datos);

    /* Si la busqueda falla, no hay archivos o no se pudo abrir la carpeta. */
    if (busqueda == -1) {
        return 0;
    }

    /* Se recorren todos los resultados encontrados. */
    do {
        /* Se ignoran subdirectorios y se filtran solo archivos gait_10MWT. */
        if ((datos.attrib & _A_SUBDIR) == 0 && nombre_es_gait_10mwt(datos.name) && total < max_archivos) {
            copiar_texto(archivos[total].nombre_fichero, datos.name, MAX_TEXTO);
            unir_ruta(archivos[total].ruta_completa, carpeta, datos.name);
            total++;
        }
    } while (_findnext(busqueda, &datos) == 0);

    /* Se libera el recurso de busqueda. */
    _findclose(busqueda);
#else
    /* Puntero al directorio abierto en Linux/macOS. */
    DIR *dir = opendir(carpeta);

    /* Estructura que representa cada entrada del directorio. */
    struct dirent *entrada;

    /* Si no se pudo abrir la carpeta, se devuelve cero. */
    if (dir == NULL) {
        return 0;
    }

    /* Se leen las entradas una por una. */
    while ((entrada = readdir(dir)) != NULL && total < max_archivos) {
        /* Se filtra por nombre para conservar solo CSV de marcha 10MWT. */
        if (nombre_es_gait_10mwt(entrada->d_name)) {
            copiar_texto(archivos[total].nombre_fichero, entrada->d_name, MAX_TEXTO);
            unir_ruta(archivos[total].ruta_completa, carpeta, entrada->d_name);
            total++;
        }
    }

    /* Se cierra el directorio. */
    closedir(dir);
#endif

    /* Se ordena alfabeticamente la lista para que los indices sean estables. */
    qsort(archivos, (size_t)total, sizeof(ArchivoCSV), comparar_archivos_por_nombre);

    /* Se devuelve la cantidad de archivos cargados. */
    return total;
}

/* ------------------------------------------------------------------------- */
void imprimir_resumen_carpeta(const char carpeta[], int total_archivos) {
    /* Imprime un resumen breve de la carpeta cargada. */
    printf("Resumen de carpeta cargada\n");
    printf("--------------------------\n");
    printf("Carpeta: %s\n", carpeta);
    printf("Archivos gait_10MWT encontrados: %d\n\n", total_archivos);
}

/* ------------------------------------------------------------------------- */
void imprimir_lista_archivos(const ArchivoCSV archivos[], int total_archivos) {
    /* Muestra la lista de archivos disponibles con indices. */
    printf("Archivos disponibles\n");
    printf("--------------------\n");

    /* Cada archivo se imprime con su indice para que pueda ser seleccionado. */
    for (int i = 0; i < total_archivos; i++) {
        printf("[%d] %s\n", i, archivos[i].nombre_fichero);
    }

    printf("\n");
}

/* ------------------------------------------------------------------------- */
int seleccionar_indice_archivo(int total_archivos, int argc, char *argv[]) {
    /*
        Obtiene el indice del archivo a analizar.

        Prioridad:
        1. Si existe argv[2], se usa ese valor.
        2. Si no existe, se pide por teclado.
        3. Si la lectura por teclado falla, se usa 0 como opcion segura.
    */

    /* Variable donde se guardara el indice. */
    int indice = 0;

    /* Si el usuario paso el indice por consola, se convierte a entero. */
    if (argc > 2) {
        return atoi(argv[2]);
    }

    /* Si no paso indice, se solicita interactivamente. */
    printf("Seleccione el indice del archivo a analizar [0-%d]: ", total_archivos - 1);

    /* scanf devuelve 1 si pudo leer correctamente un entero. */
    if (scanf("%d", &indice) != 1) {
        printf("No se ingreso un indice valido. Se usara el indice 0 por defecto.\n");
        indice = 0;
    }

    /* Se devuelve el indice elegido. */
    return indice;
}

/* ------------------------------------------------------------------------- */
void unir_ruta(char destino[], const char carpeta[], const char fichero[]) {
    /* Une carpeta + nombre de archivo respetando si la carpeta ya termina en / o \\. */

    /* Se obtiene la longitud de la ruta de carpeta. */
    size_t n = strlen(carpeta);

    /* Si ya termina en separador, se concatena directamente. */
    if (n > 0 && (carpeta[n - 1] == '/' || carpeta[n - 1] == '\\')) {
        snprintf(destino, MAX_RUTA, "%s%s", carpeta, fichero);
    } else {
        /* Si no termina en separador, se agrega '/'. */
        snprintf(destino, MAX_RUTA, "%s/%s", carpeta, fichero);
    }
}

/* ------------------------------------------------------------------------- */
int nombre_es_csv(const char nombre[]) {
    /* Devuelve 1 si el nombre termina en .csv, sin distinguir mayusculas/minusculas. */

    /* Se calcula la longitud del nombre. */
    size_t n = strlen(nombre);

    /* Un nombre menor que 4 caracteres no puede terminar en .csv. */
    if (n < 4) {
        return 0;
    }

    /* Se comparan los ultimos cuatro caracteres. */
    return tolower((unsigned char)nombre[n - 4]) == '.' &&
           tolower((unsigned char)nombre[n - 3]) == 'c' &&
           tolower((unsigned char)nombre[n - 2]) == 's' &&
           tolower((unsigned char)nombre[n - 1]) == 'v';
}

/* ------------------------------------------------------------------------- */
int nombre_es_gait_10mwt(const char nombre[]) {
    /*
        Devuelve 1 si el archivo es CSV y contiene gait_10MWT en el nombre.
        La busqueda no distingue mayusculas/minusculas.
    */

    /* Primero se verifica que efectivamente sea un CSV. */
    if (!nombre_es_csv(nombre)) {
        return 0;
    }

    /* Luego se verifica que corresponda al protocolo de marcha 10MWT. */
    return contiene_texto_insensible(nombre, "gait_10mwt");
}

/* ------------------------------------------------------------------------- */
int contiene_texto_insensible(const char texto[], const char patron[]) {
    /* Busca patron dentro de texto sin distinguir mayusculas/minusculas. */

    /* Longitud del texto principal. */
    size_t n_texto = strlen(texto);

    /* Longitud del patron buscado. */
    size_t n_patron = strlen(patron);

    /* Si el patron esta vacio, se considera encontrado. */
    if (n_patron == 0) {
        return 1;
    }

    /* Si el patron es mas largo que el texto, no puede estar contenido. */
    if (n_patron > n_texto) {
        return 0;
    }

    /* Se prueban todas las posiciones posibles del texto. */
    for (size_t i = 0; i <= n_texto - n_patron; i++) {
        /* Bandera local: asume coincidencia hasta que se demuestre lo contrario. */
        int coincide = 1;

        /* Se compara caracter por caracter. */
        for (size_t j = 0; j < n_patron; j++) {
            char a = (char)tolower((unsigned char)texto[i + j]);
            char b = (char)tolower((unsigned char)patron[j]);

            if (a != b) {
                coincide = 0;
                break;
            }
        }

        /* Si todos los caracteres coincidieron, se encontro el patron. */
        if (coincide) {
            return 1;
        }
    }

    /* Si se recorrieron todas las posiciones sin exito, no se encontro. */
    return 0;
}

/* ------------------------------------------------------------------------- */
int comparar_archivos_por_nombre(const void *a, const void *b) {
    /* Funcion de comparacion para qsort(). */

    /* Se convierten los punteros genericos a punteros ArchivoCSV. */
    const ArchivoCSV *archivo_a = (const ArchivoCSV *)a;
    const ArchivoCSV *archivo_b = (const ArchivoCSV *)b;

    /* strcmp devuelve el orden alfabetico entre ambos nombres. */
    return strcmp(archivo_a->nombre_fichero, archivo_b->nombre_fichero);
}

/* ------------------------------------------------------------------------- */
int analizar_archivo(const char archivo[]) {
    /*
        Analiza el CSV seleccionado.

        Esta funcion concentra el mismo flujo que tenia la version anterior,
        pero ahora recibe la ruta generada por el proceso de seleccion.
    */

    /* Variable donde se guarda la frecuencia de muestreo. */
    double fs = 0.0;

    /* Arreglo donde se cargaran las muestras del bloque numerico. */
    Datos registros[MAX_MUESTRAS];

    /* Cantidad real de muestras cargadas. */
    int total = 0;

    /* Se lee Sampling Frequency desde el bloque de metadatos. */
    if (!cargar_metadata(archivo, &fs)) {
        printf("No se pudo leer la frecuencia de muestreo del archivo: %s\n", archivo);
        return 1;
    }

    /* Se leen las columnas necesarias del bloque numerico. */
    if (!cargar_datos(archivo, registros, MAX_MUESTRAS, &total)) {
        printf("Error al cargar los datos del archivo: %s\n", archivo);
        return 1;
    }

    /* Si no se obtuvo ninguna muestra valida, se detiene el analisis. */
    if (total == 0) {
        printf("No se encontraron muestras validas en el archivo: %s\n", archivo);
        return 1;
    }

    /* Se cambia el signo de Linear_Acceleration_Z. */
    corregir_aceleracion(registros, total);

    /* Se ubica la primera activacion de Sync. */
    int inicio_sync = buscar_indice_primera_sync(registros, total);

    /* Se ubica la ultima activacion de Sync. */
    int fin_sync = buscar_indice_ultima_sync(registros, total);

    /* Cantidad de muestras/intervalos dentro de la ventana Sync. */
    int muestras_sync = 0;

    /* La ventana es valida solo si existe inicio y fin posterior. */
    if (inicio_sync >= 0 && fin_sync > inicio_sync) {
        muestras_sync = fin_sync - inicio_sync;
    }

    /* Se cuentan pasos por transiciones S3 -> S0 en la ventana Sync. */
    int pasos = contar_transiciones_S3_S0(registros, inicio_sync, fin_sync);

    /* Se calcula la duracion total del registro. */
    double tiempo_total = calcular_longitud_temporal(total, fs);

    /* Se inicializan metricas en cero para el caso de ventana invalida. */
    double velocidad_marcha = 0.0;
    double velocidad_pasos = 0.0;
    double longitud_zancada = 0.0;

    /* Las metricas principales solo se calculan si hay ventana Sync valida. */
    if (muestras_sync > 0) {
        velocidad_marcha = calcular_velocidad_marcha(muestras_sync, fs);
        velocidad_pasos = calcular_velocidad_pasos(pasos, muestras_sync, fs);
        longitud_zancada = calcular_longitud_zancada(velocidad_marcha, velocidad_pasos);
    }

    /* Se imprime un resumen final. */
    imprimir_resultados(
        archivo,
        total,
        fs,
        inicio_sync,
        fin_sync,
        muestras_sync,
        pasos,
        velocidad_marcha,
        velocidad_pasos,
        longitud_zancada,
        tiempo_total
    );

    /* Finalizacion correcta. */
    return 0;
}

/* ------------------------------------------------------------------------- */
int cargar_metadata(const char archivo[], double *frecuencia_muestreo) {
    /* Busca Sampling Frequency dentro del bloque inicial de metadatos. */

    /* Se abre el archivo en modo lectura. */
    FILE *fp = fopen(archivo, "r");

    /* Si fopen falla, se devuelve error. */
    if (!fp) {
        return 0;
    }

    /* Buffer para cada linea. */
    char linea[MAX_LINEA];

    /* Lectura linea por linea hasta encontrar el campo o llegar a la linea vacia. */
    while (fgets(linea, sizeof(linea), fp)) {
        /* Se eliminan saltos de linea. */
        quitar_salto_linea(linea);

        /* La primera linea vacia indica fin de metadatos. */
        if (linea_vacia(linea)) {
            break;
        }

        /* Se busca el campo de frecuencia de muestreo. */
        if (strstr(linea, "Sampling Frequency") != NULL) {
            /* Se localiza la coma que separa campo y valor. */
            char *coma = strchr(linea, ',');

            /* Si la coma existe, se convierte el texto posterior a numero. */
            if (coma != NULL) {
                *frecuencia_muestreo = atof(coma + 1);
                fclose(fp);
                return *frecuencia_muestreo > 0.0;
            }
        }
    }

    /* Se cierra el archivo si no se encontro la frecuencia. */
    fclose(fp);

    /* Fallo: no se encontro Sampling Frequency o no era valido. */
    return 0;
}

/* ------------------------------------------------------------------------- */
int cargar_datos(const char archivo[], Datos registros[], int max_registros, int *total) {
    /*
        Carga solamente las columnas necesarias.

        En lugar de depender de posiciones fijas, esta version detecta el
        indice real de cada columna a partir de la cabecera del CSV. Esto hace
        el lector mas robusto si el orden de columnas cambia en el futuro.
    */

    /* Se abre el archivo CSV. */
    FILE *fp = fopen(archivo, "r");

    /* Si el archivo no se puede abrir, se informa fallo. */
    if (!fp) {
        return 0;
    }

    /* Buffer de lectura. */
    char linea[MAX_LINEA];

    /* Bandera que indica si ya se paso el bloque de metadatos. */
    int metadata_done = 0;

    /* Indices de las columnas requeridas. */
    int idx_angle_x = -1;
    int idx_acc_z = -1;
    int idx_seg = -1;
    int idx_sync = -1;

    /* Inicialmente no se cargo ninguna muestra. */
    *total = 0;

    /* Se recorre el archivo linea por linea. */
    while (fgets(linea, sizeof(linea), fp)) {
        /* Se limpia el salto de linea. */
        quitar_salto_linea(linea);

        /* La linea vacia separa metadatos de bloque numerico. */
        if (linea_vacia(linea)) {
            metadata_done = 1;
            continue;
        }

        /* Antes de terminar metadatos, no se procesan senales. */
        if (!metadata_done) {
            continue;
        }

        /* Si todavia no tenemos indices, esta linea debe ser la cabecera. */
        if (idx_angle_x < 0 || idx_acc_z < 0 || idx_seg < 0 || idx_sync < 0) {
            /* Se verifica que realmente sea la cabecera buscada. */
            if (strstr(linea, "Angle_X") == NULL) {
                continue;
            }

            /* Se detectan posiciones reales de las columnas. */
            idx_angle_x = indice_columna_csv(linea, "Angle_X");
            idx_acc_z = indice_columna_csv(linea, "Linear_Acceleration_Z");
            idx_seg = indice_columna_csv(linea, "Segmentation_output");
            idx_sync = indice_columna_csv(linea, "Sync");

            /* Si alguna columna no existe, se detiene la carga. */
            if (idx_angle_x < 0 || idx_acc_z < 0 || idx_seg < 0 || idx_sync < 0) {
                fclose(fp);
                return 0;
            }

            /* Se pasa a la siguiente linea, donde empiezan los datos. */
            continue;
        }

        /* Se evita sobrepasar la capacidad maxima del arreglo. */
        if (*total >= max_registros) {
            break;
        }

        /* Campos temporales extraidos como texto. */
        char campo_angle_x[MAX_TEXTO];
        char campo_acc_z[MAX_TEXTO];
        char campo_seg[MAX_TEXTO];
        char campo_sync[MAX_TEXTO];

        /* Muestra temporal antes de guardarla en el arreglo. */
        Datos registro;

        /* Se extrae cada columna por indice. */
        if (!obtener_campo_csv(linea, idx_angle_x, campo_angle_x, sizeof(campo_angle_x)) ||
            !obtener_campo_csv(linea, idx_acc_z, campo_acc_z, sizeof(campo_acc_z)) ||
            !obtener_campo_csv(linea, idx_seg, campo_seg, sizeof(campo_seg)) ||
            !obtener_campo_csv(linea, idx_sync, campo_sync, sizeof(campo_sync))) {
            continue;
        }

        /* Se convierten los campos al tipo adecuado. */
        if (!convertir_double(campo_angle_x, &registro.angle_x) ||
            !convertir_double(campo_acc_z, &registro.linear_acceleration_z) ||
            !convertir_int(campo_seg, &registro.segmentation_output) ||
            !convertir_int(campo_sync, &registro.sync)) {
            continue;
        }

        /* Se guarda la muestra valida. */
        registros[*total] = registro;

        /* Se incrementa el contador de muestras. */
        (*total)++;
    }

    /* Se cierra el archivo. */
    fclose(fp);

    /* La carga es exitosa si se leyo al menos una muestra valida. */
    return *total > 0;
}

/* ------------------------------------------------------------------------- */
void corregir_aceleracion(Datos registros[], int total) {
    /* Invierte el signo de Linear_Acceleration_Z en todas las muestras. */
    for (int i = 0; i < total; i++) {
        registros[i].linear_acceleration_z = -registros[i].linear_acceleration_z;
    }
}

/* ------------------------------------------------------------------------- */
double calcular_longitud_temporal(int total, double fs) {
    /* Calcula el tiempo total del registro completo: T = N / fs. */
    if (total <= 0 || fs <= 0.0) {
        return 0.0;
    }
    return (double)total / fs;
}

/* ------------------------------------------------------------------------- */
int buscar_indice_primera_sync(const Datos registros[], int total) {
    /* Busca la primera muestra donde Sync es distinto de cero. */
    for (int i = 0; i < total; i++) {
        if (registros[i].sync != 0) {
            return i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------------- */
int buscar_indice_ultima_sync(const Datos registros[], int total) {
    /* Busca la ultima muestra donde Sync es distinto de cero. */
    for (int i = total - 1; i >= 0; i--) {
        if (registros[i].sync != 0) {
            return i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------------- */
int contar_transiciones_S3_S0(const Datos registros[], int inicio, int fin) {
    /* Cuenta transiciones 3 -> 0 dentro de la ventana Sync. */
    if (inicio < 0 || fin <= inicio) {
        return 0;
    }

    int pasos = 0;

    for (int i = inicio + 1; i <= fin; i++) {
        if (registros[i - 1].segmentation_output == 3 && registros[i].segmentation_output == 0) {
            pasos++;
        }
    }

    return pasos;
}

/* ------------------------------------------------------------------------- */
double calcular_velocidad_marcha(int muestras_sync, double fs) {
    /* Calcula velocidad media de marcha: v = 6 * fs / Nsync. */
    if (muestras_sync <= 0 || fs <= 0.0) {
        return 0.0;
    }
    return DISTANCIA_UTIL_10MWT_M * fs / (double)muestras_sync;
}

/* ------------------------------------------------------------------------- */
double calcular_velocidad_pasos(int pasos, int muestras_pasos, double fs) {
    /* Calcula velocidad media de pasos: pasos / Tsync. */
    if (pasos <= 0 || muestras_pasos <= 0 || fs <= 0.0) {
        return 0.0;
    }
    return (double)pasos * fs / (double)muestras_pasos;
}

/* ------------------------------------------------------------------------- */
double calcular_longitud_zancada(double velocidad_marcha, double velocidad_pasos) {
    /* Calcula longitud media por paso: L = v_marcha / v_pasos. */
    if (velocidad_marcha <= 0.0 || velocidad_pasos <= 0.0) {
        return 0.0;
    }
    return velocidad_marcha / velocidad_pasos;
}

/* ------------------------------------------------------------------------- */
void imprimir_resultados(
    const char archivo[],
    int total,
    double fs,
    int inicio_sync,
    int fin_sync,
    int muestras_sync,
    int pasos,
    double velocidad_marcha,
    double velocidad_pasos,
    double longitud_zancada,
    double tiempo_total
) {
    /* Imprime el resumen final del analisis del archivo seleccionado. */
    printf("Resultados del analisis\n");
    printf("-----------------------\n");
    printf("Archivo analizado: %s\n", archivo);
    printf("Muestras leidas: %d\n", total);
    printf("Frecuencia de muestreo: %.3f Hz\n", fs);
    printf("Indice primera activacion Sync: %d\n", inicio_sync);
    printf("Indice ultima activacion Sync: %d\n", fin_sync);
    printf("Muestras entre Sync: %d\n", muestras_sync);
    printf("Velocidad de marcha: %.3f m/s\n", velocidad_marcha);
    printf("Pasos detectados (S3->S0): %d\n", pasos);
    printf("Velocidad media de pasos: %.3f pasos/s\n", velocidad_pasos);
    printf("Longitud media estimada por paso: %.3f m\n", longitud_zancada);
    printf("Correccion aplicada: cambio de signo en Linear_Acceleration_Z.\n");
    printf("Tiempo total del registro: %.3f s\n", tiempo_total);
}

/* ------------------------------------------------------------------------- */
void quitar_salto_linea(char texto[]) {
    /* Elimina '\n' y '\r' al final de una cadena leida con fgets(). */
    size_t len = strlen(texto);

    while (len > 0 && (texto[len - 1] == '\n' || texto[len - 1] == '\r')) {
        texto[len - 1] = '\0';
        len--;
    }
}

/* ------------------------------------------------------------------------- */
int linea_vacia(const char texto[]) {
    /* Devuelve 1 si la linea esta vacia o contiene solo espacios. */
    for (size_t i = 0; texto[i] != '\0'; i++) {
        if (!isspace((unsigned char)texto[i])) {
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------------- */
void copiar_texto(char destino[], const char origen[], size_t tamano) {
    /* Copia texto de forma segura y garantiza terminador '\0'. */
    if (tamano == 0) {
        return;
    }

    strncpy(destino, origen, tamano - 1);
    destino[tamano - 1] = '\0';
}

/* ------------------------------------------------------------------------- */
void limpiar_espacios(char texto[]) {
    /* Elimina espacios al inicio y al final de una cadena. */

    char *inicio = texto;
    char *fin;
    size_t largo;

    while (*inicio != '\0' && isspace((unsigned char)*inicio)) {
        inicio++;
    }

    fin = inicio + strlen(inicio);

    while (fin > inicio && isspace((unsigned char)*(fin - 1))) {
        fin--;
    }

    largo = (size_t)(fin - inicio);
    memmove(texto, inicio, largo);
    texto[largo] = '\0';
}

/* ------------------------------------------------------------------------- */
int indice_columna_csv(const char cabecera[], const char nombre_columna[]) {
    /* Devuelve el indice de una columna dentro de la cabecera CSV. */

    int indice = 0;
    const char *inicio = cabecera;
    const char *p = cabecera;

    while (1) {
        char campo[MAX_TEXTO];
        size_t largo;

        if (*p == ',' || *p == '\0') {
            largo = (size_t)(p - inicio);

            if (largo >= sizeof(campo)) {
                largo = sizeof(campo) - 1;
            }

            memcpy(campo, inicio, largo);
            campo[largo] = '\0';
            limpiar_espacios(campo);

            if (strcmp(campo, nombre_columna) == 0) {
                return indice;
            }

            if (*p == '\0') {
                break;
            }

            indice++;
            inicio = p + 1;
        }

        p++;
    }

    return -1;
}

/* ------------------------------------------------------------------------- */
int obtener_campo_csv(const char linea[], int indice_campo, char destino[], size_t tamano) {
    /* Extrae el campo ubicado en indice_campo desde una linea CSV simple. */

    int indice = 0;
    const char *inicio = linea;
    const char *p = linea;

    while (1) {
        if (*p == ',' || *p == '\0') {
            if (indice == indice_campo) {
                size_t largo = (size_t)(p - inicio);

                if (largo >= tamano) {
                    largo = tamano - 1;
                }

                memcpy(destino, inicio, largo);
                destino[largo] = '\0';
                limpiar_espacios(destino);
                return 1;
            }

            if (*p == '\0') {
                break;
            }

            indice++;
            inicio = p + 1;
        }

        p++;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int convertir_double(const char texto[], double *valor) {
    /* Convierte texto a double y descarta cadenas no numericas o NaN. */

    char *fin;
    double temporal;

    if (texto == NULL || texto[0] == '\0') {
        return 0;
    }

    temporal = strtod(texto, &fin);

    if (fin == texto || !isfinite(temporal)) {
        return 0;
    }

    *valor = temporal;
    return 1;
}

/* ------------------------------------------------------------------------- */
int convertir_int(const char texto[], int *valor) {
    /* Convierte texto a int. Se usa para Segmentation_output y Sync. */

    char *fin;
    long temporal;

    if (texto == NULL || texto[0] == '\0') {
        return 0;
    }

    temporal = strtol(texto, &fin, 10);

    if (fin == texto) {
        return 0;
    }

    *valor = (int)temporal;
    return 1;
}
