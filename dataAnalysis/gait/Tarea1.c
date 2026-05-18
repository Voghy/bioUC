#include <ctype.h>    /* Permite usar isspace(), tolower() e isalnum(). */
#include <stdint.h>   /* Se conserva por compatibilidad con la plantilla original. */
#include <stdio.h>    /* Permite usar FILE, fopen(), fgets(), printf(), fclose(), etc. */
#include <stdlib.h>   /* Permite usar strtod(), atoi(), etc. */
#include <string.h>   /* Permite usar strlen(), strncpy(), strstr(), strcmp(), etc. */
#include <errno.h>    /* Permite consultar errores simples al crear carpetas. */
#include <math.h>     /* Permite validar valores numericos con isfinite(). */

#ifdef _WIN32
#include <direct.h>   /* En Windows permite usar _mkdir(). */
#include <io.h>       /* En Windows permite usar _findfirst(), _findnext(), _findclose(). */
#else
#include <dirent.h>   /* En Linux/macOS permite recorrer carpetas con opendir()/readdir(). */
#include <sys/stat.h> /* En Linux/macOS permite usar mkdir(). */
#include <sys/types.h>
#endif

/*
    Tarea1.c

    Procesamiento de Datos Biomecanicos - Guia 1
    Version en lenguaje C basada en el template entregado.

    Objetivo general del programa:
    1. Cargar una carpeta con archivos CSV del experimento de marcha.
    2. Construir una base de datos simple con un registro por archivo.
    3. Leer los metadatos de cada archivo.
    4. Leer las senales Angle_X, Linear_Acceleration_Z, Segmentation_output y Sync.
    5. Obtener la frecuencia de muestreo desde los metadatos.
    6. Generar archivos de datos y scripts de Gnuplot para graficar:
       - Angle_X en funcion del tiempo.
       - Linear_Acceleration_Z en funcion del tiempo.
       - sombreado de fondo cuando Sync esta activo.

    Observacion importante:
    La guia original fue planteada para Python, donde el graficado se hace con
    matplotlib. En lenguaje C estandar no existe una libreria grafica incluida.
    Por eso, esta version genera archivos .dat y .plt compatibles con Gnuplot.
    Si el usuario tiene Gnuplot instalado, puede ejecutar los scripts .plt para
    producir automaticamente imagenes .png.
*/

#define MAX_ARCHIVOS 100        /* Cantidad maxima de archivos CSV a cargar. */
#define MAX_METADATOS 40        /* Cantidad maxima de metadatos por archivo. */
#define MAX_TEXTO 256           /* Longitud maxima para nombres, campos y valores. */
#define MAX_LINEA 2048          /* Longitud maxima de una linea del CSV. */
#define MAX_RUTA 512            /* Longitud maxima de rutas internas. */
#define MAX_MUESTRAS 20000      /* Cantidad maxima de muestras numericas por archivo. */

#define CAMPO_FRECUENCIA "Sampling Frequency" /* Nombre exacto del metadato de frecuencia. */

/*
    Estructura principal de la base de datos.

    Cada RegistroCSV representa un archivo CSV del experimento.
    En Python esto se haria con una clase o dataclass; en C se usa struct.
*/
typedef struct {
    char nombre_fichero[MAX_TEXTO];                      /* Nombre del archivo CSV. */
    int total_metadatos;                                 /* Cantidad real de metadatos leidos. */
    char campos[MAX_METADATOS][MAX_TEXTO];               /* Nombres de campos de metadatos. */
    char valores[MAX_METADATOS][MAX_TEXTO];              /* Valores asociados a cada campo. */

    int total_muestras;                                  /* Cantidad real de muestras numericas. */
    double angle_x[MAX_MUESTRAS];                        /* Senal Angle_X. */
    double linear_acceleration_z[MAX_MUESTRAS];          /* Senal Linear_Acceleration_Z. */
    int segmentation_output[MAX_MUESTRAS];               /* Senal de segmentacion. */
    int sync[MAX_MUESTRAS];                              /* Senal Sync. */

    double frecuencia_muestreo;                          /* Frecuencia de muestreo obtenida desde metadatos. */
} RegistroCSV;

