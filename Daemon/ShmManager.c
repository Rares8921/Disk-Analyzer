#include "Utility.h"
#include "ShmManager.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

static int shmFD_counter, shmFD_TaskInfo;
pthread_mutex_t mtx;

int createSHM(char shmName[], int *shmFD, int size) {
    *shmFD = shm_open(shmName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if ((*shmFD) < 0) {
        perror("Failed to create SHM object!");
        return errno;
    }
    if (ftruncate(*shmFD, getpagesize() * size) == -1) {
        perror("Failed to resize SHM object!");
        shm_unlink(shmName);
        return errno;
    }
    return 0;
}

void *getSHM(char name[], int shmFD, int offset, int len) {
    void *shmPtr = mmap(0, len, PROT_WRITE | PROT_READ, MAP_SHARED, shmFD, getpagesize() * offset);
    if (shmPtr == MAP_FAILED) {
        perror("Failed to access SHM object!");
        shm_unlink(name);
        return NULL;
    }
    return shmPtr;
}

int initTasks() {
    createSHM("TaskCounter", &shmFD_counter, 1);
    createSHM("TaskInfo", &shmFD_TaskInfo, DA_MAX_PROCESSES);

    int *taskCounter = getNextTask();
    if (taskCounter == NULL) {
        return errno;
    }
    *taskCounter = 0;
    return 0;
}

int *getNextTask() {
    void *taskCounter = getSHM("TaskCounter", shmFD_counter, 0, sizeof(int));
    return taskCounter;
}

int *get_TaskInfo(int id) {
    void *TaskInfo = getSHM("TaskInfo", shmFD_TaskInfo, id, sizeof(int));
    return TaskInfo;
}

void initMutex() {
    pthread_mutex_init(&mtx, NULL);
}

pthread_mutex_t* GetMutex() {
    return &mtx;
}

void closeShm(void *shmPtr, int len) {
    int ret = munmap(shmPtr, len);
    if (ret == -1) {
        perror("munmap failed");
        return;
    }
}