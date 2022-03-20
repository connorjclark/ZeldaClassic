#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#include <stdio.h>

#ifdef __EMSCRIPTEN
#include <emscripten.h>
#endif

#ifdef LINUX
#include <unistd.h>
#endif
#ifdef WINDOWS
// #include <windows.h>
#endif

void wait(int ms)
{
#ifdef LINUX
    sleep(ms);
#endif
#ifdef WINDOWS
    Sleep(ms);
#endif
}

pthread_mutex_t mutex;
pthread_cond_t cond;
int pending_jobs = 0;

// I consume jobs.
void *thread_a_fn()
{
	printf("thread_a start\n");

	while (1) {
		pthread_mutex_lock(&mutex);
		while (!pending_jobs && pthread_cond_wait(&cond, &mutex));
		while (pending_jobs) {
			printf("job processed\n");
			pending_jobs -= 1;
		}
		pthread_mutex_unlock(&mutex);
	}

	return NULL;
}

// I produce jobs.
void *thread_b_fn()
{
	printf("thread_b start\n");

	while (1) {
		pthread_mutex_lock(&mutex);
		pending_jobs += 1;
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);

		wait(500);
	}

	return NULL;
}

void noop() {}

int main(int argc, char **argv)
{
	printf("main start\n");

	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);

	pthread_t thread_a;
	pthread_create(&thread_a, NULL, &thread_a_fn, NULL);
	pthread_t thread_b;
	pthread_create(&thread_b, NULL, &thread_b_fn, NULL);

#ifdef __EMSCRIPTEN
	emscripten_set_main_loop(noop, 0, 0);
#endif

	return 0;
}
