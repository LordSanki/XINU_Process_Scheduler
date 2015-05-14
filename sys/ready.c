/* ready.c - ready */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
extern int global_sched_class;
/*------------------------------------------------------------------------
 * ready  --  make a process eligible for CPU service
 *------------------------------------------------------------------------
 */
int ready(int pid, int resch)
{
	register struct	pentry	*pptr;

	if (isbadpid(pid))
		return(SYSERR);
	pptr = &proctab[pid];
	pptr->pstate = PRREADY;
  switch(global_sched_class){
    case LINUXSCHED:
      insert(pid,rdyhead, pptr->eprio + pptr->quanta);
      break;
    case MULTIQSCHED:
      if(proctab[pid].rt == TRUE)
        insert(pid,rtrdyhead, pptr->quanta);
      else
        insert(pid,rdyhead, pptr->eprio + pptr->quanta);
      break;
    case DEFAULTSCHED:
    defalut:
      insert(pid,rdyhead, pptr->pprio);
      break;
  }
	if (resch)
		resched();
	return(OK);
}
