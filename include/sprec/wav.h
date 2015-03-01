/*
 * wav.h
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Sun 15/04/2012.
 */

#ifndef __SPREC_WAV_H__
#define __SPREC_WAV_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <FLAC/all.h>

/*
 * _without_ the data header (8 bytes)
 */
#define SPREC_WAV_HEADER_SIZE 36

typedef struct sprec_wav_header {
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
} sprec_wav_header;

/*
 * Allocates a new WAV file header from the raw header data
 * (i. e. the first SPREC_WAV_HEADER_SIZE bytes of a WAV file).
 * Returns NULL on error.
 * Should be free()'d after use.
 */
sprec_wav_header *sprec_wav_header_from_data(const FLAC__byte *ptr);

/*
 * Allocates a new WAV file header from the given PCM parameters.
 * Returns NULL on error.
 * Should be free()'d after use.
 */
sprec_wav_header *sprec_wav_header_from_params(
	uint32_t sample_rate,
	uint16_t bit_depth,
	uint16_t channels
);

/*
 * Writes a WAV header to a file represented by `fd'.
 * The stream position indicator should be at the beginning
 * of the file.
 * Returns 0 on success, non-0 on error.
 */
int sprec_wav_header_write(FILE *fd, sprec_wav_header *hdr);

/*
 * Records a WAV (PCM) audio file to the file `filename', with the
 * parameters represented by `hdr', for `duration_ms' milliseconds.
 * Blocks until the recording is fully completed (this time may be
 * longer than the requested recording time).
 * Returns 0 on success.
 * On Mac OS X and iOS, a non-zero return code should be
 * interpreted as an AudioQueue error code.
 * On other Unices, the return code is an ALSA (libasound) status code.
 */
int sprec_record_wav(const char *filename, sprec_wav_header *hdr, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__SPREC_WAV_H__ */
