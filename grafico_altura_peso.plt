set terminal png size 1000,600
set output 'altura_peso.png'
set xlabel 'Altura (cm)'
set ylabel 'Peso (kg)'
set title 'Relación Altura vs Peso de Sujetos'
set grid
plot 'metadatos_gait.csv' using 4:5 with points pt 7 ps 2 lc rgb 'red' title 'Datos'
