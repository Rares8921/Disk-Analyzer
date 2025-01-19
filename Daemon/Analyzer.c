#include "Analyzer.h"
#include "Utility.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

void updateUnitAndCopy(LL *copy, char *unit);

int IsDirectory(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

LL GetFileSize(const char *path) {
    struct stat path_stat;
    if (stat(path, &path_stat) == 0) {
        return path_stat.st_size;
    }
    return 0;
}

void SetNextPath(const char *basePath, const char *name, char *next_path) {
    strcpy(next_path, basePath);
    if (next_path[strlen(next_path) - 1] != '/') {
        strcat(next_path, "/");
    }
    strcat(next_path, name);
}

void CountDirectories(const char *basePath, LL *total_size) {

    DIR *dir = opendir(basePath);
    if (dir == NULL) return;

    struct dirent *dp;
    char next_path[DA_PATH_MAX_LENGTH];

    while (1) {
        dp = readdir(dir);
        if (dp == NULL) break;

        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            SetNextPath(basePath, dp->d_name, next_path);
            *total_size += GetFileSize(next_path);
            if (IsDirectory(next_path)) {
                CountDirectories(next_path, total_size);
            }
        }
    }
    closedir(dir);
}

LL OutputData(const char *basePath, char *unit, char *hashtags, 
               FILE *analysis_fd, const char *statusPath, LL *total, 
               int first, int *fileCount, int *dirCount, int jobID) {
    DIR *dir = opendir(basePath);
    if (dir == NULL) return 0;

    struct dirent *dp;
    char next_path[DA_PATH_MAX_LENGTH];
    LL cur_dir_size = 0;

    for (dp = readdir(dir); dp != NULL; dp = readdir(dir)) {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            SetNextPath(basePath, dp->d_name, next_path);
            cur_dir_size += GetFileSize(next_path);
            if (IsDirectory(next_path)) {
                cur_dir_size += OutputData(next_path, unit, hashtags, analysis_fd,
                                             statusPath, total, 0, fileCount, dirCount, jobID);
                (*dirCount)++;
            } else {
                (*fileCount)++;
            }
        }
    }
    closedir(dir);

    double percentage = (double) cur_dir_size / (double) *total;
    int hashtag_cnt = (int)(percentage * 40.0 + 1);
    hashtag_cnt = (hashtag_cnt > 40) ? 40 : hashtag_cnt;

    LL copy = cur_dir_size;
    updateUnitAndCopy(&copy, unit);

    memset(hashtags, '#', hashtag_cnt);
    hashtags[hashtag_cnt] = '\0';

    if (!first) {
        // %0.2lf%% =>
        //      % - variabila in sirul de caractere, aici procentaj
        //      0.2 - 0 spatii inainte de valoarea afisata, .2 - afisam cu 2 zecimale
        // lf - format double
        fprintf(analysis_fd, "|-%s/  %0.2lf%%  %lld%s  %s\n", basePath, percentage * 100, copy, unit, hashtags);
    }

    // Scriem statusul
    FILE *statusFD = safeFileOpen(statusPath, "w");
    if (statusFD) {
        percentage = (double) (*fileCount + *dirCount) / (double) *total;
        fprintf(statusFD, "%d%%\n%d files\n%d dirs", (int)(percentage * 100), *fileCount, *dirCount);
        safeFileClose(statusFD);
    }

    return cur_dir_size;
}

void analyze(const char *path, int jobID) {
    char output_path[DA_PATH_MAX_LENGTH], statusPath[DA_PATH_MAX_LENGTH];
    snprintf(output_path, sizeof(output_path), "%s%s", getCurrentPath(), DA_ANALYSIS_PATH);
    snprintf(statusPath, sizeof(statusPath), "%s%s", getCurrentPath(), DA_STATUS_PATH);

    FILE *fd = safeFileOpen(output_path, "w");
    FILE *statusFD = safeFileOpen(statusPath, "w");
    fprintf(statusFD, "%d%%\n%d files\n%d dirs", 0, 0, 0);
    safeFileClose(statusFD);

    if (fd == NULL) {
        fprintf(stderr, "Path doesn't exist!\n");
        return;
    }

    LL total_size = 0;
    CountDirectories(path, &total_size);

    char unit[3], hashtags[42];
    LL copy = total_size;
    updateUnitAndCopy(&copy, unit); 

    memset(hashtags, '#', 41);
    hashtags[41] = '\0';

    fprintf(fd, "Path  Usage  Size  Amount\n%s  100%%  %lld%s  %s\n|\n", path, copy, unit, hashtags);

    int fileCount = 0, dirCount = 0;
    OutputData(path, unit, hashtags, fd, statusPath, &total_size, 1, &fileCount, &dirCount, jobID);

    safeFileClose(fd);
}


void updateUnitAndCopy(LL *copy, char *unit) {
    if (*copy > 1024) {
        *copy /= 1024;
        strcpy(unit, "KB");
    }
    if (*copy > 1024) {
        *copy /= 1024;
        strcpy(unit, "MB");
    }
    if (*copy > 1024) {
        *copy /= 1024;
        strcpy(unit, "GB");
    }
    if (*copy > 1024) {
        *copy /= 1024;
        strcpy(unit, "TB");
    }
}