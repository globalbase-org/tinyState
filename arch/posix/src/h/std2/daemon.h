

#ifndef ___DAEMON_H___
#define ___DAEMON_H___

#ifdef __cplusplus
extern "C" {
#endif


int
launch_daemon(int argc,char ** argv);
void
shutdown_daemon();


#ifdef __cplusplus
}
#endif

#endif

