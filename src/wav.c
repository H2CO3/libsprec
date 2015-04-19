/*
 * wav.c
 * libsprec
 *
 * Created by Árpád Goretity (H2CO3)
 * on Sun 15/04/2012.
 */

#include <unistd.h>
#include <sprec/wav.h>
#include <alsa/asoundlib.h>	
#include <alloca.h>
#if defined _WIN64 || defined _WIN32
	#error "This has to be implemented yet!"
#elif defined __APPLE__
	#include <CoreFoundation/CoreFoundation.h>
	#include <AudioToolbox/AudioQueue.h>
	#include <AudioToolbox/AudioFile.h>

	#define NUM_BUFFERS 3
	#define kAudioConverterPropertyMaximumOutputPacketSize 'xops'

static void sprec_delay_millisec(int ms);
static void sprec_setup_audio_format(
	sprec_wav_header *hdr,
	AudioStreamBasicDescription *desc
);

static void sprec_calculate_buffsize(
	AudioQueueRef audio_queue,
	AudioStreamBasicDescription desc,
	Float64 seconds,
	UInt32 *buff_size
);

static void sprec_handle_input_buffer(
	void *data,
	AudioQueueRef inAQ,
	AudioQueueBufferRef buffer,
	const AudioTimeStamp *start_time,
	UInt32 num_packets,
	const AudioStreamPacketDescription *desc
);

typedef struct sprec_record_state {
	AudioFileID audio_file;
	AudioStreamBasicDescription data_format;
	AudioQueueRef queue;
	AudioQueueBufferRef buffers[NUM_BUFFERS];
	UInt32 buffer_byte_size;
	SInt64 current_packet;
} sprec_record_state;

#else
	#include <alsa/asoundlib.h>
#endif

