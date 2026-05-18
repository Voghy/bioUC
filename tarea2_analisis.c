#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

#define MAX_LINEA 1024
#define MAX_MUESTRAS 20000
#define MAX_ARCHIVOS 100
#define DISTANCIA_UTIL_10MWT_M 6.0

typedef struct {
    char archivo[256];
    char sujeto[10];
    int muestras;
    double fs;
    int pasos;
    double vel_marcha;
    double vel_pasos;
    double long_zancada;
    double tiempo_total;
    int muestras_sync;
} ResultadoMarcha;

typedef struct {
    double angle_x;
    double linear_acceleration_z;
    int segmentation_output;
    int sync;
} Datos;

void quitar_salto_linea(char texto[]) {
    size_t len = strlen(texto);
    if (len > 0 && texto[len - 1] == '\n') {
        texto[len - 1] = '\0';
    }
    if (len > 1 && texto[len - 2] == '\r') {
        texto[len - 2] = '\0';
    }
}

int linea_vacia(const char texto[]) {
    for (size_t i = 0; texto[i]; i++) {
        if (!isspace((unsigned char)texto[i])) {
            return 0;
        }
    }
    return 1;
}

int cargar_metadata(const char archivo[], double *frecuencia_muestreo, char *sujeto) {
    FILE *fp = fopen(archivo, "r");
    if (!fp) return 0;

    char linea[MAX_LINEA];
    while (fgets(linea, sizeof(linea), fp)) {
        quitar_salto_linea(linea);

        if (linea_vacia(linea)) break;

        if (strstr(linea, "Sampling Frequency") != NULL) {
            char *coma = strchr(linea, ',');
            if (coma != NULL) {
                *frecuencia_muestreo = atof(coma + 1);
            }
        }

        if (strstr(linea, "Subject,") != NULL) {
            char *coma = strchr(linea, ',');
            if (coma != NULL) {
                strcpy(sujeto, coma + 1);
            }
        }
    }

    fclose(fp);
    return *frecuencia_muestreo > 0.0;
}

int cargar_datos(const char archivo[], Datos registros[], int max_registros, int *total) {
    FILE *fp = fopen(archivo, "r");
    if (!fp) return 0;

    char linea[MAX_LINEA];
    int metadata_done = 0, en_datos = 0;
    *total = 0;

    while (fgets(linea, sizeof(linea), fp)) {
        quitar_salto_linea(linea);

        if (linea_vacia(linea)) {
            metadata_done = 1;
            continue;
        }

        if (!metadata_done) continue;

        if (!en_datos) {
            if (strstr(linea, "Angle_X") != NULL) {
                en_datos = 1;
            }
            continue;
        }

        if (*total >= max_registros) break;

        Datos registro;
        if (sscanf(linea, "%lf,%*lf,%*lf,%*lf,%*lf,%*lf,%*lf,%*lf,%lf,%*lf,%*lf,%d,%d",
                   &registro.angle_x, &registro.linear_acceleration_z,
                   &registro.segmentation_output, &registro.sync) == 4) {
            registros[*total] = registro;
            (*total)++;
        }
    }

    fclose(fp);
    return *total > 0;
}

int buscar_indice_primera_sync(const Datos registros[], int total) {
    for (int i = 0; i < total; i++) {
        if (registros[i].sync != 0) return i;
    }
    return -1;
}

int buscar_indice_ultima_sync(const Datos registros[], int total) {
    for (int i = total - 1; i >= 0; i--) {
        if (registros[i].sync != 0) return i;
    }
    return -1;
}

int contar_transiciones_S3_S0(const Datos registros[], int inicio, int fin) {
    if (inicio < 0 || fin < 0 || inicio >= fin) return 0;

    int pasos = 0;
    for (int i = inicio; i < fin; i++) {
        if (registros[i].segmentation_output == 3 &&
            registros[i + 1].segmentation_output == 0) {
            pasos++;
        }
    }
    return pasos;
}

double calcular_velocidad_marcha(int muestras_sync, double fs) {
    if (fs <= 0.0 || muestras_sync <= 0) return 0.0;
    double tiempo_sync = (double)muestras_sync / fs;
    return DISTANCIA_UTIL_10MWT_M / tiempo_sync;
}

double calcular_velocidad_pasos(int pasos, int muestras_pasos, double fs) {
    if (fs <= 0.0 || muestras_pasos <= 0 || pasos <= 0) return 0.0;
    double tiempo_pasos = (double)muestras_pasos / fs;
    return (double)pasos / tiempo_pasos;
}

double calcular_longitud_zancada(double velocidad_marcha, double velocidad_pasos) {
    if (velocidad_pasos <= 0.0) return 0.0;
    return velocidad_marcha / velocidad_pasos;
}

