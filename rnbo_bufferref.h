#ifndef _RNBO_BUFFERREF_H
#define _RNBO_BUFFERREF_H

//RNBO.h seems to have to come first in visual studio or std::numeric_limits<_>::max() breaks..
#include <RNBO.h>
#include <memory>

#include <ext.h>
#include <ext_obex.h>
#include <ext_buffer.h>

// Max keeps one class per name for the whole process and every RNBO external registers this
// class, so the name doubles as the layout version: externals built before and after a change
// to the struct (or to the new/free/notify methods) must not resolve to each other's class.
// Bump the suffix whenever the struct or those methods change.
#define RNBO_BUFFERREF_CLASSNAME "rnbo_bufferref_v2"

extern "C" {
	// the dataref object which is used for managing all references
	// to internal Max buffers that are used by RNBO
	typedef struct _rnbo_bufferref {
		t_object		obj;
		t_buffer_ref	*r_buffer_ref;
		t_symbol 		*r_name;
		float			*r_lastKnownAddress; // The Max sample pointer both sides agreed on at the last sync
		t_buffer_obj	*r_object; // The buffer~ the ref resolved to at lock time, used for the rest of the vector (the ref can be re-pointed from the main thread)
		t_buffer_obj	*r_lockedObject; // The buffer~ we locked, so unlock hits the same object even if the ref was re-pointed meanwhile
		// RNBO allocated its own memory (sized buffer~, preset restore) and we asked the buffer~
		// to take that shape so we can copy into it and borrow it. The request is bookkept by
		// target object, the shape we asked for (and the one before it, so an older request
		// landing late is not mistaken for someone else changing the buffer), what the buffer~
		// reported when we asked (so we can tell "unchanged" from "changed by someone else"),
		// and when we asked (a resize that failed inside buffer~ is retried).
		t_buffer_obj	*r_reqObject;
		long			r_requestedFrames;
		long			r_requestedChannels;
		long			r_prevRequestedFrames;
		long			r_prevRequestedChannels;
		float			*r_reqObservedSamples;
		long			r_reqObservedFrames;
		long			r_reqObservedChannels;
		double			r_reqTimeMs;
		char			r_resizeRequested;
		char			r_islocked;
		char			r_markDirty; // we wrote into the Max buffer this vector, mark it dirty once unlocked
	} t_rnbo_bufferref;

	void rnbo_bufferref_register();
	t_rnbo_bufferref *rnbo_bufferref_new(t_symbol *buffername);
	void rnbo_bufferref_free(t_rnbo_bufferref *x);
	t_max_err rnbo_bufferref_notify(t_rnbo_bufferref *x, t_symbol *s, t_symbol *msg, void *sender, void *data);
	float *rnbo_bufferref_lock(t_rnbo_bufferref *x);
	char rnbo_bufferref_islocked(t_rnbo_bufferref *x);
	void rnbo_bufferref_unlock(t_rnbo_bufferref *x);
	void rnbo_bufferref_setdirty(t_rnbo_bufferref *x);
	void rnbo_bufferref_setname(t_rnbo_bufferref *x, t_symbol * name);
	t_symbol * rnbo_bufferref_getname(t_rnbo_bufferref *x);
	void rnbo_bufferref_setlastaddress(t_rnbo_bufferref *x, float *lastAddress);
	float *rnbo_bufferref_getlastaddress(t_rnbo_bufferref *x);
	bool rnbo_bufferref_buffer_exists(t_rnbo_bufferref *x);
	t_buffer_obj *rnbo_bufferref_getobject(t_rnbo_bufferref *x); // the object resolved by the last rnbo_bufferref_lock
	// ask the buffer~ to take this shape (deferred by buffer~ to the main thread), recording what
	// the buffer~ looked like when we asked. Returns false if the same request is already
	// outstanding for this object or the buffer~ cannot resize. Call while unlocked.
	bool rnbo_bufferref_request_resize(t_rnbo_bufferref *x, t_buffer_obj *b, long frames, long channels,
			float *observedSamples, long observedFrames, long observedChannels);
	bool rnbo_bufferref_resize_requested(t_rnbo_bufferref *x);
	void rnbo_bufferref_clear_resize_request(t_rnbo_bufferref *x);
}

namespace RNBO {
	void DataRefBindMaxBuffer(
			ExternalDataIndex dataRefIndex,
			const ExternalDataRef* ref,
			t_rnbo_bufferref *dataref,
			UpdateRefCallback updateDataRef,
			ReleaseRefCallback releaseDataRef
	);
	void DataRefUnbindMaxBuffer(
			const ExternalDataRef* externalRef,
			t_rnbo_bufferref *dataref
	);
};
#endif
