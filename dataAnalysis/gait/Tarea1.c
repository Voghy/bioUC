/*
Tarea 1 
Alumno: Ezequiel Zelaya
Intitucion: Universidad Catolica de Asuncion - Facultad de ciencias y tecnologia
Carrera: Electronica 
Año: 2026
*/


#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#else
#include <dirent.h>
#endif

/*
    tarea1.c

    Lee los archivos CSV de data/raw/gait y extrae los metadatos iniciales.

    La organizacion queda separada en pasos:
    1. cargar_carpeta: carga solamente los nombres de los ficheros en nuestra "macro" estructura.
    2. contar_metadatos: cuenta cuantos metadatos tiene cada fichero.
    3. leer_campos: lee los nombres de los campos de metadatos y los carga en la "macro" estructura.
    4. leer_valores: lee los valores correspondientes a esos campos y los carga en la "macro" estructura.
    5. imprimir_metadatos: muestra el resultado recorriendo la "macro" estructura..
*/

#define MAX_ARCHIVOS 100
#define MAX_METADATOS 40
#define MAX_TEXTO 256
#define MAX_LINEA 1024
#define MAX_RUTA 512
#define MAX_MUESTRAS 2000

// "macro" estructura
typedef struct {
    char nombre_fichero[MAX_TEXTO];
    int total_metadatos;
    char campos[MAX_METADATOS][MAX_TEXTO];
    char valores[MAX_METADATOS][MAX_TEXTO];
    int total_muestras;
    double angle_x[MAX_MUESTRAS];
    double linear_acceleration_z[MAX_MUESTRAS];
    int segmentation_output[MAX_MUESTRAS];
    int sync[MAX_MUESTRAS];
} RegistroCSV;

//funcion que busca todos los ficheros disponibles y carga sus nombres en la estructura de datos
int cargar_carpeta(const char carpeta[], RegistroCSV registros[]);
//funcion que cuenta cuantas lineas de metadatos tiene el fichero
void contar_metadatos(const char carpeta[], RegistroCSV registros[], int total);
//funcion que lee los datos
void leer_campos(const char carpeta[], RegistroCSV registros[], int total);
//funciones  a implementar
void leer_valores(const char carpeta[], RegistroCSV registros[], int total);
void imprimir_metadatos(const RegistroCSV registros[], int total);

// Funciones de soporte
void unir_ruta(char destino[], const char carpeta[], const char fichero[]);
void copiar_texto(char destino[], const char origen[], size_t tamano);
void quitar_salto_linea(char texto[]);
int linea_vacia(const char texto[]);
int separar_metadata(const char linea_original[], char campo[], char valor[]);
int buscar_campo(const RegistroCSV *registro, const char campo[]);
void limpiar_espacios(char texto[]);
void leer_senales(const char carpeta[], RegistroCSV registros[], int total);
int buscar_columna(char columnas[][MAX_TEXTO], int total_columnas, const char nombre[]);
int separar_columnas(char linea[], char columnas[][MAX_TEXTO], int max_columnas);
void crear_csv_senales_por_sujeto(const RegistroCSV registros[], int total, const char carpeta_salida[]);
const char *obtener_valor_campo(const RegistroCSV *registro, const char campo[]);
void obtener_prueba_desde_nombre(const char nombre_fichero[], char prueba[], size_t tamano);
void graficar_registro_gnuplot(const RegistroCSV *registro, double fs);

int main(int argc, char *argv[]) {
    static RegistroCSV registros[MAX_ARCHIVOS];
    const char *carpeta = "./data/raw/gait";
    int total;

    if (argc > 1) {
        carpeta = argv[1];
    }

    total = cargar_carpeta(carpeta, registros);
    printf("Carpeta usada: %s\n", carpeta);
    printf("Archivos encontrados: %d\n", total);
    contar_metadatos(carpeta, registros, total);
    leer_campos(carpeta, registros, total);
    leer_valores(carpeta, registros, total);
    leer_senales(carpeta, registros, total);
    imprimir_metadatos(registros, total);

    int i;

    for (i = 0; i < total; i++) {
        double fs;

        fs = atof(obtener_valor_campo(&registros[i], "Sampling Frequency"));
        graficar_registro_gnuplot(&registros[i], fs);
    }
}


