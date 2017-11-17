#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__

// Kernel implementations of syscalls

extern int kernelFork(void);
extern int kernelExec(char *filename, char **argvec);
extern void kernelExit(int status, UserContext* ctx);
extern int kernelWait(int *status_ptr, UserContext* ctx);
extern int kernelGetPid(void);
extern int kernelBrk(void *addr);
extern int kernelDelay(int clock_ticks, UserContext* ctx);
extern int kernelTtyRead(int tty_id, void *buf, int len);
extern int kernelTtyWrite(int tty_id, void *buf, int len, UserContext* ctx);
extern int kernelPipeInit(int *pipe_idp);
extern int kernelPipeRead(int pipe_id, void *buf, int len);
extern int kernelPipeWrite(int pipe_id, void *buf, int len);
extern int kernelLockInit(int *lock_idp);
extern int kernelAcquire(int lock_id, UserContext* ctx);
extern int kernelRelease(int lock_id);
extern int kernelCvarInit(int *cvar_idp);
extern int kernelCvarSignal(int cvar_id);
extern int kernelCvarBroadcast(int cvar_id);
extern int kernelCvarWait(int cvar_id, int lock_id, UserContext* ctx);
extern int kernelReclaim(int id);
extern int kernelPS(int tty_id);

#endif
