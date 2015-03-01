/*
 * flac_encoder.h
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Sun 15/04/2012.
 */

#ifndef __SPREC_FLAC_ENCODER_H__
#define __SPREC_FLAC_ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*
 * Converts a WAV PCM file at the path `wavfile'
 * to a FLAC file with the same sample rate,
 * channel number and bit depth. Writes the result
 * to the file at path `flacfile'.
 * Returns a pointer to FLAC data buffer on success,
 * NULL on error. On success, *size will be set to
 * the size of the FLAC data (in bytes).
 */
void *sprec_flac_encode(const char *wavfile, size_t *size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__SPREC_FLAC_ENCODER_H__ */
