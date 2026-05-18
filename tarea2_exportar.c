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
    int ensayo;
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

    // Extraer número de ensayo del nombre
    if (strstr(ruta, "_01.csv")) resultado->ensayo = 1;
    else if (strstr(ruta, "_02.csv")) resultado->ensayo = 2;
    else if (strstr(ruta, "_03.csv")) resultado->ensayo = 3;
    else resultado->ensayo = 0;

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

void exportar_csv(ResultadoMarcha resultados[], int total) {
    FILE *csv = fopen("tarea2_resultados.csv", "w");
    if (!csv) {
        printf("Error: No se pudo crear tarea2_resultados.csv\n");
        return;
    }

    // Cabecera
    fprintf(csv, "Archivo,Sujeto,Ensayo,Muestras,Fs (Hz),Muestras_SYNC,");
    fprintf(csv, "Pasos_Detectados,Vel_Marcha (m/s),Vel_Pasos (p/s),");
    fprintf(csv, "Long_Zancada (m),Tiempo_Total (s)\n");

    // Datos
    for (int i = 0; i < total; i++) {
        fprintf(csv, "%s,%s,%d,%d,%.3f,%d,%d,%.4f,%.4f,%.4f,%.3f\n",
                resultados[i].archivo,
                resultados[i].sujeto,
                resultados[i].ensayo,
                resultados[i].muestras,
                resultados[i].fs,
                resultados[i].muestras_sync,
                resultados[i].pasos,
                resultados[i].vel_marcha,
                resultados[i].vel_pasos,
                resultados[i].long_zancada,
                resultados[i].tiempo_total);
    }

    fclose(csv);
    printf("✓ CSV exportado: tarea2_resultados.csv\n");
}

void mostrar_estadisticas(ResultadoMarcha resultados[], int total) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║                    ESTADÍSTICAS GENERALES                 ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");

    if (total == 0) {
        printf("No hay datos para mostrar.\n");
        return;
    }

    double sum_vel = 0, sum_pasos = 0, sum_long = 0;
    double min_vel = resultados[0].vel_marcha, max_vel = resultados[0].vel_marcha;
    double min_long = resultados[0].long_zancada, max_long = resultados[0].long_zancada;
    int min_pasos = resultados[0].pasos, max_pasos = resultados[0].pasos;

    for (int i = 0; i < total; i++) {
        sum_vel += resultados[i].vel_marcha;
        sum_pasos += resultados[i].pasos;
        sum_long += resultados[i].long_zancada;

        if (resultados[i].vel_marcha < min_vel) min_vel = resultados[i].vel_marcha;
        if (resultados[i].vel_marcha > max_vel) max_vel = resultados[i].vel_marcha;
        if (resultados[i].long_zancada < min_long) min_long = resultados[i].long_zancada;
        if (resultados[i].long_zancada > max_long) max_long = resultados[i].long_zancada;
        if (resultados[i].pasos < min_pasos) min_pasos = resultados[i].pasos;
        if (resultados[i].pasos > max_pasos) max_pasos = resultados[i].pasos;
    }

    printf("Total de registros: %d\n\n", total);

    printf("VELOCIDAD DE MARCHA:\n");
    printf("  Promedio:  %.4f m/s\n", sum_vel / total);
    printf("  Mínimo:    %.4f m/s\n", min_vel);
    printf("  Máximo:    %.4f m/s\n", max_vel);
    printf("  Rango:     %.4f m/s\n\n", max_vel - min_vel);

    printf("PASOS DETECTADOS (S3→S0):\n");
    printf("  Promedio:  %.2f pasos\n", (double)sum_pasos / total);
    printf("  Mínimo:    %d pasos\n", min_pasos);
    printf("  Máximo:    %d pasos\n\n", max_pasos);

    printf("LONGITUD DE ZANCADA:\n");
    printf("  Promedio:  %.4f m\n", sum_long / total);
    printf("  Mínimo:    %.4f m\n", min_long);
    printf("  Máximo:    %.4f m\n", max_long);
    printf("  Rango:     %.4f m\n\n", max_long - min_long);
}

int main() {
    DIR *dir = opendir("data/raw/gait");
    if (!dir) {
        printf("Error: No se puede abrir data/raw/gait\n");
        return 1;
    }

    ResultadoMarcha resultados[MAX_ARCHIVOS];
    int count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) && count < MAX_ARCHIVOS) {
        if (strstr(entry->d_name, ".csv") && strstr(entry->d_name, "gait")) {
            char ruta[512];
            snprintf(ruta, sizeof(ruta), "data/raw/gait/%s", entry->d_name);

            printf("Procesando: %-35s ", entry->d_name);
            if (procesar_archivo(ruta, &resultados[count])) {
                printf("✓\n");
                count++;
            } else {
                printf("✗\n");
            }
        }
    }

    closedir(dir);

    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("Procesados %d archivos exitosamente\n", count);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    exportar_csv(resultados, count);
    mostrar_estadisticas(resultados, count);

    printf("\n✓ Exportación completada.\n");
    printf("  Archivo: tarea2_resultados.csv\n");

    return 0;
}
