/*
 * web_client.c
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Tue 17/04/2012
 */

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <sprec/web_client.h>

#define BUF_SIZE 0x1000

static size_t http_callback(char *ptr, size_t count, size_t blocksize, void *userdata);

sprec_server_response *
sprec_send_audio_data(
	const void *data,
	size_t length,
	const char *apikey,
	const char *language,
	uint32_t sample_rate
)
{
	CURL *conn_hndl;
	struct curl_httppost *form, *lastptr;
	struct curl_slist *headers;
	sprec_server_response *resp;
	char url[0x100];
	char header[0x100];

	if (data == NULL) {
		return NULL;
	}

	/*
	 * Initialize the variables
	 * Put the language code to the URL query string
	 * If no language given, default to U. S. English
	 */
	snprintf(
		url,
		sizeof(url),
		"https://www.google.com/speech-api/v2/recognize?output=json&key=%s&lang=%s",
		apikey,
		language ? language : "en-US"
	);

	resp = malloc(sizeof(*resp));
	if (resp == NULL) {
		return NULL;
	}

	resp->data = NULL;
	resp->length = 0;

	conn_hndl = curl_easy_init();
	if (conn_hndl == NULL) {
		sprec_free_response(resp);
		return NULL;
	}

	form = NULL;
	lastptr = NULL;
	headers = NULL;
	snprintf(
		header,
		sizeof(header),
		"Content-Type: audio/x-flac; rate=%" PRIu32,
		sample_rate
	);
	headers = curl_slist_append(headers, header);

	curl_formadd(
		&form,
		&lastptr,
		CURLFORM_COPYNAME,
		"myfile",
		CURLFORM_CONTENTSLENGTH,
		(long)length,
		CURLFORM_PTRCONTENTS,
		data,
		CURLFORM_END
	);

	/*
	 * Setup the cURL handle
	 */
	curl_easy_setopt(conn_hndl, CURLOPT_URL, url);
	curl_easy_setopt(conn_hndl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(conn_hndl, CURLOPT_HTTPPOST, form);
	curl_easy_setopt(conn_hndl, CURLOPT_WRITEFUNCTION, http_callback);
	curl_easy_setopt(conn_hndl, CURLOPT_WRITEDATA, resp);

	/*
	 * SSL certificates are not available on iOS, so we have to trust Google
	 * (0 means false)
	 */
	curl_easy_setopt(conn_hndl, CURLOPT_SSL_VERIFYPEER, 0);

	/*
	 * Initiate the HTTP(S) transfer
	 */
	curl_easy_perform(conn_hndl);

	/*
	 * Clean up
	 */
	curl_formfree(form);
	curl_slist_free_all(headers);
	curl_easy_cleanup(conn_hndl);

	/*
	 * NULL-terminate the JSON response string
	 */
	resp->data[resp->length] = '\0';

	return resp;
}

void sprec_free_response(sprec_server_response *resp)
{
	if (resp) {
		free(resp->data);
		free(resp);
	}
}

static size_t http_callback(char *ptr, size_t count, size_t blocksize, void *userdata)
{
	sprec_server_response *response = userdata;
	size_t size = count * blocksize;

	response->data = realloc(response->data, response->length + size);
	memcpy(response->data + response->length, ptr, size);
	response->length += size;

	return size;
}
