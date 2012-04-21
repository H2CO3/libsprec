/**
 * flac_encoder.c
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Sun 15/04/2012.
**/

#include <stdlib.h>
#include <unistd.h>
#include <FLAC/all.h>
#include <sprec/flac_encoder.h>
#include <sprec/wav.h>

#define BUFFSIZE (1 << 16)

/**
 * Search a NUL-terminated C string in a byte array
 * Returns: location of the string, or
 * NULL if not found
**/
char *memstr(char *haystack, char *needle, int size);

/**
 * BUFFSIZE samples * 2 bytes per sample * 2 channels
**/
static FLAC__byte buffer[BUFFSIZE * 2 * 2];
/**
 * BUFFSIZE samples * 2 channels
**/
static FLAC__int32 pcm[BUFFSIZE * 2];

int sprec_flac_encode(const char *wavfile, const char *flacfile)
{
	FLAC__StreamEncoder *encoder;
	FILE *infile;
	char *data_location;
	uint32_t sample_rate;
	uint32_t total_samples;
	uint32_t channels;
	uint32_t bits_per_sample;
	uint32_t data_offset;
	int err;

	if (!wavfile || !flacfile)
	{
		return -1;
	}
	
	/**
	 * Remove the original file, if present, in order
	 * not to leave chunks of old data inside
	**/
	remove(flacfile);

	/**
	 * Read the first 64kB of the file. This somewhat guarantees
	 * that we will find the beginning of the data section, even
	 * if the WAV header is non-standard and contains
	 * other garbage before the data (NB Apple's 4kB FLLR section!)
	**/
	infile = fopen(wavfile, "rb");
	if (!infile)
	{
		return -1;
	}
	fread(buffer, BUFFSIZE, 1, infile);

	/**
	 * Search the offset of the data section
	**/
	data_location = memstr((char *)buffer, "data", BUFFSIZE);
	if (!data_location)
	{
		fclose(infile);
		return -1;
	}
	data_offset = data_location - (char *)buffer;

	/**
	 * For an explanation on why the 4 + 4 byte extra offset is there,
	 * see the comment for calculating the number of total_samples.
	**/
	fseek(infile, data_offset + 4 + 4, SEEK_SET);
	
	struct sprec_wav_header *hdr = sprec_wav_header_from_data((char *)buffer);
	if (!hdr)
	{
		fclose(infile);
		return -1;
	}

	/**
	 * Sample rate must be between 16000 and 44000
	 * for the Google Speech APIs.
	 * There should be two channels.
	 * Sample depth is 16 bit signed, little endian.
	**/
	sample_rate = hdr->sample_rate;
	channels = hdr->number_of_channels;
	bits_per_sample = hdr->bits_per_sample;
	
	/**
	 * hdr->file_size contains actual file size - 8 bytes.
	 * the eight bytes at position `data_offset' are:
	 * 'data' then a 32-bit unsigned int, representing
	 * the length of the data section.
	**/
	total_samples = ((hdr->file_size + 8) - (data_offset + 4 + 4)) / (channels * bits_per_sample / 8);

	/**
	 * Create and initialize the FLAC encoder
	**/
	encoder = FLAC__stream_encoder_new();
	if (!encoder)
	{
		fclose(infile);
		free(hdr);
		return -1;
	}

	FLAC__stream_encoder_set_verify(encoder, true);
	FLAC__stream_encoder_set_compression_level(encoder, 5);
	FLAC__stream_encoder_set_channels(encoder, channels);
	FLAC__stream_encoder_set_bits_per_sample(encoder, bits_per_sample);
	FLAC__stream_encoder_set_sample_rate(encoder, sample_rate);
	FLAC__stream_encoder_set_total_samples_estimate(encoder, total_samples);

	err = FLAC__stream_encoder_init_file(encoder, flacfile, NULL, NULL);
	if (err)
	{
		fclose(infile);
		free(hdr);
		FLAC__stream_encoder_delete(encoder);
		return -1;
	}

	/**
	 * Feed the PCM data to the encoder in 64kB chunks
	**/
	size_t left = total_samples;
	while (left > 0)
	{
		size_t need = left > BUFFSIZE ? BUFFSIZE : left;
		fread(buffer, channels * bits_per_sample / 8, need, infile);

		size_t i;
		for (i = 0; i < need * channels; i++)
		{
			if (bits_per_sample == 16)
			{
				/**
				 * 16 bps, signed little endian
				**/
				pcm[i] = *(int16_t *)(buffer + i * 2);
			}
			else
			{
				/**
				 * 8 bps, unsigned
				**/
				pcm[i] = *(uint8_t *)(buffer + i);
			}
		}
		
		FLAC__bool succ = FLAC__stream_encoder_process_interleaved(encoder, pcm, need);
		if (!succ)
		{
			fclose(infile);
			free(hdr);
			FLAC__stream_encoder_delete(encoder);
			return -1;
		}

		left -= need;
	}

	/**
	 * Write out/finalize the file
	**/
	FLAC__stream_encoder_finish(encoder);

	/**
	 * Clean up
	**/
	FLAC__stream_encoder_delete(encoder);
	fclose(infile);
	free(hdr);
	
	return 0;
}

char *memstr(char *haystack, char *needle, int size)
{
	char *p;
	char needlesize = strlen(needle);

	for (p = haystack; p <= haystack - needlesize + size; p++)
	{
		if (memcmp(p, needle, needlesize) == 0)
		{
			/**
			 * Found it
			**/
			return p;
		}
	}
	
	/**
	 * Not found
	**/
	return NULL;
}

