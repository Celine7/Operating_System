//Haoyue Cui (hac113）
#include "spinlock.h"
	struct condvar {
	struct spinlock lk;
};
