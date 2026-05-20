#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DATOS 100

typedef struct {
    char archivo[256];
    char sujeto[10];
    float edad;
    float altura;
    float peso;
    float velocidad;
    int muestras;
} DatoGait;

int leer_csv(const char *archivo, DatoGait datos[]) {
    FILE *fp = fopen(archivo, "r");
    if (!fp) return 0;

    char linea[512];
    int count = 0;

    fgets(linea, sizeof(linea), fp);  // Saltar encabezado

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

void generar_html(DatoGait datos[], int total) {
    FILE *html = fopen("reporte_graficos.html", "w");
    if (!html) return;

    fprintf(html, "<!DOCTYPE html>\n");
    fprintf(html, "<html lang=\"es\">\n");
    fprintf(html, "<head>\n");
    fprintf(html, "    <meta charset=\"UTF-8\">\n");
    fprintf(html, "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    fprintf(html, "    <title>Análisis Biomecánico - Tarea 1</title>\n");
    fprintf(html, "    <script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>\n");
    fprintf(html, "    <style>\n");
    fprintf(html, "        * { margin: 0; padding: 0; box-sizing: border-box; }\n");
    fprintf(html, "        body {\n");
    fprintf(html, "            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\n");
    fprintf(html, "            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n");
    fprintf(html, "            min-height: 100vh;\n");
    fprintf(html, "            padding: 20px;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        .container {\n");
    fprintf(html, "            max-width: 1400px;\n");
    fprintf(html, "            margin: 0 auto;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        header {\n");
    fprintf(html, "            background: white;\n");
    fprintf(html, "            padding: 30px;\n");
    fprintf(html, "            border-radius: 10px;\n");
    fprintf(html, "            margin-bottom: 30px;\n");
    fprintf(html, "            box-shadow: 0 10px 30px rgba(0,0,0,0.3);\n");
    fprintf(html, "        }\n");
    fprintf(html, "        h1 {\n");
    fprintf(html, "            color: #333;\n");
    fprintf(html, "            margin-bottom: 10px;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        .stats {\n");
    fprintf(html, "            display: grid;\n");
    fprintf(html, "            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));\n");
    fprintf(html, "            gap: 20px;\n");
    fprintf(html, "            margin-top: 20px;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        .stat-box {\n");
    fprintf(html, "            background: #f8f9fa;\n");
    fprintf(html, "            padding: 15px;\n");
    fprintf(html, "            border-radius: 8px;\n");
    fprintf(html, "            border-left: 4px solid #667eea;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        .stat-box h3 {\n");
    fprintf(html, "            color: #667eea;\n");
    fprintf(html, "            font-size: 14px;\n");
    fprintf(html, "            margin-bottom: 5px;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        .stat-value {\n");
    fprintf(html, "            font-size: 24px;\n");
    fprintf(html, "            font-weight: bold;\n");
    fprintf(html, "            color: #333;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        .graficos {\n");
    fprintf(html, "            display: grid;\n");
    fprintf(html, "            grid-template-columns: repeat(auto-fit, minmax(600px, 1fr));\n");
    fprintf(html, "            gap: 30px;\n");
    fprintf(html, "            margin-bottom: 30px;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        .grafico-container {\n");
    fprintf(html, "            background: white;\n");
    fprintf(html, "            padding: 20px;\n");
    fprintf(html, "            border-radius: 10px;\n");
    fprintf(html, "            box-shadow: 0 10px 30px rgba(0,0,0,0.3);\n");
    fprintf(html, "        }\n");
    fprintf(html, "        .grafico-titulo {\n");
    fprintf(html, "            font-size: 18px;\n");
    fprintf(html, "            font-weight: bold;\n");
    fprintf(html, "            color: #333;\n");
    fprintf(html, "            margin-bottom: 15px;\n");
    fprintf(html, "            text-align: center;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        canvas {\n");
    fprintf(html, "            max-height: 400px;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        table {\n");
    fprintf(html, "            width: 100%%;\n");
    fprintf(html, "            border-collapse: collapse;\n");
    fprintf(html, "            background: white;\n");
    fprintf(html, "            border-radius: 10px;\n");
    fprintf(html, "            box-shadow: 0 10px 30px rgba(0,0,0,0.3);\n");
    fprintf(html, "            overflow: hidden;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        thead {\n");
    fprintf(html, "            background: #667eea;\n");
    fprintf(html, "            color: white;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        th, td {\n");
    fprintf(html, "            padding: 12px;\n");
    fprintf(html, "            text-align: left;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        tbody tr:nth-child(even) {\n");
    fprintf(html, "            background: #f8f9fa;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        tbody tr:hover {\n");
    fprintf(html, "            background: #e9ecef;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        .tabla-titulo {\n");
    fprintf(html, "            font-size: 20px;\n");
    fprintf(html, "            font-weight: bold;\n");
    fprintf(html, "            color: white;\n");
    fprintf(html, "            padding: 20px;\n");
    fprintf(html, "            background: #667eea;\n");
    fprintf(html, "            margin-bottom: 0;\n");
    fprintf(html, "            border-radius: 10px 10px 0 0;\n");
    fprintf(html, "        }\n");
    fprintf(html, "        .datos-tabla {\n");
    fprintf(html, "            margin-top: 30px;\n");
    fprintf(html, "            border-radius: 10px;\n");
    fprintf(html, "            overflow: hidden;\n");
    fprintf(html, "            box-shadow: 0 10px 30px rgba(0,0,0,0.3);\n");
    fprintf(html, "        }\n");
    fprintf(html, "        footer {\n");
    fprintf(html, "            text-align: center;\n");
    fprintf(html, "            color: white;\n");
    fprintf(html, "            margin-top: 40px;\n");
    fprintf(html, "            padding: 20px;\n");
    fprintf(html, "        }\n");
    fprintf(html, "    </style>\n");
    fprintf(html, "</head>\n");
    fprintf(html, "<body>\n");
    fprintf(html, "    <div class=\"container\">\n");

    // Header con estadísticas
    fprintf(html, "        <header>\n");
    fprintf(html, "            <h1>📊 Análisis de Datos Biomecánicos de Marcha - Tarea 1</h1>\n");
    fprintf(html, "            <p>Dataset: BioMecanicaUC | Protocolo: 10 Meter Walking Test (10MWT)</p>\n");

    // Calcular estadísticas
    float sum_edad = 0, sum_altura = 0, sum_peso = 0, sum_velocidad = 0;
    float min_edad = datos[0].edad, max_edad = datos[0].edad;
    float min_velocidad = datos[0].velocidad, max_velocidad = datos[0].velocidad;

    for (int i = 0; i < total; i++) {
        sum_edad += datos[i].edad;
        sum_altura += datos[i].altura;
        sum_peso += datos[i].peso;
        sum_velocidad += datos[i].velocidad;
        
        if (datos[i].edad < min_edad) min_edad = datos[i].edad;
        if (datos[i].edad > max_edad) max_edad = datos[i].edad;
        if (datos[i].velocidad < min_velocidad) min_velocidad = datos[i].velocidad;
        if (datos[i].velocidad > max_velocidad) max_velocidad = datos[i].velocidad;
    }

    fprintf(html, "            <div class=\"stats\">\n");
    fprintf(html, "                <div class=\"stat-box\">\n");
    fprintf(html, "                    <h3>Total de Registros</h3>\n");
    fprintf(html, "                    <div class=\"stat-value\">%d</div>\n", total);
    fprintf(html, "                </div>\n");
    fprintf(html, "                <div class=\"stat-box\">\n");
    fprintf(html, "                    <h3>Edad Promedio</h3>\n");
    fprintf(html, "                    <div class=\"stat-value\">%.1f años</div>\n", sum_edad / total);
    fprintf(html, "                </div>\n");
    fprintf(html, "                <div class=\"stat-box\">\n");
    fprintf(html, "                    <h3>Velocidad Promedio</h3>\n");
    fprintf(html, "                    <div class=\"stat-value\">%.2f m/s</div>\n", sum_velocidad / total);
    fprintf(html, "                </div>\n");
    fprintf(html, "                <div class=\"stat-box\">\n");
    fprintf(html, "                    <h3>Peso Promedio</h3>\n");
    fprintf(html, "                    <div class=\"stat-value\">%.1f kg</div>\n", sum_peso / total);
    fprintf(html, "                </div>\n");
    fprintf(html, "            </div>\n");
    fprintf(html, "        </header>\n\n");

    // Gráficos
    fprintf(html, "        <div class=\"graficos\">\n");

    // Gráfico 1: Scatter - Edad vs Velocidad
    fprintf(html, "            <div class=\"grafico-container\">\n");
    fprintf(html, "                <div class=\"grafico-titulo\">Edad vs Velocidad de Marcha</div>\n");
    fprintf(html, "                <canvas id=\"chart1\"></canvas>\n");
    fprintf(html, "            </div>\n");

    // Gráfico 2: Scatter - Altura vs Peso
    fprintf(html, "            <div class=\"grafico-container\">\n");
    fprintf(html, "                <div class=\"grafico-titulo\">Altura vs Peso</div>\n");
    fprintf(html, "                <canvas id=\"chart2\"></canvas>\n");
    fprintf(html, "            </div>\n");

    // Gráfico 3: Histograma - Edades
    fprintf(html, "            <div class=\"grafico-container\">\n");
    fprintf(html, "                <div class=\"grafico-titulo\">Distribución de Edades</div>\n");
    fprintf(html, "                <canvas id=\"chart3\"></canvas>\n");
    fprintf(html, "            </div>\n");

    // Gráfico 4: Histograma - Velocidades
    fprintf(html, "            <div class=\"grafico-container\">\n");
    fprintf(html, "                <div class=\"grafico-titulo\">Distribución de Velocidades</div>\n");
    fprintf(html, "                <canvas id=\"chart4\"></canvas>\n");
    fprintf(html, "            </div>\n");

    fprintf(html, "        </div>\n\n");

    // Tabla de datos
    fprintf(html, "        <div class=\"datos-tabla\">\n");
    fprintf(html, "            <div class=\"tabla-titulo\">📋 Datos Detallados</div>\n");
    fprintf(html, "            <table>\n");
    fprintf(html, "                <thead>\n");
    fprintf(html, "                    <tr>\n");
    fprintf(html, "                        <th>Archivo</th>\n");
    fprintf(html, "                        <th>Sujeto</th>\n");
    fprintf(html, "                        <th>Edad</th>\n");
    fprintf(html, "                        <th>Altura (cm)</th>\n");
    fprintf(html, "                        <th>Peso (kg)</th>\n");
    fprintf(html, "                        <th>Velocidad (m/s)</th>\n");
    fprintf(html, "                        <th>Muestras</th>\n");
    fprintf(html, "                    </tr>\n");
    fprintf(html, "                </thead>\n");
    fprintf(html, "                <tbody>\n");

    for (int i = 0; i < total; i++) {
        fprintf(html, "                    <tr>\n");
        fprintf(html, "                        <td>%s</td>\n", datos[i].archivo);
        fprintf(html, "                        <td>%s</td>\n", datos[i].sujeto);
        fprintf(html, "                        <td>%.0f</td>\n", datos[i].edad);
        fprintf(html, "                        <td>%.1f</td>\n", datos[i].altura);
        fprintf(html, "                        <td>%.1f</td>\n", datos[i].peso);
        fprintf(html, "                        <td>%.3f</td>\n", datos[i].velocidad);
        fprintf(html, "                        <td>%d</td>\n", datos[i].muestras);
        fprintf(html, "                    </tr>\n");
    }

    fprintf(html, "                </tbody>\n");
    fprintf(html, "            </table>\n");
    fprintf(html, "        </div>\n\n");

    fprintf(html, "        <footer>\n");
    fprintf(html, "            <p>Generado automáticamente por graficos_html.c | %s</p>\n", "Tarea 1 - Ingeniería de Software II");
    fprintf(html, "        </footer>\n");
    fprintf(html, "    </div>\n\n");

    // JavaScript para gráficos
    fprintf(html, "    <script>\n");

    // Preparar datos para Chart.js
    fprintf(html, "        const datos = [\n");
    for (int i = 0; i < total; i++) {
        fprintf(html, "            { edad: %.0f, altura: %.1f, peso: %.1f, velocidad: %.3f },\n",
                datos[i].edad, datos[i].altura, datos[i].peso, datos[i].velocidad);
    }
    fprintf(html, "        ];\n\n");

    // Gráfico 1: Scatter Edad vs Velocidad
    fprintf(html, "        const ctx1 = document.getElementById('chart1').getContext('2d');\n");
    fprintf(html, "        new Chart(ctx1, {\n");
    fprintf(html, "            type: 'scatter',\n");
    fprintf(html, "            data: {\n");
    fprintf(html, "                datasets: [{\n");
    fprintf(html, "                    label: 'Edad vs Velocidad',\n");
    fprintf(html, "                    data: datos.map(d => ({x: d.edad, y: d.velocidad})),\n");
    fprintf(html, "                    backgroundColor: 'rgba(102, 126, 234, 0.7)',\n");
    fprintf(html, "                    borderColor: 'rgba(102, 126, 234, 1)',\n");
    fprintf(html, "                    borderWidth: 2,\n");
    fprintf(html, "                    pointRadius: 6,\n");
    fprintf(html, "                    pointHoverRadius: 8,\n");
    fprintf(html, "                }]\n");
    fprintf(html, "            },\n");
    fprintf(html, "            options: {\n");
    fprintf(html, "                responsive: true,\n");
    fprintf(html, "                maintainAspectRatio: true,\n");
    fprintf(html, "                plugins: { legend: { display: false } },\n");
    fprintf(html, "                scales: {\n");
    fprintf(html, "                    x: { title: { display: true, text: 'Edad (años)' } },\n");
    fprintf(html, "                    y: { title: { display: true, text: 'Velocidad (m/s)' } }\n");
    fprintf(html, "                }\n");
    fprintf(html, "            }\n");
    fprintf(html, "        });\n\n");

    // Gráfico 2: Scatter Altura vs Peso
    fprintf(html, "        const ctx2 = document.getElementById('chart2').getContext('2d');\n");
    fprintf(html, "        new Chart(ctx2, {\n");
    fprintf(html, "            type: 'scatter',\n");
    fprintf(html, "            data: {\n");
    fprintf(html, "                datasets: [{\n");
    fprintf(html, "                    label: 'Altura vs Peso',\n");
    fprintf(html, "                    data: datos.map(d => ({x: d.altura, y: d.peso})),\n");
    fprintf(html, "                    backgroundColor: 'rgba(118, 75, 162, 0.7)',\n");
    fprintf(html, "                    borderColor: 'rgba(118, 75, 162, 1)',\n");
    fprintf(html, "                    borderWidth: 2,\n");
    fprintf(html, "                    pointRadius: 6,\n");
    fprintf(html, "                    pointHoverRadius: 8,\n");
    fprintf(html, "                }]\n");
    fprintf(html, "            },\n");
    fprintf(html, "            options: {\n");
    fprintf(html, "                responsive: true,\n");
    fprintf(html, "                maintainAspectRatio: true,\n");
    fprintf(html, "                plugins: { legend: { display: false } },\n");
    fprintf(html, "                scales: {\n");
    fprintf(html, "                    x: { title: { display: true, text: 'Altura (cm)' } },\n");
    fprintf(html, "                    y: { title: { display: true, text: 'Peso (kg)' } }\n");
    fprintf(html, "                }\n");
    fprintf(html, "            }\n");
    fprintf(html, "        });\n\n");

    // Gráfico 3: Histograma Edades
    fprintf(html, "        const edadRanges = ['20-22', '22-24', '24-26', '26-28', '28-30', '30-32', '32-34', '34-36', '36-38', '38-40'];\n");
    fprintf(html, "        const edadCounts = [0,0,0,0,0,0,0,0,0,0];\n");
    fprintf(html, "        datos.forEach(d => {\n");
    fprintf(html, "            let bin = Math.floor((d.edad - 20) / 2);\n");
    fprintf(html, "            if (bin >= 0 && bin < 10) edadCounts[bin]++;\n");
    fprintf(html, "        });\n");
    fprintf(html, "        const ctx3 = document.getElementById('chart3').getContext('2d');\n");
    fprintf(html, "        new Chart(ctx3, {\n");
    fprintf(html, "            type: 'bar',\n");
    fprintf(html, "            data: {\n");
    fprintf(html, "                labels: edadRanges,\n");
    fprintf(html, "                datasets: [{\n");
    fprintf(html, "                    label: 'Frecuencia',\n");
    fprintf(html, "                    data: edadCounts,\n");
    fprintf(html, "                    backgroundColor: 'rgba(102, 126, 234, 0.7)',\n");
    fprintf(html, "                    borderColor: 'rgba(102, 126, 234, 1)',\n");
    fprintf(html, "                    borderWidth: 2,\n");
    fprintf(html, "                }]\n");
    fprintf(html, "            },\n");
    fprintf(html, "            options: {\n");
    fprintf(html, "                responsive: true,\n");
    fprintf(html, "                maintainAspectRatio: true,\n");
    fprintf(html, "                plugins: { legend: { display: false } },\n");
    fprintf(html, "                scales: {\n");
    fprintf(html, "                    y: { beginAtZero: true, ticks: { stepSize: 1 } }\n");
    fprintf(html, "                }\n");
    fprintf(html, "            }\n");
    fprintf(html, "        });\n\n");

    // Gráfico 4: Histograma Velocidades
    fprintf(html, "        const velRanges = ['0.5-0.6', '0.6-0.7', '0.7-0.8', '0.8-0.9', '0.9-1.0', '1.0-1.1', '1.1-1.2', '1.2-1.3', '1.3-1.4', '1.4-1.5'];\n");
    fprintf(html, "        const velCounts = [0,0,0,0,0,0,0,0,0,0];\n");
    fprintf(html, "        datos.forEach(d => {\n");
    fprintf(html, "            let bin = Math.floor((d.velocidad - 0.5) * 10);\n");
    fprintf(html, "            if (bin >= 0 && bin < 10) velCounts[bin]++;\n");
    fprintf(html, "        });\n");
    fprintf(html, "        const ctx4 = document.getElementById('chart4').getContext('2d');\n");
    fprintf(html, "        new Chart(ctx4, {\n");
    fprintf(html, "            type: 'bar',\n");
    fprintf(html, "            data: {\n");
    fprintf(html, "                labels: velRanges,\n");
    fprintf(html, "                datasets: [{\n");
    fprintf(html, "                    label: 'Frecuencia',\n");
    fprintf(html, "                    data: velCounts,\n");
    fprintf(html, "                    backgroundColor: 'rgba(118, 75, 162, 0.7)',\n");
    fprintf(html, "                    borderColor: 'rgba(118, 75, 162, 1)',\n");
    fprintf(html, "                    borderWidth: 2,\n");
    fprintf(html, "                }]\n");
    fprintf(html, "            },\n");
    fprintf(html, "            options: {\n");
    fprintf(html, "                responsive: true,\n");
    fprintf(html, "                maintainAspectRatio: true,\n");
    fprintf(html, "                plugins: { legend: { display: false } },\n");
    fprintf(html, "                scales: {\n");
    fprintf(html, "                    y: { beginAtZero: true, ticks: { stepSize: 1 } }\n");
    fprintf(html, "                }\n");
    fprintf(html, "            }\n");
    fprintf(html, "        });\n");

    fprintf(html, "    </script>\n");
    fprintf(html, "</body>\n");
    fprintf(html, "</html>\n");

    fclose(html);
    printf("✓ HTML generado: reporte_graficos.html\n");
}

int main() {
    DatoGait datos[MAX_DATOS];
    int total = leer_csv("metadatos_gait.csv", datos);

    if (total == 0) {
        printf("Error: No se pudo leer metadatos_gait.csv\n");
        return 1;
    }

    printf("Procesando %d registros...\n", total);
    generar_html(datos, total);
    printf("Abre reporte_graficos.html en tu navegador.\n");

    return 0;
}
