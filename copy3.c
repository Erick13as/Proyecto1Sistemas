#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/wait.h>

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
            copyFile(sourcePath, destinationPath);
            printf("Archivo copiado: %s\n", entry->d_name);
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

    // Crear pool de procesos
    pid_t processes[MAX_PROCESSES];
    int processCount = 0;

    // Procesar directorio y subdirectorios
    processDirectory(sourceDir, destinationDir);

    // Esperar a que todos los procesos hijos terminen
    int i;
    for (i = 0; i < processCount; i++) {
        wait(NULL);
    }

    printf("Copia de archivos completa.\n");

    return 0;
}