sprec_wav_header *sprec_wav_header_from_data(const FLAC__byte *ptr)
{
	sprec_wav_header *hdr;
	hdr = malloc(sizeof *hdr);
	if (hdr == NULL) {
		return NULL;
	}

	/*
	 * We could use __attribute__((__packed__)) and a single memcpy(),
	 * but we choose this approach for the sake of portability.
	 */
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

sprec_wav_header *sprec_wav_header_from_params(
	uint32_t sample_rate,
	uint16_t bit_depth,
	uint16_t channels
)
{
	sprec_wav_header *hdr;
	hdr = malloc(sizeof *hdr);
	if (hdr == NULL) {
		return NULL;
	}

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

int sprec_wav_header_write(FILE *f, sprec_wav_header *hdr)
{
	if (hdr == NULL) {
		return -1;
	}

#define CHECK(call) do { if ((call) != 1) { return -1; } } while (0)

	CHECK(fwrite(&hdr->RIFF_marker, 4, 1, f));
	CHECK(fwrite(&hdr->file_size, 4, 1, f));
	CHECK(fwrite(&hdr->filetype_header, 4, 1, f));
	CHECK(fwrite(&hdr->format_marker, 4, 1, f));
	CHECK(fwrite(&hdr->data_header_length, 4, 1, f));
	CHECK(fwrite(&hdr->format_type, 2, 1, f));
	CHECK(fwrite(&hdr->number_of_channels, 2, 1, f));
	CHECK(fwrite(&hdr->sample_rate, 4, 1, f));
	CHECK(fwrite(&hdr->bytes_per_second, 4, 1, f));
	CHECK(fwrite(&hdr->bytes_per_frame, 2, 1, f));
	CHECK(fwrite(&hdr->bits_per_sample, 2, 1, f));
	CHECK(fwrite("data", 4, 1, f));

	uint32_t data_size = hdr->file_size + 8 - 44;
	CHECK(fwrite(&data_size, 4, 1, f));

#undef CHECK

	return 0;
}

int sprec_record_wav(const char *filename, sprec_wav_header *hdr, uint32_t duration_ms)
{
#if defined _WIN64 || defined _WIN32
	/*
	 * Windows
	 */
	return -1;
	#error "This needs to be implemented yet..."
#elif defined __APPLE__
	/*
	 * Mac OS X or iOS
	 */
	sprec_record_state record_state;
	int i;
	OSStatus status;
	memset(&record_state, 0, sizeof record_state);
	sprec_setup_audio_format(hdr, &record_state.data_format);

	/*
	 * Create a new AudioFile
	 */
	CFURLRef file_url = CFURLCreateFromFileSystemRepresentation(NULL, (UInt8 *)filename, strlen(filename), false);
	status = AudioQueueNewInput(
		&record_state.data_format,
		sprec_handle_input_buffer,
		&record_state,
		NULL,
		NULL,
		0,
		&record_state.queue
	);

	if (status) {
		return status;
	}

	status = AudioFileCreateWithURL(
		file_url,
		kAudioFileWAVEType,
		&record_state.data_format,
		kAudioFileFlags_EraseFile,
		&record_state.audio_file
	);
	CFRelease(file_url);

	if (status) {
		return status;
	}

	/*
	 * Allocate the buffers and enqueue them
	 */
	sprec_calculate_buffsize(
		record_state.queue,
		record_state.data_format,
		0.5,
		&record_state.buffer_byte_size
	);

	for (i = 0; i < NUM_BUFFERS; i++) {
		status = AudioQueueAllocateBuffer(
			record_state.queue,
			record_state.buffer_byte_size,
			&record_state.buffers[i]
		);

		if (status) {
			return status;
		}

		status = AudioQueueEnqueueBuffer(
			record_state.queue,
			record_state.buffers[i],
			0,
			NULL
		);

		if (status) {
			return status;
		}
	}

	status = AudioQueueStart(record_state.queue, NULL);
	if (status) {
		return status;
	}

	record_state.current_packet = 0;

	/*
	 * In general, it takes about one second for the
	 * buffers to be fully empty, so we wait (duration_ms + 1000) milliseconds
	 * in order not to loose audio data.
	 */

	sprec_delay_millisec(duration_ms + 1000);

	AudioQueueFlush(record_state.queue);
	AudioQueueStop(record_state.queue, false);

	for (i = 0; i < NUM_BUFFERS; i++) {
		AudioQueueFreeBuffer(record_state.queue, record_state.buffers[i]);
	}

	AudioQueueDispose(record_state.queue, true);
	AudioFileClose(record_state.audio_file);

	return 0;
#else
	/*
	 * Linux, Solaris, etc.
	 * Let's hope they have an available ALSA port
	 */
	int i;
	int size;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	unsigned int val;
	int dir;
	snd_pcm_uframes_t frames;
	char *buffer;
	FILE *f;
	int err;

	/*
	 * Open PCM device for recording
	 */
	err = snd_pcm_open(&handle, "pulse", SND_PCM_STREAM_CAPTURE, 0);
	if (err) {
        fprintf(stderr, "%s,%d: snd_pcm_open failed\n",	
				__FILE__, __LINE__);
		return err;
	}

	/*
	 * Allocate a hardware parameters object.
	 */
	snd_pcm_hw_params_alloca(&params);

	/*
	 * Fill it in with default values, then set
	 * the non-default ones manually
	 */
	snd_pcm_hw_params_any(handle, params);

	err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err) {
        fprintf(stderr, "%s,%d: snd_pcm_hw_params_set_access failed\n",	
				__FILE__, __LINE__);
		snd_pcm_close(handle);
		return err;
	}

	if (hdr->bits_per_sample == 16) {
		snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
	} else {
		snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_U8);
	}

	err = snd_pcm_hw_params_set_channels(handle, params, hdr->number_of_channels);
	if (err) {
        fprintf(stderr, "%s,%d: snd_pcm_hw_params_set_channels failed\n",	
				__FILE__, __LINE__);
		snd_pcm_close(handle);
		return err;
	}

	val = hdr->sample_rate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
	if (err) {
        fprintf(stderr, "%s,%d: snd_pcm_params_set_rate_near failed\n",	
				__FILE__, __LINE__);
		snd_pcm_close(handle);
		return err;
	}

	hdr->sample_rate = val;

	/*
	 * Interrupt period size equals 32 frames
	 */
	frames = 32;
	err = snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
	if (err) {
        fprintf(stderr, "%s,%d: snd_pcm_hw_params_set_period_size_near failed\n"
				, __FILE__, __LINE__);
		snd_pcm_close(handle);
		return err;
	}

	/*
	 * Write the parameters to the driver
	 */
	err = snd_pcm_hw_params(handle, params);
	if (err) {
        fprintf(stderr, "%s,%d: snd_pcm_hw_params failed\n",	
				__FILE__, __LINE__);
		snd_pcm_close(handle);
		return err;
	}

	err = snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	if (err) {
        fprintf(stderr, "%s,%d: snd_pcm_hw_params_get_period_size failed\n",	
				__FILE__, __LINE__);
		snd_pcm_close(handle);
		return err;
	}

	/*
	 * multiply by number of bytes/sample
	 * and number of channels
	 */
	size = frames * hdr->bits_per_sample / 8 * hdr->number_of_channels;
	buffer = malloc(size);
	if (buffer == NULL) {
        fprintf(stderr, "%s,%d: malloc failed\n",	
				__FILE__, __LINE__);
		snd_pcm_close(handle);
		return -1;
	}

	/*
	 * Calculate number of loops and the size of
	 * the raw PCM data
	 */
	err = snd_pcm_hw_params_get_period_time(params, &val, &dir);
	if (err) {
        fprintf(stderr, "%s,%d: snd_pcm_hw_params_get_period_time failed\n",	
				__FILE__, __LINE__);
		snd_pcm_close(handle);
		free(buffer);
		return err;
	}

	uint32_t pcm_data_size = hdr->sample_rate * hdr->bytes_per_frame * duration_ms / 1000;
	hdr->file_size = pcm_data_size + 44 - 8;

	/*
	 * Open the WAV file for output;
	 * write out the WAV header
	 */
	f = fopen(filename, "wb");
	if (f == NULL) {
        fprintf(stderr, "%s,%d: fopen failed\n",	
				__FILE__, __LINE__);
		snd_pcm_close(handle);
		free(buffer);
		return -1;
	}

	err = sprec_wav_header_write(f, hdr);
	if (err) {
        fprintf(stderr, "%s,%d: sprec_wav_header_write failed\n",	
				__FILE__, __LINE__);
		snd_pcm_close(handle);
		free(buffer);
		fclose(f);
		return err;
	}

	for (i = duration_ms * 1000 / val; i > 0; i--) {
		err = snd_pcm_readi(handle, buffer, frames);
		if (err == -EPIPE) {
			/*
			 * minus EPIPE means X-run
			 */
        	fprintf(stderr, "%s,%d: snd_pcm_readi EPIPE failed\n",	
				__FILE__, __LINE__);
			err = snd_pcm_recover(handle, err, 0);
		}

		/* still not good */
		if (err) {
        	fprintf(stderr, "%s,%d: snd_pcm_readi failed\n",	
				__FILE__, __LINE__);
			snd_pcm_close(handle);
			free(buffer);
			fclose(f);
			return err;
		}

		fwrite(buffer, size, 1, f);
	}

	/*
	 * Clean up
	 */
	fclose(f);
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	free(buffer);

	return 0;
#endif
}

