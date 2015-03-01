# Libsprec

## Speech recognizer library in C using the Google Speech v2.0 API.

Requires libcurl >= 7.25.0, libflac and libogg.

For iOS, you have to grab these libraries either from Cydia or my web page.
Libflac, libogg and libcurl should be already in your favourite Unix distro's
package management system (OS X and Homebrew are no exception!).

For iOS, you can download the Debian packages from here:

 * [cURL 7.25.0](http://apaczai.elte.hu/~13akga/content/download.php?file=curl_7.25.0_iphoneos-arm.deb)
 * [libogg](http://apaczai.elte.hu/~13akga/content/download.php?file=libogg_1.3.0_iphoneos-arm.deb)
 * [libflac](http://apaczai.elte.hu/~13akga/content/download.php?file=libflac_1.2.1_iphoneos-arm.deb)

Tested on iOS 4.2.1, Ubuntu 11.10 and OS X 10.9.5.

If immediate recording of FLAC audio is possible on a platform, then it should be
done using 16000 samples/second, 2 channels, 16 bit/sample (signed little endian);
and only the functions in `web_client.h` have to be used, e. g.:

 * `sprec_send_audio_data()` takes a pointer to the contents of a FLAC file and
   sends it to the Google Speech API along with the appropriate
   headers and other parameters. Returns the server's response.

If immediate FLAC recording is not available, then the audio should be recorded in
WAV (uncompressed interleaved PCM), 16 bits/sample, signed, little endian, 2
channels and 16000 Hz sample rate). Then the resulting WAV file should be converted
to a FLAC one using `sprec_flac_encode()` from `flac_encoder.h`. Then one can
proceed as described above. You can use the `sprec_record_wav()` function for
recording in the appropriate format.

## The simple API

To simplify this task, two convenience functions, `sprec_recognize_sync()` and
`sprec_recognize_async()` are also available (the latter needs POSIX threads).

See `examples/simple.c` for further API usage information.

The usage of the example program is:

    ./simple <API key> <language code> <duration>

## A word about API keys

The Google Speech v2.0 API requires an API key, and rate-limits the application to
50 keys per day. In order to obtain an API key, follow the instructions at
[http://www.chromium.org/developers/how-tos/api-keys](http://www.chromium.org/developers/how-tos/api-keys),
except that you *don't have to* make an OAuth key (a browser/server/iOS app key under
the tab "Public API Access" does the job just as well).

## Response format

Some people [complained](https://raspberrypi.stackexchange.com/questions/10384/speech-processing-on-the-raspberry-pi/10392#10392)
that libsprec depends on my libjsonz library. I've thus removed the dependency.
I tried to use yajl instead, but Google's JSON response is in a very, um, particular
format (it's sometimes two JSON objects separated by a newline), and yajl doesn't
seem to be able to digest it. So libsprec now just spits out verbatim the JSON
it has got, and it's the developer's responsibility to make sense of it.
