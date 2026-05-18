#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_DATOS 100
#define ANCHO_GRAFICO 80
#define ALTO_GRAFICO 20

typedef struct {
    char archivo[256];
    char sujeto[10];
    float edad;
    float altura;
    float peso;
    float velocidad;
    int muestras;
} DatoGait;

// Leer CSV
int leer_csv(const char *archivo, DatoGait datos[]) {
    FILE *fp = fopen(archivo, "r");
    if (!fp) return 0;

    char linea[512];
    int count = 0;

    // Saltar encabezado
    fgets(linea, sizeof(linea), fp);

    while (fgets(linea, sizeof(linea), fp) && count < MAX_DATOS) {
        if (linea[0] == '\0') break;

        sscanf(linea, "%[^,],%[^,],%f,%f,%f,%f,%d",
               datos[count].archivo,
               datos[count].sujeto,
               &datos[count].edad,
               &datos[count].altura,
               &datos[count].peso,
               &datos[count].velocidad,
               &datos[count].muestras);
        count++;
    }

    fclose(fp);
    return count;
}

// Encontrar min y max
void encontrar_rango(DatoGait datos[], int total, float *min_x, float *max_x,
                     float *min_y, float *max_y, int campo_x, int campo_y) {
    *min_x = *max_x = (campo_x == 0) ? datos[0].edad : datos[0].altura;
    *min_y = *max_y = (campo_y == 0) ? datos[0].edad : 
                       (campo_y == 1) ? datos[0].altura :
                       (campo_y == 2) ? datos[0].peso :
                       (campo_y == 3) ? datos[0].velocidad : datos[0].edad;

    for (int i = 1; i < total; i++) {
        float x = (campo_x == 0) ? datos[i].edad : datos[i].altura;
        float y = (campo_y == 0) ? datos[i].edad :
                  (campo_y == 1) ? datos[i].altura :
                  (campo_y == 2) ? datos[i].peso :
                  (campo_y == 3) ? datos[i].velocidad : datos[i].edad;

        if (x < *min_x) *min_x = x;
        if (x > *max_x) *max_x = x;
        if (y < *min_y) *min_y = y;
        if (y > *max_y) *max_y = y;
    }
}

// Obtener valor de campo
float obtener_valor(DatoGait *d, int campo) {
    switch (campo) {
        case 0: return d->edad;
        case 1: return d->altura;
        case 2: return d->peso;
        case 3: return d->velocidad;
        case 4: return d->muestras;
        default: return 0;
    }
}

// Nombres de campos
const char* nombre_campo(int campo) {
    switch (campo) {
        case 0: return "Edad (años)";
        case 1: return "Altura (cm)";
        case 2: return "Peso (kg)";
        case 3: return "Velocidad (m/s)";
        case 4: return "Muestras";
        default: return "Desconocido";
    }
}

// Graficar dispersión (scatter plot)
void graficar_scatter(DatoGait datos[], int total, const char *titulo,
                       int campo_x, int campo_y) {
    float min_x, max_x, min_y, max_y;
    encontrar_rango(datos, total, &min_x, &max_x, &min_y, &max_y, campo_x, campo_y);

    // Añadir margen
    float rango_x = max_x - min_x;
    float rango_y = max_y - min_y;
    min_x -= rango_x * 0.1;
    max_x += rango_x * 0.1;
    min_y -= rango_y * 0.1;
    max_y += rango_y * 0.1;

    // Matriz de puntos
    char grafico[ALTO_GRAFICO][ANCHO_GRAFICO + 1];
    memset(grafico, ' ', sizeof(grafico));

    // Rellenar
    for (int i = 0; i < ALTO_GRAFICO; i++) {
        grafico[i][ANCHO_GRAFICO] = '\0';
    }

    // Dibujar eje X
    for (int i = 0; i < ANCHO_GRAFICO; i++) {
        grafico[ALTO_GRAFICO - 1][i] = '-';
    }

    // Dibujar eje Y
    for (int i = 0; i < ALTO_GRAFICO; i++) {
        grafico[i][0] = '|';
    }

    grafico[ALTO_GRAFICO - 1][0] = '+';

    // Plotear puntos
    for (int i = 0; i < total; i++) {
        float x = obtener_valor(&datos[i], campo_x);
        float y = obtener_valor(&datos[i], campo_y);

        // Normalizar a coordenadas de gráfico
        int px = (int)((x - min_x) / (max_x - min_x) * (ANCHO_GRAFICO - 2)) + 1;
        int py = (int)(ALTO_GRAFICO - 1 - (y - min_y) / (max_y - min_y) * (ALTO_GRAFICO - 2));

        if (px >= 1 && px < ANCHO_GRAFICO && py >= 0 && py < ALTO_GRAFICO - 1) {
            if (grafico[py][px] == ' ') {
                grafico[py][px] = '*';
            } else if (grafico[py][px] == '*') {
                grafico[py][px] = '+';  // Superpuesto
            }
        }
    }

    // Imprimir
    printf("\n%s\n", titulo);
    printf("═══════════════════════════════════════════════════════════════════════════════════\n");

    for (int i = 0; i < ALTO_GRAFICO; i++) {
        printf("%s\n", grafico[i]);
    }

    printf("%-10s%.1f%-30s%.1f\n", nombre_campo(campo_x), min_x, " ", max_x);
    printf("%s: %.1f - %.1f\n", nombre_campo(campo_y), min_y, max_y);
    printf("\nLeyenda: * = punto, + = puntos superpuestos\n");
}