/* ------------------------------------------------------------------------- */
/* Prototipos principales del flujo de trabajo.                              */
/* ------------------------------------------------------------------------- */
int cargar_carpeta(const char carpeta[], RegistroCSV registros[]);
void contar_metadatos(const char carpeta[], RegistroCSV registros[], int total);
void leer_campos(const char carpeta[], RegistroCSV registros[], int total);
void leer_valores(const char carpeta[], RegistroCSV registros[], int total);
void leer_senales(const char carpeta[], RegistroCSV registros[], int total);
void obtener_frecuencias(RegistroCSV registros[], int total);
void imprimir_resumen_base(const RegistroCSV registros[], int total);
void imprimir_metadatos(const RegistroCSV registros[], int total);
void generar_archivos_graficos(const char carpeta_salida[], const RegistroCSV registros[], int total);

/* ------------------------------------------------------------------------- */
/* Prototipos de funciones auxiliares.                                        */
/* ------------------------------------------------------------------------- */
void unir_ruta(char destino[], const char carpeta[], const char fichero[]);
void copiar_texto(char destino[], const char origen[], size_t tamano);
void quitar_salto_linea(char texto[]);
int linea_vacia(const char texto[]);
int separar_metadata(const char linea_original[], char campo[], char valor[]);
void limpiar_espacios(char texto[]);
void quitar_comillas_externas(char texto[]);
int buscar_campo(const RegistroCSV *registro, const char campo[]);
double obtener_frecuencia_muestreo(const RegistroCSV *registro);
int termina_con_csv(const char nombre[]);
int contiene_texto_sin_mayusculas(const char texto[], const char patron[]);
int es_csv_gait_10mwt(const char nombre[]);
void ordenar_registros_por_nombre(RegistroCSV registros[], int total);
void inicializar_registro(RegistroCSV *registro);
int obtener_campo_csv(const char linea[], int indice_buscado, char campo[], size_t tamano);
int buscar_indice_columna(const char cabecera[], const char nombre_columna[]);
int convertir_double(const char texto[], double *valor);
int convertir_entero_desde_csv(const char texto[], int *valor);
void crear_carpeta_si_no_existe(const char ruta[]);
void nombre_base_seguro(const char nombre_fichero[], char salida[], size_t tamano);
void escribir_sombreado_sync(FILE *plt, const RegistroCSV *registro, int id_inicial);

