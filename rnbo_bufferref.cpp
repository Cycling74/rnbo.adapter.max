#include "rnbo_bufferref.h"

namespace {
	t_class *s_rnbo_bufferref_class = nullptr;
}

extern "C" {

	void rnbo_bufferref_register()
	{
		if (s_rnbo_bufferref_class)
			return;
		auto c = class_new(RNBO_BUFFERREF_CLASSNAME, (method)rnbo_bufferref_new, (method)rnbo_bufferref_free, (long)sizeof(t_rnbo_bufferref), 0L, A_CANT, 0);

		class_addmethod(c, (method)rnbo_bufferref_notify, "notify", A_CANT, 0);

		class_register(CLASS_NOBOX, c);
		s_rnbo_bufferref_class = c;
	}

	t_rnbo_bufferref *rnbo_bufferref_new(t_symbol *buffername)
	{
		t_rnbo_bufferref *x = (t_rnbo_bufferref *)object_alloc(s_rnbo_bufferref_class);
		if (x) {
			x->r_buffer_ref = buffer_ref_new((t_object *)x, buffername);
			x->r_name = buffername;
			x->r_lastKnownAddress = NULL;
			x->r_object = NULL;
			x->r_lockedObject = NULL;
			x->r_reqObject = NULL;
			x->r_requestedFrames = 0;
			x->r_requestedChannels = 0;
			x->r_prevRequestedFrames = 0;
			x->r_prevRequestedChannels = 0;
			x->r_reqObservedSamples = NULL;
			x->r_reqObservedFrames = 0;
			x->r_reqObservedChannels = 0;
			x->r_reqTimeMs = 0;
			x->r_resizeRequested = false;
			x->r_islocked = false;
			x->r_markDirty = false;
		}

		return x;
	}

	void rnbo_bufferref_free(t_rnbo_bufferref *x)
	{
		if (x->r_buffer_ref) object_free(x->r_buffer_ref);
	}

	t_max_err rnbo_bufferref_notify(t_rnbo_bufferref *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
	{
		return buffer_ref_notify(x->r_buffer_ref, s, msg, sender, data);
	}

	float *rnbo_bufferref_lock(t_rnbo_bufferref *x)
	{
		// resolve the ref once per vector: it can be re-pointed from the main thread (attribute set)
		// at any time, and everything we do this vector (lock, size queries, unlock) has to hit
		// the same object
		t_buffer_obj *b = buffer_ref_getobject(x->r_buffer_ref);
		float *smps = buffer_locksamples(b);
		x->r_object = b;
		x->r_islocked = smps != nullptr;
		x->r_lockedObject = x->r_islocked ? b : NULL;
		return smps;
	}

	t_buffer_obj *rnbo_bufferref_getobject(t_rnbo_bufferref *x)
	{
		return x->r_object;
	}

	char rnbo_bufferref_islocked(t_rnbo_bufferref *x)
	{
		return x->r_islocked;
	}

	void rnbo_bufferref_unlock(t_rnbo_bufferref *x)
	{
		if (x->r_lockedObject) {
			buffer_unlocksamples(x->r_lockedObject);
			x->r_lockedObject = NULL;
		}
		x->r_islocked = false;
	}

	void rnbo_bufferref_setdirty(t_rnbo_bufferref *x)
	{
		t_buffer_obj *b = buffer_ref_getobject(x->r_buffer_ref);
		buffer_setdirty(b);
	}

	void rnbo_bufferref_setname(t_rnbo_bufferref *x, t_symbol * name)
	{
		x->r_name = name;
		buffer_ref_set(x->r_buffer_ref, name);
	}

	t_symbol * rnbo_bufferref_getname(t_rnbo_bufferref *x)
	{
		return x->r_name;
	}

	void rnbo_bufferref_setlastaddress(t_rnbo_bufferref *x, float *lastAddress)
	{
		x->r_lastKnownAddress = lastAddress;
	}

	float *rnbo_bufferref_getlastaddress(t_rnbo_bufferref *x)
	{
		return x->r_lastKnownAddress;
	}

	bool rnbo_bufferref_buffer_exists(t_rnbo_bufferref *x)
	{
		return buffer_ref_exists(x->r_buffer_ref);
	}

	bool rnbo_bufferref_request_resize(t_rnbo_bufferref *x, t_buffer_obj *b, long frames, long channels,
			float *observedSamples, long observedFrames, long observedChannels)
	{
		// buffer~ registers sizeinsamps with long arguments (A_DEFLONG A_DEFLONG); the handler
		// only defers the actual resize (buffer_dosize_insamples) to the main thread, it never
		// touches the samples itself, so it is safe to call from the audio thread.
		static t_symbol *sizeinsamps = NULL;
		if (!sizeinsamps) sizeinsamps = gensym("sizeinsamps");

		if (!b) return false;

		bool sameObject = x->r_resizeRequested && x->r_reqObject == b;
		if (sameObject && x->r_requestedFrames == frames && x->r_requestedChannels == channels)
			return false;
		if (!object_getmethod(b, sizeinsamps)) {
			object_warn(b, "Can't sync RNBO buffer to Max buffer: Max buffer does not support sizeinsamps");
			return false;
		}

		object_method(b, sizeinsamps, (t_atom_long)frames, (t_atom_long)channels);

		if (sameObject) {
			// an earlier request to this object is still queued ahead of this one; remember its
			// shape so its late arrival is recognised as ours and not as somebody else's change
			x->r_prevRequestedFrames = x->r_requestedFrames;
			x->r_prevRequestedChannels = x->r_requestedChannels;
		} else {
			x->r_prevRequestedFrames = 0;
			x->r_prevRequestedChannels = 0;
		}
		x->r_reqObject = b;
		x->r_requestedFrames = frames;
		x->r_requestedChannels = channels;
		x->r_reqObservedSamples = observedSamples;
		x->r_reqObservedFrames = observedFrames;
		x->r_reqObservedChannels = observedChannels;
		x->r_reqTimeMs = systime_ms();
		x->r_resizeRequested = true;
		return true;
	}

	bool rnbo_bufferref_resize_requested(t_rnbo_bufferref *x)
	{
		return x->r_resizeRequested;
	}

	void rnbo_bufferref_clear_resize_request(t_rnbo_bufferref *x)
	{
		x->r_resizeRequested = false;
		x->r_reqObject = NULL;
		x->r_requestedFrames = 0;
		x->r_requestedChannels = 0;
		x->r_prevRequestedFrames = 0;
		x->r_prevRequestedChannels = 0;
		x->r_reqObservedSamples = NULL;
		x->r_reqObservedFrames = 0;
		x->r_reqObservedChannels = 0;
		x->r_reqTimeMs = 0;
	}

}

namespace RNBO {
	// Ownership is one-directional: Max allocates, RNBO borrows. RNBO memory is never exposed
	// to Max. When RNBO allocates its own memory for an external buffer~ (a sized buffer~ inside
	// the patcher, a preset restore), we ask the buffer~ to take that shape, then copy our
	// samples into it and borrow it, freeing ours.
	void DataRefBindMaxBuffer(
			ExternalDataIndex dataRefIndex,
			const ExternalDataRef* ref,
			t_rnbo_bufferref *dataref,
			UpdateRefCallback updateDataRef,
			ReleaseRefCallback releaseDataRef
	)
	{
		if (dataref) {
			float *smps = rnbo_bufferref_lock(dataref); // might not actually lock if there is no buffer
			if (!ref->getData() && !smps) return; // don't bother if there's no data to bind

			Index channelcount = 0;
			double samplerate = 44100;
			SampleIndex framecount = 0;
			// same object the lock resolved, never re-resolve mid-vector
			t_buffer_obj *b = rnbo_bufferref_getobject(dataref);

			if (b) {
				channelcount = static_cast<Index>(buffer_getchannelcount(b));
				samplerate = buffer_getsamplerate(b);
				framecount = static_cast<SampleIndex>(buffer_getframecount(b));
			}

			size_t sizeInBytes = framecount * channelcount * sizeof(float);
			auto type = ref->getType();

			// Max buffers are all 32-bit floats. If the RNBO buffer doesn't have the same type,
			// then we're not going to be able to sync them.
			if (type.type != RNBO::DataType::Float32AudioBuffer) {
				object_warn(b, "Can't sync RNBO buffer to Max buffer: RNBO buffer is not 32-bit");
				return;
			}

			bool inSync = ref->getData() == (char *)smps
				&& ref->getSizeInBytes() == sizeInBytes
				&& type.audioBufferInfo.samplerate == samplerate
				&& type.audioBufferInfo.channels == channelcount;

			if (inSync) return;

			Float32AudioBuffer newType(channelcount, samplerate);

			// Borrow whatever the buffer~ has right now. A null smps (being edited, or gone) unbinds
			// until it is valid again. If RNBO held memory of its own, updateDataRef frees it.
			auto borrowFromMax = [&]() {
				updateDataRef(dataRefIndex, (char *)smps, smps ? sizeInBytes : 0, newType);
				rnbo_bufferref_setlastaddress(dataref, smps);
			};

			// lastAddress is what both sides agreed on at the last sync. RNBO's pointer differing
			// from it means RNBO allocated memory of its own since.
			float *lastAddress = rnbo_bufferref_getlastaddress(dataref);
			bool rnboChanged = ref->getData() && (float *)ref->getData() != lastAddress;

			if (rnboChanged) {
				long rnboChannels = (long)type.audioBufferInfo.channels;
				long rnboFrames = rnboChannels > 0 ? (long)(ref->getSizeInBytes() / (rnboChannels * sizeof(float))) : 0;

				if (smps && (long)framecount == rnboFrames && (long)channelcount == rnboChannels) {
					// The buffer~ has our shape: copy our samples in (we hold its lock and we are
					// the thread that reads and writes our own memory, so nothing races this),
					// then borrow it. updateDataRef frees our allocation.
					Platform::memcpy(smps, ref->getData(), sizeInBytes);
					dataref->r_markDirty = true;
					updateDataRef(dataRefIndex, (char *)smps, sizeInBytes, newType);
					rnbo_bufferref_setlastaddress(dataref, smps);
					rnbo_bufferref_clear_resize_request(dataref);
					return;
				}

				// Not our shape yet. Keep running on our own memory meanwhile: it is valid whatever
				// the buffer~ is doing, and unbinding here would free it.
				if (!b) return; // nothing to shape

				// a request outstanding for another buffer~ (the ref was re-pointed) is moot
				if (rnbo_bufferref_resize_requested(dataref) && dataref->r_reqObject != b)
					rnbo_bufferref_clear_resize_request(dataref);

				bool requestIsCurrent = rnbo_bufferref_resize_requested(dataref)
					&& dataref->r_requestedFrames == rnboFrames
					&& dataref->r_requestedChannels == rnboChannels;
				if (!requestIsCurrent) {
					// First sight of this allocation on this buffer~ (or RNBO reallocated to a new
					// shape while an older request is still queued): ask for our shape and wait.
					// Unlock first: buffer~ defers the resize, but if we are on the main thread
					// (DSP off) the deferred call runs immediately and waits on the in-use count.
					rnbo_bufferref_unlock(dataref);
					rnbo_bufferref_request_resize(dataref, b, rnboFrames, rnboChannels, smps, (long)framecount, (long)channelcount);
					return;
				}

				// A request is outstanding for this buffer~. What does it report now?
				if (!smps) return; // being edited (probably our resize), wait

				bool unchanged = smps == dataref->r_reqObservedSamples
					&& (long)framecount == dataref->r_reqObservedFrames
					&& (long)channelcount == dataref->r_reqObservedChannels;
				if (unchanged) {
					// buffer_edit_begin inside the deferred resize gives up after 2 s (other lockers,
					// or a lock count that was lost); if nothing has happened for well beyond that,
					// the resize failed: ask again.
					if (systime_ms() - dataref->r_reqTimeMs > 3000.0) {
						rnbo_bufferref_clear_resize_request(dataref);
						rnbo_bufferref_unlock(dataref);
						rnbo_bufferref_request_resize(dataref, b, rnboFrames, rnboChannels, smps, (long)framecount, (long)channelcount);
					}
					return;
				}

				bool staleCompletion = dataref->r_prevRequestedChannels > 0
					&& (long)framecount == dataref->r_prevRequestedFrames
					&& (long)channelcount == dataref->r_prevRequestedChannels;
				if (staleCompletion) {
					// an older request of ours landed; the current one is still queued behind it
					dataref->r_reqObservedSamples = smps;
					dataref->r_reqObservedFrames = (long)framecount;
					dataref->r_reqObservedChannels = (long)channelcount;
					dataref->r_prevRequestedFrames = 0;
					dataref->r_prevRequestedChannels = 0;
					return;
				}

				// Both sides changed: RNBO allocated, and while our resize was outstanding the
				// buffer~ took a shape we did not ask for (the user replaced it, or the request
				// was clamped). Max wins: drop our allocation and borrow what it has (Max never
				// referenced our memory, so freeing it is safe). Two accepted consequences of the
				// resize being a deferred call we cannot cancel: a replace that lands before it
				// runs is resized to our shape afterwards, and a replace that happens to have our
				// shape is overwritten by our copy above (the patcher asked for that shape and
				// content).
				rnbo_bufferref_clear_resize_request(dataref);
				borrowFromMax();
				return;
			}

			// RNBO did not change, so the difference is Max's: a new allocation, a layout change
			// at the same address, the buffer~ being edited or gone (smps null), or RNBO having
			// no bytes yet. Use the Max buffer.
			borrowFromMax();
			return;
		}

		if (ref->getData()) {
			releaseDataRef(dataRefIndex);
		}
	}

	void DataRefUnbindMaxBuffer(
			const ExternalDataRef* externalRef,
			t_rnbo_bufferref *dataref
	)
	{
			if (dataref) {
				bool dirty = externalRef->getTouched() || dataref->r_markDirty;
				dataref->r_markDirty = false;
				// whatever was written this vector went into the object we locked, which may no
				// longer be what the ref resolves to
				t_buffer_obj *written = dataref->r_lockedObject;

				if (rnbo_bufferref_islocked(dataref)) {
					rnbo_bufferref_unlock(dataref);
				}

				if (dirty && written) {
					buffer_setdirty(written);
				}
			}
	}
}
