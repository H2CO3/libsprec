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
#include <jsonz/jsonz.h>
#include <sprec/web_client.h>

#define BUF_SIZE 0x1000
#define RESPONSE_SIZE 0x100

static size_t http_callback(char *ptr, size_t count, size_t blocksize, void *userdata);

struct sprec_server_response *sprec_send_audio_data(void *data, int length, const char *language, uint32_t sample_rate)
{
	CURL *conn_hndl;
	struct curl_httppost *form, *lastptr;
	struct curl_slist *headers;
	struct sprec_server_response *resp;
	char url[128];
	char header[128];
	
	if (!data)
		return 0;

	/*
	 * Initialize the variables
	 * Put the language code to the URL query string
	 * If no language given, default to U. S. English
	 */
	sprintf(url, "https://www.google.com/speech-api/v1/recognize?xjerr=1&client=chromium&lang=%s", language ? language : "en-US");
	resp = malloc(sizeof(*resp));
	if (!resp)
		return NULL;
	
	resp->data = malloc(1024);
	resp->length = 0;
	conn_hndl = curl_easy_init();
	if (!conn_hndl) {
		sprec_free_response(resp);
		return NULL;
	}
	
	form = NULL;
	lastptr = NULL;
	headers = NULL;
	sprintf(header, "Content-Type: audio/x-flac; rate=%lu", (unsigned long)sample_rate);
	headers = curl_slist_append(headers, header);

	curl_formadd(&form, &lastptr, CURLFORM_COPYNAME, "myfile", CURLFORM_CONTENTSLENGTH, length, CURLFORM_PTRCONTENTS, data, CURLFORM_END);

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

void sprec_free_response(struct sprec_server_response *resp)
{
	if (resp) {
		free(resp->data);
		free(resp);
	}
}

char *sprec_get_text_from_json(const char *json)
{
	void *root;
	void *guesses_array;
	void *guess;
	void *str_obj;
	const char *str_tmp;
	char *text;
	
	if (!json)
		return NULL;
	
	/*
	 * Find the appropriate object in the JSON structure.
	 * Here we don't check for (return value != NULL), as
	 * libjsonz handles NULL inputs correctly.
	 */
	root = jsonz_parse(json);
	guesses_array = jsonz_dict_get(root, "hypotheses");
	
	guess = jsonz_array_get(guesses_array, 0);
	str_obj = jsonz_dict_get(guess, "utterance");
	/*
	 * The only exception: strdup(NULL) is undefined behaviour
	 */
	str_tmp = jsonz_string_get_str(str_obj);
	text = str_tmp ? strdup(str_tmp) : NULL;
	
	jsonz_object_free(root);
	return text;
}

double sprec_get_confidence_from_json(const char *json)
{
	void *root;
	void *guesses_array;
	void *guess;
	void *num_obj;
	double result;
	
	if (!json)
		return 0.0;
	
	/*
	 * Find the appropriate object in the JSON structure.
	 * Here we don't check for (return value != NULL), as
	 * libjsonz handles NULL inputs correctly.
	 */
	root = jsonz_parse(json);
	guesses_array = jsonz_dict_get(root, "hypotheses");
	guess = jsonz_array_get(guesses_array, 0);
	num_obj = jsonz_dict_get(guess, "confidence");
	result = jsonz_number_get_float_value(num_obj);
	jsonz_object_free(root);
	return result;
}

int sprec_get_file_contents(const char *file, void **cont, size_t *size)
{
	/*
	 * Open file for reading
	 */
	int fd = open(file, O_RDONLY);
	if (fd < 0)
		return -1;
	
	off_t total = lseek(fd, 0, SEEK_END);
	if (total < 0)
		return -1;
	
	lseek(fd, 0, SEEK_SET);
	
	ssize_t n, rest = total, readb = 0;
	unsigned char buf[BUF_SIZE];
	unsigned char *result = NULL;

	/*
	 * Read the whole file into memory
	 */
	while (rest > 0) {
		n = read(fd, buf, BUF_SIZE);
		if (n < 0) {
			close(fd);
			free(result);
			return -1;
		}
		
		unsigned char *tmp = realloc(result, readb + n);
		if (!tmp) {
			close(fd);
			free(result);
			return -1;
		}
		
		result = tmp;
		memcpy(result + readb, buf, n);
		readb += n;
		rest -= n;
	}

	close(fd);
	*cont = result;
	*size = total;
	
	return 0;
}

static size_t http_callback(char *ptr, size_t count, size_t blocksize, void *userdata)
{
	struct sprec_server_response *response = userdata;
	size_t size = count * blocksize;
	memcpy(response->data + response->length, ptr, size);
	response->length += size;
	return size;
}

