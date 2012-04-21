/**
 * record_apple.m
 * libsprec
 * 
 * Created by Árpád Goretity (H2CO3)
 * on Thu 19/04/2012
**/

#include <Foundation/Foundation.h>
#include <CoreAudio/CoreAudio.h>
#include <AVFoundation/AVFoundation.h>
#include "record_apple.h"

#ifdef __APPLE__
void sprec_record_wav_apple(const char *filename, struct sprec_wav_header *hdr, uint32_t duration_ms)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	NSDictionary *settings = [[NSDictionary alloc] initWithObjectsAndKeys:
		[NSNumber numberWithInt:kAudioFormatLinearPCM], AVFormatIDKey,
		[NSNumber numberWithFloat:(float)(hdr->sample_rate)], AVSampleRateKey,
		[NSNumber numberWithInt:hdr->number_of_channels], AVNumberOfChannelsKey,
		NULL];
	
	/**
	 * Start recording audio
	**/
	NSURL *fileURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename]];
	AVAudioRecorder *recorder = [[AVAudioRecorder alloc] initWithURL:fileURL settings:settings error:NULL];
	[settings release];
	[recorder record];
	
	/**
	 * Loop until the desired time elapses
	**/
	NSTimer *tmr = [NSTimer scheduledTimerWithTimeInterval:1.0 target:NULL selector:NULL userInfo:NULL repeats:YES];
	[[NSRunLoop currentRunLoop] addTimer:tmr forMode:NSDefaultRunLoopMode];
	NSDate *stopDate = [[NSDate date] dateByAddingTimeInterval:(NSTimeInterval)(duration_ms / 1000.0)];
	[[NSRunLoop currentRunLoop] runUntilDate:stopDate];
	
	/**
	 * Stop recording and clean up
	**/
	[recorder stop];
	[recorder release];
	[pool release];
}
#endif

