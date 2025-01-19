#ifndef DISKANALYZER_SHMMANAGER_H
#define DISKANALYZER_SHMMANAGER_H

int initTasks();
int *getNextTask();
int *get_TaskInfo(int);

void initMutex();
pthread_mutex_t* GetMutex();

void closeShm(void *, int);

#endif //DISKANALYZER_SHMMANAGER_H
