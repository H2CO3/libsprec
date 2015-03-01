/*
 * web_client.h
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Tue 17/04/2012
 */

#ifndef __SPREC_WEB_CLIENT_H__
#define __SPREC_WEB_CLIENT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

typedef struct sprec_server_response {
	char *data;
	size_t length;
} sprec_server_response;

/*
 * Sends the FLAC-encoded audio data.
 * Returns a struct server_response pointer,
 * in which the API's JSON response is present.
 * Should be freed with sprec_free_response().
 * Returns NULL on error.
 */
sprec_server_response *
sprec_send_audio_data(
	const void *data,
	size_t length,
	const char *apikey,
	const char *language,
	uint32_t sample_rate
);

void sprec_free_response(sprec_server_response *resp);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__SPREC_WEB_CLIENT_H__ */
