/**
 * @file webrtc_aec.c  WebRTC Acoustic Echo Cancellation
 *
 * Copyright (C) 2010 Creytiv.com
 * Copyright (C) 2017 Jonathan Sieber
 */
 
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern "C" {
#include <re.h>
#include <baresip.h>
}

#include <webrtc_audio_processing/module_common_types.h>
#include <webrtc_audio_processing/audio_processing.h>


/**
 * @defgroup webrtc_aec webrtc_aec
 *
 * Acoustic Echo Cancellation (AEC) from libwebrtc-audio-processing
 */


struct webrtc_st {
	webrtc::AudioProcessing* apm;
	int ptime;
	int sample_rate_hz;
};

struct enc_st {
	struct aufilt_enc_st af;  /* base class */
	struct webrtc_st *st;
};

struct dec_st {
	struct aufilt_dec_st af;  /* base class */
	struct webrtc_st *st;
};


static void enc_destructor(void *arg)
{
	struct enc_st *st = (enc_st*)arg;

	list_unlink(&st->af.le);
	mem_deref(st->st);
}


static void dec_destructor(void *arg)
{
	struct dec_st *st = (dec_st*)arg;

	list_unlink(&st->af.le);
	mem_deref(st->st);
}

static void webrtc_aec_destructor(void *arg)
{
	struct webrtc_st *st = (webrtc_st*)arg;

	if (st->apm) {
		webrtc::AudioProcessing::Destroy(st->apm);
		st->apm = 0;
	}
	
	// TODO: Anything to delete?
}


static int aec_alloc(struct webrtc_st **stp, void **ctx, struct aufilt_prm *prm)
{
	struct webrtc_st *st;
	uint32_t sampc;
	int err, tmp, fl;

	if (!stp || !ctx || !prm)
		return EINVAL;

	if (*ctx) {
		*stp = (webrtc_st*)mem_ref(*ctx);
		return 0;
	}

	st = (webrtc_st*)mem_zalloc(sizeof(*st), webrtc_aec_destructor);
	if (!st)
		return ENOMEM;
		
	st->apm = webrtc::AudioProcessing::Create(0);

	err = st->apm->set_sample_rate_hz(prm->srate);
	
	if (err) {
		// webrtc-audio-processing support 32kHz max.
		error("webrtc_aec: Could not set sample rate %d\n", prm->srate);
		return EINVAL;
	}
	
	// A higher number of input channels can be used, and processed
	// to fewer output channels, to do beam-forming and stuff
	st->apm->set_num_channels(prm->ch, prm->ch); // Input = Output channels
	st->apm->set_num_reverse_channels(prm->ch);
	
	// Drift can occur when different audio clock sources are involved
	// For example, input from USB Webcam Mic and output on soundcard
	// Disabled for now
	st->apm->echo_cancellation()->enable_drift_compensation(false);
	// Must be used in combination with set_stream_drift_samples()
	// from encode(), when a difference in samples played and recorded is detected
	
	// High-Pass-Filter compensates lower frequency limit of cancellation algorithm
	st->apm->high_pass_filter()->Enable(true);
	
	//st->apm->noise_suppression()->set_level(webrtc::NoiseSuppression::kVeryHigh);
	//st->apm->noise_suppression()->Enable(true);

	//st->apm->gain_control()->set_analog_level_limits(0, 255);
	//st->apm->gain_control()->set_mode(webrtc::GainControl::kAdaptiveDigital);
	// For this mode, we'd need to mess with the audio hw mixer
	// apm->gain_control()->set_mode(kAdaptiveAnalog);
	//apm->gain_control()->Enable(true);

	// Could baresip make use of this?
	// apm->voice_detection()->Enable(true);
	
	
	// That's why we're here
	// st->apm->echo_cancellation()->Enable(true);
	
	// Found this in pulseaudio webrtc_aec code, maybe it helps
	//st->apm->echo_cancellation()->set_device_sample_rate_hz(prm->srate);
	
	// Try the alternative mobile algorithm
	
	//apm->echo_control_mobile()->set_routing_mode( webrtc::EchoControlMobile::kEarpiece);
    //st->apm->echo_control_mobile()->enable_comfort_noise(true);
    
	st->apm->echo_control_mobile()->Enable(true);
	
	st->sample_rate_hz = prm->srate;
	st->ptime = prm->ptime;
	if (prm->ptime % 10) {
		error("webrtc_aec: ptime must be a multiple of 10 ms\n");
		return EINVAL;
	}

	info("webrtc_aec: WebRTC AEC loaded: srate = %uHz, "
		"num_channels = %d, ptime=%d\n",
		prm->srate, prm->ch, prm->ptime);
		
out:
	if (err)
		mem_deref(st);
	else
		*ctx = *stp = st;

	return err;
}