int cargar_carpeta(const char carpeta[], RegistroCSV registros[]) {
    int total = 0;

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
            copiar_texto(registros[total].nombre_fichero, datos.name, MAX_TEXTO);
            registros[total].total_metadatos = 0;
            total++;
        }
    } while (_findnext(busqueda, &datos) == 0);

    _findclose(busqueda);

    return total;
}
//------------------------------------------------------------------------------
void unir_ruta(char destino[], const char carpeta[], const char fichero[]) {
    size_t n = strlen(carpeta);

    if (n > 0 && (carpeta[n - 1] == '/' || carpeta[n - 1] == '\\')) {
        snprintf(destino, MAX_RUTA, "%s%s", carpeta, fichero);
    } else {
        snprintf(destino, MAX_RUTA, "%s/%s", carpeta, fichero);
    }
}
//------------------------------------------------------------------------------
void copiar_texto(char destino[], const char origen[], size_t tamano) {
    if (tamano == 0) {
        return;
    }
    strncpy(destino, origen, tamano - 1);
    destino[tamano - 1] = '\0';
}

//------------------------------------------------------------------------------
/*
    Cuenta cuantas lineas de metadatos tiene cada fichero.
    La cuenta termina cuando aparece la primera linea vacia.
*/
void contar_metadatos(const char carpeta[], RegistroCSV registros[], int total) {
    int i;

    for (i = 0; i < total; i++) {
        char ruta[MAX_RUTA];
        char linea[MAX_LINEA];
        FILE *archivo;
        int contador = 0;

        unir_ruta(ruta, carpeta, registros[i].nombre_fichero);
        archivo = fopen(ruta, "r");

        if (archivo == NULL) {  //si no se pudo abrir el fichero
            registros[i].total_metadatos = 0;
            continue;
        }

        while (fgets(linea, sizeof(linea), archivo) != NULL) {
            quitar_salto_linea(linea);

            if (linea_vacia(linea)) { // si la liena esta bvacia, se detiene la busqueda.
                break;
            }

            if (strchr(linea, ',') != NULL && contador < MAX_METADATOS) { //si la linea tiene coma y es menor que numero de metadatos (es linea de metadato)
                contador++;
            }
        }

        fclose(archivo);
        registros[i].total_metadatos = contador;
    }
}

