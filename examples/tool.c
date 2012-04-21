/**
 * tool.c
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Sun 15/04/2012.
**/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sprec/sprec.h>

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("Usage: %s <language code> <duration in seconds>\n", argv[0]);
		exit(1);
	}
	
	struct sprec_wav_header *hdr;
	struct sprec_server_response *resp;
	char *flac_file_buf;
	uint32_t flac_file_len;
	char *text;
	
	/**
	 * Generate two temporary files to store the WAV and FLAC data
	**/
	char wavfile[L_tmpnam + 5];
	char flacfile[L_tmpnam + 6];
	char *tmp_filename_stub = tmpnam(NULL);
	sprintf(wavfile, "%s.wav", tmp_filename_stub);
	sprintf(flacfile, "%s.flac", tmp_filename_stub);
	
	/**
	 * Start recording a WAV: sample rate = 16000Hz, bit depth = 16bps, stereo
	**/
	hdr = sprec_wav_header_from_params(16000, 16, 2);
	sprec_record_wav(wavfile, hdr, 1000 * strtod(argv[2], NULL));
	free(hdr);
	
	/**
	 * Make the raw PCM in the WAV file into a FLAC file
	**/
	sprec_flac_encode(wavfile, flacfile);
	/**
	 * Read the entire FLAC file...
	**/
	sprec_get_file_contents(flacfile, &flac_file_buf, &flac_file_len);
	/**
	 * ...and send it to Google
	**/
	resp = sprec_send_audio_data(flac_file_buf, flac_file_len, argv[1], 16000);
	free(flac_file_buf);
	
	/**
	 * Get the JSON from the response object,
	 * then parse it to get the actual text
	**/
	text = sprec_get_text_from_json(resp->data);
	sprec_free_response(resp);
	
	printf("%s\n", text ? text : "(null)");
	free(text);
	
	/**
	 * Let's not fill up the entire /tmp folder
	**/
	remove(wavfile);
	remove(flacfile);
	
	return 0;
}