static int encode_update(struct aufilt_enc_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm)
{
	struct enc_st *st;
	int err;

	if (!stp || !ctx || !af || !prm)
		return EINVAL;

	if (*stp)
		return 0;

	st = (enc_st*)mem_zalloc(sizeof(*st), enc_destructor);
	if (!st)
		return ENOMEM;

	err = aec_alloc(&st->st, ctx, prm);

	if (err)
		mem_deref(st);
	else
		*stp = (struct aufilt_enc_st *)st;

	return err;
}


static int decode_update(struct aufilt_dec_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm)
{
	struct dec_st *st;
	int err;

	if (!stp || !ctx || !af || !prm)
		return EINVAL;

	if (*stp)
		return 0;

	st = (dec_st*)mem_zalloc(sizeof(*st), dec_destructor);
	if (!st)
		return ENOMEM;

	err = aec_alloc(&st->st, ctx, prm);

	if (err)
		mem_deref(st);
	else
		*stp = (struct aufilt_dec_st *)st;

	return err;
}


static int encode(struct aufilt_enc_st *st, int16_t *sampv, size_t *sampc)
{
	int i;
	struct enc_st *est = (struct enc_st *)st;
	struct webrtc_st *sp = est->st;
	
	const size_t sample_rate_hz = sp->sample_rate_hz;
	const size_t samples = sample_rate_hz / 100; // 10 ms
	webrtc::AudioFrame frame;
		
	for (i = 0; i < sp->ptime; i += 10) {
		// TODO: First two values should contain RTP id and timestamp
		frame.UpdateFrame(0, 0, sampv, samples,
			sample_rate_hz,
			webrtc::AudioFrame::kUndefined,
			webrtc::AudioFrame::kVadUnknown);
		
		// The exact total delay of a sample between playout and recording
		// No idea how to calculate this
		// For unknown reasons, this needs to be reset after every decode()
		sp->apm->set_stream_delay_ms(0);
	
		int ret = sp->apm->ProcessStream(&frame);
		
		if (ret) {
			warning("webrtc_aec: AudioProcessing::ProcessStream "
				"returned unexpected value %d\n", ret);
			warning("stream delay set %i ms\n", 
				sp->apm->stream_delay_ms());
		}
		
		memcpy(sampv, frame._payloadData, samples);
		
		sampv += samples;
	}
	
	
	return 0;
}


static int decode(struct aufilt_dec_st *st, int16_t *sampv, size_t *sampc)
{
	struct dec_st *dst = (struct dec_st *)st;
	struct webrtc_st *sp = dst->st;

	if (*sampc) {
		const size_t samples = sp->sample_rate_hz / 100; // 10 ms
		webrtc::AudioFrame frame;
		
		for (int i = 0; i < sp->ptime; i += 10) {
			// TODO: First two values should contain RTP id and timestamp
			frame.UpdateFrame(0, 0, sampv, samples,
				sp->sample_rate_hz,
				webrtc::AudioFrame::kUndefined,
				webrtc::AudioFrame::kVadUnknown);
			
			int ret = sp->apm->AnalyzeReverseStream(&frame);
			if (ret) {
				warning("webrtc_aec: "
					"AudioProcessing::AnalyzeReverseStream "
					"returned unexpected value %d\n", ret);
			}
			
			// Workaround so AnalyzeReverseStream doesn't reset the was_stream_set variable
		}
	}

	return 0;
}


static struct aufilt webrtc_aec = {
	LE_INIT, "webrtc_aec", encode_update, encode, decode_update, decode
};


static int module_init(void)
{
	aufilt_register(baresip_aufiltl(), &webrtc_aec);
	return 0;
}


static int module_close(void)
{
	aufilt_unregister(&webrtc_aec);
	return 0;
}

extern "C"
EXPORT_SYM const struct mod_export DECL_EXPORTS(speex_aec) = {
	"webrtc_aec",
	"filter",
	module_init,
	module_close
};
