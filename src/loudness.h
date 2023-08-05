#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct loudness loudness_t;

loudness_t *loudness_create(int track);
void loudness_destroy(loudness_t *);

/** \brief Get the loudness calculation results.
 *
 * @param loudness The context.
 * @param results An array that the results will be written. These values will be written in order.
 *   - momentary loudness
 *   - short term loudness
 *   - integrated loudness
 *   - LRA
 *   - peak
 * @param flags Indicates which data to get. Available options are as below.
 *   - LOUDNESS_GET_SHORT returns momentary and short term loudness.
 *   - LOUDNESS_GET_LONG returns integrated loudness, LRA, peak.
 */
#define LOUDNESS_GET_SHORT (1 << 0)
#define LOUDNESS_GET_LONG (1 << 1)
void loudness_get(loudness_t *loudness, double results[5], uint32_t flags);

void loudness_set_pause(loudness_t *loudness, bool paused);
void loudness_reset(loudness_t *loudness);

#ifdef __cplusplus
} // extern "C"
#endif
