/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2021. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#pragma once
#include "VsamFile.h"
#include <queue>
#include <thread>

int gettid();
void vsamThread(VsamFile *pVsamFile, std::condition_variable *pcv,
                std::mutex *pmtx, std::queue<ST_VsamThreadMsg *> *pqueue);
