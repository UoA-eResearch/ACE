/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "nvacl_defs.h"

/**
 * Audio input is required to be 16 kHz
 */
#define AUDIO_SAMPLE_RATE 16000

/**
 * Audio input is required to be single channel mono
 */
#define AUDIO_CHANNELS 1

/**
 * Internally nvacl attempts to send audio over the network in 500 ms chunks, or 8000 samples
 */
#define AUDIO_PREFERRED_CHUNK_SIZE (AUDIO_SAMPLE_RATE / 2)

typedef enum NvACEStatus {
  NV_ACE_STATUS_OK = 0,
  /**
   * signals end of anim data to a callback
   */
  NV_ACE_STATUS_OK_NO_MORE_FRAMES,
  NV_ACE_STATUS_ERROR_UNKNOWN,
  /**
   * error communicating with service
   */
  NV_ACE_STATUS_ERROR_CONNECTION,
  /**
   * invalid input passed to nvacl
   */
  NV_ACE_STATUS_ERROR_INVALID_INPUT,
  /**
   * received output from service that we couldn't handle
   */
  NV_ACE_STATUS_ERROR_UNEXPECTED_OUTPUT,
} NvACEStatus;

typedef struct NvACEA2XConnection NvACEA2XConnection;

/**
 * Parameters for updating the facial characteristics
 */
typedef struct NvACEA2XParameters NvACEA2XParameters;

typedef struct NvACEA2XSession NvACEA2XSession;

/**
 * Handle to an instance of the ACE Client Library
 * TODO: Eventually add a way to register a logging callback
 */
typedef struct NvACEClientLibrary NvACEClientLibrary;

typedef struct NvACEConnectionInfo {
  const char *dest_uri;
  const char *api_key;
  const char *nvcf_function_id;
  const char *nvcf_function_version;
} NvACEConnectionInfo;

/**
 * One frame of animation data.
 * blend_shape_names is array of null-terminated UTF-8 strings.
 * audio_samples is 16 kHz mono 16-bit signed integer samples.
 * Use status to determine how to treat contents of frame.
 * - NV_ACE_STATUS_OK: normal frame
 * - NV_ACE_STATUS_OK_NO_MORE_FRAMES: dummy frame to indicate callback will not be called again for this session
 * - NV_ACE_STATUS_ERROR_UNEXPECTED_OUTPUT: something unusual found in data received from a2x service
 */
typedef struct NvACEAnimDataFrame {
  const char *const *blend_shape_names;
  size_t blend_shape_name_count;
  const float *blend_shape_weights;
  size_t blend_shape_weight_count;
  const int16_t *audio_samples;
  size_t audio_sample_count;
  double timestamp;
  enum NvACEStatus status;
} NvACEAnimDataFrame;

/**
 * Application context
 * Opaque to nvacl, passed through to callback when a frame of animation data is received
 */
typedef void *AnimDataContext;

/**
 * Called when a frame of blend shape and audio data is available. You provide this and it must be re-entrant.
 * There is no guarantee that nvacl will have completed a previous callback before calling with a new frame of data.
 * When no frames are left in the current sequence, the frame will have a status of NV_ACE_STATUS_OK_NO_MORE_FRAMES.
 */
typedef void (*AnimDataCallback)(const struct NvACEAnimDataFrame *frame, AnimDataContext context);

/**
 * Each emotion state value must be in the range of 0.0 - 1.0. Values outside that range are ignored.
 * Setting everything to 0.0 is equivalent to neutral emotion.
 */
typedef struct NvACEEmotionState {
  float amazement;
  float anger;
  float cheekiness;
  float disgust;
  float fear;
  float grief;
  float joy;
  float out_of_breath;
  float pain;
  float sadness;
} NvACEEmotionState;

/**
 * Parameters relative to the emotion blending and processing before using it to generate blendshapes
 */
typedef struct NvACEEmotionParameters {
  /**
   * Increases the spread between emotion values by pushing them higher or lower.
   * Default value: 1
   * Min: 0.3
   * Max: 3
   */
  float emotion_contrast;
  /**
   * Coefficient for smoothing emotions over time
   *  0 means no smoothing at all (can be jittery)
   *  1 means extreme smoothing (emotion values not updated over time)
   * Default value: 0.7
   * Min: 0
   * Max: 1
   */
  float live_blend_coef;
  /**
   * Activate blending between the preferred emotions (passed as input) and the emotions detected by A2E.
   * Default: True
   */
  bool enable_preferred_emotion;
  /**
   * Sets the strength of the preferred emotions (passed as input) relative to emotions detected by A2E.
   * 0 means only A2E output will be used for emotion rendering.
   * 1 means only the preferred emotions will be used for emotion rendering.
   * Default value: 0.5
   * Min: 0
   * Max: 1
   */
  float preferred_emotion_strength;
  /**
   * Sets the strength of generated emotions relative to neutral emotion.
   * This multiplier is applied globally after the mix of emotion is done.
   * If set to 0, emotion will be neutral.
   * If set to 1, the blend of emotion will be fully used. (can be too intense)
   * Default value: 0.6
   * Min: 0
   * Max: 1
   */
  float emotion_strength;
  /**
   * Sets a firm limit on the quantity of emotion sliders engaged by A2E
   * emotions with highest weight will be prioritized
   * Default value: 3
   * Min: 1
   * Max: 6
   */
  int32_t max_emotions;
} NvACEEmotionParameters;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Return the version string that was built into NvACL at compile time.
 */
const char *nvace_get_version(void);

/**
 * Create an instance of the ACE Client library
 *
 * You should save the NvACEClientLibrary pointer and release it with nvace_release_client_library.
 * You probably only want to call this once, because each instance will create its own thread pool for background work.
 */
