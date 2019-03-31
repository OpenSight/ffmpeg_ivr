# ffmpeg_ivr
ffmpeg_ivr is an extension of FFmpeg library, it provides a library which register a new cloud-oriented media fragment muxer into the ffmpeg avformat framework, and a new application, called ffmpeg_ivr, which make use of the new library. The new muxer is usually used as a video recorder, which is able to upload the media fragments(TS format) to cloud-storage system like Aliyun OSS through HTTP, as well as save the fragments to the local filesystem like XFS。On the other hand, it also send the infomation of each fragment to the specified http server, which can gather these meta-info the generate the corresponding m3u8 list for these fragments.  

> "ivr" suffix comes from our project name, means Internet Video Recorder

## Features 

* Supports media fragments for M3U8 playback format (M3U8 is the most popular video format for internet nowadays).
* Supports almost all video/audio codec.
* Provides a fragment queue to overcome the IO jitter, which can work in block or non-block mode
* A standalone IO thread to handle fragment writing, so that there is no disturbance to the main muxing thread.
* Support various writer type, which includes dummy writer, file writer, and ivr writer by default, and you can register your own one.
* IVR writer can upload the fragments by HTTP, as well as save the fragments to the local file system. 
* Metadata of each fragments is post to the specifiled URL through http in IVR writer. 
* Support pre-allocation and fragment aggregation for local filesystem in IVR writer.

## Dependencies

To install it, you'll need to satisfy the following dependencies:


* [FFmpeg](https://janus.conf.meetecho.com/) (2.8.x version must used, API of later version is not compatible)
* [curl](https://curl.haxx.se/)

## Compilation

After install the dependencies successfully, configure and compile as usual to start the whole compilation process:

	./configure 
	make
	make install

After the above, a library, called libffmpeg_ivr, and a application, named ffmpeg_ivr would be installed into your system at the default path(usually at /usr/local). If you want to change the default path, you can use the following command to re-configure 

	./configure --prefix=/your new path/
  
## Usage
  
After installation, you can use "ffmpeg_ivr" command like the original ffmpeg, but support the new mux format, named "cseg". like:
  
	ffmpeg_ivr -i your_live_video_url -f cseg dummy://dummy
 
which is writing the fragments to a dummy writer(just print the metadata of each segment)

	ffmpeg_ivr -i your_live_video_url -f cseg file://test_media
  
which is writing the fragments to the local file with prefix test_media at the root path.

	ffmpeg_ivr -i your_live_video_url [other_ffmpeg_options] -f cseg [cseg_options] ivr://ivr_service_url
  
which posts the meta info of each fragment to the ivr_service_url and get back the storage url for the corresponding fragment, then upload/save the fragment to this url.



