/**
 * @file omx.c     Raspberry Pi VideoCoreIV OpenMAX interface
 *
 * Copyright (C) 2016 - 2017 Creytiv.com
 * Copyright (C) 2016 - 2017 Jonathan Sieber
 */

#include "omx.h"

#include <re/re.h>
#include <rem/rem.h>
#include <baresip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Avoids a VideoCore header warning about clock_gettime() */
#include <time.h>
#include <sys/time.h>

/**
 * @defgroup omx omx
 *
 * TODO:
 *  * Proper sync OMX events across threads, instead of busy waiting
 */

#ifdef RASPBERRY_PI
static const int VIDEO_RENDER_PORT = 90;
#else
static const int VIDEO_RENDER_PORT = 0;
#endif

/*
static void setHeader(OMX_PTR header, OMX_U32 size) {
  OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
  *((OMX_U32*)header) = size;

  ver->s.nVersionMajor = VERSIONMAJOR;
  ver->s.nVersionMinor = VERSIONMINOR;
  ver->s.nRevision = VERSIONREVISION;
  ver->s.nStep = VERSIONSTEP;
}
* */


static OMX_ERRORTYPE EventHandler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
	OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2,
	OMX_PTR pEventData)
{
	(void) hComponent;

	switch (eEvent) {

	case OMX_EventCmdComplete:
		debug("omx.EventHandler: Previous command completed\n"
		      "d1=%x\td2=%x\teventData=%p\tappdata=%p\n",
		      nData1, nData2, pEventData, pAppData);
		/* TODO: Put these event into a multithreaded queue,
		 * properly wait for them in the issuing code */
		break;

	case OMX_EventError:
		warning("omx.EventHandler: Error event type "
			"data1=%x\tdata2=%x\n", nData1, nData2);
		break;

	default:
		warning("omx.EventHandler: Unknown event type %d\t"
			"data1=%x data2=%x\n", eEvent, nData1, nData2);
		return -1;
		break;
	}

	return 0;
}


static OMX_ERRORTYPE EmptyBufferDone(OMX_HANDLETYPE hComponent,
				     OMX_PTR pAppData,
				     OMX_BUFFERHEADERTYPE* pBuffer)
{
	(void) hComponent;
	(void) pAppData;
	(void) pBuffer;

	/* TODO: Wrap every call that can generate an event,
	 * and panic if an unexpected event arrives */
	return 0;
}


static OMX_ERRORTYPE FillBufferDone(OMX_HANDLETYPE hComponent,
	OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer)
{
	(void) hComponent;
	(void) pAppData;
	(void) pBuffer;
	debug("FillBufferDone\n");
	return 0;
}


static struct OMX_CALLBACKTYPE callbacks = {
	EventHandler,
	EmptyBufferDone,
	FillBufferDone
};


int omx_init(struct omx_state* st)
{
	OMX_ERRORTYPE err;
#ifdef RASPBERRY_PI
	bcm_host_init();
#endif

	st->buffers = NULL;
	err = OMX_Init();
	/* TODO: Handle failure when RPi code runs in emulator */
#ifdef RASPBERRY_PI
	err |= OMX_GetHandle(&st->video_render,
		"OMX.broadcom.video_render", 0, &callbacks);
#else
	err |= OMX_GetHandle(&st->video_render,
		"OMX.st.video.xvideosink", 0, &callbacks);
#endif

	if (!st->video_render || err != 0) {
		error("Failed to create OMX video_render component\n");
		return ENOENT;
	}
	else {
		info("created video_render component\n");
		return 0;
	}
}


/* Some busy loops to verify we're running in order */
static void block_until_state_changed(OMX_HANDLETYPE hComponent,
	OMX_STATETYPE wanted_eState)
{
	OMX_STATETYPE eState;
	unsigned int i = 0;
	while (i++ == 0 || eState != wanted_eState) {
		OMX_GetState(hComponent, &eState);
		if (eState != wanted_eState) {
			sys_usleep(10000);
		}
	}
}

