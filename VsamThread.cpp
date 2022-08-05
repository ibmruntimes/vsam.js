/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2020, 2021. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 */
#include "VsamThread.h"

#include <assert.h>

#ifdef DEBUG
static const char *getMessageStr(VSAM_THREAD_MSGID msgid) {
  switch (msgid) {
  case MSG_OPEN: return "OPEN";
  case MSG_CLOSE: return "CLOSE";
  case MSG_WRITE: return "WRITE";
  case MSG_UPDATE: return "UPDATE";
  case MSG_DELETE: return "DELETE";
  case MSG_FIND: return "FIND";
  case MSG_READ: return "READ";
  case MSG_FIND_UPDATE: return "FIND_UPDATE";
  case MSG_FIND_DELETE: return "FIND_DELETE";
  case MSG_EXIT: return "EXIT";
  default: return "UNKNOWN";
  }
  assert(0);
}
#endif

int gettid() { return (int)(pthread_self().__ & 0x7fffffff); }

void vsamThread(VsamFile *pVsamFile, std::condition_variable *pcv,
                std::mutex *pmtx, std::queue<ST_VsamThreadMsg *> *pqueue) {
#ifdef DEBUG
  fprintf(stderr, "vsamThread tid=%d started.\n", gettid());
#endif
  ST_VsamThreadMsg *pmsg;
  while (1) {
    std::unique_lock<std::mutex> lck(*pmtx);
    while (pqueue->empty())
      pcv->wait(lck);

    if (pqueue->empty())
      continue;
    pmsg = pqueue->front();
    pqueue->pop();

#if defined(DEBUG) || defined(DEBUG_CRUD)
    fflush(stderr);
    fflush(stdout);
    fprintf(stderr, "vsamThread tid=%d got message %s.\n", gettid(),
            getMessageStr(pmsg->msgid));
#endif
    switch (pmsg->msgid) {
    case MSG_OPEN:
    case MSG_CLOSE:
    case MSG_WRITE:
    case MSG_UPDATE:
    case MSG_DELETE:
    case MSG_FIND:
    case MSG_READ:
    case MSG_FIND_UPDATE:
    case MSG_FIND_DELETE:
      (pVsamFile->*(pmsg->pWorkFunc))(pmsg->pdata);
      pmsg->rc = pmsg->pdata->rc_;
      if (pmsg->msgid == MSG_CLOSE) {
        pVsamFile->detachVsamThread();
        pmsg->cv.notify_one();
#ifdef DEBUG
        fprintf(stderr, "vsamThread tid=%d terminating after message CLOSE.\n",
                gettid());
#endif
        return;
      }
      pmsg->cv.notify_one();
      break;
    case MSG_EXIT:
      pmsg->rc = 0;
      pVsamFile->detachVsamThread();
      pmsg->cv.notify_one();
#ifdef DEBUG
      fprintf(stderr, "vsamThread tid=%d terminating after message EXIT.\n",
              gettid());
#endif
      return;
    default:
#ifdef DEBUG
      fprintf(stderr,
              "vsamThread tid=%d terminating after message UNKNOWN (%d)",
              gettid(), pmsg->msgid);
#endif
      pVsamFile->detachVsamThread();
      pmsg->cv.notify_one();
      assert(0);
    }
  }
}
