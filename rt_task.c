#include "dl_syscalls.h"
#include "rt_task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

static int g_log = 1;
static int duration;
static task_data_t task;
static task_result_t task_rst;
static int pure_overhead = 0;

void print_result_exit(struct timespec *t_now, task_data_t *p_task, task_result_t *p_rst)
{	
	struct timespec t_duration;
	int64_t i_duration;
	t_duration = timespec_sub(t_now, &p_rst->t_exit);
	i_duration = timespec_to_nsec(&t_duration) + (int64_t)duration * 1E9;
	p_rst->i_whole_duration = i_duration;
	printf("===begin===\nstart=\t%lld ns\nend=\t%lld ns\nduration=\t%lld ns\ncnt=\t%d\ncorrect_cnt=\t%d\nmiss_cnt=\t%d\t%3.2f%%\nmiss_cnt_after_middle=\t%d\t%3.2f%%\nthread_runtime=\t%lld ns\ncorrect_thread_runtime=\t%lld ns\navg_thread_runtime=\t%lldns\t%3.2f%%\t%3.2f%%\t%3.2f%%\n",
						timespec_to_nsec(&p_rst->t_exit) - (int64_t)(duration * 1E9),
						timespec_to_nsec(t_now),
						i_duration,
						p_rst->cnt,
						p_rst->correct_cnt,//(int64_t)(duration * 1E9) / timespec_to_nsec(&p_rst->dl_period),
						p_rst->miss_cnt,
						(double)p_rst->miss_cnt/(double)p_rst->cnt * 100.0,
						p_rst->miss_cnt_after_middle,
						(double)p_rst->miss_cnt_after_middle/(double)p_rst->cnt_after_middle * 100.0,
						p_rst->i_whole_thread_runtime,
						p_rst->i_corrent_whole_thread_runtime,
						p_rst->i_whole_thread_runtime / p_rst->cnt,
						(double)p_rst->i_whole_thread_runtime / p_rst->cnt / timespec_to_nsec(&p_rst->dl_exec) * 100.0,
						(double)p_rst->i_whole_thread_runtime / p_rst->cnt / timespec_to_nsec(&p_rst->dl_budget) * 100.0,
						(double)p_rst->i_whole_thread_runtime / p_rst->cnt / timespec_to_nsec(&p_rst->dl_period) * 100.0);

						

	exit(0);
	
}


void signal_handler(int signum)
{
		switch(signum)
		{
			case SIGALRM:
			{
				struct timespec t_alarm;
						clock_gettime(CLOCK_REALTIME, &t_alarm);
						printf("got SIGALARM at %lld ns\n", timespec_to_nsec(&t_alarm));
				break;
			}
			case SIGINT:
			{
				struct timespec t_now;
				clock_gettime(CLOCK_REALTIME, &t_now);
				print_result_exit(&t_now, &task, &task_rst);
				break;
			}
			default:
			{
				break;
			}
		}
}

static inline int busy_wait(struct timespec *to, struct timespec *dl)
{
	struct timespec t_step, t_wall;
	while(1)
	{
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_step);
		if (!timespec_lower(&t_step, to))
		{
			return 0;
		}	
		clock_gettime(CLOCK_REALTIME, &t_wall);
		if (!timespec_lower(&t_wall, dl))
		{
			return 1;
		}
	}
}

static inline void process_log(task_data_t *p_task, task_result_t *p_rst)
{
	static int first = 1;
	int64_t i_offset, i_diff, i_slack, i_thread_run, i_thread_remain;
	int after_middle;
	i_offset = timespec_to_nsec(&p_task->t_offset);
	i_diff = timespec_to_nsec(&p_task->t_diff);
	i_slack = timespec_to_nsec(&p_task->t_slack);
	i_thread_run = timespec_to_nsec(&p_task->t_thread_run);
	i_thread_remain = timespec_to_nsec(&p_task->t_thread_remain);

	after_middle = timespec_lower(&p_rst->t_middle, &p_task->t_begin);
	++p_rst->cnt;
	if (after_middle)
	{
		++p_rst->cnt_after_middle;
	}
	if (i_thread_remain > 0)
	{	
		++p_rst->lack_cnt;
		if (after_middle)
		{
			++p_rst->miss_cnt_after_middle;
		}
	}
	if (i_slack < 0)
	{
		++p_rst->miss_cnt;
	}
	p_rst->i_whole_thread_runtime += i_thread_run;
	
	if (first)
	{
		printf("%12s\t%12s\t%12s\t%12s\t%12s\n", "offset", "diff", "slack", "exec_time", "unfinished"); 
		first = 0;
	}
	if (g_log)
	{
		printf("%12lld\t%12lld\t%12lld\t%12lld\t%12lld\n",
					i_offset,
					i_diff,
					i_slack,
					i_thread_run,
					i_thread_remain);
	}
}

void bye(void)
{
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &task_rst.t_whole_thread_finish);
	task_rst.t_whole_thread_run = timespec_sub(&task_rst.t_whole_thread_finish, &task_rst.t_whole_thread_start);
	printf("thread_total= %lld ns\t%3.2f%%\t%3.2f%%\nthread_run= %lld ns\t%3.2f%%\t%3.2f%%\n===end===\n", 
			timespec_to_nsec(&task_rst.t_whole_thread_finish),
			(double)timespec_to_nsec(&task_rst.t_whole_thread_finish) / (double)timespec_to_nsec(&task_rst.dl_budget) / (double)task_rst.correct_cnt * 100.0,
			(double)timespec_to_nsec(&task_rst.t_whole_thread_finish) / (double)timespec_to_nsec(&task_rst.dl_period) / (double)task_rst.correct_cnt * 100.0,
			timespec_to_nsec(&task_rst.t_whole_thread_run),
			(double)timespec_to_nsec(&task_rst.t_whole_thread_run) / (double)timespec_to_nsec(&task_rst.dl_budget) / (double)task_rst.correct_cnt * 100.0,
			(double)timespec_to_nsec(&task_rst.t_whole_thread_run) / (double)timespec_to_nsec(&task_rst.dl_period) / (double)task_rst.correct_cnt * 100.0);
}

