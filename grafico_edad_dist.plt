set terminal png size 1000,600
set output 'distribucion_edades.png'
set xlabel 'Edad (años)'
set ylabel 'Frecuencia'
set title 'Distribución de Edades'
set style data histogram
set style histogram clustered gap 1
plot 'metadatos_gait.csv' using 3 with boxes lc rgb 'green' title 'Edades'