#if defined __APPLE__

static void sprec_delay_millisec(int ms)
{
	int i;
	for (i = 0; i < ms; i++) {
		usleep(1000);
	}
}

static void sprec_setup_audio_format(sprec_wav_header *hdr, AudioStreamBasicDescription *desc)
{
	desc->mFormatID = kAudioFormatLinearPCM;
	desc->mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
	desc->mReserved = 0;
	desc->mSampleRate = hdr->sample_rate;
	desc->mChannelsPerFrame = hdr->number_of_channels;
	desc->mBitsPerChannel = hdr->bits_per_sample;
	desc->mBytesPerFrame = hdr->bytes_per_frame;
	desc->mFramesPerPacket = 1;
	desc->mBytesPerPacket = desc->mFramesPerPacket * desc->mBytesPerFrame;
}

static void sprec_handle_input_buffer(
	void *data,
	AudioQueueRef inAQ,
	AudioQueueBufferRef buffer,
	const AudioTimeStamp *start_time,
	UInt32 num_packets,
	const AudioStreamPacketDescription *desc
)
{
	/*
	 * "Audio data received" callback
	 */
	sprec_record_state *aq_data = data;

	if (!num_packets && aq_data->data_format.mBytesPerPacket) {
		num_packets = buffer->mAudioDataByteSize / aq_data->data_format.mBytesPerPacket;
	}

	/*
	 * Write the PCM chunk into the file
	 */
	if (AudioFileWritePackets(
		aq_data->audio_file,
		false,
		buffer->mAudioDataByteSize,
		desc,
		aq_data->current_packet,
		&num_packets,
		buffer->mAudioData
	) == noErr) {
		aq_data->current_packet += num_packets;
		AudioQueueEnqueueBuffer(aq_data->queue, buffer, 0, NULL);
	}
}

static void sprec_calculate_buffsize(
	AudioQueueRef audio_queue,
	AudioStreamBasicDescription desc,
	Float64 seconds,
	UInt32 *buff_size
)
{
	/*
	 * Constrain the maximum buffer size to 50 kB
	 */
	const int max_buf_size = 0x50000;
	int max_packet_size = desc.mBytesPerPacket;

	if (max_packet_size == 0) {
		UInt32 packet_size_size = sizeof max_packet_size;
		AudioQueueGetProperty(
			audio_queue,
			kAudioConverterPropertyMaximumOutputPacketSize,
			&max_packet_size,
			&packet_size_size
		);
	}

	Float64 buff_size_tmp = desc.mSampleRate * max_packet_size * seconds;
	*buff_size = (UInt32)((buff_size_tmp < max_buf_size) ? buff_size_tmp : max_buf_size);
}

#endif /* __APPLE__ */
