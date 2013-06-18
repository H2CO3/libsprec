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
	char *language;
	float duration;
	sprec_callback callback;
	void *userdata;
};

void *sprec_pthread_fn(void *ctx);

struct sprec_result *sprec_recognize_sync(const char *lang, float dur_s)
{
	struct sprec_wav_header *hdr;
	struct sprec_server_response *resp;
	struct sprec_result *res;
	int err;
	size_t len;
	char *text, *tmpstub, *buf;
	char wavfile[L_tmpnam + 5];
	char flacfile[L_tmpnam + 6];
	double confidence;
	
	tmpstub = tmpnam(NULL);
	sprintf(wavfile, "%s.wav", tmpstub);
	sprintf(flacfile, "%s.flac", tmpstub);

	/*
	 * sample rate = 16000Hz, bit depth = 16bps, stereo
	 */
	hdr = sprec_wav_header_from_params(16000, 16, 2);
	if (hdr == NULL)
		return NULL;

	err = sprec_record_wav(wavfile, hdr, 1000 * dur_s);
	if (err != 0) {
		free(hdr);
		return NULL;
	}


	/*
	 * Convert the WAV file to a FLAC file
	 */
	err = sprec_flac_encode(wavfile, flacfile);
	if (err != 0) {
		free(hdr);
		return NULL;
	}

	/*
	 * Read the entire FLAC file into memory...
	 */
	err = sprec_get_file_contents(flacfile, (void **)&buf, &len);
	if (err != 0) {
		free(hdr);
		return NULL;
	}

	/*
	 * ...and send it to Google
	 */
	resp = sprec_send_audio_data(buf, len, lang, hdr->sample_rate);
	free(buf);
	free(hdr);
	if (resp == NULL)
		return NULL;

	/*
	 * Get the JSON from the response object,
	 * then parse it to get the actual text and confidence
	 */
	text = sprec_get_text_from_json(resp->data);
	confidence = sprec_get_confidence_from_json(resp->data);
	sprec_free_response(resp);

	/*
	 * Remove the temporary files in order
	 * not fill the /tmp folder with garbage
	 */
	remove(wavfile);
	remove(flacfile);

	/*
	 * Compose the return value
	 */
	res = malloc(sizeof(*res));
	if (res == NULL) {
		free(text);
		return NULL;
	}
	
	res->text = text;
	res->confidence = confidence;
	return res;
}

void sprec_result_free(struct sprec_result *res)
{
	if (res != NULL) {
		free(res->text);
		free(res);
	}
}

pthread_t sprec_recognize_async(const char *lang, float dur_s, sprec_callback cb, void *userdata)
{
	pthread_t tid;
	pthread_attr_t tattr;
	int err;
	struct sprec_recattr_internal *context;

	/* Fill in the context structure */
	context = malloc(sizeof(*context));
	if (context == NULL) {
		cb(NULL, userdata);
		return 0;
	}

	context->language = strdup(lang);
	if (context->language == NULL) {
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
		free(context->language);
		free(context);
		cb(NULL, userdata);
		return 0;
	}

	err = pthread_create(&tid, &tattr, sprec_pthread_fn, context);
	if (err != 0) {
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
	struct sprec_result *result;

	/* Use the synchronous recognition function */
	context = ctx;
	result = sprec_recognize_sync(context->language, context->duration);
	/* Call the callback */
	context->callback(result, context->userdata);

	sprec_result_free(result);	
	free(context->language);
	free(context);

	return NULL;
}

