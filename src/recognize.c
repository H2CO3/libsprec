/**
 * recognize.c
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Sun 20/05/2012.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sprec/wav.h>
#include <sprec/flac_encoder.h>
#include <sprec/web_client.h>
#include <sprec/recognize.h>

struct sprec_recattr_internal {
	char *apikey;
	char *language;
	double duration;
	sprec_callback callback;
	void *userdata;
};

void *sprec_pthread_fn(void *ctx);

char *sprec_recognize_sync(const char *apikey, const char *lang, double dur_s)
{
	struct sprec_wav_header *hdr;
	sprec_server_response *resp;
	int err;
	size_t len;
	char *text, *tmpstub, *buf;
	char wavfile[L_tmpnam + 5];


	tmpstub = tmpnam(NULL);
	sprintf(wavfile, "%s.wav", tmpstub);

	/*
	 * sample rate = 16000Hz, bit depth = 16bps, stereo
	 */
	hdr = sprec_wav_header_from_params(16000, 16, 2);
	if (hdr == NULL) {
		return NULL;
	}

	err = sprec_record_wav(wavfile, hdr, 1000 * dur_s);
	if (err != 0) {
		free(hdr);
		return NULL;
	}


	/*
	 * Convert the WAV file to FLAC data...
	 */
	buf = sprec_flac_encode(wavfile, &len);
	if (buf == NULL) {
		free(hdr);
		return NULL;
	}

	/*
	 * ...and send it to Google
	 */
	resp = sprec_send_audio_data(buf, len, apikey, lang, hdr->sample_rate);
	free(buf);
	free(hdr);

	if (resp == NULL) {
		return NULL;
	}

	/*
	 * Get the JSON from the response object,
	 * then parse it to get the actual text and confidence
	 */
	text = strdup(resp->data);
	sprec_free_response(resp);

	/*
	 * Remove the temporary files in order
	 * not fill the /tmp folder with garbage
	 */
	remove(wavfile);

	return text;
}

pthread_t sprec_recognize_async(
	const char *apikey,
	const char *lang,
	double dur_s,
	sprec_callback cb,
	void *userdata
)
{
	pthread_t tid;
	pthread_attr_t tattr;
	int err;
	struct sprec_recattr_internal *context;

	/* Fill in the context structure */
	context = malloc(sizeof *context);
	if (context == NULL) {
		cb(NULL, userdata);
		return 0;
	}

	context->apikey = strdup(apikey);
	if (context->apikey == NULL) {
		free(context);
		cb(NULL, userdata);
		return 0;
	}

	context->language = strdup(lang);
	if (context->language == NULL) {
		free(context->apikey);
		free(context);
		cb(NULL, userdata);
		return 0;
	}

	context->duration = dur_s;
	context->callback = cb;
	context->userdata = userdata;

	/* Create a new thread */
	err = pthread_attr_init(&tattr);
	if (err != 0) {
		free(context->apikey);
		free(context->language);
		free(context);
		cb(NULL, userdata);
		return 0;
	}

	err = pthread_create(&tid, &tattr, sprec_pthread_fn, context);
	if (err != 0) {
		free(context->apikey);
		free(context->language);
		free(context);
		cb(NULL, userdata);
		return 0;
	}

	pthread_attr_destroy(&tattr);

	return tid;
}

void *sprec_pthread_fn(void *ctx)
{
	struct sprec_recattr_internal *context;

	/* Use the synchronous recognition function */
	context = ctx;
	char *res = sprec_recognize_sync(context->apikey, context->language, context->duration);
	/* Call the callback */
	context->callback(res, context->userdata);

	free(res);
	free(context->apikey);
	free(context->language);
	free(context);

	return NULL;
}
