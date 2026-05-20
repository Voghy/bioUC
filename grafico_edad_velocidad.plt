set terminal png size 1000,600
set output 'edad_velocidad.png'
set xlabel 'Edad (años)'
set ylabel 'Velocidad (m/s)'
set title 'Relación entre Edad y Velocidad de Marcha'
set grid
plot 'metadatos_gait.csv' using 3:6 with points pt 7 ps 2 lc rgb 'blue' title 'Datos'