enum NvACEStatus nvace_create_client_library(struct NvACEClientLibrary **handle_ptr);

/**
 * Release an NvACEClientLibrary that was created with nvace_create_client_library
 *
 * When this function completes, it is guaranteed that no asynchronous work spawned by the anim data receiver is still running.
 * So don't invoke this from your AnimDataCallback or you'll deadlock.
 */
enum NvACEStatus nvace_release_client_library(struct NvACEClientLibrary *acl);

/**
 * Create a connection to the a2x service
 *
 * Blocks until connection is established, or returns NV_ACE_STATUS_ERROR_CONNECTION if one can't be established.
 * You should save the NvACEA2XConnection pointer and release it with nvace_release_a2x_connection.
 * Must specify scheme (http or https), host, and port with dest_uri in the NvACEConnectionInfo struct,
 * and optionally provide NVCF connection parameters as well.
 * example dest_uri strings:
 * "http://12.34.567.89:52000"
 * "https://a2f.example.com:52010"
 * "https://grpc.nvcf.nvidia.com:443"
 */
enum NvACEStatus nvace_create_a2x_connection(const struct NvACEClientLibrary *self,
                                             const struct NvACEConnectionInfo *connection_info,
                                             struct NvACEA2XConnection **connection_ptr);

/**
 * Release a connection that was created with nvace_create_a2x_connection
 *
 * TODO: document async details
 */
enum NvACEStatus nvace_release_a2x_connection(struct NvACEA2XConnection *connection);

/**
 * Create an a2x session
 *
 * Blocks until session is established, or returns NV_ACE_STATUS_ERROR_CONNECTION if one can't be established.
 * A session allows sending a stream of audio samples and getting callbacks with anim data frames.
 * Use nvace_send_audio_samples to send samples, and nvace_close_a2x_session to indicate end of samples.
 */
enum NvACEStatus nvace_create_a2x_session(const struct NvACEClientLibrary *self,
                                          struct NvACEA2XConnection *connection,
                                          AnimDataCallback callback,
                                          AnimDataContext context,
                                          struct NvACEA2XSession **session_ptr);

/**
 * Send a chunk of audio samples to an a2x session, optionally setting a new emotion state.
 *
 * This call is non-blocking. Samples will be copied and then sent in the background.
 * You may call this as many times as needed using whatever sample chunk size is convenient for your application.
 * Internally, the audio samples will be buffered and sent to the a2x service in chunks of AUDIO_PREFERRED_CHUNK_SIZE.
 *
 * The first call needs to be at least AUDIO_PREFERRED_CHUNK_SIZE for the stream to start or the function will return NV_ACE_STATUS_ERROR_INVALID_INPUT
 *
 * If emotion is null, the a2x service will keep the previous emotion state from this session. Default is neutral emotion.
 * Use nvace_close_a2x_session to indicate end of audio samples.
 */
enum NvACEStatus nvace_send_audio_samples(struct NvACEClientLibrary *self,
                                          struct NvACEA2XConnection *connection,
                                          struct NvACEA2XSession *session,
                                          const int16_t *samples,
                                          size_t sample_count,
                                          const struct NvACEEmotionState *emotion,
                                          const struct NvACEEmotionParameters *emotion_params,
                                          const struct NvACEA2XParameters *params);

/**
 * Release an a2x session handle.
 *
 * This call is non-blocking if abort_session is false, or blocking if abort_session is true.
 *
 * If abort_session is false, any unsent data will still be sent in the background, and any unreceived data will still
 * be sent to the application in future callbacks.
 *
 * If abort_session is true, any audio samples that haven't been sent to the a2x service yet will be dropped, and any
 * new character data coming back from the a2x service will be ignored. If abort_session is true, it is guaranteed that
 * no more application callbacks will be sent to the application after this call completes.
 * So don't invoke this from your AnimDataCallback or you'll deadlock.
 *
 * It is an error to use the NvACEA2XSession pointer after calling this function.
 */
enum NvACEStatus nvace_release_a2x_session(const struct NvACEClientLibrary *self,
                                           struct NvACEA2XSession *session,
                                           bool abort_session);

/**
 * Create a2x params
 *
 * Creates an instance holding a2x parameters.
 * Parameters are key value pairs that can be set/cleared using nvace_set_a2x_param and nvace_clear_a2x_param
 * Use NvACEA2XParameters when sending audio data with nvace_send_audio_samples.
 * Release parameter instance with nvace_release_a2x_params
 */
enum NvACEStatus nvace_create_a2x_params(struct NvACEA2XParameters **handle_ptr);

/**
 * Release a2x params
 * Release parameters that was created with nvace_create_a2x_params
 */
enum NvACEStatus nvace_release_a2x_params(struct NvACEA2XParameters *params);

/**
 * Set a2x parameter value
 * Set parameter with key (string) to value (float).
 * If the key already exists the key will be updated
 */
void nvace_set_a2x_param(struct NvACEA2XParameters *self, const char *key, float value);

/**
 * Clean a2x parameter value
 * Remove the parameter key (string) from parameters
 */
void nvace_clear_a2x_param(struct NvACEA2XParameters *self, const char *key);

/**
 * Indicate end of outgoing samples for an a2x session.
 *
 * This call is non-blocking.
 * It is an error to send more samples using the NvACEA2XSession pointer after calling this function.
 * In that case you'd receive NV_ACE_STATUS_ERROR_INVALID_INPUT from nvace_send_audio_samples.
 */
enum NvACEStatus nvace_close_a2x_session(struct NvACEA2XSession *session);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
