#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

/*
    tarea2_template.c

    Plantilla base para la tarea 2 en C.

    La idea de esta plantilla es que el estudiante complete las funciones
    necesarias para:
    1. abrir un archivo CSV de marcha,
    2. extraer la frecuencia de muestreo desde los metadatos,
    3. leer las columnas Angle_X, Linear_Acceleration_Z,
       Segmentation_output y Sync,
    4. corregir el signo de Linear_Acceleration_Z,
    5. detectar la ventana Sync,
    6. contar pasos con transiciones S3 -> S0,
    7. calcular metricas del registro.
*/

#define MAX_LINEA 1024
#define MAX_MUESTRAS 20000
#define DISTANCIA_UTIL_10MWT_M 6.0

typedef struct {
    double angle_x;
    double linear_acceleration_z;
    int segmentation_output;
    int sync;
} Datos; //Se utiliza para función de buscar_indice_primera_sync (AP)

/* Prototipos */
int cargar_metadata(const char archivo[], double *frecuencia_muestreo);
int cargar_datos(
    const char archivo[],
    Datos registros[],
    int max_registros,
    int *total
);
void corregir_aceleracion(Datos registros[], int total);
double calcular_longitud_temporal(int total, double fs);
int buscar_indice_primera_sync(const Datos registros[], int total);
int buscar_indice_ultima_sync(const Datos registros[], int total);
int contar_transiciones_S3_S0(const Datos registros[], int inicio, int fin);
double calcular_velocidad_marcha(int muestras_sync, double fs);
double calcular_velocidad_pasos(int pasos, int muestras_pasos, double fs);
double calcular_longitud_zancada(
    double velocidad_marcha,
    double velocidad_pasos
);
void imprimir_resultados(
    int total,
    double fs,
    int muestras_sync,
    int pasos,
    double velocidad_marcha,
    double velocidad_pasos,
    double longitud_zancada
);
void quitar_salto_linea(char texto[]);
int linea_vacia(const char texto[]);

int main(int argc, char *argv[]) {
    /*
        Flujo principal del programa.

        La estructura ya esta lista.
        El estudiante debe completar las funciones principales para que
        este flujo produzca resultados reales.
    */
    const char *archivo = "data/raw/gait/S01_gait_10MWT_01.csv";
    if (argc > 1) {
        archivo = argv[1];
    }

    double fs = 0.0;
    if (!cargar_metadata(archivo, &fs)) {
        archivo = "../../data/raw/gait/S01_gait_10MWT_01.csv";
        if (!cargar_metadata(archivo, &fs)) {
            printf(
                "No se pudo leer la frecuencia de muestreo del archivo: %s\n",
                archivo
            );
            return 1;
        }
    }

    Datos registros[MAX_MUESTRAS];
    int total = 0;
    if (!cargar_datos(archivo, registros, MAX_MUESTRAS, &total)) {
        printf("Error al cargar los datos del archivo: %s\n", archivo);
        return 1;
    }

    if (total == 0) {
        printf("No se encontraron muestras de datos en el archivo: %s\n", archivo);
        return 1;
    }

    corregir_aceleracion(registros, total);

    int inicio_sync = buscar_indice_primera_sync(registros, total);
    int fin_sync = buscar_indice_ultima_sync(registros, total);
    int muestras_sync = 0;

    if (inicio_sync >= 0 && fin_sync > inicio_sync) {
        muestras_sync = fin_sync - inicio_sync;
    }

    int pasos = contar_transiciones_S3_S0(registros, inicio_sync, fin_sync);
    double tiempo_total = calcular_longitud_temporal(total, fs);
    double velocidad_marcha = 0.0;
    double velocidad_pasos = 0.0;
    double longitud_zancada = 0.0;

    if (muestras_sync > 0) {
        velocidad_marcha = calcular_velocidad_marcha(muestras_sync, fs);
        velocidad_pasos = calcular_velocidad_pasos(pasos, muestras_sync, fs);

        if (velocidad_pasos > 0.0) {
            longitud_zancada = calcular_longitud_zancada(
                velocidad_marcha,
                velocidad_pasos
            );
        }
    }

    imprimir_resultados(
        total,
        fs,
        muestras_sync,
        pasos,
        velocidad_marcha,
        velocidad_pasos,
        longitud_zancada
    );

    printf(
        "Correccion aplicada: cambio de signo en "
        "Linear_Acceleration_Z de cada muestra.\n"
    );
    printf("Tiempo total del registro: %.3f s\n", tiempo_total);

    return 0;
}

