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
 */
void loudness_get(loudness_t *loudness, double results[5]);

void loudness_set_pause(loudness_t *loudness, bool paused);
void loudness_reset(loudness_t *loudness);

#ifdef __cplusplus
} // extern "C"
#endif