/* ------------------------------------------------------------------------- */
/* Funcion principal.                                                        */
/* ------------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    /*
        El programa espera una carpeta con archivos CSV.

        Uso recomendado:
            ./Tarea1_PDB_Aldo_Pastore data/raw/gait

        Si no se pasa argumento, intenta usar la carpeta por defecto
        indicada en la guia.
    */

    static RegistroCSV registros[MAX_ARCHIVOS];       /* Base de datos simple. Se declara static para evitar desbordar el stack. */
    const char *carpeta = "data/raw/gait";            /* Ruta por defecto si se ejecuta desde la raiz del proyecto. */
    const char *carpeta_salida = "salida_tarea1";     /* Carpeta donde se guardaran .dat y .plt. */
    int total = 0;                                    /* Cantidad de archivos cargados. */

    /* Si el usuario pasa una carpeta por consola, se usa esa carpeta. */
    if (argc > 1) {
        carpeta = argv[1];
    }

    /* Si el usuario pasa una segunda ruta, se usa como carpeta de salida. */
    if (argc > 2) {
        carpeta_salida = argv[2];
    }

    /* Paso 1: cargar nombres de archivos CSV de marcha 10MWT. */
    total = cargar_carpeta(carpeta, registros);

    /* Si no se encuentran archivos, se prueba una ruta relativa comun desde dataAnalysis/gait. */
    if (total == 0 && argc <= 1) {
        carpeta = "../../data/raw/gait";
        total = cargar_carpeta(carpeta, registros);
    }

    /* Si tampoco hay archivos, el programa no puede continuar. */
    if (total == 0) {
        printf("No se encontraron archivos CSV de marcha 10MWT en la carpeta indicada.\n");
        printf("Carpeta evaluada: %s\n", carpeta);
        return 1;
    }

    /* Paso 2: contar cuantos metadatos tiene cada archivo. */
    contar_metadatos(carpeta, registros, total);

    /* Paso 3: leer los nombres de los campos de metadatos. */
    leer_campos(carpeta, registros, total);

    /* Paso 4: leer los valores asociados a esos campos. */
    leer_valores(carpeta, registros, total);

    /* Paso 5: leer las senales numericas de interes. */
    leer_senales(carpeta, registros, total);

    /* Paso 6: obtener la frecuencia de muestreo de cada registro. */
    obtener_frecuencias(registros, total);

    /* Paso 7: imprimir un resumen general de la base cargada. */
    imprimir_resumen_base(registros, total);

    /* Paso 8: imprimir metadatos principales para verificar lectura. */
    imprimir_metadatos(registros, total);

    /* Paso 9: generar archivos .dat y .plt para graficar con Gnuplot. */
    generar_archivos_graficos(carpeta_salida, registros, total);

    /* Mensaje final. */
    printf("\nProceso finalizado.\n");
    printf("Archivos de salida generados en: %s\n", carpeta_salida);
    printf("Para crear las imagenes PNG, ejecute cada script .plt con Gnuplot.\n");
    printf("Ejemplo: gnuplot %s/<nombre_del_archivo>.plt\n", carpeta_salida);

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Carga de carpeta.                                                         */
/* ------------------------------------------------------------------------- */
int cargar_carpeta(const char carpeta[], RegistroCSV registros[]) {
    /*
        Recorre la carpeta indicada y carga en registros[] los archivos CSV
        compatibles con el protocolo gait_10MWT.

        Se filtra gait_10MWT para evitar mezclar archivos como subject_key,
        stair_ascent o stair_descent, ya que la Guia 1 se centra en marcha 10MWT.
    */

    int total = 0; /* Contador de archivos validos encontrados. */

#ifdef _WIN32
    /* En Windows se usa _findfirst() con un patron de busqueda. */
    char patron[MAX_RUTA];
    struct _finddata_t datos;
    intptr_t busqueda;

    unir_ruta(patron, carpeta, "*.csv");
    busqueda = _findfirst(patron, &datos);

    if (busqueda == -1) {
        return 0;
    }

    do {
        if ((datos.attrib & _A_SUBDIR) == 0 && total < MAX_ARCHIVOS) {
            if (es_csv_gait_10mwt(datos.name)) {
                inicializar_registro(&registros[total]);
                copiar_texto(registros[total].nombre_fichero, datos.name, MAX_TEXTO);
                total++;
            }
        }
    } while (_findnext(busqueda, &datos) == 0);

    _findclose(busqueda);
#else
    /* En Linux/macOS se usa opendir() y readdir(). */
    DIR *directorio = opendir(carpeta);
    struct dirent *entrada;

    if (directorio == NULL) {
        return 0;
    }

    while ((entrada = readdir(directorio)) != NULL && total < MAX_ARCHIVOS) {
        if (es_csv_gait_10mwt(entrada->d_name)) {
            inicializar_registro(&registros[total]);
            copiar_texto(registros[total].nombre_fichero, entrada->d_name, MAX_TEXTO);
            total++;
        }
    }

    closedir(directorio);
#endif

    /* Se ordena alfabeticamente para que los indices sean estables. */
    ordenar_registros_por_nombre(registros, total);

    return total;
}

/* ------------------------------------------------------------------------- */
/* Conteo de metadatos.                                                      */
/* ------------------------------------------------------------------------- */
void contar_metadatos(const char carpeta[], RegistroCSV registros[], int total) {
    /*
        Cuenta cuantas lineas de metadatos tiene cada archivo.
        La cuenta termina al encontrar la primera linea vacia.
    */

    for (int i = 0; i < total; i++) {
        char ruta[MAX_RUTA];
        char linea[MAX_LINEA];
        FILE *archivo;
        int contador = 0;

        unir_ruta(ruta, carpeta, registros[i].nombre_fichero);
        archivo = fopen(ruta, "r");

        if (archivo == NULL) {
            registros[i].total_metadatos = 0;
            continue;
        }

        while (fgets(linea, sizeof(linea), archivo) != NULL) {
            quitar_salto_linea(linea);

            if (linea_vacia(linea)) {
                break;
            }

            if (strchr(linea, ',') != NULL && contador < MAX_METADATOS) {
                contador++;
            }
        }

        fclose(archivo);
        registros[i].total_metadatos = contador;
    }
}