// Histograma
void graficar_histograma(DatoGait datos[], int total, const char *titulo,
                         int campo) {
    // Crear bins
    int bins = 10;
    int histograma[bins];
    memset(histograma, 0, sizeof(histograma));

    float min_val = obtener_valor(&datos[0], campo);
    float max_val = min_val;

    for (int i = 1; i < total; i++) {
        float val = obtener_valor(&datos[i], campo);
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    float rango = max_val - min_val;
    if (rango == 0) rango = 1;

    // Contar
    for (int i = 0; i < total; i++) {
        float val = obtener_valor(&datos[i], campo);
        int bin = (int)((val - min_val) / rango * (bins - 0.01));
        if (bin >= bins) bin = bins - 1;
        histograma[bin]++;
    }

    // Encontrar máximo para escala
    int max_count = 0;
    for (int i = 0; i < bins; i++) {
        if (histograma[i] > max_count) max_count = histograma[i];
    }

    // Imprimir
    printf("\n%s\n", titulo);
    printf("═══════════════════════════════════════════════════════════════════════════════════\n");

    for (int i = 0; i < bins; i++) {
        float rango_min = min_val + (i / (float)bins) * rango;
        float rango_max = min_val + ((i + 1) / (float)bins) * rango;

        printf("[%6.1f-%6.1f] | ", rango_min, rango_max);
        int barras = (int)(histograma[i] * 60.0 / max_count);
        for (int j = 0; j < barras; j++) {
            printf("#");
        }
        printf(" (%d)\n", histograma[i]);
    }
}

// Estadísticas
void mostrar_estadisticas(DatoGait datos[], int total) {
    printf("\n═══════════════════════════════════════════════════════════════════════════════════\n");
    printf("ESTADISTICAS DE LOS DATOS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════════\n");

    float suma_edad = 0, suma_altura = 0, suma_peso = 0, suma_velocidad = 0;

    for (int i = 0; i < total; i++) {
        suma_edad += datos[i].edad;
        suma_altura += datos[i].altura;
        suma_peso += datos[i].peso;
        suma_velocidad += datos[i].velocidad;
    }

    printf("Total de registros: %d\n\n", total);
    printf("EDAD:\n");
    printf("  Promedio: %.2f años\n", suma_edad / total);

    printf("\nALTURA:\n");
    printf("  Promedio: %.2f cm\n", suma_altura / total);

    printf("\nPESO:\n");
    printf("  Promedio: %.2f kg\n", suma_peso / total);

    printf("\nVELOCIDAD DE MARCHA:\n");
    printf("  Promedio: %.3f m/s\n", suma_velocidad / total);
}

int main() {
    DatoGait datos[MAX_DATOS];
    int total = leer_csv("metadatos_gait.csv", datos);

    if (total == 0) {
        printf("Error: No se pudo leer metadatos_gait.csv\n");
        return 1;
    }

    printf("\n╔═════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    ANALISIS DE DATOS BIOMECÁNICOS DE MARCHA                    ║\n");
    printf("╚═════════════════════════════════════════════════════════════════════════════════╝\n");

    // Mostrar estadísticas
    mostrar_estadisticas(datos, total);

    // Gráfico 1: Edad vs Velocidad
    graficar_scatter(datos, total, "GRÁFICO 1: Edad vs Velocidad de Marcha", 0, 3);

    // Gráfico 2: Altura vs Peso
    graficar_scatter(datos, total, "GRÁFICO 2: Altura vs Peso", 1, 2);

    // Gráfico 3: Histograma de Edades
    graficar_histograma(datos, total, "GRÁFICO 3: Distribución de Edades", 0);

    // Gráfico 4: Histograma de Velocidades
    graficar_histograma(datos, total, "GRÁFICO 4: Distribución de Velocidades de Marcha", 3);

    printf("\n═══════════════════════════════════════════════════════════════════════════════════\n");
    printf("Análisis completado.\n");

    return 0;
}