//------------------------------------------------------------------------------
void quitar_salto_linea(char texto[]) {
    size_t n = strlen(texto);

    while (n > 0 && (texto[n - 1] == '\n' || texto[n - 1] == '\r')) {
        texto[n - 1] = '\0';
        n--;
    }
}
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------
/*
    Lee solo los nombres de los campos de metadatos.
    Ejemplo: de "Age,38" guarda solamente "Age".
*/
void leer_campos(const char carpeta[], RegistroCSV registros[], int total) {
    int i;

    for (i = 0; i < total; i++) {
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
//------------------------------------------------------------------------------
/*
    Divide una linea de metadatos en campo y valor usando la primera coma.
    Esto permite que el valor contenga mas comas.
*/
int separar_metadata(const char linea_original[], char campo[], char valor[]) {
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

    // limpiar_espacios(campo);
    // limpiar_espacios(valor);
    // quitar_comillas(valor);

    return 1;
}
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
int buscar_campo(const RegistroCSV *registro, const char campo[]) {
    int i;

    for (i = 0; i < registro->total_metadatos; i++) {
        if (strcmp(registro->campos[i], campo) == 0) {
            return i;
        }
    }

    return -1;
}

//------------------------------------------------------------------------------
/*
    Devuelve el valor asociado a un campo de metadatos.

    Ejemplo:
    obtener_valor_campo(registro, "Sampling Frequency")
    devuelve "62.5" si ese valor existe.
*/
const char *obtener_valor_campo(const RegistroCSV *registro, const char campo[]) {
    int posicion;

    if (registro == NULL) {
        return "";
    }

    posicion = buscar_campo(registro, campo);

    if (posicion == -1) {
        return "";
    }

    return registro->valores[posicion];
}

void leer_valores(const char carpeta[], RegistroCSV registros[], int total) {
    int i;

    for (i = 0; i < total; i++) {
        char ruta[MAX_RUTA];
        char linea[MAX_LINEA];
        FILE *archivo;

        unir_ruta(ruta, carpeta, registros[i].nombre_fichero);
        archivo = fopen(ruta, "r");

        if (archivo == NULL) {
            continue;
        }

        while (fgets(linea, sizeof(linea), archivo) != NULL) {
            char campo[MAX_TEXTO];
            char valor[MAX_TEXTO];
            int posicion;

            quitar_salto_linea(linea);

            if (linea_vacia(linea)) {
                break;
            }

            if (separar_metadata(linea, campo, valor)) {
                limpiar_espacios(valor);
                posicion = buscar_campo(&registros[i], campo);
                if (posicion != -1) {
                    copiar_texto(registros[i].valores[posicion], valor, MAX_TEXTO);
                }
            }
        }

        fclose(archivo);
    }
}

void imprimir_metadatos(const RegistroCSV registros[], int total) {
    int i;
    printf("\n");
    printf("============================================================\n");
    printf("RESUMEN DE METADATOS CSV\n");
    printf("============================================================\n");
    printf("Total de archivos encontrados: %d\n", total);
    printf("============================================================\n\n");

    for (i = 0; i < total; i++) {
        int j;

        printf("Archivo: %s\n", registros[i].nombre_fichero);
        printf("Total de metadatos: %d\n", registros[i].total_metadatos);
        printf("------------------------------------------------------------\n");

        for (j = 0; j < registros[i].total_metadatos; j++) {
            printf("%-30s : %s\n", registros[i].campos[j], registros[i].valores[j]);
        }

        printf("============================================================\n\n");
    }
}

int separar_columnas(char linea[], char columnas[][MAX_TEXTO], int max_columnas) {
    int total = 0;
    char *token;

    token = strtok(linea, ",");

    while (token != NULL && total < max_columnas) {
        quitar_salto_linea(token);
        limpiar_espacios(token);
        copiar_texto(columnas[total], token, MAX_TEXTO);

        total++;
        token = strtok(NULL, ",");
    }

    return total;
}

//------------------------------------------------------------------------------
/*
    Busca una columna por nombre dentro del encabezado del bloque numerico.
*/
int buscar_columna(char columnas[][MAX_TEXTO], int total_columnas, const char nombre[]) {
    int i;

    for (i = 0; i < total_columnas; i++) {
        if (strcmp(columnas[i], nombre) == 0) {
            return i;
        }
    }

    return -1;
}

//------------------------------------------------------------------------------
/*
    Lee las señales numericas del bloque posterior a la linea vacia.

    Guarda solamente:
    - angle_x
    - linear_acceleration_z
    - segmentation_output
    - sync
*/
void leer_senales(const char carpeta[], RegistroCSV registros[], int total) {
    int i;

    for (i = 0; i < total; i++) {
        char ruta[MAX_RUTA];
        char linea[MAX_LINEA];
        FILE *archivo;

        int encontro_bloque_numerico = 0;
        int total_columnas = 0;

        int col_angle_x = -1;
        int col_linear_acceleration_z = -1;
        int col_segmentation_output = -1;
        int col_sync = -1;

        registros[i].total_muestras = 0;

        unir_ruta(ruta, carpeta, registros[i].nombre_fichero);
        archivo = fopen(ruta, "r");

        if (archivo == NULL) {
            continue;
        }

        /*
            Primero saltamos el bloque de metadatos.
            El bloque numerico empieza despues de la primera linea vacia.
        */
        while (fgets(linea, sizeof(linea), archivo) != NULL) {
            quitar_salto_linea(linea);

            if (linea_vacia(linea)) {
                encontro_bloque_numerico = 1;
                break;
            }
        }

        if (!encontro_bloque_numerico) {
            fclose(archivo);
            continue;
        }

        /*
            La siguiente linea despues de la linea vacia deberia ser
            la cabecera del bloque numerico.
        */
        if (fgets(linea, sizeof(linea), archivo) == NULL) {
            fclose(archivo);
            continue;
        }

        {
            char linea_cabecera[MAX_LINEA];
            char columnas[100][MAX_TEXTO];

            copiar_texto(linea_cabecera, linea, MAX_LINEA);

            total_columnas = separar_columnas(linea_cabecera, columnas, 100);

            col_angle_x = buscar_columna(columnas, total_columnas, "angle_x");
            col_linear_acceleration_z = buscar_columna(columnas, total_columnas, "linear_acceleration_z");
            col_segmentation_output = buscar_columna(columnas, total_columnas, "segmentation_output");
            col_sync = buscar_columna(columnas, total_columnas, "sync");

            /*
                Por si el archivo usa nombres con mayusculas iniciales.
            */
            if (col_angle_x == -1) {
                col_angle_x = buscar_columna(columnas, total_columnas, "Angle_X");
            }

            if (col_linear_acceleration_z == -1) {
                col_linear_acceleration_z = buscar_columna(columnas, total_columnas, "Linear_Acceleration_Z");
            }

            if (col_segmentation_output == -1) {
                col_segmentation_output = buscar_columna(columnas, total_columnas, "Segmentation_output");
            }

            if (col_sync == -1) {
                col_sync = buscar_columna(columnas, total_columnas, "Sync");
            }
        }

        if (col_angle_x == -1 ||
            col_linear_acceleration_z == -1 ||
            col_segmentation_output == -1 ||
            col_sync == -1) {

            printf("Advertencia: no se encontraron todas las columnas de senales en %s\n",
                   registros[i].nombre_fichero);

            fclose(archivo);
            continue;
        }

        /*
            Leemos las filas numericas.
        */
        while (fgets(linea, sizeof(linea), archivo) != NULL &&
               registros[i].total_muestras < MAX_MUESTRAS) {

            char linea_datos[MAX_LINEA];
            char columnas[100][MAX_TEXTO];
            int n;
            int m;

            quitar_salto_linea(linea);

            if (linea_vacia(linea)) {
                continue;
            }

            copiar_texto(linea_datos, linea, MAX_LINEA);
            n = separar_columnas(linea_datos, columnas, 100);

            if (col_angle_x >= n ||
                col_linear_acceleration_z >= n ||
                col_segmentation_output >= n ||
                col_sync >= n) {
                continue;
            }

            m = registros[i].total_muestras;

            registros[i].angle_x[m] = atof(columnas[col_angle_x]);
            registros[i].linear_acceleration_z[m] = atof(columnas[col_linear_acceleration_z]);
            registros[i].segmentation_output[m] = atoi(columnas[col_segmentation_output]);
            registros[i].sync[m] = atoi(columnas[col_sync]);

            registros[i].total_muestras++;
        }

        fclose(archivo);

        printf("Senales leidas en %s: %d muestras\n",
               registros[i].nombre_fichero,
               registros[i].total_muestras);
    }
}

//------------------------------------------------------------------------------
/*
    Grafica las señales de un registro usando Gnuplot.

    Genera:
    - un archivo temporal .dat con los datos numericos
    - un archivo temporal .plt con las instrucciones para Gnuplot
    - una imagen .png con dos graficos:
        1) Angle_X
        2) Linear_Acceleration_Z

    Ademas sombrea los intervalos donde Sync = 1.
*/
void graficar_registro_gnuplot(const RegistroCSV *registro, double fs) {
    FILE *datos;
    FILE *script;
    char nombre_dat[MAX_RUTA];
    char nombre_plt[MAX_RUTA];
    char nombre_png[MAX_RUTA];
    int i;
    int objeto;
    int dentro_intervalo;
    double inicio_intervalo;
    double fin_intervalo;

    if (registro == NULL) {
        return;
    }

    if (registro->total_muestras <= 0) {
        printf("No hay muestras para graficar en %s\n", registro->nombre_fichero);
        return;
    }

    if (fs <= 0.0) {
        printf("Frecuencia de muestreo invalida en %s\n", registro->nombre_fichero);
        return;
    }

    /*
        Nombres de archivos temporales/de salida.
        Para simplificar, usamos nombres fijos.
    */
    snprintf(nombre_dat, MAX_RUTA, "grafico_datos.dat");
    snprintf(nombre_plt, MAX_RUTA, "grafico_script.plt");
    snprintf(nombre_png, MAX_RUTA, "%s.png", registro->nombre_fichero);

    /*
        1) Crear archivo de datos.
        Columnas:
        tiempo, angle_x, linear_acceleration_z, segmentation_output, sync
    */
    datos = fopen(nombre_dat, "w");

    if (datos == NULL) {
        printf("No se pudo crear el archivo de datos para graficar.\n");
        return;
    }

    fprintf(datos, "# tiempo angle_x linear_acceleration_z segmentation_output sync\n");

    for (i = 0; i < registro->total_muestras; i++) {
        double tiempo;

        tiempo = (double)i / fs;

        fprintf(datos, "%.8f %.10f %.10f %d %d\n",
                tiempo,
                registro->angle_x[i],
                registro->linear_acceleration_z[i],
                registro->segmentation_output[i],
                registro->sync[i]);
    }

    fclose(datos);

    /*
        2) Crear script de Gnuplot.
    */
    script = fopen(nombre_plt, "w");

    if (script == NULL) {
        printf("No se pudo crear el script de Gnuplot.\n");
        return;
    }

    fprintf(script, "set terminal pngcairo size 1200,800\n");
    fprintf(script, "set output '%s'\n", nombre_png);

    fprintf(script, "set multiplot layout 2,1 title '%s'\n", registro->nombre_fichero);

    fprintf(script, "set grid\n");
    fprintf(script, "set xlabel 'Tiempo [s]'\n");

    /*
        Creamos objetos de sombreado para los intervalos donde sync = 1.
        Estos objetos se aplican sobre ambos subplots.
    */
    objeto = 1;
    dentro_intervalo = 0;
    inicio_intervalo = 0.0;

    for (i = 0; i < registro->total_muestras; i++) {
        double tiempo;

        tiempo = (double)i / fs;

        if (registro->sync[i] == 1 && dentro_intervalo == 0) {
            dentro_intervalo = 1;
            inicio_intervalo = tiempo;
        }

        if ((registro->sync[i] == 0 || i == registro->total_muestras - 1) &&
            dentro_intervalo == 1) {

            dentro_intervalo = 0;
            fin_intervalo = tiempo;

            fprintf(script,
                    "set object %d rect from %.8f, graph 0 to %.8f, graph 1 "
                    "fc rgb '#dddddd' fs solid 0.35 noborder behind\n",
                    objeto,
                    inicio_intervalo,
                    fin_intervalo);

            objeto++;
        }
    }

    /*
        Primer subplot: angle_x.
    */
    fprintf(script, "set ylabel 'Angle_X'\n");
    fprintf(script, "plot '%s' using 1:2 with lines title 'Angle_X'\n", nombre_dat);

    /*
        Segundo subplot: linear_acceleration_z.
    */
    fprintf(script, "set ylabel 'Linear Acceleration Z'\n");
    fprintf(script, "plot '%s' using 1:3 with lines title 'Linear Acceleration Z'\n", nombre_dat);

    fprintf(script, "unset multiplot\n");
    fprintf(script, "set output\n");

    fclose(script);

    /*
        3) Ejecutar Gnuplot desde C.
    */
    system("gnuplot grafico_script.plt");

    printf("Grafico generado: %s\n", nombre_png);
}