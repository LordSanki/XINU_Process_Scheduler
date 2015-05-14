#include <conf.h>
#include <kernel.h>
//#include <sched.h>
#include <stdio.h>

#define LOOP 100

int prA, prB, prC, prD, prE, prF, prG, prH;
int proc(char c);

int main() {
  int i;
  int count = 0;
  char buf[8];

  for(i = 0; i < 10; i++){
    srand(i);
    printf("%d\n", rand());
  }

  /* Linux like Scheduler */
  setschedclass(MULTIQSCHED);
  resume(prA = create(proc, 2000, 5, "proc A", 1, 'A'));
  resume(prD = createReal(proc, 2000, 5, "proc D", 1, 'D'));
  resume(prB = create(proc, 2000, 50, "proc B", 1, 'B'));
  resume(prE = createReal(proc, 2000, 5, "proc E", 1, 'E'));
  resume(prC = create(proc, 2000, 90, "proc C", 1, 'C'));
  resume(prF = createReal(proc, 2000, 5, "proc F", 1, 'F'));
  while (count++ < LOOP) {
    kprintf("M");
    for (i = 0; i < 100000000; i++)
      ;
    if(count == 20) {
      kprintf("\n");
      chprio(48,95);
    }
    if(count == 5) {
      kprintf("\n");
      resume(prG = create(proc, 2000, 50, "proc G", 1, 'G'));
      resume(prH = createReal(proc, 2000, 20, "proc H", 1, 'H'));
    }
  }
  return 0;
}

int proc(char c) {
  int i;
  int count = 0;

  while (count++ < LOOP) {
    kprintf("%c", c);
    for (i = 0; i < 100000000; i++)
      ;
  }
  return 0;
}

