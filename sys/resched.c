/* resched.c  -  resched */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <stdio.h>


#define RTQ_SELECT_PERCENT 70
#define RAND_100 (rand()%100)
#define RTP_QUANTA 100

unsigned long currSP;	/* REAL sp of current process */
int global_sched_class = DEFAULTSCHED;
extern int ctxsw(int, int, int, int);

static int default_sched();
static int linuxsched();
static int multiqsched();

static void epoch_rtq();
static void epoch_nq();
static int sched_null_proc(struct pentry *optr);
static Bool sched_rtq(struct pentry *optr);
static Bool sched_nq(struct pentry *optr);
static void cleanq();
static Bool continue_proc(struct pentry *optr);

/*-----------------------------------------------------------------------
 * resched  --  reschedule processor to highest priority ready process
 *
 * Notes:	Upon entry, currpid gives current process id.
 *		Proctab[currpid].pstate gives correct NEXT state for
 *			current process if other than PRREADY.
 *------------------------------------------------------------------------
 */
int resched()
{
  switch(global_sched_class)
  {
    case LINUXSCHED:
      linuxsched();
      break;
    case MULTIQSCHED:
      multiqsched();
      break;
    case DEFAULTSCHED:
    default:
      default_sched();
      break;
  }
#ifdef	RTCLOCK
    preempt = QUANTUM;		/* reset preemption counter	*/
#endif
  return OK;
}

void setschedclass(int sched_class)
{
  global_sched_class = sched_class;
}

int getschedclass()
{
  return global_sched_class;
}

static int default_sched()
{
	register struct	pentry	*optr;	/* pointer to old process entry */
	register struct	pentry	*nptr;	/* pointer to new process entry */

	/* no switch needed if current process priority higher than next*/

	if ( ( (optr= &proctab[currpid])->pstate == PRCURR) &&
	   (lastkey(rdytail)<optr->pprio)) {
		return(OK);
	}
	
	/* force context switch */

	if (optr->pstate == PRCURR) {
		optr->pstate = PRREADY;
		insert(currpid,rdyhead,optr->pprio);
	}

	/* remove highest priority process at end of ready list */

	nptr = &proctab[ (currpid = getlast(rdytail)) ];
	nptr->pstate = PRCURR;		/* mark it currently running	*/
#ifdef	RTCLOCK
	preempt = QUANTUM;		/* reset preemption counter	*/
#endif
	
	ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	
	/* The OLD process returns here when resumed. */
	return OK;
}

static int linuxsched()
{
	register struct	pentry	*optr;	/* pointer to old process entry */
  optr = &proctab[currpid];

  /* continue with same proc if quanta left*/
  if(continue_proc(optr) == TRUE){ return OK; }

  /* schedule next proc in queue*/
  if(sched_nq(optr) == TRUE){ return OK; }
  
  /* new EPOCH begins as no runnable proc left*/
  epoch_nq();

  /* schedule next proc in queue*/
  if(sched_nq(optr) == TRUE){ return OK; }

  /* run PRNULL if no proc left to run*/
  return sched_null_proc(optr);
}

int multiqsched()
{
  register struct	pentry	*optr;	/* pointer to old process entry */
  optr = &proctab[currpid];

  /* reducing quanta of currproc and checking if it is to be scheduled again*/
  if(continue_proc(optr) == TRUE){ return OK; }

  if(optr->rt == TRUE){ /* RTQ was selected for this EPOCH */
    if(sched_rtq(optr) == TRUE){ return OK; } /* Selecting a proc from RTQ*/
  }
  else{ /* NQ was selected for this EPOCH */
    if(sched_nq(optr) == TRUE){ return OK; } /* Selecting proc from NQ */
  }
  if (RTQ_SELECT_PERCENT > RAND_100 ){/* Random func selects RTQ */
    epoch_rtq(); /* filling up RTQ */
    if(sched_rtq(optr) == TRUE){ return OK; }/* selecting proc from rtq */
    epoch_nq(); /* filling up nq */
    if(sched_nq(optr) == TRUE){ return OK; }/* selecting proc from nq */
    return sched_null_proc(optr); /* selecting PRNULL */
  }
  else{ /* Random func selects NQ */
    epoch_nq(); /* filling up nq */
    if(sched_nq(optr) == TRUE){ return OK; }/* selecting proc from nq */
    epoch_rtq();/* filling up RTQ */
    if(sched_rtq(optr) == TRUE){ return OK; }/* selecting proc from rtq */
    return sched_null_proc(optr); /* selecting PRNULL */
  }
  return OK;
}

static void epoch_nq()
{
  int pid;
  cleanq(rdyhead, rdytail);
  for(pid =1; pid <NPROC; pid++)
  {
    if(proctab[pid].rt == FALSE) {
      if(proctab[pid].pstate != PRFREE){
        proctab[pid].eprio = proctab[pid].pprio;
        proctab[pid].quanta = proctab[pid].quanta/2 + proctab[pid].eprio;
      }
      if(proctab[pid].pstate == PRREADY){
        insert(pid, rdyhead, proctab[pid].quanta + proctab[pid].eprio);
      }
    }
  }
}

static void epoch_rtq()
{
  int pid;
  cleanq(rtrdyhead, rtrdytail);
  for(pid =1; pid <NPROC; pid++)
  {
    if(proctab[pid].rt == TRUE){
      if(proctab[pid].pstate != PRFREE){
        proctab[pid].quanta = RTP_QUANTA;
      }
      if(proctab[pid].pstate == PRREADY){
        insert(pid, rtrdyhead, proctab[pid].quanta);
      }
    }
  }
}

/* Check if curr proc can be rescheduled */
static Bool continue_proc(struct pentry *optr)
{
  /* reduce quanta of curr proc*/
  optr->quanta--;
  if(optr->quanta <= 0){ optr->quanta = 0; }

  /* check if proc is still active */
  if (PRCURR == optr->pstate){
    /* if proc still has quanta left then resched it*/
    if(optr->quanta > 0 )
      return TRUE;
    else /* else put it in ready state*/
      optr->pstate = PRREADY;
  }
  return FALSE;
}

static void cleanq(int head, int tail)
{
  while(nonempty(head)){
    if(lastkey(tail) <= 0 )
      getlast(tail);
    else
      break;
  }
}

/* Try to schedule a process from RTQ */
static Bool sched_rtq(struct pentry *optr)
{
  struct pentry * nptr;
  cleanq(rtrdyhead, rtrdytail);
  
  if(nonempty(rtrdyhead)){ //schedule proc from RTQ if available
    nptr = &proctab[ (currpid = getlast(rtrdytail)) ];
    nptr->pstate = PRCURR;
    if(nptr != optr)
      ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
    return TRUE;
  }
  return FALSE;
}

/* Try to schedule a process from NQ */
static Bool sched_nq(struct pentry* optr)
{
  struct pentry * nptr;
  cleanq(rdyhead, rdytail);
  
  if(nonempty(rdyhead)){
    nptr = &proctab[ (currpid = getlast(rdytail)) ];
    nptr->pstate = PRCURR;
    if(nptr != optr)
      ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
    return TRUE;
  }      
  return FALSE;
}

static int sched_null_proc(struct pentry * optr)
{
  // now running NULL proc
  struct pentry * nptr = &proctab[ (currpid = 0) ];
  nptr->quanta = 1;
  nptr->eprio = 0;
  nptr->pstate = PRCURR;
  if(nptr != optr)
    ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
  return OK;
}


