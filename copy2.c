#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <libgen.h>

#define MAX_PROCESSES 10
#define BUFFER_SIZE 257

// Estructura para almacenar información sobre cada archivo copiado
typedef struct {
    char filename[BUFFER_SIZE];
    pid_t process_id;
    time_t start_time;
    time_t end_time;
} CopyInfo;

// Función para copiar un archivo
void copy_file(char *source, char *destination, pid_t process_id) {
    // Abrir archivo origen para lectura
    FILE *src = fopen(source, "rb");
    if (src == NULL) {
        perror("Error al abrir el archivo de origen");
        exit(EXIT_FAILURE);
    }

    // Construir la ruta del archivo de destino
    char dest_path[BUFFER_SIZE];
    snprintf(dest_path, BUFFER_SIZE, "%s/%s", destination, basename(source));

    // Abrir archivo de destino para escritura
    FILE *dest = fopen(dest_path, "wb");
    if (dest == NULL) {
        perror("Error al abrir el archivo de destino");
        fclose(src);
        exit(EXIT_FAILURE);
    }

    // Copiar contenido del archivo
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, src)) > 0) {
        fwrite(buffer, 1, bytes_read, dest);
    }

    // Cerrar archivos
    fclose(src);
    fclose(dest);

    // Obtener tiempo de finalización de la copia
    time_t end_time;
    time(&end_time);

    // Mostrar mensaje de finalización
    printf("Archivo copiado: %s\n", source);

    // Escribir información sobre la copia en el archivo de bitácora
    FILE *log_file = fopen("logfile.csv", "a");
    if (log_file == NULL) {
        perror("Error al abrir el archivo de bitácora");
    } else {
        fprintf(log_file, "%s,%d,%ld,%ld\n", source, process_id, (long)end_time, (long)(end_time - process_id));
        fclose(log_file);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <directorio_origen> <directorio_destino>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *source_directory = argv[1];
    char *destination_directory = argv[2];

    // Abrir directorio de origen
    DIR *dir = opendir(source_directory);
    if (dir == NULL) {
        perror("Error al abrir el directorio de origen");
        exit(EXIT_FAILURE);
    }

    // Crear pool de procesos
    int process_count = 0;
    CopyInfo copy_info[MAX_PROCESSES];
    pid_t pid;
    while ((pid = fork()) > 0 && process_count < MAX_PROCESSES) {
        process_count++;
    }

    // Procesar archivos en el directorio
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char source_path[BUFFER_SIZE];
        snprintf(source_path, BUFFER_SIZE, "%s/%s", source_directory, entry->d_name);

        if (pid == 0) { // Proceso hijo
            copy_file(source_path, destination_directory, getpid());
            exit(EXIT_SUCCESS);
        }
    }

    // Esperar a que los procesos hijos terminen
    if (pid > 0) {
        for (int i = 0; i < process_count; i++) {
            wait(NULL);
        }
    }

    closedir(dir);
    return 0;
}