int procesar_archivo(const char ruta[], ResultadoMarcha *resultado) {
    double fs = 0.0;
    char sujeto[10] = "";

    if (!cargar_metadata(ruta, &fs, sujeto)) return 0;

    strcpy(resultado->sujeto, sujeto);
    resultado->fs = fs;
    resultado->muestras = 0;

    Datos registros[MAX_MUESTRAS];
    int total = 0;

    if (!cargar_datos(ruta, registros, MAX_MUESTRAS, &total)) return 0;

    resultado->muestras = total;
    resultado->tiempo_total = total / fs;

    int inicio = buscar_indice_primera_sync(registros, total);
    int fin = buscar_indice_ultima_sync(registros, total);

    if (inicio >= 0 && fin > inicio) {
        resultado->muestras_sync = fin - inicio;
        resultado->pasos = contar_transiciones_S3_S0(registros, inicio, fin);
        resultado->vel_marcha = calcular_velocidad_marcha(resultado->muestras_sync, fs);
        resultado->vel_pasos = calcular_velocidad_pasos(resultado->pasos, resultado->muestras_sync, fs);
        resultado->long_zancada = calcular_longitud_zancada(resultado->vel_marcha, resultado->vel_pasos);
    } else {
        resultado->muestras_sync = 0;
        resultado->pasos = 0;
        resultado->vel_marcha = 0.0;
        resultado->vel_pasos = 0.0;
        resultado->long_zancada = 0.0;
    }

    strcpy(resultado->archivo, ruta);
    return 1;
}

