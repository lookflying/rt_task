#ifndef RT_TASK_H
#define RT_TASK_H
#include <time.h>
#include <stdint.h>
typedef struct task_data
{
	struct timespec t_period;
	struct timespec t_deadline;
	struct timespec t_begin;
	struct timespec t_end;
	struct timespec t_diff;
	struct timespec t_offset;
	struct timespec t_slack;
	struct timespec t_thread_start;
	struct timespec t_thread_finish;
	struct timespec t_thread_run;
	struct timespec t_thread_remain;
} task_data_t;

typedef struct task_result
{
	struct timespec t_whole_thread_start;
	struct timespec t_whole_thread_finish;
	struct timespec t_whole_thread_run;
	struct timespec dl_period;
	struct timespec dl_budget;
	struct timespec dl_exec;
	struct timespec t_exit;
	struct timespec t_middle;
	int cnt;
	int correct_cnt;
	int lack_cnt;
	int miss_cnt;
	int miss_cnt_after_middle;
	int cnt_after_middle;
	int64_t i_whole_thread_runtime; //total exec thread time, mainly in busy_wait
	int64_t i_corrent_whole_thread_runtime;//exec thread time according to paramters
	int64_t i_whole_duration;//wall clock duration of the whole program
 
} task_result_t;

int64_t timespec_to_nsec(struct timespec *ts)
{
	return ts->tv_sec * 1E9 + ts->tv_nsec;
}


struct timespec usec_to_timespec(unsigned long usec)
{
	struct timespec ts;

	ts.tv_sec = usec / 1000000;
	ts.tv_nsec = (usec % 1000000) * 1000;
	
	return ts;
}

struct timespec msec_to_timespec(unsigned int msec)
{
	struct timespec ts;

	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000000;

	return ts;
}

struct timespec timespec_add(struct timespec *t1, struct timespec *t2)
{
	struct timespec ts;

	ts.tv_sec = t1->tv_sec + t2->tv_sec;
	ts.tv_nsec = t1->tv_nsec + t2->tv_nsec;

	while (ts.tv_nsec >= 1E9) {
		ts.tv_nsec -= 1E9;
		ts.tv_sec++;
	}

	return ts;
}

struct timespec timespec_sub(struct timespec *t1, struct timespec *t2)
{
	struct timespec ts;
	
	if (t1->tv_nsec < t2->tv_nsec) {
		ts.tv_sec = t1->tv_sec - t2->tv_sec -1;
		ts.tv_nsec = t1->tv_nsec  + 1000000000 - t2->tv_nsec; 
	} else {
		ts.tv_sec = t1->tv_sec - t2->tv_sec;
		ts.tv_nsec = t1->tv_nsec - t2->tv_nsec; 
	}

	return ts;

}

int timespec_lower(struct timespec *what, struct timespec *than)
{
	if (what->tv_sec > than->tv_sec)
		return 0;

	if (what->tv_sec < than->tv_sec)
		return 1;

	if (what->tv_nsec < than->tv_nsec)
		return 1;

	return 0;
}


#endif