int cargar_metadata(const char archivo[], double *frecuencia_muestreo) {
    /*
        Recorre el bloque de metadatos del CSV hasta encontrar:
            Sampling Frequency,<valor>
        En cuanto lo encuentra, convierte ese valor a double.
    */
    FILE *fp = fopen(archivo, "r");
    if (!fp) {
        return 0;
    }

    char linea[MAX_LINEA];
    while (fgets(linea, sizeof(linea), fp)) {
        quitar_salto_linea(linea);

        /* La primera linea vacia marca el fin de los metadatos. */
        if (linea_vacia(linea)) {
            break;
        }

        if (strstr(linea, "Sampling Frequency") != NULL) {
            char *coma = strchr(linea, ',');

            if (coma != NULL) {
                *frecuencia_muestreo = atof(coma + 1);
                fclose(fp);
                return *frecuencia_muestreo > 0.0;
            }
        }
    }

    fclose(fp);
    return 0;
}

int cargar_datos(
    const char archivo[],
    Datos registros[],
    int max_registros,
    int *total
) {
    /*
        Lee el bloque numerico del CSV.

        1. avanzar hasta terminar los metadatos,
        2. detectar la cabecera donde aparece Angle_X,
        3. leer solo las 4 columnas de interes usando sscanf.
    */
    FILE *fp = fopen(archivo, "r");
    if (!fp) {
        return 0;
    }

    char linea[MAX_LINEA];
    int metadata_done = 0;
    int en_datos = 0;
    *total = 0;

    while (fgets(linea, sizeof(linea), fp)) {
        quitar_salto_linea(linea);

        if (linea_vacia(linea)) {
            metadata_done = 1;
            continue;
        }

        if (!metadata_done) {
            continue;
        }

        if (!en_datos) {
            if (strstr(linea, "Angle_X") != NULL) {
                en_datos = 1;
            }
            continue;
        }

        if (*total >= max_registros) {
            break;
        }

        Datos registro;
        if (
            sscanf(
                linea,
                "%lf,%*lf,%*lf,%*lf,%*lf,%*lf,%*lf,%*lf,%lf,%*lf,%*lf,%d,%d",
                &registro.angle_x,
                &registro.linear_acceleration_z,
                &registro.segmentation_output,
                &registro.sync
            ) == 4
        ) {
            registros[*total] = registro;
            (*total)++;
        }
    }

    fclose(fp);
    return *total > 0;
}

void corregir_aceleracion(Datos registros[], int total) {
    /*
        Tarea del estudiante: Taniar Romero
        1. recorrer todas las muestras,
        2. multiplicar por -1 el valor linear_acceleration_z.

        Esta version no modifica nada.
    */
    int i;
    for (i = 0; i < total; i++) {
        registros[i].linear_acceleration_z = registros[i].linear_acceleration_z * -1.0;
    }
}

double calcular_longitud_temporal(int total, double fs) {
    /*
        Tarea del estudiante:
        1. verificar que fs sea mayor que cero,
        2. calcular el tiempo total como:
              tiempo = total / fs
        3. devolver el resultado en segundos.

        Mientras no este implementada, devuelve 0.0.
    */
    if (fs <= 0.0) {
        printf("Frecuencia de muestreo invalida: %.3f Hz\n", fs);
        return 0.0;
    }
   double seg = (double)total / fs;
    return 0.0;
}

int buscar_indice_primera_sync(const Datos registros[], int total) {
    /*
        Tarea del estudiante:
        1. recorrer el arreglo desde el inicio,
        2. encontrar la primera muestra donde sync != 0,
        3. devolver ese indice,
        4. si no hay ninguna, devolver -1.
    */

    /* Busca la primera muestra donde Sync es distinto de cero. AP*/
    for (int i = 0; i < total; i++) {
        if (registros[i].sync != 0) {
            return i;
        }
    }
        
    (void)registros;
    (void)total;
    return -1;
}

int buscar_indice_ultima_sync(const Datos registros[], int total) {
    /*
        Tarea del estudiante:
        1. recorrer el arreglo desde el final,
        2. encontrar la ultima muestra donde sync != 0,
        3. devolver ese indice,
        4. si no hay ninguna, devolver -1.
    */
    (void)registros;
    (void)total;
    return -1;
}

