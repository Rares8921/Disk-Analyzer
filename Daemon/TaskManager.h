#ifndef DISKANALYZER_TASKMANAGER_H
#define DISKANALYZER_TASKMANAGER_H

#include "Utility.h"
#include <openssl/md5.h>

typedef struct {
    char hash[33];
    char filepath[1024];
} FileHash;

void addTask();
void updateTasks();
void finishAllTasks();

int processSignal(struct SignalInfo);

#endif //DISKANALYZER_TASKMANAGER_H
