#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_FILES 100
#define MAX_PROCESSES 5

void copyFile(const char *source, const char *destination) {
    FILE *sourceFile = fopen(source, "rb");
    FILE *destinationFile = fopen(destination, "wb");

    if (sourceFile == NULL || destinationFile == NULL) {
        perror("Error al abrir archivo");
        exit(EXIT_FAILURE);
    }

    char buffer[1024];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), sourceFile)) > 0) {
        fwrite(buffer, 1, bytesRead, destinationFile);
    }

    fclose(sourceFile);
    fclose(destinationFile);
}

void writeToLogfile(const char *filename, int processId, double duration) {
    FILE *logfile = fopen("logfile.csv", "a");
    if (logfile == NULL) {
        perror("Error al abrir el archivo de bitácora");
        exit(EXIT_FAILURE);
    }

    time_t now = time(NULL);
    struct tm *localTime = localtime(&now);
    fprintf(logfile, "%s,%d,%.2f\n", filename, processId, duration);

    fclose(logfile);
}

void processDirectory(const char *dirPath, const char *destination) {
    DIR *dir = opendir(dirPath);
    struct dirent *entry;

    if (dir == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Si es un archivo regular
            char sourcePath[512];
            snprintf(sourcePath, sizeof(sourcePath), "%s/%s", dirPath, entry->d_name);
            char destinationPath[512];
            snprintf(destinationPath, sizeof(destinationPath), "%s/%s", destination, entry->d_name);

            clock_t startTime = clock();
            copyFile(sourcePath, destinationPath);
            clock_t endTime = clock();
            double duration = (double)(endTime - startTime) / CLOCKS_PER_SEC;

            printf("Archivo copiado: %s\n", entry->d_name);
            writeToLogfile(entry->d_name, getpid(), duration);
        } else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) { // Si es un directorio
            char subdirPath[512];
            snprintf(subdirPath, sizeof(subdirPath), "%s/%s", dirPath, entry->d_name);
            processDirectory(subdirPath, destination);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s directorio_origen directorio_destino\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *sourceDir = argv[1];
    const char *destinationDir = argv[2];

    // Crear archivo de bitácora (logfile)
    FILE *logfile = fopen("logfile.csv", "w");
    if (logfile == NULL) {
        perror("Error al crear el archivo de bitácora");
        exit(EXIT_FAILURE);
    }
    fprintf(logfile, "Filename,Process ID,Duration\n");
    fclose(logfile);

    // Procesar directorio y subdirectorios
    processDirectory(sourceDir, destinationDir);

    printf("Copia de archivos completa.\n");

    return 0;
}
