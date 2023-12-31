/*
OBS Loudness Dock
Copyright (C) 2023 Norihiro Kamae <norihiro@nagater.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <obs-module.h>
#include <util/threading.h>
#include "loudness.h"
#include "ebur128.h"
#include "plugin-macros.generated.h"

typedef DARRAY(float) float_array_t;

struct loudness
{
	int track;
	ebur128_state *state;
	pthread_mutex_t mutex;
	float_array_t buf;
	bool paused;
};

void audio_cb(void *param, size_t mix_idx, struct audio_data *data);

static bool init_state(loudness_t *loudness)
{
	struct obs_audio_info oai;
	if (!obs_get_audio_info(&oai))
		return false;

	int mode = EBUR128_MODE_M | EBUR128_MODE_S | EBUR128_MODE_I | EBUR128_MODE_LRA | EBUR128_MODE_TRUE_PEAK;
	loudness->state = ebur128_init(get_audio_channels(oai.speakers), oai.samples_per_sec, mode);
	if (!loudness->state) {
		blog(LOG_ERROR, "Failed to initialize libebur128");
		return false;
	}

	return true;
}

loudness_t *loudness_create(int track)
{
	loudness_t *loudness = bzalloc(sizeof(loudness_t));
	loudness->track = track;

	if (!init_state(loudness)) {
		bfree(loudness);
		return NULL;
	}

	pthread_mutex_init(&loudness->mutex, NULL);

	obs_add_raw_audio_callback(track, NULL, audio_cb, loudness);

	return loudness;
}

void loudness_destroy(loudness_t *loudness)
{
	if (!loudness)
		return;

	obs_remove_raw_audio_callback(loudness->track, audio_cb, loudness);
	if (loudness->state)
		ebur128_destroy(&loudness->state);
	pthread_mutex_destroy(&loudness->mutex);
	da_free(loudness->buf);
	bfree(loudness);
}

#ifdef ENABLE_PROFILE
static const char *name_loudness_get = "loudness_get";
#endif

void loudness_get(loudness_t *loudness, double results[5], uint32_t flags)
{
#ifdef ENABLE_PROFILE
	profile_start(name_loudness_get);
#endif

	pthread_mutex_lock(&loudness->mutex);

	if (loudness->state && (flags & LOUDNESS_GET_SHORT)) {
		ebur128_loudness_momentary(loudness->state, &results[0]);
		ebur128_loudness_shortterm(loudness->state, &results[1]);
	}

	if (loudness->state && (flags & LOUDNESS_GET_LONG)) {
		double peak = 0.0;
		ebur128_loudness_global(loudness->state, &results[2]);
		ebur128_loudness_range(loudness->state, &results[3]);
		for (size_t ch = 0; ch < loudness->state->channels; ch++) {
			double peak_ch;
			if (ebur128_true_peak(loudness->state, ch, &peak_ch) == 0) {
				if (peak_ch > peak)
					peak = peak_ch;
			}
		}
		results[4] = obs_mul_to_db(peak);
	}

	pthread_mutex_unlock(&loudness->mutex);

#ifdef ENABLE_PROFILE
	profile_end(name_loudness_get);
#endif
}

#ifdef ENABLE_PROFILE
static const char *name_audio_cb = "loudness-audio_cb";
#endif

void audio_cb(void *param, size_t mix_idx, struct audio_data *data)
{
#ifdef ENABLE_PROFILE
	profile_start(name_audio_cb);
#endif

	UNUSED_PARAMETER(mix_idx);
	loudness_t *loudness = param;

	pthread_mutex_lock(&loudness->mutex);

	if (loudness->state) {
		const size_t nch = loudness->state->channels;
		da_resize(loudness->buf, nch * data->frames);

		float *array = loudness->buf.array;
		const float **data_in = (const float **)data->data;
		for (size_t iframe = 0, k = 0; iframe < data->frames; iframe++) {
			for (size_t ich = 0; ich < nch; ich++)
				array[k++] = data_in[ich][iframe];
		}

		ebur128_add_frames_float(loudness->state, array, data->frames);
	}

	pthread_mutex_unlock(&loudness->mutex);

#ifdef ENABLE_PROFILE
	profile_end(name_audio_cb);
#endif
}

void loudness_set_pause(loudness_t *loudness, bool paused)
{
	if (paused == loudness->paused)
		return;

	loudness->paused = paused;

	if (paused) {
		obs_remove_raw_audio_callback(loudness->track, audio_cb, loudness);
	}
	else {
		obs_add_raw_audio_callback(loudness->track, NULL, audio_cb, loudness);
	}
}

void loudness_reset(loudness_t *loudness)
{
	pthread_mutex_lock(&loudness->mutex);

	if (loudness->state)
		ebur128_destroy(&loudness->state);
	init_state(loudness);

	pthread_mutex_unlock(&loudness->mutex);
}