void generar_html(ResultadoMarcha resultados[], int total) {
    FILE *html = fopen("tarea2_analisis.html", "w");
    if (!html) return;

    fprintf(html, "<!DOCTYPE html>\n<html>\n<head>\n");
    fprintf(html, "<meta charset='UTF-8'>\n");
    fprintf(html, "<title>Tarea 2 - Análisis de Marcha</title>\n");
    fprintf(html, "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>\n");
    fprintf(html, "<style>\n");
    fprintf(html, "body {font-family: Arial; background: #f5f5f5; padding: 20px;}\n");
    fprintf(html, ".header {background: #667eea; color: white; padding: 20px; border-radius: 5px; margin-bottom: 20px;}\n");
    fprintf(html, "table {width: 100%%; border-collapse: collapse; background: white; margin-bottom: 30px;}\n");
    fprintf(html, "th {background: #667eea; color: white; padding: 12px; text-align: left;}\n");
    fprintf(html, "td {padding: 10px; border-bottom: 1px solid #ddd;}\n");
    fprintf(html, "tr:hover {background: #f5f5f5;}\n");
    fprintf(html, ".grafico {background: white; padding: 20px; border-radius: 5px; margin-bottom: 30px; box-shadow: 0 2px 5px rgba(0,0,0,0.1);}\n");
    fprintf(html, ".stats {display: grid; grid-template-columns: repeat(4, 1fr); gap: 20px; margin-bottom: 30px;}\n");
    fprintf(html, ".stat-box {background: white; padding: 15px; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1);}\n");
    fprintf(html, ".stat-box h3 {color: #667eea; margin: 0 0 10px 0;}\n");
    fprintf(html, ".stat-value {font-size: 24px; font-weight: bold; color: #333;}\n");
    fprintf(html, "</style>\n</head>\n<body>\n");

    fprintf(html, "<div class='header'>\n");
    fprintf(html, "<h1>📊 Tarea 2 - Análisis de Marcha (Protocolo 10MWT)</h1>\n");
    fprintf(html, "<p>Análisis comparativo de %d sujetos</p>\n", total);
    fprintf(html, "</div>\n");

    // Calcular estadísticas
    double prom_vel = 0, prom_pasos = 0, prom_long = 0, prom_muestras = 0;
    double min_vel = resultados[0].vel_marcha, max_vel = resultados[0].vel_marcha;

    for (int i = 0; i < total; i++) {
        prom_vel += resultados[i].vel_marcha;
        prom_pasos += resultados[i].vel_pasos;
        prom_long += resultados[i].long_zancada;
        prom_muestras += resultados[i].muestras_sync;

        if (resultados[i].vel_marcha < min_vel) min_vel = resultados[i].vel_marcha;
        if (resultados[i].vel_marcha > max_vel) max_vel = resultados[i].vel_marcha;
    }

    prom_vel /= total;
    prom_pasos /= total;
    prom_long /= total;
    prom_muestras /= total;

    fprintf(html, "<div class='stats'>\n");
    fprintf(html, "<div class='stat-box'><h3>Vel. Marcha Prom.</h3><div class='stat-value'>%.3f m/s</div></div>\n", prom_vel);
    fprintf(html, "<div class='stat-box'><h3>Vel. Pasos Prom.</h3><div class='stat-value'>%.3f p/s</div></div>\n", prom_pasos);
    fprintf(html, "<div class='stat-box'><h3>Longitud Zancada</h3><div class='stat-value'>%.3f m</div></div>\n", prom_long);
    fprintf(html, "<div class='stat-box'><h3>Rango Vel.</h3><div class='stat-value'>%.3f-%.3f</div></div>\n", min_vel, max_vel);
    fprintf(html, "</div>\n");

    // Tabla de datos
    fprintf(html, "<h2>Resultados Detallados</h2>\n");
    fprintf(html, "<table>\n");
    fprintf(html, "<thead><tr><th>Sujeto</th><th>Pasos</th><th>Vel. Marcha</th><th>Vel. Pasos</th><th>Long. Zancada</th><th>Tiempo Total</th></tr></thead>\n");
    fprintf(html, "<tbody>\n");

    for (int i = 0; i < total; i++) {
        fprintf(html, "<tr><td>%s</td><td>%d</td><td>%.3f m/s</td><td>%.3f p/s</td><td>%.3f m</td><td>%.2f s</td></tr>\n",
                resultados[i].sujeto, resultados[i].pasos,
                resultados[i].vel_marcha, resultados[i].vel_pasos,
                resultados[i].long_zancada, resultados[i].tiempo_total);
    }

    fprintf(html, "</tbody>\n</table>\n");

    // Gráficos
    fprintf(html, "<div class='grafico'>\n<h2>Velocidad de Marcha por Sujeto</h2>\n");
    fprintf(html, "<canvas id='chart1' style='max-height: 400px;'></canvas>\n</div>\n");

    fprintf(html, "<div class='grafico'>\n<h2>Longitud de Zancada por Sujeto</h2>\n");
    fprintf(html, "<canvas id='chart2' style='max-height: 400px;'></canvas>\n</div>\n");

    // JavaScript para gráficos
    fprintf(html, "<script>\n");
    fprintf(html, "const sujetos = [");
    for (int i = 0; i < total; i++) {
        fprintf(html, "'%s'%s", resultados[i].sujeto, (i < total - 1) ? "," : "");
    }
    fprintf(html, "];\n");

    fprintf(html, "const velocidades = [");
    for (int i = 0; i < total; i++) {
        fprintf(html, "%.3f%s", resultados[i].vel_marcha, (i < total - 1) ? "," : "");
    }
    fprintf(html, "];\n");

    fprintf(html, "const longitudes = [");
    for (int i = 0; i < total; i++) {
        fprintf(html, "%.3f%s", resultados[i].long_zancada, (i < total - 1) ? "," : "");
    }
    fprintf(html, "];\n");

    fprintf(html, "new Chart(document.getElementById('chart1'), {\n");
    fprintf(html, "  type: 'bar',\n");
    fprintf(html, "  data: {\n");
    fprintf(html, "    labels: sujetos,\n");
    fprintf(html, "    datasets: [{label: 'Velocidad (m/s)', data: velocidades, backgroundColor: 'rgba(102, 126, 234, 0.7)'}]\n");
    fprintf(html, "  },\n");
    fprintf(html, "  options: {responsive: true, maintainAspectRatio: true, scales: {y: {beginAtZero: true}}}\n");
    fprintf(html, "});\n");

    fprintf(html, "new Chart(document.getElementById('chart2'), {\n");
    fprintf(html, "  type: 'bar',\n");
    fprintf(html, "  data: {\n");
    fprintf(html, "    labels: sujetos,\n");
    fprintf(html, "    datasets: [{label: 'Longitud (m)', data: longitudes, backgroundColor: 'rgba(118, 75, 162, 0.7)'}]\n");
    fprintf(html, "  },\n");
    fprintf(html, "  options: {responsive: true, maintainAspectRatio: true, scales: {y: {beginAtZero: true}}}\n");
    fprintf(html, "});\n");

    fprintf(html, "</script>\n</body>\n</html>\n");

    fclose(html);
}

int main() {
    DIR *dir = opendir("data/raw/gait");
    if (!dir) return 1;

    ResultadoMarcha resultados[MAX_ARCHIVOS];
    int count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) && count < MAX_ARCHIVOS) {
        if (strstr(entry->d_name, ".csv") && strstr(entry->d_name, "gait")) {
            char ruta[512];
            snprintf(ruta, sizeof(ruta), "data/raw/gait/%s", entry->d_name);

            printf("Procesando: %s... ", entry->d_name);
            if (procesar_archivo(ruta, &resultados[count])) {
                printf("✓ (vel: %.3f m/s, pasos: %d)\n",
                       resultados[count].vel_marcha, resultados[count].pasos);
                count++;
            } else {
                printf("✗\n");
            }
        }
    }

    closedir(dir);

    printf("\nGenerando reporte HTML...\n");
    generar_html(resultados, count);
    printf("✓ Reporte generado: tarea2_analisis.html\n");

    return 0;
}
