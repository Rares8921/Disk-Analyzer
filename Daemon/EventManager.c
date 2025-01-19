#include <stdio.h>
#include <time.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include "EventManager.h"
#include "TaskManager.h"

static struct SignalInfo *current_signal = NULL;

void saveDaemonPID(const pid_t pid) {
    char *complete_pid_path = getCurrentPath();
    strcat(complete_pid_path, DA_PID_PATH);
    printf("Saving PID to: %s\n", complete_pid_path);
    char data[DA_MAX_PID_SIZE];
    sprintf(data, "%d", pid);
    writeToFile(complete_pid_path, data);
}

void writeDaemonStatus(char *data) {
    char *complete_output_path = getCurrentPath();
    strcat(complete_output_path, DA_ANSWER_PATH);
    writeToFile(complete_output_path, data);
}

struct SignalInfo *readDaemonEvent() {
    char *complete_instruction_path = getCurrentPath();
    strcat(complete_instruction_path, INSTRUCTION_PATH);
    char *data = readFromFile(complete_instruction_path);

    struct SignalInfo *incoming_signal = malloc(sizeof(*incoming_signal));
    sscanf(data, "TYPE %d", &incoming_signal->type);

    if (incoming_signal->type == ADD) {
        incoming_signal->path = malloc(sizeof(char) * FILENAME_MAX);
        sscanf(data, "TYPE %d\nPRIORITY %d\nPATH %s\nPPID %d",
               &incoming_signal->type, &incoming_signal->priority,
               incoming_signal->path, &incoming_signal->ppid);
    } else {
        incoming_signal->path = malloc(sizeof(char) * FILENAME_MAX);
        sscanf(data, "TYPE %d\nPID %d\nPPID %d", &incoming_signal->type,
               &incoming_signal->pid, &incoming_signal->ppid);
    }

    free(data);
    free(complete_instruction_path);
    return incoming_signal;
}

void HandleSignal(int signo) {
    printf("Received Signal %d on USR1\n", signo);
    if (current_signal) {
        free(current_signal->path);
        free(current_signal);
    }
    current_signal = readDaemonEvent();
}

void removeTemporaryFiles() {
    char* temp_dir = getCurrentPath();
    strncat(temp_dir, DA_DAEMON_DATA_PATH, strlen(DA_DAEMON_DATA_PATH));
    DIR *d = opendir(temp_dir);
    struct dirent *temp_file;
    if(d) {
        while((temp_file = readdir(d))!=NULL){
            if(strcmp(temp_file->d_name, ".")==0 || strcmp(temp_file->d_name, "..")==0) {
                continue;
            }
            char temp_file_full_path[DA_PATH_MAX_LENGTH] = "";
            strcat(temp_file_full_path, temp_dir);
            strcat(temp_file_full_path, temp_file->d_name);
            remove(temp_file_full_path);
        }
    }
}

void HandleKillSignal(int signo) {
    finishAllTasks();
    removeTemporaryFiles();
}

struct SignalInfo *getCurrentEvent() {
    return current_signal;
}

void resetCurrentEvent() {
    current_signal = NULL;
}

int sendEvent(pid_t pid) {
    kill(pid, SIGUSR2);
    printf("Signal send to pid: %d\n", pid);
    return 0;
}

void initEvents() {
    saveDaemonPID(getpid());
    signal(SIGUSR1, HandleSignal);
    signal(SIGTERM, HandleKillSignal);
}
