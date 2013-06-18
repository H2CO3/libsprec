/*
 * recognize.h
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Sun 20/05/2012.
 */

#ifndef __SPREC_RECOGNIZE_H__
#define __SPREC_RECOGNIZE_H__

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct sprec_result {
	char *text;
	double confidence;
};

typedef void (*sprec_callback)(struct sprec_result *, void *);

/*
 * Performs a synchronous text recognition session in the given language,
 * listening for the duration specified by `dur_s' (in seconds),
 * then returns the recognized text and the recognition confidence.
 * The return value must be freed using sprec_result_free().
 * Returns NULL on error.
 */
struct sprec_result *sprec_recognize_sync(const char *lang, float dur_s);

/*
 * Frees the resources used by `res'. Invalidates `res'.
 */
void sprec_result_free(struct sprec_result *res);

/*
 * Performs an asynchronous text recognition session in the given language,
 * listening for the duration specified by `dur_s' (in seconds).
 * Returns immediately. When the recognition finishes or an eror occurs,
 * it calls the `cb' callback function with a valid sprec_result structure
 * and the `userdata' specified here. The callback function *must not*
 * sprec_result_free() its first parameter!
 */
pthread_t sprec_recognize_async(const char *lang, float dur_s, sprec_callback cb, void *userdata);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__SPREC_RECOGNIZE_H__ */

