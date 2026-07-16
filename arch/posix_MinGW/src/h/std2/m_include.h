


#ifndef ____M_INCLUDE_H___
#define ____M_INCLUDE_H___

/*
 * MinGW variant. The POSIX overlays pull <pty.h> here; MinGW has no pty layer
 * (DM_TTY is stubbed in the posix_MinGW ts2System overlay), so this is empty
 * apart from the guard. Windows-port design memo.
 */

#endif
