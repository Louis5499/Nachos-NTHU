/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__ 
#define __USERPROG_KSYSCALL_H__ 

#include "kernel.h"

#include "synchconsole.h"


void SysHalt()
{
  kernel->interrupt->Halt();
}

void SysPrintInt(int val)
{ 
  DEBUG(dbgTraCode, "In ksyscall.h:SysPrintInt, into synchConsoleOut->PutInt, " << kernel->stats->totalTicks);
  kernel->synchConsoleOut->PutInt(val);
  DEBUG(dbgTraCode, "In ksyscall.h:SysPrintInt, return from synchConsoleOut->PutInt, " << kernel->stats->totalTicks);
}

int SysAdd(int op1, int op2)
{
  return op1 + op2;
}

int SysCreate(char *filename)
{
    // return value
    // 1: success
    // 0: failed
    return kernel->fileSystem->Create(filename);
}

OpenFileId SysOpen(char * filename) {
    DEBUG(dbgTraCode, "In ksyscall.h:SysOpen." << kernel->stats->totalTicks);
    int fd = kernel->fileSystem->OpenAFile(filename);
    DEBUG(dbgTraCode, "In ksyscall.h:OpenAFile Completed." << kernel->stats->totalTicks);

    return fd;    
}

int SysWrite(char * buffer, int size, OpenFileId id) {
    DEBUG(dbgTraCode, "In ksyscall.h:SysWrite." << kernel->stats->totalTicks);
    int count = kernel->fileSystem->WriteFile(buffer, size, id);
    DEBUG(dbgTraCode, "In ksyscall.h:WriteFile Completed." << kernel->stats->totalTicks);

    return count;
}

int SysRead(char * buffer, int size, OpenFileId id) {
    DEBUG(dbgTraCode, "In ksyscall.h:SysRead." << kernel->stats->totalTicks);
    int count = kernel->fileSystem->ReadFile(buffer, size, id);
    DEBUG(dbgTraCode, "In ksyscall.h:ReadFile Completed." << kernel->stats->totalTicks);

    return count;
}

int SysClose(OpenFileId id) {
    DEBUG(dbgTraCode, "In ksyscall.h:SysClose." << kernel->stats->totalTicks);
    int success = kernel->fileSystem->CloseFile(id);
    DEBUG(dbgTraCode, "In ksyscall.h:CloseFile Completed." << kernel->stats->totalTicks);

    return success;
}

#endif /* ! __USERPROG_KSYSCALL_H__ */

