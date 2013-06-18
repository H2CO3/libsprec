/*
 * flac_encoder.c
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Sun 15/04/2012.
 */

#include <stdlib.h>
#include <unistd.h>
#include <FLAC/all.h>
#include <sprec/flac_encoder.h>
#include <sprec/wav.h>

#define BUFSIZE 0x5000

/*
 * Search a NUL-terminated C string in a byte array
 * Returns: location of the string, or
 * NULL if not found
 */
const char *memstr(const void *haystack, const char *needle, size_t size);

int sprec_flac_encode(const char *wavfile, const char *flacfile)
{
	FLAC__StreamEncoder *encoder;
	FILE *infile;
	const char *dataloc;
	uint32_t rate;		/* sample rate */
	uint32_t total;		/* number of samples in file */
	uint32_t channels;	/* number of channels */
	uint32_t bps;		/* bits per sample */
	uint32_t dataoff;	/* offset of PCM data within the file */
	int err;

	/*
	 * BUFFSIZE samples * 2 bytes per sample * 2 channels
	 */
	FLAC__byte buffer[BUFSIZE * 2 * 2];

	/*
	 * BUFFSIZE samples * 2 channels
	 */
	FLAC__int32 pcm[BUFSIZE * 2];

	if (!wavfile || !flacfile)
		return -1;
	
	/*
	 * Remove the original file, if present, in order
	 * not to leave chunks of old data inside
	 */
	remove(flacfile);

	/*
	 * Read the first 64kB of the file. This somewhat guarantees
	 * that we will find the beginning of the data section, even
	 * if the WAV header is non-standard and contains
	 * other garbage before the data (NB Apple's 4kB FLLR section!)
	 */
	infile = fopen(wavfile, "r");
	if (!infile)
		return -1;

	fread(buffer, sizeof(buffer), 1, infile);

	/*
	 * Search the offset of the data section
	 */
	dataloc = memstr(buffer, "data", sizeof(buffer));
	if (!dataloc) {
		fclose(infile);
		return -1;
	}
	
	dataoff = dataloc - (char *)buffer;

	/*
	 * For an explanation on why the 4 + 4 byte extra offset is there,
	 * see the comment for calculating the number of total_samples.
	 */
	fseek(infile, dataoff + 4 + 4, SEEK_SET);
	
	struct sprec_wav_header *hdr = sprec_wav_header_from_data(buffer);
	if (!hdr) {
		fclose(infile);
		return -1;
	}

	/*
	 * Sample rate must be between 16000 and 44000
	 * for the Google Speech APIs.
	 * There should be two channels.
	 * Sample depth is 16 bit signed, little endian.
	 */
	rate = hdr->sample_rate;
	channels = hdr->number_of_channels;
	bps = hdr->bits_per_sample;
	
	/*
	 * hdr->file_size contains actual file size - 8 bytes.
	 * the eight bytes at position `data_offset' are:
	 * 'data' then a 32-bit unsigned int, representing
	 * the length of the data section.
	 */
	total = ((hdr->file_size + 8) - (dataoff + 4 + 4)) / (channels * bps / 8);

	/*
	 * Create and initialize the FLAC encoder
	 */
	encoder = FLAC__stream_encoder_new();
	if (!encoder) {
		fclose(infile);
		free(hdr);
		return -1;
	}

	FLAC__stream_encoder_set_verify(encoder, true);
	FLAC__stream_encoder_set_compression_level(encoder, 5);
	FLAC__stream_encoder_set_channels(encoder, channels);
	FLAC__stream_encoder_set_bits_per_sample(encoder, bps);
	FLAC__stream_encoder_set_sample_rate(encoder, rate);
	FLAC__stream_encoder_set_total_samples_estimate(encoder, total);

	err = FLAC__stream_encoder_init_file(encoder, flacfile, NULL, NULL);
	if (err) {
		fclose(infile);
		free(hdr);
		FLAC__stream_encoder_delete(encoder);
		return -1;
	}

	/*
	 * Feed the PCM data to the encoder in 64kB chunks
	 */
	size_t left = total;
	while (left > 0) {
		size_t readn = fread(buffer, channels * bps / 8, BUFSIZE, infile);

		for (size_t i = 0; i < readn * channels; i++) {
			if (bps == 16) {
				/*
				 * 16 bps, signed little endian
				 */
				uint16_t lsb = *(uint8_t *)(buffer + i * 2 + 0);
				uint16_t msb = *(uint8_t *)(buffer + i * 2 + 1);
				uint16_t usample = (msb << 8) | lsb;
				
				/* hooray, shifting into the sign bit is UB,
				 * so we must memcpy() into the signed integer.
				 * Thanks C standard, what a waste of LOC...
				 */
				int16_t ssample;
				memcpy(&ssample, &usample, sizeof(ssample));
				pcm[i] = ssample;
			} else {
				/*
				 * 8 bps, unsigned
				 */
				pcm[i] = *(uint8_t *)(buffer + i);
			}
		}
		
		FLAC__bool succ = FLAC__stream_encoder_process_interleaved(encoder, pcm, readn);
		if (!succ) {
			fclose(infile);
			free(hdr);
			FLAC__stream_encoder_delete(encoder);
			return -1;
		}

		left -= readn;
	}

	/*
	 * Write out/finalize the file
	 */
	FLAC__stream_encoder_finish(encoder);

	/*
	 * Clean up
	 */
	FLAC__stream_encoder_delete(encoder);
	fclose(infile);
	free(hdr);
	
	return 0;
}

const char *memstr(const void *haystack, const char *needle, size_t size)
{
	const char *p;
	const char *hs = haystack;
	size_t needlesize = strlen(needle);

	for (p = haystack; p <= hs + size - needlesize; p++)
		if (memcmp(p, needle, needlesize) == 0) /* found it */
			return p;
	
	/* not found */
	return NULL;
}

