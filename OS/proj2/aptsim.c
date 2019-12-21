//Haoyue Cui(hac113)
#include <sys/mman.h>
#include <linux/unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include "sem.h"
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

struct options{
  int num_tenants;
  int num_agents;
  int probability_tenant;
  int delay_tenant;
  int seed_tenant;
  int probability_agent;
  int delay_agent;
  int seed_agent;
};

struct cs1550_sem *tn_arrives;
struct cs1550_sem *ag_allow_in;
struct cs1550_sem *apt_open;
struct cs1550_sem *apt_empty;
struct cs1550_sem *mutex;
int *visitors;
time_t *start;

// returns 1 if the two strings are equal, and 0 otherwise.
int streq(const char* a, const char* b) {
	return strcmp(a, b) == 0;
}
// generate a random number
int rand_gen(int min, int max) {
    return((rand() % (max + 1 - min)) + min);
}

void tenantArrives(int tenants){
  up(tn_arrives);
  printf("Tenant %d arrives at time %d.\n", tenants,(int)(time(NULL)-*start));
  down(apt_open);
  down(mutex);
  *visitors+=1;
  up(mutex);
}

void agentArrives(int agents){
  printf("Agent %d arrives at time %d.\n", agents,(int)(time(NULL)-*start));
  down(ag_allow_in);
  down(tn_arrives);
}

void viewApt(int tenants){
  printf("Tenant %d inspects the apartment at time %d.\n", tenants, (int)(time(NULL)-*start));
  sleep(2);
}

void openApt(int agents){
  printf("Agent %d opens the apartment for inspection at time %d.\n", agents, (int)(time(NULL)-*start));
  int i = 0;
  for(;i<10;i++){
    up(apt_open);
  }
  down(apt_empty);
}


void tenantLeaves(int tenants){
  printf("Tenant %d leaves the apartment at time %d\n", tenants, (int)(time(NULL)-*start));
  down(mutex);
  *visitors-=1;
  if(*visitors == 0)
    up(apt_empty);
  up(mutex);
  down(tn_arrives);
}

void agentLeaves(int agents){
  down(mutex);
  if(apt_open->value > 0)
    apt_open->value = 0;
  up(mutex);
  up(ag_allow_in);
  up(tn_arrives);
  printf("Agent %d leaves the apartment at time %d\n", agents, (int)(time(NULL)-*start));
}

int main(int argc, char** argv) {
  printf("The apartment is now empty.\n");
  struct options opt;
  int i = 0;
    for(; i < argc; i++){
      if(streq(argv[i],"-m")==1) opt.num_tenants = atoi(argv[i+1]);
      else if(streq(argv[i],"-k")==1) opt.num_agents = atoi(argv[i+1]);
      else if(streq(argv[i],"-pt")==1) opt.probability_tenant = atoi(argv[i+1]);
      else if(streq(argv[i],"-dt")==1) opt.delay_tenant = atoi(argv[i+1]);
      else if(streq(argv[i],"-st")==1) opt.seed_tenant = atoi(argv[i+1]);
      else if(streq(argv[i],"-pa")==1) opt.probability_agent = atoi(argv[i+1]);
      else if(streq(argv[i],"-da")==1) opt.delay_agent = atoi(argv[i+1]);
      else if(streq(argv[i],"-sa")==1) opt.seed_agent = atoi(argv[i+1]);
    }
    //Allocate a shared memory region to store five semaphores and one integer and one time_t
    struct cs1550_sem *sems = mmap(NULL, sizeof(struct cs1550_sem)*5+sizeof(int)+sizeof(time_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
    memset(sems, sizeof(struct cs1550_sem)*5+sizeof(int)+sizeof(time_t), 0);

    //pointer arithmetic
    tn_arrives = (struct cs1550_sem*)sems;
    ag_allow_in = tn_arrives+1;
    apt_open = ag_allow_in+1;
    apt_empty = apt_open+1;
    mutex = apt_empty+1;
    visitors = (int *)(mutex + 1);
    start = (time_t *)(visitors+1);

    //initialize the value
    tn_arrives->value = 0;
    ag_allow_in->value = 1;
    apt_open->value = 0;
    apt_empty->value = 0;
    mutex->value = 1;
    *visitors = 0;

    // Allocate an array for storing tenant and agent ids
    pid_t *pids = (void *) mmap(NULL, sizeof(pid_t) * 2, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
    memset(pids, sizeof(pid_t) * 2, 0);

    //record the start time
    *start = time(NULL);

    //Create two tenant and agent processes
    int pid = fork(); // Create the first child tenant process
    if (pid == 0) {
        pids[0] = getpid();
        //generate a seed for rand()
        time_t seeder = opt.seed_agent;
        srand(time(&seeder));
        // Launch tenant processes
        int j = 1;
        for(; j <= opt.num_tenants; j++) {
            if(pids[0] == getpid()) {
                pid = fork();
                //determine whether if tenants should wait except the first tenant
                if(j != 1){
                  int random_number = rand_gen(0, 100);
                  if(random_number %100 > opt.probability_tenant){
                    sleep(opt.delay_tenant);
                  }
                }
                if(pid == 0) {
                    tenantArrives(j);
                    viewApt(j);
                    tenantLeaves(j);
                    exit(0);
                }
            }
        }
        if(pids[1] == getpid()) {
          int k = 0;
            for(; k < opt.num_agents; k++) {
                wait(NULL);
            }
        }
    }
    else {
        pid = fork(); // Create a second child process
        if(pid == 0) {
            pids[1] = getpid();
            //generate a seed for rand()
            time_t seeder = opt.seed_agent;
            srand(time(&seeder));
            // Launch agent processes
            int p = 1;
            for(; p <= opt.num_agents; p++) {
                if(pids[1] == getpid()) {
                    pid = fork();
                    //determine whether if agents should wait except the first agent
                    if(p != 1){
                      int random_number = rand_gen(0, 100);
                      if(random_number %100 > opt.probability_agent){
                        sleep(opt.delay_agent);
                      }
                    }
                    if(pid == 0) {
                        agentArrives(p);
                        openApt(p);
                        agentLeaves(p);
                    }
                }
            }
            if(pids[1] == getpid()) {
              int x = 0;
                for(; x < opt.num_agents; x++) {
                    wait(NULL);
                }
            }
        }
        else
        {
            sleep(1);
            wait(NULL);
            wait(NULL);
        }
    }
  return 0;
}