int contar_transiciones_S3_S0(const Datos registros[], int inicio, int fin) {
    /*
        Tarea del estudiante:
        1. verificar que inicio y fin definan una ventana valida,
        2. recorrer segmentation_output entre inicio y fin,
        3. contar cuantas veces aparece la transicion 3 -> 0,
        4. devolver la cantidad de pasos detectados.
    */
 if (inicio < 0 || fin <= inicio) {
        printf("Ventana Sync invalida: inicio=%d, fin=%d\n", inicio, fin);
        return 0;
    }
        int pasos = 0;
    for (int i = inicio + 1; i < fin; i++) {
        if (registros[i - 1].segmentation_output == 3 &&
            registros[i].segmentation_output == 0) {
            pasos++;
        }
    }
    return 0;
}

double calcular_velocidad_marcha(int muestras_sync, double fs) {
    /*
        Tarea del estudiante:
        1. calcular el tiempo de la ventana Sync:
              tiempo = muestras_sync / fs
        2. usar la distancia util de 6 metros,
        3. devolver distancia / tiempo.

        Mientras no este implementada, devuelve 0.0.
    */
    (void)muestras_sync;
    (void)fs;
    return 0.0;
}

double calcular_velocidad_pasos(int pasos, int muestras_pasos, double fs) {
    /*
        Tarea del estudiante: Tania Romero
        1. verificar que pasos, muestras_pasos y fs sean validos,
        2. calcular el tiempo de la ventana:
              tiempo = muestras_pasos / fs
        3. dividir la cantidad de pasos por ese tiempo,
        4. devolver el resultado en pasos/s.

        Mientras no este implementada, devuelve 0.0.
    */
    if (fs <= 0.0 || muestras_pasos <= 0 || pasos < 0) {
        return 0.0;
    }
    double tiempo = (double)muestras_pasos / fs;
    return (double)pasos / tiempo;
}

double calcular_longitud_zancada(double velocidad_marcha, double velocidad_pasos) {
    /*
        Tarea del estudiante:
        1. verificar que velocidad_pasos sea mayor que cero,
        2. calcular:
              longitud = velocidad_marcha / velocidad_pasos
        3. devolver el resultado en metros por paso.

        Mientras no este implementada, devuelve 0.0.
    */

    /* Calcula longitud media por paso: L = v_marcha / v_pasos. AP*/
    if (velocidad_marcha <= 0.0 || velocidad_pasos <= 0.0) {
        return 0.0;
    }
    return velocidad_marcha / velocidad_pasos;
    
    (void)velocidad_marcha;
    (void)velocidad_pasos;
    //return 0.0;
}

void imprimir_resultados(
    int total,
    double fs,
    int muestras_sync,
    int pasos,
    double velocidad_marcha,
    double velocidad_pasos,
    double longitud_zancada
) {
    /*
        Funcion ya resuelta.
        Imprime en pantalla el resumen final del analisis.
    */
    printf("Muestras leidas: %d\n", total);
    printf("Frecuencia de muestreo: %.3f Hz\n", fs);
    printf("Muestras entre SYNC: %d\n", muestras_sync);
    printf("Velocidad de marcha: %.3f m/s\n", velocidad_marcha);
    printf("Pasos detectados (S3->S0): %d\n", pasos);
    printf("Velocidad media de pasos: %.3f pasos/s\n", velocidad_pasos);
    printf("Longitud media de zancada estimada: %.3f m\n", longitud_zancada);
}

void quitar_salto_linea(char texto[]) {
    /*
        Funcion ya resuelta.
        Elimina \n y \r al final de una linea leida con fgets.
    */
    size_t len = strlen(texto);

    if (len > 0 && texto[len - 1] == '\n') {
        texto[len - 1] = '\0';
    }

    if (len > 1 && texto[len - 2] == '\r') {
        texto[len - 2] = '\0';
    }
}

int linea_vacia(const char texto[]) {
    /*
        Funcion ya resuelta.
        Devuelve 1 si la linea solo contiene espacios o esta vacia.
    */
    for (size_t i = 0; texto[i]; i++) {
        if (!isspace((unsigned char)texto[i])) {
            return 0;
        }
    }

    return 1;
}
