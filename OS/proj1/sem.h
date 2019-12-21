/*
 sem.h: CS1550 semaphore helper library 
 Haoyue Cui(hac113)
*/

#ifndef SEM_H_INCLUDED
#define SEM_H_INCLUDED

//make Node struct which connects the next node and record the task
struct Node{
	struct Node* next;
	struct task_struct* status;
};

//record the head and tail to ensure easily enqueue and dequeue
//int value to record semaphore
struct cs1550_sem {
	int value;
	struct Node* head;
	struct Node* tail;
};
#endif
