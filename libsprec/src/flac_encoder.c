/*
 * flac_encoder.c
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Sun 15/04/2012.
 */

#include <stdlib.h>
#include <unistd.h>

#include <sprec/flac_encoder.h>
#include <sprec/wav.h>

#include <FLAC/all.h>

#define BUFSIZE 0x5000

/*
 * Search a NUL-terminated C string in a byte array
 * Returns: location of the string, or
 * NULL if not found
 */
static const char *memstr(const void *haystack, const char *needle, size_t size);

typedef struct sprec_encoder_state {
	unsigned char *buf;
	size_t length;
} sprec_encoder_state;

static FLAC__StreamEncoderWriteStatus flac_write_callback(
	const FLAC__StreamEncoder *encoder,
	const FLAC__byte buffer[],
	size_t bytes,
	unsigned samples,
	unsigned current_frame,
	void *client_data
)
{
	sprec_encoder_state *flac_data = client_data;

	flac_data->buf = realloc(flac_data->buf, flac_data->length + bytes);
	memcpy(flac_data->buf + flac_data->length, buffer, bytes);
	flac_data->length += bytes;

	return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

void *sprec_flac_encode(const char *wavfile, size_t *size)
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
	 * Initialized to 0 in case file is not long enough.
	 */
	FLAC__byte buffer[BUFSIZE * 2 * 2] = { 0 };

	/*
	 * BUFFSIZE samples * 2 channels
	 */
	FLAC__int32 pcm[BUFSIZE * 2] = { 0 };

	/*
	 * Read the first 64kB of the file. This somewhat guarantees
	 * that we will find the beginning of the data section, even
	 * if the WAV header is non-standard and contains
	 * other garbage before the data (NB Apple's 4kB FLLR section!)
	 */
	infile = fopen(wavfile, "r");
	if (infile == NULL) {
		return NULL;
	}

	fread(buffer, sizeof buffer, 1, infile);

	/*
	 * Search the offset of the data section
	 */
	dataloc = memstr(buffer, "data", sizeof buffer);
	if (dataloc == NULL) {
		fclose(infile);
		return NULL;
	}

	dataoff = dataloc - (char *)buffer;

	/*
	 * For an explanation on why the 4 + 4 byte extra offset is there,
	 * see the comment for calculating the number of total_samples.
	 */
	fseek(infile, dataoff + 4 + 4, SEEK_SET);

	sprec_wav_header *hdr = sprec_wav_header_from_data(buffer);
	if (hdr == NULL) {
		fclose(infile);
		return NULL;
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
	if (encoder == NULL) {
		fclose(infile);
		free(hdr);
		return NULL;
	}

	FLAC__stream_encoder_set_verify(encoder, true);
	FLAC__stream_encoder_set_compression_level(encoder, 5);
	FLAC__stream_encoder_set_channels(encoder, channels);
	FLAC__stream_encoder_set_bits_per_sample(encoder, bps);
	FLAC__stream_encoder_set_sample_rate(encoder, rate);
	FLAC__stream_encoder_set_total_samples_estimate(encoder, total);

	sprec_encoder_state flac_data = {
		.buf = NULL,
		.length = 0
	};

	err = FLAC__stream_encoder_init_stream(
		encoder,
		flac_write_callback,
		NULL, // seek() stream
		NULL, // tell() stream
		NULL, // metadata writer
		&flac_data
	);

	if (err) {
		fclose(infile);
		free(hdr);
		free(flac_data.buf);
		FLAC__stream_encoder_delete(encoder);
		return NULL;
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
				memcpy(&ssample, &usample, sizeof ssample);
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
			free(flac_data.buf);
			return NULL;
		}

		left -= readn;
	}

	/*
	 * Write out/finalize the output stream
	 */
	FLAC__stream_encoder_finish(encoder);

	/*
	 * Clean up
	 */
	FLAC__stream_encoder_delete(encoder);
	fclose(infile);
	free(hdr);

	*size = flac_data.length;
	return flac_data.buf;
}

static const char *memstr(const void *haystack, const char *needle, size_t size)
{
	const char *p;
	const char *hs = haystack;
	size_t needlesize = strlen(needle);

	for (p = haystack; p + needlesize <= hs + size; p++) {
		if (memcmp(p, needle, needlesize) == 0) {
			/* found it */
			return p;
		}
	}

	/* not found */
	return NULL;
}
