/**
 * @file mpi_ao.h
 * @brief Header file for audio output (AO) module interface.
 *
 * This file provides the declarations for initializing, deinitializing,
 * and sending data to the audio output module.
 */

/**
 * @brief Sends audio data to the audio output module.
 *
 * @param data Pointer to the audio data buffer.
 * @param len Length of the audio data buffer in bytes.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int sendData(void* data, int len);

/**
 * @brief Initializes the audio output module.
 *
 * @param s32DevId Device ID of the audio output module.
 * @param s32ChnNum Number of audio channels.
 * @param s32SampleRate Sample rate of the audio output in Hz.
 * @param s32BitWidth Bit width of the audio samples.
 * @param s32ReSmpSampleRate Resampling sample rate in Hz (if applicable).
 * @return int Returns 0 on success, or a negative value on failure.
 */
int init_ao(int s32DevId, int s32ChnNum, int s32SampleRate, int s32BitWidth, int s32ReSmpSampleRate);

/**
 * @brief Deinitializes the audio output module.
 *
 * @return int Returns 0 on success, or a negative value on failure.
 */
int deinit_ao();