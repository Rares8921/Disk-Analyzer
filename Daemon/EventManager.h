#ifndef DISKANALYZER_EVENTMANAGER_H
#define DISKANALYZER_EVENTMANAGER_H

#include "Utility.h"

void initEvents();
struct SignalInfo *getCurrentEvent();
void resetCurrentEvent();
int sendEvent(pid_t);

void writeDaemonStatus(char *);

#endif //DISKANALYZER_EVENTMANAGER_H
