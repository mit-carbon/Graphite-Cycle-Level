#ifndef THREAD_SUPPORT_PRIVATE_H
#define THREAD_SUPPORT_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

void CarbonThreadStart(core_id_t core_id);
void CarbonThreadExit();
ThreadSpawnRequest CarbonDequeueThreadSpawnReq();

void* CarbonManagedThread(void*);
void CarbonPthreadCreate(pthread_t* thread);
void CarbonPthreadAttrInitOtherAttr(pthread_attr_t *attr);

#ifdef __cplusplus
}
#endif

#endif