int main(int argc, char* argv[])
{	
	pid_t pid;
	long period, budget, exec;
	char* token;
	struct sched_attr dl_attr;
	int ret;
	unsigned int flags = 0;
	struct sigaction sa;
	struct itimerval timer;

	memset(&task, 0, sizeof(task));
	memset(&task_rst, 0, sizeof(task_rst));

	ret = atexit(bye);
	if (ret != 0)
	{
		perror("atexit");
		exit(1);
	}
	if (argc >= 3)
	{
		pid = getpid();
		printf("%d\t===pid===\n", (int)pid);

		printf("lock pages in memory\n");
		ret = mlockall(MCL_CURRENT | MCL_FUTURE);
		if (ret < 0)
		{
			perror("mlockall");
			exit(1);
		}
	
		token = strtok(argv[1], ":");
		period = strtol(token, NULL, 10);
		token = strtok(NULL, ":");
		budget = strtol(token, NULL, 10);
		token = strtok(NULL, ":");
		exec = strtol(token, NULL, 10);
		printf("period = %ld us, budget = %ld us, exec = %ld us\n", period, budget, exec);
	
		if (exec == 0)
		{
			pure_overhead = 1;
		}
		else
		{
			pure_overhead = 0;
		}
		//duration is a must
		duration = atoi(argv[2]);
		clock_gettime(CLOCK_REALTIME, &task_rst.t_exit);
		memcpy((void*)&task_rst.t_middle, (void*)&task_rst.t_exit, sizeof(task_rst.t_middle));
		task_rst.t_exit.tv_sec = task_rst.t_exit.tv_sec + duration;
		task_rst.t_middle.tv_sec = task_rst.t_middle.tv_sec + duration / 2;//set halfway timestamp

		if (argc >= 4)
		{
			g_log = atoi(argv[3]);
		}

		//set deadline scheduling
		pid = 0;

		assert(period >= budget && budget >= exec);
		task_rst.dl_period = usec_to_timespec(period);
		task_rst.dl_budget = usec_to_timespec(budget);
		task_rst.dl_exec = usec_to_timespec(exec);
		dl_attr.size = sizeof(dl_attr);
		dl_attr.sched_flags = 0;
		dl_attr.sched_policy = SCHED_DEADLINE;
		dl_attr.sched_priority = 0;
		dl_attr.sched_runtime = timespec_to_nsec(&task_rst.dl_budget);
		dl_attr.sched_deadline = timespec_to_nsec(&task_rst.dl_period);
		dl_attr.sched_period = timespec_to_nsec(&task_rst.dl_period);
	
		task_rst.correct_cnt =(int)((int64_t)(duration * 1E9) / timespec_to_nsec(&task_rst.dl_period));
		task_rst.i_corrent_whole_thread_runtime = (int64_t)task_rst.correct_cnt * timespec_to_nsec(&task_rst.dl_exec);

		ret = sched_setattr(pid, &dl_attr, flags);
		if (ret != 0)
		{
			perror("sched_setattr");
			exit(1);
		}
#if 0
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = &signal_handler;
		sigaction(SIGALRM, &sa, NULL);
		sigaction(SIGINT, &sa, NULL);
		timer.it_value.tv_sec = dl_period.tv_sec;
		timer.it_value.tv_usec = dl_period.tv_nsec / 1000;
		timer.it_interval.tv_sec = dl_period.tv_sec;
		timer.it_interval.tv_usec = dl_period.tv_nsec / 1000;
		setitimer(ITIMER_REAL, &timer, NULL);
#endif
		struct timespec t_exec;

		clock_gettime(CLOCK_REALTIME, &task.t_period);
		task.t_period = timespec_add(&task.t_period, &task_rst.dl_period);//start from next period 
		task.t_deadline = timespec_add(&task.t_period, &task_rst.dl_period);
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &task.t_period, NULL);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &task_rst.t_whole_thread_start);	
		while(1)
		{
			clock_gettime(CLOCK_REALTIME, &task.t_begin);
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &task.t_thread_start);
			t_exec = timespec_add(&task.t_thread_start, &task_rst.dl_exec);
			if (!pure_overhead)
			{
				busy_wait(&t_exec, &task.t_deadline);
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &task.t_thread_finish);
			clock_gettime(CLOCK_REALTIME, &task.t_end);


			task.t_thread_run = timespec_sub(&task.t_thread_finish, &task.t_thread_start);
			task.t_thread_remain = timespec_sub(&t_exec, &task.t_thread_finish);
			task.t_diff = timespec_sub(&task.t_end, &task.t_begin);
			task.t_slack = timespec_sub(&task.t_deadline, &task.t_end);
			task.t_offset = timespec_sub(&task.t_begin, &task.t_period);
			process_log(&task, &task_rst);

			task.t_period = timespec_add(&task.t_period, &task_rst.dl_period);
			task.t_deadline = timespec_add(&task.t_deadline, &task_rst.dl_period);

			//check total test time
			struct timespec t_now;
			clock_gettime(CLOCK_REALTIME, &t_now);
			if (duration && timespec_lower(&task_rst.t_exit, &t_now))
			{
				print_result_exit(&t_now, &task, &task_rst);
								
			}

			clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &task.t_period, NULL);
		}

	}
	else
	{
		printf("usage: rt_task <period>:<budget>:<exec> <duration> [<log_switch>]\n");
	}
	return 0;
}
