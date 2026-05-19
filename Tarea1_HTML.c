#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

    Para esta tarea implementé los gráficos por medio de Dashboards con HTML, otra forma de implementar los gráficos en C.
    Integración a futuro: Buscando explorar más sobre el tema planeo trabajar más con el mismo y ampliar más el uso anhadiendo 
    las otras dos carpetas.

    Para que funcione cambiar las direcciones del archivo (no pude automatizar bien esa parte) :
    const char *carpeta = "D:/bioUC/bioUC/data/raw/gait";
    FILE *archivo = fopen("D:\\bioUC\\bioUC\\reporte_metadatos.html", "w");
    system("start \"\" \"D:\\bioUC\\bioUC\\reporte_metadatos.html\"");

*/

#define MAX_ARCHIVOS 100
#define MAX_METADATOS 40
#define MAX_TEXTO 256
#define MAX_LINEA 1024
#define MAX_RUTA 512
#define MAX_MUESTRAS 5000

// "macro" estructura
typedef struct {
    char nombre_fichero[MAX_TEXTO];
    int total_metadatos;
    char campos[MAX_METADATOS][MAX_TEXTO];
    char valores[MAX_METADATOS][MAX_TEXTO];
    
    int total_muestras;
    float angle_x[MAX_MUESTRAS];
    float accel_z[MAX_MUESTRAS];
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

// Implementar HTML para gráficos
void generar_reporte_html(const RegistroCSV registros[], int total);
void leer_senales(const char carpeta[], RegistroCSV registros[], int total);

// Funciones de soporte
void unir_ruta(char destino[], const char carpeta[], const char fichero[]);
void copiar_texto(char destino[], const char origen[], size_t tamano);
void quitar_salto_linea(char texto[]);
int linea_vacia(const char texto[]);
int separar_metadata(const char linea_original[], char campo[], char valor[]);
int buscar_campo(const RegistroCSV *registro, const char campo[]);

int main(int argc, char *argv[]) {
    static RegistroCSV registros[MAX_ARCHIVOS];
    const char *carpeta = "D:/bioUC/bioUC/data/raw/gait";
    int total;

    if (argc > 1) {
        carpeta = argv[1];
    }

    total = cargar_carpeta(carpeta, registros);

    contar_metadatos(carpeta, registros, total);
    leer_campos(carpeta, registros, total);
    leer_valores(carpeta, registros, total);
    
    leer_senales(carpeta, registros, total); // Leer para el reporte html

    imprimir_metadatos(registros, total);   // Imprime en la terminal

    //
    generar_reporte_html(registros, total);
    return 0;
}

//------------------------------------------------------------------------------
/*
    Recorre la carpeta y carga solamente el nombre de cada fichero CSV
    dentro del arreglo registros.
*/
int cargar_carpeta(const char carpeta[], RegistroCSV registros[]) {
    int total = 0;

#ifdef _WIN32
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

#else
    // Versión para Linux/Unix usando POSIX
    DIR *directorio;
    struct dirent *entrada;

    directorio = opendir(carpeta);
    if (directorio == NULL) {
        return 0;
    }

    while ((entrada = readdir(directorio)) != NULL && total < MAX_ARCHIVOS) {
        // Verificar si el archivo termina con .csv
        size_t len = strlen(entrada->d_name);
        if (len >= 4 && strcmp(entrada->d_name + len - 4, ".csv") == 0) {
            copiar_texto(registros[total].nombre_fichero, entrada->d_name, MAX_TEXTO);
            registros[total].total_metadatos = 0;
            total++;
        }
    }

    closedir(directorio);
#endif

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
    Lee los valores de cada metadato y los almacena en la estructura.
    Utiliza la función buscar_campo para encontrar el índice correspondiente.
*/
void leer_valores(const char carpeta[], RegistroCSV registros[], int total) {
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

        while (fgets(linea, sizeof(linea), archivo) != NULL) {
            char campo[MAX_TEXTO];
            char valor[MAX_TEXTO];
            int indice;

            quitar_salto_linea(linea);

            if (linea_vacia(linea)) {
                break;
            }

            if (separar_metadata(linea, campo, valor)) {
                indice = buscar_campo(&registros[i], campo);
                if (indice != -1) {
                    copiar_texto(registros[i].valores[indice], valor, MAX_TEXTO);
                }
                j++;
            }
        }

        fclose(archivo);
    }
}
//------------------------------------------------------------------------------
/*
    Imprime todos los metadatos almacenados en la estructura.
*/
//------------------------------------------------------------------------------
/*
    Imprime todos los metadatos almacenados en la estructura de forma organizada.
    Muestra el nombre del archivo, el total de campos encontrados y cada par campo:valor.
*/
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
            // Se puede usar limpiar_espacios aquí si se desea un resultado más pulido
            printf("  %-25s : %s\n", registros[i].campos[j], registros[i].valores[j]);
        }
        printf("--------------------------------------------------------\n");
    }

    printf("\nFin del reporte.\n");
}