static void block_until_port_changed(OMX_HANDLETYPE hComponent,
	OMX_U32 nPortIndex, OMX_BOOL bEnabled) {

	OMX_ERRORTYPE r;
	OMX_PARAM_PORTDEFINITIONTYPE portdef;
	OMX_U32 i = 0;

	memset(&portdef, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	portdef.nVersion.nVersion = OMX_VERSION;
	portdef.nPortIndex = nPortIndex;

	while (i++ == 0 || portdef.bEnabled != bEnabled) {
		r = OMX_GetParameter(hComponent,
			OMX_IndexParamPortDefinition, &portdef);
		if (r != OMX_ErrorNone) {
			error("block_until_port_changed: OMX_GetParameter "
				" failed with Result=%d\n", r);
		}
		if (portdef.bEnabled != bEnabled) {
			sys_usleep(10000);
		}
	}
}

#define DUMP_CASE(x) case x: return #x;

const char* dump_OMX_ERRORTYPE (OMX_ERRORTYPE error)
{
	switch (error){
		DUMP_CASE (OMX_ErrorNone)
		DUMP_CASE (OMX_ErrorInsufficientResources)
		DUMP_CASE (OMX_ErrorUndefined)
		DUMP_CASE (OMX_ErrorInvalidComponentName)
		DUMP_CASE (OMX_ErrorComponentNotFound)
		DUMP_CASE (OMX_ErrorInvalidComponent)
		DUMP_CASE (OMX_ErrorBadParameter)
		DUMP_CASE (OMX_ErrorNotImplemented)
		DUMP_CASE (OMX_ErrorUnderflow)
		DUMP_CASE (OMX_ErrorOverflow)
		DUMP_CASE (OMX_ErrorHardware)
		DUMP_CASE (OMX_ErrorInvalidState)
		DUMP_CASE (OMX_ErrorStreamCorrupt)
		DUMP_CASE (OMX_ErrorPortsNotCompatible)
		DUMP_CASE (OMX_ErrorResourcesLost)
		DUMP_CASE (OMX_ErrorNoMore)
		DUMP_CASE (OMX_ErrorVersionMismatch)
		DUMP_CASE (OMX_ErrorNotReady)
		DUMP_CASE (OMX_ErrorTimeout)
		DUMP_CASE (OMX_ErrorSameState)
		DUMP_CASE (OMX_ErrorResourcesPreempted)
		DUMP_CASE (OMX_ErrorPortUnresponsiveDuringAllocation)
		DUMP_CASE (OMX_ErrorPortUnresponsiveDuringDeallocation)
		DUMP_CASE (OMX_ErrorPortUnresponsiveDuringStop)
		DUMP_CASE (OMX_ErrorIncorrectStateTransition)
		DUMP_CASE (OMX_ErrorIncorrectStateOperation)
		DUMP_CASE (OMX_ErrorUnsupportedSetting)
		DUMP_CASE (OMX_ErrorUnsupportedIndex)
		DUMP_CASE (OMX_ErrorBadPortIndex)
		DUMP_CASE (OMX_ErrorPortUnpopulated)
		DUMP_CASE (OMX_ErrorComponentSuspended)
		DUMP_CASE (OMX_ErrorDynamicResourcesUnavailable)
		DUMP_CASE (OMX_ErrorMbErrorsInFrame)
		DUMP_CASE (OMX_ErrorFormatNotDetected)
		DUMP_CASE (OMX_ErrorContentPipeOpenFailed)
		DUMP_CASE (OMX_ErrorContentPipeCreationFailed)
		DUMP_CASE (OMX_ErrorSeperateTablesUsed)
		DUMP_CASE (OMX_ErrorTunnelingUnsupported)
		/*DUMP_CASE (OMX_ErrorDiskFull)
		DUMP_CASE (OMX_ErrorMaxFileSize)
		DUMP_CASE (OMX_ErrorDrmUnauthorised)
		DUMP_CASE (OMX_ErrorDrmExpired)
		DUMP_CASE (OMX_ErrorDrmGeneral) */
		default: return "unknown OMX_ERRORTYPE";
	}
}

void change_state (OMX_HANDLETYPE component, OMX_STATETYPE state){
	OMX_ERRORTYPE err;

	if ((err = OMX_SendCommand (component, OMX_CommandStateSet, state,
								0))) {
		error("error: OMX_SendCommand: %s\n",
			dump_OMX_ERRORTYPE (err));
	}
}


void disable_port (OMX_HANDLETYPE component, OMX_U32 port)
{
	OMX_ERRORTYPE err;
	if ((err = OMX_SendCommand (component, OMX_CommandPortDisable,
							port, 0))){
	error("error: OMX_SendCommand: %s\n",
	dump_OMX_ERRORTYPE (err));
  }
}

void enable_port (OMX_HANDLETYPE component, OMX_U32 port)
{
	OMX_ERRORTYPE err;
	if ((err = OMX_SendCommand (component, OMX_CommandPortEnable,
							port, 0))) {
		error("error: OMX_SendCommand: %s\n",
		dump_OMX_ERRORTYPE (err));
  }
}

void omx_deinit(struct omx_state* st)
{
	info("omx_deinit");
	/* TODO: Sanity check if component is really in "LOADED" state */
	OMX_FreeHandle(st->video_render);
	OMX_Deinit();
}


void omx_display_disable(struct omx_state* st)
{
	int i;
	(void)st;

	#ifdef RASPBERRY_PI
	OMX_ERRORTYPE err;
	OMX_CONFIG_DISPLAYREGIONTYPE config;
	memset(&config, 0, sizeof(OMX_CONFIG_DISPLAYREGIONTYPE));
	config.nSize = sizeof(OMX_CONFIG_DISPLAYREGIONTYPE);
	config.nVersion.nVersion = OMX_VERSION;
	config.nPortIndex = VIDEO_RENDER_PORT;
	config.fullscreen = 0;
	config.set = OMX_DISPLAY_SET_FULLSCREEN;

	err = OMX_SetParameter(st->video_render,
		OMX_IndexConfigDisplayRegion, &config);

	if (err != 0) {
		warning("omx_display_disable command failed");
	}

	#endif

	debug("omx: send video_render compontent to StateIdle\n");
	change_state(st->video_render, OMX_StateIdle);
	block_until_state_changed(st->video_render, OMX_StateIdle);

	debug("omx: disable input port of video_render component\n");
	disable_port(st->video_render, VIDEO_RENDER_PORT);

	debug("omx: freeing buffers\n");
	if (st->buffers) {
		for (i = 0; i < st->num_buffers; i++) {
			OMX_FreeBuffer(st->video_render, VIDEO_RENDER_PORT,
				st->buffers[i]);
		}
		free(st->buffers);
		st->buffers = 0;
	}

	block_until_port_changed(st->video_render, VIDEO_RENDER_PORT, false);

//	exit(1);
	debug("omx: send video_render component to StateLoaded\n");
	change_state(st->video_render, OMX_StateLoaded);
	block_until_state_changed(st->video_render, OMX_StateLoaded);
}


int omx_display_enable(struct omx_state* st,
	int width, int height, int stride)
{
	unsigned int i;
	OMX_PARAM_PORTDEFINITIONTYPE portdef;
#ifdef RASPBERRY_PI
	OMX_CONFIG_DISPLAYREGIONTYPE config;
#endif
	OMX_ERRORTYPE err = 0;

	info("omx_update_size %d %d\n", width, height);

	#ifdef RASPBERRY_PI
	memset(&config, 0, sizeof(OMX_CONFIG_DISPLAYREGIONTYPE));
	config.nSize = sizeof(OMX_CONFIG_DISPLAYREGIONTYPE);
	config.nVersion.nVersion = OMX_VERSION;
	config.nPortIndex = VIDEO_RENDER_PORT;
	config.fullscreen = 1;
	config.set = OMX_DISPLAY_SET_FULLSCREEN;

	err |= OMX_SetParameter(st->video_render,
		OMX_IndexConfigDisplayRegion, &config);

	#endif

	memset(&portdef, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	portdef.nVersion.nVersion = OMX_VERSION;
	portdef.nPortIndex = VIDEO_RENDER_PORT;

	/* specify buffer requirements */
	err |= OMX_GetParameter(st->video_render,
		OMX_IndexParamPortDefinition, &portdef);
	if (err != 0) {
		error("omx_display_enable: couldn't retrieve port def\n");
		err = ENOMEM;
		goto exit;
	}

	info("omx port definition: h=%d w=%d s=%d sh=%d\n",
		portdef.format.video.nFrameWidth,
		portdef.format.video.nFrameHeight,
		portdef.format.video.nStride,
		portdef.format.video.nSliceHeight);

	portdef.format.video.nFrameWidth = width;
	portdef.format.video.nFrameHeight = height;
	portdef.format.video.nStride = stride;
	portdef.format.video.nSliceHeight = height;
	portdef.bEnabled = 1;

	err |= OMX_SetParameter(st->video_render,
		OMX_IndexParamPortDefinition, &portdef);

	if (err) {
		error("omx_display_enable: could not set port definition\n");
	}

	//debug("omx: video_render enable input port\n");
	//enable_port(st->video_render, VIDEO_RENDER_PORT);
	block_until_port_changed(st->video_render, VIDEO_RENDER_PORT, true);

	err |= OMX_GetParameter(st->video_render,
		OMX_IndexParamPortDefinition, &portdef);

	if (err != 0 || !portdef.bEnabled) {
		error("omx_display_enable: failed to set up video port\n");
		err = ENOMEM;
		goto exit;
	}

	/* Send the component to 'Idle' state, where buffers can be
	 * allocated. Successful state change will be indicated once
	 * the minimum amount of buffers was allocated. */
	debug("omx: sending video_render component to OMX_StateIdle\n");
	change_state(st->video_render, OMX_StateIdle);

	if (!st->buffers) {
		st->buffers =
			malloc(portdef.nBufferCountActual * sizeof(void*));
		st->num_buffers = portdef.nBufferCountActual;
		st->current_buffer = 0;

		for (i = 0; i < portdef.nBufferCountActual; i++) {
			err = OMX_AllocateBuffer(st->video_render,
				&st->buffers[i], VIDEO_RENDER_PORT,
				st, portdef.nBufferSize);
			if (err) {
				error("OMX_AllocateBuffer failed: %d\n", err);
				err = ENOMEM;
				goto exit;
			}
		}
	}

	block_until_state_changed(st->video_render, OMX_StateIdle);

	debug("omx: video_render to executing state\n");
	change_state(st->video_render, OMX_StateExecuting);
	block_until_state_changed(st->video_render, OMX_StateExecuting);

exit:
	return err;
}


int omx_display_input_buffer(struct omx_state* st,
	void** pbuf, uint32_t* plen)
{
	if (!st->buffers) return EINVAL;

	*pbuf = st->buffers[0]->pBuffer;
	*plen = st->buffers[0]->nAllocLen;

	st->buffers[0]->nFilledLen = *plen;
	st->buffers[0]->nOffset = 0;

	return 0;
}


int omx_display_flush_buffer(struct omx_state* st)
{
	if (OMX_EmptyThisBuffer(st->video_render, st->buffers[0])
		!= OMX_ErrorNone) {
		error("OMX_EmptyThisBuffer error");
	}

	return 0;
}
