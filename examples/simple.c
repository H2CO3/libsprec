/**
 * simple.c
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Sun 20/05/2012.
 */


#include <sprec/sprec.h>

#ifdef SYNCHRONOUS

int main(int argc, char **argv)
{
	struct sprec_result *res;

	res = sprec_recognize_async(argv[1], strtod(argv[2], NULL));
	printf("%s (%lf)\n", res->text, res->confidence);
	sprec_result_free(res);
	return 0;
}

#else

void callback(struct sprec_result *res, void *data)
{
	printf("Thread: %d\n", pthread_self());
	printf("%s (%lf)\n", res->text, res->confidence);
}

int main(int argc, char **argv)
{
	pthread_t pid;
	void *retval;

	pid = sprec_recognize_async(argv[1], strtod(argv[2], NULL), callback, NULL);
	printf("Thread: %d\n", pthread_self());
	pthread_join(pid, &retval);
	return 0;
}

#endif

