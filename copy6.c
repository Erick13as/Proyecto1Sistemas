#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define MAX_PROCESSES 5

struct msg_buffer {
    long msg_type;
    char filename[256];
};

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

void writeToLogfile(const char *filename, const char *sourceDir, int processId, double duration) {
    FILE *logfile = fopen("logfile.csv", "a");
    if (logfile == NULL) {
        perror("Error al abrir el archivo de bitácora");
        exit(EXIT_FAILURE);
    }

    char fullSourcePath[512];
    snprintf(fullSourcePath, sizeof(fullSourcePath), "%s/%s", sourceDir, filename);

    fprintf(logfile, "%s,%d,%.2f\n", fullSourcePath, processId, duration);

    fclose(logfile);
}

void copyDirectory(const char *source, const char *destination) {
    DIR *dir = opendir(source);
    if (dir == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char sourcePath[512];
            snprintf(sourcePath, sizeof(sourcePath), "%s/%s", source, entry->d_name);
            char destinationPath[512];
            snprintf(destinationPath, sizeof(destinationPath), "%s/%s", destination, entry->d_name);

            struct stat st;
            stat(sourcePath, &st);

            if (S_ISDIR(st.st_mode)) {
                mkdir(destinationPath, 0777);
                clock_t startTime = clock();
                copyDirectory(sourcePath, destinationPath);
                clock_t endTime = clock();
                double duration = (double)(endTime - startTime) / CLOCKS_PER_SEC;
            } else {
                clock_t startTime = clock();
                copyFile(sourcePath, destinationPath);
                clock_t endTime = clock();
                double duration = (double)(endTime - startTime) / CLOCKS_PER_SEC;
                writeToLogfile(entry->d_name, source, getpid(), duration);
            }
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

    // Crear cola de mensajes
    key_t key = ftok("progfile", 65);
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    // Crear pool de procesos
    pid_t processes[MAX_PROCESSES];
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) { // Proceso hijo
            while (1) {
                struct msg_buffer message;
                if (msgrcv(msgid, &message, sizeof(message), 1, 0) == -1) {
                    perror("msgrcv");
                    exit(EXIT_FAILURE);
                }

                if (strcmp(message.filename, "FIN") == 0) {
                    break;
                }

                char sourcePath[512];
                snprintf(sourcePath, sizeof(sourcePath), "%s/%s", sourceDir, message.filename);
                char destinationPath[512];
                snprintf(destinationPath, sizeof(destinationPath), "%s/%s", destinationDir, message.filename);

                struct stat st;
                stat(sourcePath, &st);

                if (S_ISDIR(st.st_mode)) {
                    mkdir(destinationPath, 0777);
                    copyDirectory(sourcePath, destinationPath);
                } else {
                    clock_t startTime = clock();
                    copyFile(sourcePath, destinationPath);
                    clock_t endTime = clock();
                    double duration = (double)(endTime - startTime) / CLOCKS_PER_SEC;
                    writeToLogfile(message.filename, sourceDir, getpid(), duration);
                }

                // Enviar mensaje de confirmación de terminación de copia
                message.msg_type = getpid();
                if (msgsnd(msgid, &message, sizeof(message), 0) == -1) {
                    perror("msgsnd");
                    exit(EXIT_FAILURE);
                }
            }
            exit(EXIT_SUCCESS);
        } else { // Proceso padre
            processes[i] = pid;
        }
    }

    // Enviar archivos y carpetas al pool de procesos para que los copien
    DIR *dir = opendir(sourceDir);
    struct dirent *entry;

    if (dir == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            struct msg_buffer message;
            message.msg_type = 1;
            strcpy(message.filename, entry->d_name);
            if (msgsnd(msgid, &message, sizeof(message), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Enviar mensaje de finalización al pool de procesos
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        struct msg_buffer message;
        message.msg_type = 1;
        strcpy(message.filename, "FIN");
        if (msgsnd(msgid, &message, sizeof(message), 0) == -1) {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }
    }

    // Esperar a que todos los procesos hijos terminen
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        waitpid(processes[i], NULL, 0);
    }

    // Eliminar la cola de mensajes
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(EXIT_FAILURE);
    }

    closedir(dir);
    printf("Copia de archivos y carpetas completa.\n");

    return 0;
}
