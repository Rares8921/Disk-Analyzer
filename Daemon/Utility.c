#include "Utility.h"
#include "ShmManager.h" // GetMutex();

#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <linux/stat.h>

static char *currentActivePath;

int fileSize(FILE *file) {
    if(!file) {
        return 0;
    }
    fseek(file, 0, SEEK_END);
    int size = (int) ftell(file);
    fseek(file, 0, SEEK_SET);
    return size;
}

long long directorySize(const char *folderPath) {
    DIR *dir = opendir(folderPath);
    if (!dir) {
        perror("Failed to open directory!");
        return 0;
    }

    long totalSize = 0;
    struct dirent *content;
    char filePath[1024];
    struct stat fileStat;

    while ((content = readdir(dir)) != NULL) {
        if (strcmp(content->d_name, ".") == 0 || strcmp(content->d_name, "..") == 0) {
            continue;
        }
        snprintf(filePath, sizeof(filePath), "%s/%s", folderPath, content->d_name);
        if (stat(filePath, &fileStat) == -1) {
            perror("Failed to get file status!");
            continue;
        }

        // pe versiuni mai mari de c11 mergea urmatoarea varianta
        /*switch (fileStat.st_mode & S_IFMT) {
            case S_IFREG: { 
                FILE *file = fopen(filePath, "r");
                if (file) {
                    totalSize += fileSize(file);
                    fclose(file);
                }
                break;
            }
            case S_IFDIR:
                totalSize += directorySize(filePath);
                break;
        }
        */
       if (S_ISREG(fileStat.st_mode)) {
            FILE *file = fopen(filePath, "r");
            if (file) {
                totalSize += fileSize(file);
                fclose(file);
            }
        } else if (S_ISDIR(fileStat.st_mode)) {
            totalSize += directorySize(filePath);
        }
    }

    closedir(dir);
    return totalSize;
}

void writeToFile(const char *path, const char *data) {
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "Could not open output file\n");
        return;
    }
    fprintf(file, "%s", data);
    fclose(file);
}

char *readFromFile(const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "Could not open input file\n");
        return NULL;
    }

    int file_Size = fileSize(file);
    char *data = malloc(sizeof(char) * file_Size);
    fread(data, file_Size, 1, file);
    fclose(file);

    return data;
}

char *getTaskPriority(int priority) {
    switch (priority) {
        case 1: 
            return "low";
        case 2: 
            return "normal";
        case 3: 
            return "high";
    }
    return "unknown";
}

char *getTaskStatus(int status) {
    switch (status) {
        case PENDING: 
            return "pending";
        case PROCESSING: 
            return "in progress";
        case PAUSED: 
            return "paused";
        case REMOVED:   
            return "killed";
        case DONE: 
            return "done";
    }
    return "unknown";
}

void saveCurrentPath() {
    // Initializare in memorie la 0
    currentActivePath = calloc(4096, sizeof(char));
    currentActivePath = "/home/rares/ProiectSO/";
}

char *getCurrentPath() {
    char *path = calloc(4096, sizeof(char));
    strcat(path, currentActivePath);
    return path;
}

void createDirectoryIfNotExists(const char *dirPath) {
    struct stat st;
    if (stat(dirPath, &st) == -1 && mkdir(dirPath, 0700) == -1) {
        perror("Failed to create directory");
    }
}

FILE *safeFileOpen(const char *path, const char *flag) {
    pthread_mutex_lock(GetMutex());
    FILE *out = fopen(path, flag);
    pthread_mutex_unlock(GetMutex());

    return out;
}

void safeFileClose(FILE *file) {
    pthread_mutex_lock(GetMutex());
    fclose(file);
    pthread_mutex_unlock(GetMutex());
}

int startsWith(const char *prefix, const char *str) {
    size_t lenpre = strlen(prefix), lenstr = strlen(str);
    return lenstr >= lenpre && memcmp(prefix, str, lenpre) == 0;
}
