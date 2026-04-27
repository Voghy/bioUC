#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
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

// "macro" estructura
typedef struct {
    char nombre_fichero[MAX_TEXTO];
    int total_metadatos;
    char campos[MAX_METADATOS][MAX_TEXTO];
    char valores[MAX_METADATOS][MAX_TEXTO];
} RegistroCSV;

//funcion que busca todos los ficheros disponibles y carga sus nombres en la estructura de datos
int cargar_carpeta(const char carpeta[], RegistroCSV registros[]);
//funcion que cuenta cuantas lineas de metadatos tiene el fichero
void contar_metadatos(const char carpeta[], RegistroCSV registros[], int total);
//funcion que lee los datos
void leer_campos(const char carpeta[], RegistroCSV registros[], int total);
//funciones  a implementar
//void leer_valores(const char carpeta[], RegistroCSV registros[], int total);
//void imprimir_metadatos(const RegistroCSV registros[], int total);

// Funciones de soporte
void unir_ruta(char destino[], const char carpeta[], const char fichero[]);
void copiar_texto(char destino[], const char origen[], size_t tamano);
void quitar_salto_linea(char texto[]);
int linea_vacia(const char texto[]);
int separar_metadata(const char linea_original[], char campo[], char valor[]);
int buscar_campo(const RegistroCSV *registro, const char campo[]);

int main(int argc, char *argv[]) {
    static RegistroCSV registros[MAX_ARCHIVOS];
    const char *carpeta = "./data/raw/gait";
    int total;

    if (argc > 1) {
        carpeta = argv[1];
    }

    total = cargar_carpeta(carpeta, registros);

    contar_metadatos(carpeta, registros, total);
    leer_campos(carpeta, registros, total);
    //leer_valores(carpeta, registros, total);
    //imprimir_metadatos(registros, total);

    return 0;
}

//------------------------------------------------------------------------------
/*
    Recorre la carpeta y carga solamente el nombre de cada fichero CSV
    dentro del arreglo registros.
*/
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
