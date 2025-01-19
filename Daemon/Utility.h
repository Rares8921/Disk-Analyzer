#ifndef DISKANALYZER_UTILITY_H
#define DISKANALYZER_UTILITY_H

#define LL long long

#define DA_MAX_TASKS 8
#define DA_MAX_PROCESSES 2048
#define DA_MAX_PID_SIZE 16
#define DA_PATH_MAX_LENGTH 2048

#define DA_DAEMON_DATA_PATH "/DaemonData/"
#define DA_PID_PATH "/DaemonData/DaemonPID"
#define DA_ANSWER_PATH "/DaemonData/DaemonStatus.txt"
#define INSTRUCTION_PATH "/DaemonData/DaemonEvent.txt"
#define DA_ANALYSIS_PATH "/DaemonData/Analysis_%d.txt"
#define DA_STATUS_PATH "/DaemonData/Status_%d.txt"
#define DA_BACKUP_PATH "/home/rares/ProiectSO/Backup/"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

enum TaskState {
    PENDING = 1,
    PROCESSING,
    PAUSED,
    DONE,
    REMOVED
};

enum TaskList {
    ADD = 1,
    SUSPEND,
    RESUME,
    KILL,
    INFO,
    LIST,
    PRINT,
    EXPORT,
    BACKUP,
    SORT,
    DUPLICATE,
};

struct SignalInfo {
    int type, priority;
    pid_t pid, ppid;
    char *path;
};

struct TaskInfo {
    int task_id;
    char path[DA_PATH_MAX_LENGTH];
    int status;
    int priority;
    int worker_pid;
    long long directory_size;
};

char *getTaskPriority(int);
char *getTaskStatus(int);

int fileSize(FILE *fp);
long long directorySize(const char *);

void createDirectoryIfNotExists(const char *);
FILE *safeFileOpen(const char *, const char *);
void safeFileClose(FILE *);

void writeToFile(const char *, const char *);
char *readFromFile(const char *);

void saveCurrentPath();
char *getCurrentPath();

int startsWith(const char *, const char *);


#endif //DISKANALYZER_UTILITY_H
