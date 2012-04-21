/**
 * web_client.c
 * libsprec
 * 
 * Created by Árpád Goretity (H2CO3)
 * on Tue 17/04/2012
**/

#include <curl/curl.h>
#include <jsonz/jsonz.h>
#include <sprec/web_client.h>

#define BUF_SIZE 1024
#define RESPONSE_SIZE 1024

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
	{
		return 0;
	}

	/**
	 * Initialize the variables
	 * Put the language code to the URL query string
	 * If no language given, default to U. S. English
	**/
	sprintf(url, "https://www.google.com/speech-api/v1/recognize?xjerr=1&client=chromium&lang=%s", language ? language : "en-US");
	resp = malloc(sizeof(*resp));
	if (!resp)
	{
		return NULL;
	}
	
	resp->data = malloc(1024);
	resp->length = 0;
	conn_hndl = curl_easy_init();
	if (!conn_hndl)
	{
		sprec_free_response(resp);
		return NULL;
	}
	
	form = NULL;
	lastptr = NULL;
	headers = NULL;
	sprintf(header, "Content-Type: audio/x-flac; rate=%lu", (unsigned long)sample_rate);
	headers = curl_slist_append(headers, header);

	curl_formadd(&form, &lastptr, CURLFORM_COPYNAME, "myfile", CURLFORM_CONTENTSLENGTH, length, CURLFORM_PTRCONTENTS, data, CURLFORM_END);

	/**
	 * Setup the cURL handle
	**/
	curl_easy_setopt(conn_hndl, CURLOPT_URL, url);
	curl_easy_setopt(conn_hndl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(conn_hndl, CURLOPT_HTTPPOST, form);
	curl_easy_setopt(conn_hndl, CURLOPT_WRITEFUNCTION, http_callback);
	curl_easy_setopt(conn_hndl, CURLOPT_WRITEDATA, resp);

	/**
	 * SSL certificates are not available on iOS, so we have to trust Google
	 * (0 means false)
	**/
	curl_easy_setopt(conn_hndl, CURLOPT_SSL_VERIFYPEER, 0);

	/**
	 * Initiate the HTTP(S) transfer
	**/
	curl_easy_perform(conn_hndl);

	/**
	 * Clean up
	**/
	curl_formfree(form);
	curl_slist_free_all(headers);
	curl_easy_cleanup(conn_hndl);

	/**
	 * NULL-terminate the JSON response string
	 **/
	resp->data[resp->length] = '\0';

	return resp;
}

void sprec_free_response(struct sprec_server_response *resp)
{
	if (resp)
	{
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
	char *str_tmp;
	char *text;
	
	if (!json)
	{
		return NULL;
	}
	
	/**
	 * Find the appropriate object in the JSON structure.
	 * Here we don't check for (return value != NULL), as
	 * libjsonz handles NULL inputs correctly.
	**/
	text = NULL;
	root = jsonz_object_parse(json);
	guesses_array = jsonz_object_object_get_element(root, "hypotheses");
	
	if (jsonz_object_array_length(guesses_array) > 0)
	{
		guess = jsonz_object_array_nth_element(guesses_array, 0);
		str_obj = jsonz_object_object_get_element(guess, "utterance");
		if (str_obj)
		{
			/**
			 * The only exception: strdup(NULL) is undefined behaviour
			**/
			str_tmp = jsonz_object_string_get_str(str_obj);
			if (str_tmp)
			{
				text = strdup(str_tmp);
			}
		}
	}
	
	jsonz_object_release(root);
	return text;
}

int sprec_get_file_contents(const char *file, char **buf, int *size)
{
	/**
	 * Open file for reading
	**/
	int fd = open(file, O_RDONLY);
	int total_size;
	int num_bytes = 1;

	char tmp[BUF_SIZE];
	char *result;
	char *realloc_guard;

	/**
	 * Read the whole file into memory
	**/
	for (result = NULL, total_size = 0; num_bytes > 0; num_bytes = read(fd, tmp, BUF_SIZE))
	{
		realloc_guard = realloc(result, total_size + num_bytes);
		if (!realloc_guard)
		{
			close(fd);
			free(result);
			return -1;
		}
		
		result = realloc_guard;
		memcpy(result + total_size, tmp, num_bytes);
		total_size += num_bytes;
	}

	close(fd);
	*buf = result;
	*size = total_size;
	
	return 0;
}

static size_t http_callback(char *ptr, size_t count, size_t blocksize, void *userdata)
{
	struct sprec_server_response *response = userdata;
	int size = count * blocksize;
	memcpy(response->data + response->length, ptr, size);
	response->length += size;
	return size;
}

