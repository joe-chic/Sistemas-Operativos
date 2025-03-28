# Threads V.S. Forks 

## Programas en C:
- [Problema 1.1: Forks usando malloc()](./problem1v1.c)
- [Problema 1.2: Forks usando mmap()](./problem1v2.c)
- [Problema 2.1: Threads usando queues y malloc()](./problem2v1.c)
- [Problema 2.2: Threads usando mmap()](./problem2v2.c)

## Introducción:
El presente reporte detalla los resultados de una investigación realizada sobre la eficiencia del uso de threads y mmap() en comparación con fork() y malloc() en la gestión de memoria compartida. Este análisis se enmarca dentro del contexto de optimización de procesos en sistemas operativos, con el objetivo de evaluar el impacto del uso de diferentes mecanismos de asignación de memoria en el rendimiento y consumo de recursos del sistema.

En este estudio, se han implementado soluciones basadas en procesos y memoria compartida, analizando su desempeño a través de herramientas de monitoreo de memoria y archivos abiertos. Se han empleado técnicas como la sincronización mediante pthread_mutex_t, la extracción de datos en distintos formatos y una estructura modular para facilitar pruebas y escalabilidad.

Los resultados obtenidos buscan demostrar la eficiencia de los threads con mmap() en la reducción del consumo de memoria y la optimización del acceso compartido, en contraposición con el modelo tradicional de fork() con malloc(), proporcionando una base para futuras mejoras en la gestión de memoria en entornos concurrentes.

## Librerias y paquetes necesarios para la ejecucion del codigo en Python:
- pip install spacy json re sys yake
- python -m spacy download en_core_web_md


