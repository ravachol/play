#define MINIAUDIO_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include "sound.h"
#include "file.h"
#include "stringextensions.h"
#define CHANNELS 2
#define SAMPLE_RATE 44100
#define SAMPLE_WIDTH 2
#define FRAMES_PER_BUFFER 1024

typedef struct {
    FILE* file;
    ma_decoder* decoder;
} UserData;

int eofReached = 0;
ma_device device = {0};
UserData* userData = NULL;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    if (userData == NULL) {
        return;
    }
    ma_decoder* pDecoder = (ma_decoder*)userData->decoder;
    if (pDecoder == NULL) {
        return;
    }

    ma_uint64 framesRead;
    ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount, &framesRead);

    if (framesRead < frameCount) {
        eofReached = 1;
    }  

    (void)pInput;
}

int playSoundFileDefault(const char* filePath) {
    if (filePath == NULL)
        return -1;

    ma_result result;
    ma_decoder* decoder = (ma_decoder*)malloc(sizeof(ma_decoder));
    if (decoder == NULL) {
        return -1;  // Failed to allocate memory for decoder
    }
    
    result = ma_decoder_init_file(filePath, NULL, decoder);
    if (result != MA_SUCCESS) {
        free(decoder);
        return -2;
    }
    
    if (userData == NULL) {
        userData = (UserData*)malloc(sizeof(UserData));
        if (userData == NULL) {
            ma_decoder_uninit(decoder);
            free(decoder);
            return -3;  // Failed to allocate memory for user data
        }
    }

    userData->decoder = decoder;

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = decoder->outputFormat;
    deviceConfig.playback.channels = decoder->outputChannels;
    deviceConfig.sampleRate = decoder->outputSampleRate;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = userData;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        ma_decoder_uninit(decoder);
        free(decoder);
        return -4;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        ma_decoder_uninit(decoder);
        free(decoder);
        return -5;
    }

    return 0;
}

int convertAacToPcmFile(const char* filePath, const char* outputFilePath)
{
    char ffmpegCommand[1024];
    snprintf(ffmpegCommand, sizeof(ffmpegCommand),
             "ffmpeg -v fatal -hide_banner -nostdin -y -i \"%s\" -f s16le -acodec pcm_s16le -ac %d -ar %d \"%s\"",
             filePath, CHANNELS, SAMPLE_RATE, outputFilePath);

    int res = system(ffmpegCommand);  // Execute FFmpeg command to create the output.pcm file

    if (res != 0) {        
        return -1;
    }

    return 0;
}

void pcm_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    UserData* pUserData = (UserData*)pDevice->pUserData;
    if (pUserData == NULL) {
        return;
    }

    size_t framesRead = fread(pOutput, ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels), frameCount, pUserData->file);
    if (framesRead < frameCount) {
        // If we reached the end of the file, rewind to the beginning
        eofReached = 1;
        //fseek(pUserData->file, 0, SEEK_SET);
    }

    (void)pInput;
}

int playPcmFile(const char* filePath)
{
    FILE* file = fopen(filePath, "rb");
    if (file == NULL) {
        printf("Could not open file: %s\n", filePath);
        return -2;
    }

    userData = (UserData*)malloc(sizeof(UserData));
    userData->file = file;
    ma_device_uninit(&device);
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_format_s16;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate = 44100;
    deviceConfig.dataCallback = pcm_callback;
    deviceConfig.pUserData = userData;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        fclose(file);
        return -3;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        fclose(file);
        return -4;
    }

    return 0;
}

char tempFilePath[FILENAME_MAX];

int playAacFile(const char* filePath)
{
    generateTempFilePath(tempFilePath, "temp", ".pcm");    
    if (convertAacToPcmFile(filePath, tempFilePath) != 0 ) 
    {
        //printf("Failed to run FFmpeg command:\n");
        return -1;
    }

    int ret = playPcmFile(tempFilePath);

    if (ret != 0)
    {
        //printf("Failed to play file: %d\n", ret);
        return -1;
    }

    // delete pcm file

    return 0;
}

int playSoundFile(const char* filePath)
{
  eofReached = 0;
  int ret = playSoundFileDefault(filePath);
  if (ret != 0)
   ret = playAacFile(filePath);
  return ret;
}

int playPlaylist(char* filePath)
{
return 0;
}

int paused = 0;

void resumePlayback()
{
    if (!ma_device_is_started(&device)) {
        ma_device_start(&device);
    } 
}

void pausePlayback()
{

    if (ma_device_is_started(&device)) {
        ma_device_stop(&device);
        paused = 1;
    } else if (paused == 1)
    {
        resumePlayback();
        paused = 0;
    }
}

int isPlaybackDone() {
    return eofReached;
}

void cleanupPlaybackDevice()
{   
    UserData* userData = (UserData*)device.pUserData;
    if (userData != NULL) {
        if (userData->decoder)
            ma_decoder_uninit((ma_decoder*)userData->decoder);
    }
    ma_device_uninit(&device);
    deleteFile(tempFilePath);    
}

void extract_audio_duration(const char* input_filepath, const char* output_filepath) {
    char command[256];
    sprintf(command, "ffmpeg -i \"%s\" -hide_banner 2>&1 | grep Duration > \"%s\"", input_filepath, output_filepath);
    system(command);
}


float get_audio_duration(const char* filepath) {
    FILE* fp;
    char duration[50];

    fp = fopen(filepath, "r");
    if (fp == NULL) {
        printf("Error opening the file.\n");
        return -1.0;
    }

    // Check if the duration line is read successfully
    if (fgets(duration, sizeof(duration), fp) == NULL) {
        printf("Error reading duration information.\n");
        fclose(fp);
        return -1.0;
    }

    fclose(fp);

    float hours, minutes, seconds;
    int scanned = sscanf(duration, "  Duration: %f:%f:%f,", &hours, &minutes, &seconds);

    // Check if the scanning of duration components is successful
    if (scanned != 3) {
        printf("Error parsing duration components.\n");
        return -1.0;
    }

    // Calculate milliseconds separately
    int milliseconds = 0;
    sscanf(duration, "  Duration: %*d:%*d:%*d.%d,", &milliseconds);

    float totalSeconds = (hours * 3600) + (minutes * 60) + seconds + (milliseconds / 1000.0);

    return totalSeconds;
}

float getDuration(const char* filepath, const char* tempFile) {
    extract_audio_duration(filepath, tempFile);
    float duration_seconds = get_audio_duration(tempFile);
    return duration_seconds;
}