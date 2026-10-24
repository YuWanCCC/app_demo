
/**
 * @file mpi_ai.h
 * @brief Header file for AI (Audio Input) module interface.
 *
 * This file provides the interface for initializing, deinitializing, and managing
 * audio input functionality, including callback handling for audio data.
 */

#ifndef __MPI_AI_H__
#define __MPI_AI_H__

/**
 * @typedef AIDATA
 * @brief Callback function type for processing audio data.
 *
 * This callback is invoked with audio data and its length.
 * 
 * @param s Pointer to the audio data buffer.
 * @param len Length of the audio data buffer.
 */
typedef void (*AIDATA)(unsigned char *s, int len);

/**
 * @brief Initializes the audio input module.
 *
 * This function sets up the audio input with the specified parameters and registers
 * a callback function to handle incoming audio data.
 *
 * @param channel Number of audio channels (e.g., mono = 1, stereo = 2).
 * @param sampleRate Sampling rate of the audio input (e.g., 44100 Hz).
 * @param bitWidth Bit width of the audio samples (e.g., 16 for 16-bit audio).
 * @param devId Device ID of the audio input hardware.
 * @param frame_time_ms Frame duration in milliseconds for audio data processing.
 * @param func Callback function to handle audio data.
 * @return 0 on success, or a negative error code on failure.
 */
int init_ai(int channel, int sampleRate, int bitWidth, int devId, int frame_time_ms, AIDATA func);

/**
 * @brief Deinitializes the audio input module.
 *
 * This function releases resources and stops the audio input functionality.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int deinit_ai();

/**
 * @brief Retrieves the status of the audio input data thread.
 *
 * This function checks if the audio input data thread is running.
 *
 * @return 1 if the thread is running, 0 if not, or a negative error code on failure.
 */
void getDataThread();

#endif