/* ------------------------------------------------------------------------- */
/* Lectura de campos de metadatos.                                           */
/* ------------------------------------------------------------------------- */
void leer_campos(const char carpeta[], RegistroCSV registros[], int total) {
    /*
        Lee solamente los nombres de los campos de metadatos.
        Ejemplo: de "Subject,S01" guarda "Subject".
    */

    for (int i = 0; i < total; i++) {
        char ruta[MAX_RUTA];
        char linea[MAX_LINEA];
        FILE *archivo;
        int j = 0;

        unir_ruta(ruta, carpeta, registros[i].nombre_fichero);
        archivo = fopen(ruta, "r");

        if (archivo == NULL) {
            continue;
        }

        while (fgets(linea, sizeof(linea), archivo) != NULL && j < registros[i].total_metadatos) {
            char campo[MAX_TEXTO];
            char valor[MAX_TEXTO];

            quitar_salto_linea(linea);

            if (linea_vacia(linea)) {
                break;
            }

            if (separar_metadata(linea, campo, valor)) {
                copiar_texto(registros[i].campos[j], campo, MAX_TEXTO);
                registros[i].valores[j][0] = '\0';
                j++;
            }
        }

        fclose(archivo);
    }
}

/* ------------------------------------------------------------------------- */
/* Lectura de valores de metadatos.                                          */
/* ------------------------------------------------------------------------- */
void leer_valores(const char carpeta[], RegistroCSV registros[], int total) {
    /*
        Lee los valores asociados a los campos de metadatos.
        Ejemplo: de "Subject,S01" guarda "S01" como valor.
    */

    for (int i = 0; i < total; i++) {
        char ruta[MAX_RUTA];
        char linea[MAX_LINEA];
        FILE *archivo;
        int j = 0;

        unir_ruta(ruta, carpeta, registros[i].nombre_fichero);
        archivo = fopen(ruta, "r");

        if (archivo == NULL) {
            continue;
        }

        while (fgets(linea, sizeof(linea), archivo) != NULL && j < registros[i].total_metadatos) {
            char campo[MAX_TEXTO];
            char valor[MAX_TEXTO];

            quitar_salto_linea(linea);

            if (linea_vacia(linea)) {
                break;
            }

            if (separar_metadata(linea, campo, valor)) {
                copiar_texto(registros[i].valores[j], valor, MAX_TEXTO);
                j++;
            }
        }

        fclose(archivo);
    }
}

/* ------------------------------------------------------------------------- */
/* Lectura de senales numericas.                                             */
/* ------------------------------------------------------------------------- */
void leer_senales(const char carpeta[], RegistroCSV registros[], int total) {
    /*
        Lee el bloque numerico de cada CSV y conserva solo las columnas:
        - Angle_X
        - Linear_Acceleration_Z
        - Segmentation_output
        - Sync

        Para mayor robustez, primero busca los indices de las columnas en la
        cabecera. Asi el programa no queda completamente atado a posiciones
        fijas de columnas.
    */

    for (int i = 0; i < total; i++) {
        char ruta[MAX_RUTA];
        char linea[MAX_LINEA];
        FILE *archivo;
        int metadata_done = 0;
        int cabecera_encontrada = 0;
        int idx_angle = -1;
        int idx_acc_z = -1;
        int idx_seg = -1;
        int idx_sync = -1;
        int n = 0;

        unir_ruta(ruta, carpeta, registros[i].nombre_fichero);
        archivo = fopen(ruta, "r");

        if (archivo == NULL) {
            registros[i].total_muestras = 0;
            continue;
        }

        while (fgets(linea, sizeof(linea), archivo) != NULL) {
            quitar_salto_linea(linea);

            if (linea_vacia(linea)) {
                metadata_done = 1;
                continue;
            }

            if (!metadata_done) {
                continue;
            }

            if (!cabecera_encontrada) {
                if (strstr(linea, "Angle_X") != NULL) {
                    idx_angle = buscar_indice_columna(linea, "Angle_X");
                    idx_acc_z = buscar_indice_columna(linea, "Linear_Acceleration_Z");
                    idx_seg = buscar_indice_columna(linea, "Segmentation_output");
                    idx_sync = buscar_indice_columna(linea, "Sync");

                    if (idx_angle >= 0 && idx_acc_z >= 0 && idx_seg >= 0 && idx_sync >= 0) {
                        cabecera_encontrada = 1;
                    } else {
                        break;
                    }
                }
                continue;
            }

            if (n >= MAX_MUESTRAS) {
                break;
            }

            char campo_angle[MAX_TEXTO];
            char campo_acc_z[MAX_TEXTO];
            char campo_seg[MAX_TEXTO];
            char campo_sync[MAX_TEXTO];
            double angle;
            double acc_z;
            int seg;
            int sync;

            if (!obtener_campo_csv(linea, idx_angle, campo_angle, sizeof(campo_angle))) {
                continue;
            }
            if (!obtener_campo_csv(linea, idx_acc_z, campo_acc_z, sizeof(campo_acc_z))) {
                continue;
            }
            if (!obtener_campo_csv(linea, idx_seg, campo_seg, sizeof(campo_seg))) {
                continue;
            }
            if (!obtener_campo_csv(linea, idx_sync, campo_sync, sizeof(campo_sync))) {
                continue;
            }

            if (!convertir_double(campo_angle, &angle)) {
                continue;
            }
            if (!convertir_double(campo_acc_z, &acc_z)) {
                continue;
            }
            if (!convertir_entero_desde_csv(campo_seg, &seg)) {
                continue;
            }
            if (!convertir_entero_desde_csv(campo_sync, &sync)) {
                continue;
            }

            registros[i].angle_x[n] = angle;
            registros[i].linear_acceleration_z[n] = acc_z;
            registros[i].segmentation_output[n] = seg;
            registros[i].sync[n] = sync;
            n++;
        }

        fclose(archivo);
        registros[i].total_muestras = n;
    }
}