//------------------------------------------------------------------------------
/*
    Se salta los metadatos y lee las columnas de señales numéricas (Angle_X, Accel_Z, Sync)
    para cada uno de los archivos CSV.
*/
void leer_senales(const char carpeta[], RegistroCSV registros[], int total) {
    int i;

    for (i = 0; i < total; i++) {
        char ruta[MAX_RUTA];
        char linea[MAX_LINEA];
        FILE *archivo;

        unir_ruta(ruta, carpeta, registros[i].nombre_fichero);
        archivo = fopen(ruta, "r");

        if (archivo == NULL) {
            registros[i].total_muestras = 0;
            continue;
        }

        // ETAPA 1: Saltarse los metadatos hasta encontrar la línea vacía
        while (fgets(linea, sizeof(linea), archivo) != NULL) {
            quitar_salto_linea(linea);
            if (linea_vacia(linea)) {
                break; // Llegamos al espacio en blanco que separa los bloques
            }
        }

        // ETAPA 2: Leer la línea de encabezados de las columnas
        if (fgets(linea, sizeof(linea), archivo) == NULL) {
            fclose(archivo);
            registros[i].total_muestras = 0;
            continue;
        }
        quitar_salto_linea(linea);

        // Identificar los índices de las columnas dinámicamente usando un escaneo manual
        int idx_angle_x = -1, idx_accel_z = -1, idx_sync = -1;
        int col = 0;
        char *ptr = linea;

        while (*ptr != '\0') {
            char *siguiente_coma = strchr(ptr, ',');
            char nombre_col[MAX_TEXTO] = {0};

            if (siguiente_coma != NULL) {
                size_t len = siguiente_coma - ptr;
                if (len >= MAX_TEXTO) len = MAX_TEXTO - 1;
                strncpy(nombre_col, ptr, len);
                ptr = siguiente_coma + 1;
            } else {
                strncpy(nombre_col, ptr, MAX_TEXTO - 1);
                ptr = ptr + strlen(ptr);
            }
            limpiar_espacios(nombre_col);

            if (strcmp(nombre_col, "Angle_X") == 0) idx_angle_x = col;
            else if (strcmp(nombre_col, "Linear_Acceleration_Z") == 0) idx_accel_z = col;
            else if (strcmp(nombre_col, "Sync") == 0) idx_sync = col;

            col++;
        }

        // ETAPA 3: Leer las filas de datos numéricos
        int muestras = 0;
        while (fgets(linea, sizeof(linea), archivo) != NULL && muestras < MAX_MUESTRAS) {
            quitar_salto_linea(linea);
            if (linea_vacia(linea)) continue;

            col = 0;
            ptr = linea;

            while (*ptr != '\0') {
                char *siguiente_coma = strchr(ptr, ',');
                char val_texto[64] = {0};

                if (siguiente_coma != NULL) {
                    size_t len = siguiente_coma - ptr;
                    if (len >= 64) len = 63;
                    strncpy(val_texto, ptr, len);
                    ptr = siguiente_coma + 1;
                } else {
                    strncpy(val_texto, ptr, 63);
                    ptr = ptr + strlen(ptr);
                }

                // Guardar el número convertido en la posición de la muestra actual
                if (col == idx_angle_x) {
                    registros[i].angle_x[muestras] = atof(val_texto);
                } else if (col == idx_accel_z) {
                    registros[i].accel_z[muestras] = atof(val_texto);
                } else if (col == idx_sync) {
                    registros[i].sync[muestras] = atoi(val_texto);
                }

                col++;
            }
            muestras++;
        }

        fclose(archivo);
        registros[i].total_muestras = muestras; // Guardamos cuántas muestras reales se leyeron
    }
}

