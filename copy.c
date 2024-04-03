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

void copy_file(char *source, char *destination, int process_id) {
    // Open source file for reading
    FILE *src = fopen(source, "rb");
    if (src == NULL) {
        perror("Error opening source file");
        exit(EXIT_FAILURE);
    }

    // Open destination file for writing
    char dest_path[1024];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", destination, basename(source));
    FILE *dest = fopen(dest_path, "wb");
    if (dest == NULL) {
        perror("Error opening destination file");
        fclose(src);
        exit(EXIT_FAILURE);
    }

    // Copy file contents
    char buffer[BUFSIZ];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes_read, dest);
    }

    // Close files
    fclose(src);
    fclose(dest);

    // Log the copy operation
    time_t current_time;
    time(&current_time);
    printf("%s,%d,%s,%s\n", ctime(&current_time), process_id, source, dest_path);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_directory> <destination_directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *source_directory = argv[1];
    char *destination_directory = argv[2];

    // Open source directory
    DIR *dir = opendir(source_directory);
    if (dir == NULL) {
        perror("Error opening source directory");
        exit(EXIT_FAILURE);
    }

    // Create process pool
    int process_count = 0;
    pid_t pid;
    while ((pid = fork()) > 0 && process_count < MAX_PROCESSES) {
        process_count++;
    }

    // Process files in directory
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char source_path[1024];
        snprintf(source_path, sizeof(source_path), "%s/%s", source_directory, entry->d_name);

        if (pid == 0) { // Child process
            copy_file(source_path, destination_directory, getpid());
            exit(EXIT_SUCCESS);
        }
    }

    // Wait for child processes to finish
    if (pid > 0) {
        for (int i = 0; i < process_count; i++) {
            wait(NULL);
        }
    }

    closedir(dir);
    return 0;
}
