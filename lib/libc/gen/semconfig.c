/*
 *	$Id: semconfig.c,v 1.1 1993/11/14 12:40:30 cgd Exp $
 */     

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#if __STDC__
int semconfig(int cmd, int p1, int p2, int p3)
#else
int semctl(cmd, p1, p2, p3)
	int cmd, p1, p2, p3;
#endif
{
	return (semsys(3, cmd, p1, p2, p3));
}