//------------------------------------------------------------------------------
/*
    Genera el reporte HTML final con tablas de metadatos y gráficos interactivos
    para cada sujeto utilizando la librería Chart.js.
*/
void generar_reporte_html(const RegistroCSV registros[], int total) {
    int i, j;
    FILE *archivo = fopen("D:\\bioUC\\bioUC\\reporte_metadatos.html", "w");
    
    if (archivo == NULL) {
        printf("\n[ERROR] No se pudo crear el archivo HTML en la ruta especificada.\n");
        return;
    }

    // Escribir el encabezado del HTML, estilos CSS avanzados y cargar Chart.js desde la web
    fprintf(archivo, "<!DOCTYPE html>\n<html lang='es'>\n<head>\n");
    fprintf(archivo, "    <meta charset='UTF-8'>\n");
    fprintf(archivo, "    <title>Dashboard de Procesamiento de Datos Biomecánicos</title>\n");
    fprintf(archivo, "    <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>\n"); // Librería gráfica
    fprintf(archivo, "    <style>\n");
    fprintf(archivo, "        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 30px; background-color: #f4f6f9; color: #333; }\n");
    fprintf(archivo, "        h1 { text-align: center; color: #003366; margin-bottom: 30px; }\n");
    fprintf(archivo, "        .contenedor-sujeto { background-color: #ffffff; padding: 25px; margin-bottom: 40px; border-radius: 12px; box-shadow: 0 4px 8px rgba(0,0,0,0.05); }\n");
    fprintf(archivo, "        h2 { color: #004080; border-bottom: 2px solid #ddd; padding-bottom: 8px; margin-top: 0; }\n");
    fprintf(archivo, "        .grid-contenido { display: flex; flex-direction: column; gap: 20px; }\n");
    fprintf(archivo, "        table { width: 100%%; border-collapse: collapse; background-color: #fafafa; border-radius: 6px; overflow: hidden; }\n");
    fprintf(archivo, "        th, td { padding: 10px 15px; text-align: left; border-bottom: 1px solid #eee; }\n");
    fprintf(archivo, "        th { background-color: #0059b3; color: white; font-weight: 600; }\n");
    fprintf(archivo, "        tr:hover { background-color: #f1f1f1; }\n");
    fprintf(archivo, "        .grafico-box { width: 100%%; height: 400px; margin-top: 15px; background: #fff; padding: 10px; border: 1px solid #e0e0e0; border-radius: 8px; box-sizing: border-box; }\n");
    fprintf(archivo, "    </style>\n</head>\n<body>\n");

    fprintf(archivo, "    <h1>Dashboard de Marcha Humana (10MWT)</h1>\n");

    // Recorrer cada sujeto/archivo cargado
    for (i = 0; i < total; i++) {
        fprintf(archivo, "    <div class='contenedor-sujeto'>\n");
        fprintf(archivo, "        <h2>Fichero: %s</h2>\n", registros[i].nombre_fichero);
        
        fprintf(archivo, "        <div class='grid-contenido'>\n");
        
        // 1. Tabla de Metadatos
        fprintf(archivo, "            <table>\n");
        fprintf(archivo, "                <thead>\n<tr><th>Metadato / Campo</th><th>Valor Registrado</th></tr>\n</thead>\n");
        fprintf(archivo, "                <tbody>\n");
        for (j = 0; j < registros[i].total_metadatos; j++) {
            fprintf(archivo, "                    <tr><td><strong>%s</strong></td><td>%s</td></tr>\n", 
                    registros[i].campos[j], registros[i].valores[j]);
        }
        fprintf(archivo, "                </tbody>\n");
        fprintf(archivo, "            </table>\n");

        // 2. Área del Gráfico (Canvas)
        fprintf(archivo, "            <div class='grafico-box'>\n");
        fprintf(archivo, "                <canvas id='grafico_%d'></canvas>\n", i);
        fprintf(archivo, "            </div>\n");
        
        fprintf(archivo, "        </div>\n"); // Cierre grid-contenido
        fprintf(archivo, "    </div>\n\n");  // Cierre contenedor-sujeto

        // 3. Inyección de Código JavaScript para inicializar el gráfico de este sujeto
        fprintf(archivo, "    <script>\n");
        fprintf(archivo, "        (function() {\n");
        
        // Pasar las muestras del arreglo de C a un formato de texto que entienda JS [ej: 1.2, 3.4, 5.1]
        fprintf(archivo, "            const etiquetas = [");
        for (j = 0; j < registros[i].total_muestras; j++) {
            fprintf(archivo, "%d%s", j, (j == registros[i].total_muestras - 1) ? "" : ",");
        }
        fprintf(archivo, "];\n");

        fprintf(archivo, "            const datos_angle = [");
        for (j = 0; j < registros[i].total_muestras; j++) {
            fprintf(archivo, "%.4f%s", registros[i].angle_x[j], (j == registros[i].total_muestras - 1) ? "" : ",");
        }
        fprintf(archivo, "];\n");

        fprintf(archivo, "            const datos_accel = [");
        for (j = 0; j < registros[i].total_muestras; j++) {
            fprintf(archivo, "%.4f%s", registros[i].accel_z[j], (j == registros[i].total_muestras - 1) ? "" : ",");
        }
        fprintf(archivo, "];\n");

        fprintf(archivo, "            const datos_sync = [");
        for (j = 0; j < registros[i].total_muestras; j++) {
            fprintf(archivo, "%d%s", registros[i].sync[j], (j == registros[i].total_muestras - 1) ? "" : ",");
        }
        fprintf(archivo, "];\n");

        // Configuración de Chart.js
        fprintf(archivo, "            const ctx = document.getElementById('grafico_%d').getContext('2d');\n", i);
        fprintf(archivo, "            new Chart(ctx, {\n");
        fprintf(archivo, "                type: 'line',\n");
        fprintf(archivo, "                data: {\n");
        fprintf(archivo, "                    labels: etiquetas,\n");
        fprintf(archivo, "                    datasets: [\n");
        fprintf(archivo, "                        { label: 'Angle_X (deg)', data: datos_angle, borderColor: 'rgb(255, 99, 132)', borderWidth: 2, pointRadius: 0, yAxisID: 'y' },\n");
        fprintf(archivo, "                        { label: 'Linear_Acceleration_Z (m/s²)', data: datos_accel, borderColor: 'rgb(54, 162, 235)', borderWidth: 2, pointRadius: 0, yAxisID: 'y1' }\n");
        fprintf(archivo, "                    ]\n");
        fprintf(archivo, "                },\n");
        fprintf(archivo, "                options: {\n");
        fprintf(archivo, "                    responsive: true,\n");
        fprintf(archivo, "                    maintainAspectRatio: false,\n");
        fprintf(archivo, "                    scales: {\n");
        fprintf(archivo, "                        x: { title: { display: true, text: 'Muestras (Tiempo)' } },\n");
        fprintf(archivo, "                        y: { type: 'linear', display: true, position: 'left', title: { display: true, text: 'Ángulo X' } },\n");
        fprintf(archivo, "                        y1: { type: 'linear', display: true, position: 'right', grid: { drawOnChartArea: false }, title: { display: true, text: 'Aceleración Z' } }\n");
        fprintf(archivo, "                    },\n");
        
        // Plugin personalizado para "Sombreado del fondo" donde Sync == 1
        fprintf(archivo, "                    plugins: {\n");
        fprintf(archivo, "                        autocolor: false,\n");
        fprintf(archivo, "                    }\n");
        fprintf(archivo, "                },\n");
        
        // Lógica de dibujo del fondo para la señal Sync
        fprintf(archivo, "                plugins: [{\n");
        fprintf(archivo, "                    id: 'sombreadoSync',\n");
        fprintf(archivo, "                    beforeDraw: (chart) => {\n");
        fprintf(archivo, "                        const { ctx, chartArea: { top, bottom, left, right }, scales: { x } } = chart;\n");
        fprintf(archivo, "                        ctx.save();\n");
        fprintf(archivo, "                        ctx.fillStyle = 'rgba(255, 255, 0, 0.2)';\n"); // Color amarillo transparente para el Sync
        fprintf(archivo, "                        for (let k = 0; k < datos_sync.length - 1; k++) {\n");
        fprintf(archivo, "                            if (datos_sync[k] === 1) {\n");
        fprintf(archivo, "                                const xIni = x.getPixelForValue(k);\n");
        fprintf(archivo, "                                const xFin = x.getPixelForValue(k + 1);\n");
        fprintf(archivo, "                                ctx.fillRect(xIni, top, xFin - xIni, bottom - top);\n");
        fprintf(archivo, "                            }\n");
        fprintf(archivo, "                        }\n");
        fprintf(archivo, "                        ctx.restore();\n");
        fprintf(archivo, "                    }\n");
        fprintf(archivo, "                }]\n");
        
        fprintf(archivo, "            });\n");
        fprintf(archivo, "        })();\n");
        fprintf(archivo, "    </script>\n");
    }

    fprintf(archivo, "</body>\n</html>\n");
    fclose(archivo);

    // Intentar abrir automáticamente en Windows
    system("start \"\" \"D:\\bioUC\\bioUC\\reporte_metadatos.html\"");

    printf("\n========================================================\n");
    printf("[EXITO] Reporte HTML completo con GRAFICOS generado.\n");
    printf("========================================================\n");
}
