#include <stdio.h>
#include <stdint.h>
#include <time.h>
#ifdef __unix__
#include <pthread.h>
#endif
#include "machine_protos.h"
#include "../cpu/cpu_protos.h"

/* Background run loop for native builds */
#ifdef __unix__
static void* run_thread(void* arg) {
	Nd500Machine* m = (Nd500Machine*)arg;
	while (m->run_flag) {
		if (m->cpu) nd500_cpu_step(m->cpu);
		/* Simple throttle to avoid busy looping */
		struct timespec ts = {0, 1000000}; /* 1ms */
		nanosleep(&ts, NULL);
	}
	return NULL;
}

void nd500_dbg_run(Nd500Machine* m) {
	if (!m) return;
	if (m->run_flag) return;
	m->run_flag = 1;
	pthread_t t;
	(void)pthread_create(&t, NULL, run_thread, m);
	(void)pthread_detach(t);
}

void nd500_dbg_stop(Nd500Machine* m) {
	if (!m) return;
	m->run_flag = 0;
}
#endif


