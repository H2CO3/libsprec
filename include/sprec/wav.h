/**
 * wav.h
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Sun 15/04/2012.
**/

#ifndef __WAV_H__
#define __WAV_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

/**
 * _without_ the data header (8 bytes)
**/
#define WAV_HEADER_SIZE 36

struct sprec_wav_header {
	char RIFF_marker[4];
	uint32_t file_size;
	char filetype_header[4];
	char format_marker[4];
	uint32_t data_header_length;
	uint16_t format_type;
	uint16_t number_of_channels;
	uint32_t sample_rate;
	uint32_t bytes_per_second;
	uint16_t bytes_per_frame;
	uint16_t bits_per_sample;
};

struct sprec_wav_header *sprec_wav_header_from_data(const char *ptr);
struct sprec_wav_header *sprec_wav_header_from_params(uint32_t sample_rate, uint16_t bit_depth, uint16_t channels);
void sprec_wav_header_write(int fd, struct sprec_wav_header *hdr);
void sprec_record_wav(const char *filename, struct sprec_wav_header *hdr, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WAV_H__ */

