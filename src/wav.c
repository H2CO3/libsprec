/**
 * wav.c
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Sun 15/04/2012.
**/

#include <unistd.h>
#include <sprec/wav.h>
#include "record_apple.h"

#if defined _WIN64 || defined _WIN32
#elif defined __APPLE__
#else
	#include <alsa/asoundlib.h>
#endif

struct sprec_wav_header *sprec_wav_header_from_data(const char *ptr)
{
	struct sprec_wav_header *hdr;
	hdr = malloc(sizeof(*hdr));

	/**
	 * We could use __attribute__((__packed__)) and a single memcpy(),
	 * but we choose this approach for the sake of portability.
	**/
	memcpy(&hdr->RIFF_marker, ptr + 0, 4);
	memcpy(&hdr->file_size, ptr + 4, 4);
	memcpy(&hdr->filetype_header, ptr + 8, 4);
	memcpy(&hdr->format_marker, ptr + 12, 4);
	memcpy(&hdr->data_header_length, ptr + 16, 4);
	memcpy(&hdr->format_type, ptr + 20, 2);
	memcpy(&hdr->number_of_channels, ptr + 22, 2);
	memcpy(&hdr->sample_rate, ptr + 24, 4);
	memcpy(&hdr->bytes_per_second, ptr + 28, 4);
	memcpy(&hdr->bytes_per_frame, ptr + 32, 2);
	memcpy(&hdr->bits_per_sample, ptr + 34, 2);

	return hdr;
}

struct sprec_wav_header *sprec_wav_header_from_params(uint32_t sample_rate, uint16_t bit_depth, uint16_t channels)
{
	struct sprec_wav_header *hdr;
	hdr = malloc(sizeof(*hdr));

	memcpy(&hdr->RIFF_marker, "RIFF", 4);
	memcpy(&hdr->filetype_header, "WAVE", 4);
	memcpy(&hdr->format_marker, "fmt ", 4);
	hdr->data_header_length = 16;
	hdr->format_type = 1;
	hdr->number_of_channels = channels;
	hdr->sample_rate = sample_rate;
	hdr->bytes_per_second = sample_rate * channels * bit_depth / 8;
	hdr->bytes_per_frame = channels * bit_depth / 8;
	hdr->bits_per_sample = bit_depth;

	return hdr;
}

void sprec_wav_header_write(int fd, struct sprec_wav_header *hdr)
{
	write(fd, &hdr->RIFF_marker, 4);
	write(fd, &hdr->file_size, 4);
	write(fd, &hdr->filetype_header, 4);
	write(fd, &hdr->format_marker, 4);
	write(fd, &hdr->data_header_length, 4);
	write(fd, &hdr->format_type, 2);
	write(fd, &hdr->number_of_channels, 2);
	write(fd, &hdr->sample_rate, 4);
	write(fd, &hdr->bytes_per_second, 4);
	write(fd, &hdr->bytes_per_frame, 2);
	write(fd, &hdr->bits_per_sample, 2);
	write(fd, "data", 4);

	uint32_t data_size = hdr->file_size + 8 - 44;
	write(fd, &data_size, 4);
}

void sprec_record_wav(const char *filename, struct sprec_wav_header *hdr, uint32_t duration_ms)
{
#if defined _WIN64 || defined _WIN32
	/**
	 * Windows
	**/
	#error This needs to be implemented yet...
#elif defined __APPLE__
	/**
	 * Mac OS X or iOS
	**/
	sprec_record_wav_apple(filename, hdr, duration_ms);
#else
	/**
	 * Linux, Solaris, etc.
	 * Let's hope they have an available ALSA port
	**/
	int i;
	int size;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	unsigned int val;
	int dir;
	snd_pcm_uframes_t frames;
	char *buffer;
	int fd;
	
	/**
	 * Open PCM device for recording
	**/
	snd_pcm_open(&handle, "pulse", SND_PCM_STREAM_CAPTURE, 0);
	
	/**
	 * Allocate a hardware parameters object.
	**/
	snd_pcm_hw_params_alloca(&params);

	/**
	 * Fill it in with default values, then set
	 * the non-default ones manually
	**/
	snd_pcm_hw_params_any(handle, params);
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	if (hdr->bits_per_sample == 16)
	{
		snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
	}
	else
	{
		snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_U8);
	}

	snd_pcm_hw_params_set_channels(handle, params, hdr->number_of_channels);

	val = hdr->sample_rate;
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
	hdr->sample_rate = val;

	/**
	 * Interrupt period size equals 32 frames
	**/
	frames = 32;
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

	/**
	 * Write the parameters to the driver
	**/
	snd_pcm_hw_params(handle, params);

	snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	/**
	 * multiply by number of bytes/sample
	 * and number of channels
	**/
	size = frames * hdr->bits_per_sample / 8 * hdr->number_of_channels;
	buffer = malloc(size);

	/**
	 * Calculate number of loops and the size of
	 * the raw PCM data
	**/
	snd_pcm_hw_params_get_period_time(params, &val, &dir);
	uint32_t pcm_data_size = hdr->sample_rate * hdr->bytes_per_frame * duration_ms / 1000;
	hdr->file_size = pcm_data_size + 44 - 8;
	
	/**
	 * Open the WAV file for output;
	 * write out the WAV header
	**/
	fd = open(filename, O_WRONLY | O_CREAT, 0644);
	sprec_wav_header_write(fd, hdr);

	for (i = duration_ms * 1000 / val; i > 0; i--)
	{
		int err = snd_pcm_readi(handle, buffer, frames);
		if (err == -1 * EPIPE)
		{
			/**
			 * minus EPIPE means X-run
			**/
			snd_pcm_prepare(handle);
		}
		
		write(fd, buffer, size);
	}

	/**
	 * Clean up
	**/
	close(fd);
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	free(buffer);
#endif
}

