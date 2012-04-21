/**
 * record_apple.h
 * libsprec
 * 
 * Created by Árpád Goretity (H2CO3)
 * on Thu 19/04/2012
**/

#ifndef __RECORD_APPLE_H__
#define __RECORD_APPLE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <sprec/wav.h>

#ifdef __APPLE__
void sprec_record_wav_apple(const char *filename, struct sprec_wav_header *hdr, uint32_t duration_ms);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __RECORD_APPLE_H__ */

