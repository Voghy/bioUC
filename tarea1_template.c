#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <io.h> 

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

// Prototipos simplificados
int cargar_carpeta(const char carpeta[], RegistroCSV registros[]);
void contar_metadatos(const char carpeta[], RegistroCSV registros[], int total);
void leer_metadatos(const char carpeta[], RegistroCSV registros[], int total); // Fusionada y eficiente
void imprimir_metadatos(const RegistroCSV registros[], int total);

// Funciones de soporte esenciales
void unir_ruta(char destino[], const char carpeta[], const char fichero[]);
void copiar_texto(char destino[], const char origen[], size_t tamano);
void quitar_salto_linea(char texto[]);
int linea_vacia(const char texto[]);
int separar_metadata(const char linea_original[], char campo[], char valor[]);

int main(int argc, char *argv[]) {
    static RegistroCSV registros[MAX_ARCHIVOS];
    const char *carpeta = "D:/bioUC/bioUC/data/raw/gait";
    int total;

    if (argc > 1) {
        carpeta = argv[1];
    }

    total = cargar_carpeta(carpeta, registros);

    contar_metadatos(carpeta, registros, total);
    leer_metadatos(carpeta, registros, total); 
    imprimir_metadatos(registros, total);

    return 0;
}

//------------------------------------------------------------------------------
/* Recorre la carpeta en Windows y carga los nombres de ficheros .csv */
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
/* Cuenta cuántas líneas de metadatos tiene cada fichero hasta la línea vacía */
void contar_metadatos(const char carpeta[], RegistroCSV registros[], int total) {
    int i;
    for (i = 0; i < total; i++) {
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

//------------------------------------------------------------------------------
/* Lee campos y valores al mismo tiempo en una sola pasada por archivo */
void leer_metadatos(const char carpeta[], RegistroCSV registros[], int total) {
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
                copiar_texto(registros[i].valores[j], valor, MAX_TEXTO);
                j++;
            }
        }
        fclose(archivo);
    }
}

//------------------------------------------------------------------------------
/* Divide una línea de metadatos en campo y valor usando la primera coma */
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
    return 1;
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
    if (tamano == 0) return;
    strncpy(destino, origen, tamano - 1);
    destino[tamano - 1] = '\0';
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
        if (!isspace((unsigned char)texto[i])) return 0;
        i++;
    }
    return 1;
}

//------------------------------------------------------------------------------
void imprimir_metadatos(const RegistroCSV registros[], int total) {
    int i, j;
    printf("\n========================================================");
    printf("\n   RESUMEN DE METADATOS - %d ARCHIVOS ENCONTRADOS", total);
    printf("\n========================================================\n");

    if (total == 0) {
        printf("No se encontraron registros para mostrar.\n");
        return;
    }

    for (i = 0; i < total; i++) {
        printf("\nARCHIVO [%d]: %s", i + 1, registros[i].nombre_fichero);
        printf("\nTotal de metadatos: %d", registros[i].total_metadatos);
        printf("\n--------------------------------------------------------\n");

        for (j = 0; j < registros[i].total_metadatos; j++) {
            printf("  %-25s : %s\n", registros[i].campos[j], registros[i].valores[j]);
        }
        printf("--------------------------------------------------------\n");
    }
    printf("\nFin del reporte.\n");
}