/* ------------------------------------------------------------------------- */
/* Obtencion de frecuencia de muestreo.                                      */
/* ------------------------------------------------------------------------- */
void obtener_frecuencias(RegistroCSV registros[], int total) {
    /* Recorre todos los registros y obtiene Sampling Frequency. */
    for (int i = 0; i < total; i++) {
        registros[i].frecuencia_muestreo = obtener_frecuencia_muestreo(&registros[i]);
    }
}

/* ------------------------------------------------------------------------- */
/* Impresion de resumen.                                                     */
/* ------------------------------------------------------------------------- */
void imprimir_resumen_base(const RegistroCSV registros[], int total) {
    printf("\n============================================================\n");
    printf("RESUMEN DE BASE DE DATOS CARGADA\n");
    printf("============================================================\n");
    printf("Archivos CSV de marcha 10MWT encontrados: %d\n", total);
    printf("\n");

    for (int i = 0; i < total; i++) {
        printf("[%d] %s | metadatos: %d | muestras: %d | fs: %.3f Hz\n",
               i,
               registros[i].nombre_fichero,
               registros[i].total_metadatos,
               registros[i].total_muestras,
               registros[i].frecuencia_muestreo);
    }
}

/* ------------------------------------------------------------------------- */
/* Impresion de metadatos principales.                                       */
/* ------------------------------------------------------------------------- */
void imprimir_metadatos(const RegistroCSV registros[], int total) {
    /*
        Imprime algunos metadatos principales de cada archivo.
        Esto permite verificar que la lectura del bloque inicial fue correcta.
    */

    printf("\n============================================================\n");
    printf("METADATOS PRINCIPALES\n");
    printf("============================================================\n");

    for (int i = 0; i < total; i++) {
        int idx_subject = buscar_campo(&registros[i], "Subject");
        int idx_activity = buscar_campo(&registros[i], "Activity");
        int idx_samples = buscar_campo(&registros[i], "Number of Samples");
        int idx_frequency = buscar_campo(&registros[i], CAMPO_FRECUENCIA);

        printf("\nArchivo: %s\n", registros[i].nombre_fichero);

        if (idx_subject >= 0) {
            printf("  Subject: %s\n", registros[i].valores[idx_subject]);
        }
        if (idx_activity >= 0) {
            printf("  Activity: %s\n", registros[i].valores[idx_activity]);
        }
        if (idx_samples >= 0) {
            printf("  Number of Samples: %s\n", registros[i].valores[idx_samples]);
        }
        if (idx_frequency >= 0) {
            printf("  Sampling Frequency: %s Hz\n", registros[i].valores[idx_frequency]);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Generacion de archivos para graficar.                                     */
/* ------------------------------------------------------------------------- */
void generar_archivos_graficos(const char carpeta_salida[], const RegistroCSV registros[], int total) {
    /*
        Genera por cada registro:
        - un archivo .dat con columnas: tiempo, Angle_X, Linear_Acceleration_Z, Sync.
        - un archivo .plt con instrucciones para Gnuplot.

        El script .plt crea una figura PNG con dos subgraficos y sombreado
        en las zonas donde Sync != 0.
    */

    crear_carpeta_si_no_existe(carpeta_salida);

    for (int i = 0; i < total; i++) {
        char base[MAX_TEXTO];
        char ruta_dat[MAX_RUTA];
        char ruta_plt[MAX_RUTA];
        char ruta_png[MAX_RUTA];
        FILE *dat;
        FILE *plt;
        double fs = registros[i].frecuencia_muestreo;

        if (registros[i].total_muestras <= 0 || fs <= 0.0) {
            continue;
        }

        nombre_base_seguro(registros[i].nombre_fichero, base, sizeof(base));

        snprintf(ruta_dat, sizeof(ruta_dat), "%s/%s.dat", carpeta_salida, base);
        snprintf(ruta_plt, sizeof(ruta_plt), "%s/%s.plt", carpeta_salida, base);
        snprintf(ruta_png, sizeof(ruta_png), "%s/%s.png", carpeta_salida, base);

        dat = fopen(ruta_dat, "w");
        if (dat == NULL) {
            continue;
        }

        fprintf(dat, "# tiempo_s Angle_X Linear_Acceleration_Z Sync\n");
        for (int j = 0; j < registros[i].total_muestras; j++) {
            double tiempo = (double)j / fs;
            fprintf(dat, "%.8f %.8f %.8f %d\n",
                    tiempo,
                    registros[i].angle_x[j],
                    registros[i].linear_acceleration_z[j],
                    registros[i].sync[j]);
        }
        fclose(dat);

        plt = fopen(ruta_plt, "w");
        if (plt == NULL) {
            continue;
        }

        fprintf(plt, "set terminal pngcairo size 1200,800 enhanced font 'Arial,10'\n");
        fprintf(plt, "set output '%s'\n", ruta_png);
        fprintf(plt, "set multiplot layout 2,1 title '%s'\n", registros[i].nombre_fichero);
        fprintf(plt, "set grid\n");
        fprintf(plt, "set key outside\n");
        fprintf(plt, "set xlabel 'Tiempo (s)'\n");

        fprintf(plt, "set ylabel 'Angle\\_X'\n");
        escribir_sombreado_sync(plt, &registros[i], 1);
        fprintf(plt, "plot '%s' using 1:2 with lines title 'Angle_X'\n", ruta_dat);
        fprintf(plt, "unset object\n");

        fprintf(plt, "set ylabel 'Linear\\_Acceleration\\_Z'\n");
        escribir_sombreado_sync(plt, &registros[i], 1001);
        fprintf(plt, "plot '%s' using 1:3 with lines title 'Linear_Acceleration_Z'\n", ruta_dat);
        fprintf(plt, "unset object\n");

        fprintf(plt, "unset multiplot\n");
        fclose(plt);
    }
}

/* ------------------------------------------------------------------------- */
/* Funciones auxiliares de rutas y texto.                                    */
/* ------------------------------------------------------------------------- */
void unir_ruta(char destino[], const char carpeta[], const char fichero[]) {
    size_t n = strlen(carpeta);

    if (n > 0 && (carpeta[n - 1] == '/' || carpeta[n - 1] == '\\')) {
        snprintf(destino, MAX_RUTA, "%s%s", carpeta, fichero);
    } else {
        snprintf(destino, MAX_RUTA, "%s/%s", carpeta, fichero);
    }
}

void copiar_texto(char destino[], const char origen[], size_t tamano) {
    if (tamano == 0) {
        return;
    }
    strncpy(destino, origen, tamano - 1);
    destino[tamano - 1] = '\0';
}

void quitar_salto_linea(char texto[]) {
    size_t n = strlen(texto);

    while (n > 0 && (texto[n - 1] == '\n' || texto[n - 1] == '\r')) {
        texto[n - 1] = '\0';
        n--;
    }
}

int linea_vacia(const char texto[]) {
    int i = 0;

    while (texto[i] != '\0') {
        if (!isspace((unsigned char)texto[i])) {
            return 0;
        }
        i++;
    }

    return 1;
}

int separar_metadata(const char linea_original[], char campo[], char valor[]) {
    /*
        Divide una linea de metadatos usando solamente la primera coma.
        Esto permite que el valor contenga comas internas, por ejemplo:
        Instrumentation,NP-HGAIT, HW : v5.1 , FW : v5.1
    */

    char linea[MAX_LINEA];
    char *coma;

    copiar_texto(linea, linea_original, MAX_LINEA);
    coma = strchr(linea, ',');

    if (coma == NULL) {
        return 0;
    }

    *coma = '\0';
    copiar_texto(campo, linea, MAX_TEXTO);
    copiar_texto(valor, coma + 1, MAX_TEXTO);

    limpiar_espacios(campo);
    limpiar_espacios(valor);
    quitar_comillas_externas(valor);

    return 1;
}

void limpiar_espacios(char texto[]) {
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

void quitar_comillas_externas(char texto[]) {
    size_t largo = strlen(texto);

    if (largo >= 2 && texto[0] == '"' && texto[largo - 1] == '"') {
        memmove(texto, texto + 1, largo - 2);
        texto[largo - 2] = '\0';
    }
}

int buscar_campo(const RegistroCSV *registro, const char campo[]) {
    for (int i = 0; i < registro->total_metadatos; i++) {
        if (strcmp(registro->campos[i], campo) == 0) {
            return i;
        }
    }

    return -1;
}

double obtener_frecuencia_muestreo(const RegistroCSV *registro) {
    int indice = buscar_campo(registro, CAMPO_FRECUENCIA);

    if (indice < 0) {
        return 0.0;
    }

    return atof(registro->valores[indice]);
}

/* ------------------------------------------------------------------------- */
/* Funciones auxiliares de filtrado y ordenamiento.                          */
/* ------------------------------------------------------------------------- */
int termina_con_csv(const char nombre[]) {
    size_t n = strlen(nombre);

    if (n < 4) {
        return 0;
    }

    return tolower((unsigned char)nombre[n - 4]) == '.' &&
           tolower((unsigned char)nombre[n - 3]) == 'c' &&
           tolower((unsigned char)nombre[n - 2]) == 's' &&
           tolower((unsigned char)nombre[n - 1]) == 'v';
}

int contiene_texto_sin_mayusculas(const char texto[], const char patron[]) {
    size_t n = strlen(texto);
    size_t m = strlen(patron);

    if (m == 0 || n < m) {
        return 0;
    }

    for (size_t i = 0; i <= n - m; i++) {
        size_t j = 0;
        while (j < m &&
               tolower((unsigned char)texto[i + j]) == tolower((unsigned char)patron[j])) {
            j++;
        }
        if (j == m) {
            return 1;
        }
    }

    return 0;
}

int es_csv_gait_10mwt(const char nombre[]) {
    return termina_con_csv(nombre) && contiene_texto_sin_mayusculas(nombre, "gait_10MWT");
}

void ordenar_registros_por_nombre(RegistroCSV registros[], int total) {
    for (int i = 0; i < total - 1; i++) {
        for (int j = i + 1; j < total; j++) {
            if (strcmp(registros[i].nombre_fichero, registros[j].nombre_fichero) > 0) {
                RegistroCSV temp = registros[i];
                registros[i] = registros[j];
                registros[j] = temp;
            }
        }
    }
}

void inicializar_registro(RegistroCSV *registro) {
    registro->nombre_fichero[0] = '\0';
    registro->total_metadatos = 0;
    registro->total_muestras = 0;
    registro->frecuencia_muestreo = 0.0;

    for (int i = 0; i < MAX_METADATOS; i++) {
        registro->campos[i][0] = '\0';
        registro->valores[i][0] = '\0';
    }
}

/* ------------------------------------------------------------------------- */
/* Funciones auxiliares de lectura CSV numerica.                             */
/* ------------------------------------------------------------------------- */
int obtener_campo_csv(const char linea[], int indice_buscado, char campo[], size_t tamano) {
    int indice_actual = 0;
    size_t pos = 0;

    if (indice_buscado < 0 || tamano == 0) {
        return 0;
    }

    campo[0] = '\0';

    for (size_t i = 0; ; i++) {
        char c = linea[i];

        if (c == ',' || c == '\0') {
            if (indice_actual == indice_buscado) {
                campo[pos] = '\0';
                limpiar_espacios(campo);
                quitar_comillas_externas(campo);
                return 1;
            }
            indice_actual++;
            pos = 0;

            if (c == '\0') {
                break;
            }
        } else if (indice_actual == indice_buscado) {
            if (pos + 1 < tamano) {
                campo[pos++] = c;
            }
        }
    }

    return 0;
}

int buscar_indice_columna(const char cabecera[], const char nombre_columna[]) {
    char campo[MAX_TEXTO];

    for (int indice = 0; indice < 100; indice++) {
        if (!obtener_campo_csv(cabecera, indice, campo, sizeof(campo))) {
            break;
        }

        if (strcmp(campo, nombre_columna) == 0) {
            return indice;
        }
    }

    return -1;
}

int convertir_double(const char texto[], double *valor) {
    /*
        Convierte un campo CSV a double.

        Se rechazan valores no finitos como nan o inf para imitar el criterio
        de limpieza usado normalmente en Python con dropna(). De esta manera,
        las filas incompletas no se guardan como muestras validas.
    */

    char *fin;

    if (texto == NULL || texto[0] == '\0') {
        return 0;
    }

    *valor = strtod(texto, &fin);

    if (fin == texto) {
        return 0;
    }

    if (!isfinite(*valor)) {
        return 0;
    }

    return 1;
}

int convertir_entero_desde_csv(const char texto[], int *valor) {
    /*
        Convierte un campo CSV a entero.
        Primero se lee como double para aceptar textos como "0" o "0.0".
    */

    double temporal;

    if (!convertir_double(texto, &temporal)) {
        return 0;
    }

    *valor = (int)temporal;
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Funciones auxiliares para salida grafica.                                 */
/* ------------------------------------------------------------------------- */
void crear_carpeta_si_no_existe(const char ruta[]) {
#ifdef _WIN32
    if (_mkdir(ruta) != 0 && errno != EEXIST) {
        printf("Advertencia: no se pudo crear la carpeta de salida: %s\n", ruta);
    }
#else
    if (mkdir(ruta, 0777) != 0 && errno != EEXIST) {
        printf("Advertencia: no se pudo crear la carpeta de salida: %s\n", ruta);
    }
#endif
}

void nombre_base_seguro(const char nombre_fichero[], char salida[], size_t tamano) {
    size_t j = 0;

    if (tamano == 0) {
        return;
    }

    for (size_t i = 0; nombre_fichero[i] != '\0' && j + 1 < tamano; i++) {
        char c = nombre_fichero[i];

        if (c == '.') {
            break;
        }

        if (isalnum((unsigned char)c) || c == '_' || c == '-') {
            salida[j++] = c;
        } else {
            salida[j++] = '_';
        }
    }

    salida[j] = '\0';

    if (j == 0) {
        copiar_texto(salida, "registro", tamano);
    }
}

void escribir_sombreado_sync(FILE *plt, const RegistroCSV *registro, int id_inicial) {
    int en_intervalo = 0;
    int inicio = 0;
    int id = id_inicial;
    double fs = registro->frecuencia_muestreo;

    if (fs <= 0.0) {
        return;
    }

    for (int i = 0; i < registro->total_muestras; i++) {
        if (registro->sync[i] != 0 && !en_intervalo) {
            en_intervalo = 1;
            inicio = i;
        }

        if ((registro->sync[i] == 0 || i == registro->total_muestras - 1) && en_intervalo) {
            int fin = (registro->sync[i] == 0) ? i : i + 1;
            double t_inicio = (double)inicio / fs;
            double t_fin = (double)fin / fs;

            fprintf(plt,
                    "set object %d rect from %.8f, graph 0 to %.8f, graph 1 behind "
                    "fc rgb '#dddddd' fs solid 0.35 noborder\n",
                    id,
                    t_inicio,
                    t_fin);
            id++;
            en_intervalo = 0;
        }
    }
}
