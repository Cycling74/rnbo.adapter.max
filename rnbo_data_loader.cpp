#include "rnbo_data_loader.h"

namespace {
	t_class *s_rnbo_data_loader_class = nullptr;

	typedef struct _audio_info {
		long	channels;
		long	samplerate;
		long	frames;
	} t_audio_info;

	t_symbol *ps_close;
	t_symbol *ps_error;
	t_symbol *ps_open;

	const t_fourcc s_types[] = {
		FOUR_CHAR_CODE('AIFF'),
		FOUR_CHAR_CODE('AIFC'),
		FOUR_CHAR_CODE('ULAW'),	//next/sun typed by peak and others
		FOUR_CHAR_CODE('WAVE'),
		FOUR_CHAR_CODE('FLAC'),
		FOUR_CHAR_CODE('Sd2f'),
		FOUR_CHAR_CODE('QQQQ'),	// or whatever IRCAM snd files are saved as...
		FOUR_CHAR_CODE('BINA'),	// or whatever IRCAM snd files are saved as...
		FOUR_CHAR_CODE('NxTS'),	//next/sun typed by soundhack
		FOUR_CHAR_CODE('Mp3 '),
		FOUR_CHAR_CODE('DATA'), // our own 'raw' files
		FOUR_CHAR_CODE('M4a '),
		FOUR_CHAR_CODE('CAF '),
		FOUR_CHAR_CODE('wv64')
	};
}

extern "C" {

	// Simplified "loader" object that can load audio data from a file or URL
	// without using a Max buffer.
	struct _rnbo_data_loader {
		t_object 				obj;
		RNBO::DataType::Type	_type;
		bool					_ready;
		t_symbol				*_last_requested;
		t_audio_info			_info;
		char					*_data;
		t_object				*_remote_resource;
		t_systhread_mutex 		_mutex;
	};

	t_rnbo_data_loader *rnbo_data_loader_new(RNBO::DataType::Type type);
	bool rnbo_data_loader_ready(t_rnbo_data_loader *x);
	void rnbo_data_loader_free(t_rnbo_data_loader *x);
	t_max_err rnbo_data_loader_notify(t_rnbo_data_loader *loader, t_symbol *s, t_symbol *msg, void *sender, void *data);

	static t_max_err rnbo_data_loader_storefile(
		t_rnbo_data_loader *loader,
		const char *key,
		short vol,
		const char *filename
	) {
		t_audio_info info;
		long datalen = 0;
		char *data = nullptr;

		t_object *reader = (t_object *) object_new(CLASS_NOBOX, gensym("jsoundfile"));

		t_max_err err = (t_max_err) object_method(reader, ps_open, vol, filename, 0, 0);
		if (err != MAX_ERR_NONE) {
			return err;
		}

		// Get the info
		info.channels = (t_atom_long) object_method(reader, gensym("getchannelcount"));
		info.frames = (t_atom_long) object_method(reader, gensym("getlength"));
		info.samplerate = (t_atom_long) object_method(reader, gensym("getsr"));
		long sampleLength = info.channels * info.frames;

		// Make space
		float *fdata = (float *) malloc(info.channels * info.frames * sizeof(float));

		// Read in the data
		object_method(reader, gensym("readfloats"), fdata, 0, info.frames, info.channels);

		// Close it up
		object_method(reader, ps_close);
		object_free(reader);

		// If we're 64 bit, then copy the data over
		if (loader->_type == RNBO::DataType::Float64AudioBuffer) {
			double *ddata = (double *) malloc(info.channels * info.frames * sizeof(double));
			for (unsigned long i = 0; i < sampleLength; i++) {
				ddata[i] = fdata[i];
			}
			free(fdata);
			data = (char *) ddata;
			datalen = sampleLength * sizeof(double);
		} else {
			data = (char *) fdata;
			datalen = sampleLength * sizeof(float);
		}

		systhread_mutex_lock(loader->_mutex);
		if (loader->_data) {
			free(loader->_data);
		}

		memcpy(&loader->_info, &info, sizeof(t_audio_info));
		loader->_data = data;
		loader->_ready = true;
		systhread_mutex_unlock(loader->_mutex);

		return MAX_ERR_NONE;
	}

	void rnbo_data_loader_register()
	{
		ps_close = gensym("close");
		ps_error = gensym("error");
		ps_open = gensym("open");

		if (s_rnbo_data_loader_class)
			return;


		auto c = class_new("rnbo_data_loader", (method)rnbo_data_loader_new, (method)rnbo_data_loader_free, (long)sizeof(t_rnbo_data_loader), 0L, A_CANT, 0);

		class_addmethod(c, (method)rnbo_data_loader_notify, "notify", A_CANT, 0);

		class_register(CLASS_NOBOX, c);
		s_rnbo_data_loader_class = c;
	}

	t_rnbo_data_loader *rnbo_data_loader_new(RNBO::DataType::Type type)
	{
		t_rnbo_data_loader *loader = (t_rnbo_data_loader *)object_alloc(s_rnbo_data_loader_class);
		if (loader) {
			loader->_type = type;
			loader->_ready = false;
			loader->_data = nullptr;
			loader->_last_requested = nullptr;
			systhread_mutex_new(&loader->_mutex, SYSTHREAD_MUTEX_RECURSIVE);
		}

		return loader;
	}

	bool rnbo_data_loader_ready(t_rnbo_data_loader *loader)
	{
		return loader->_ready;
	}

	void rnbo_data_loader_free(t_rnbo_data_loader *loader)
	{
		systhread_mutex_lock(loader->_mutex);
		loader->_ready = false;
		if (loader->_data) {
			free(loader->_data);
			loader->_data = nullptr;
		}
		systhread_mutex_unlock(loader->_mutex);

		if (loader->_remote_resource) {
			object_free(loader->_remote_resource);
			loader->_remote_resource = nullptr;
		}
		systhread_mutex_free(loader->_mutex);
		loader->_mutex = nullptr;
	}

	static t_max_err rnbo_data_locatefile(const char *in_filename, short *out_path, char *out_filename) {
		t_max_err err;
		char pathBuffer[MAX_PATH_CHARS];
		t_fourcc filetype;

		strncpy_zero(pathBuffer, in_filename, MAX_PATH_CHARS);
		err = locatefile_extended(pathBuffer, out_path, &filetype, s_types, sizeof(s_types) / sizeof(s_types[0]));

		if (err == MAX_ERR_NONE) {
			err = path_toabsolutesystempath(*out_path, pathBuffer, pathBuffer);
		}

		if (err == MAX_ERR_NONE) {
			err = path_frompathname(pathBuffer , out_path, out_filename);
		}

		if (err != MAX_ERR_NONE) {
			out_path = 0;
			out_filename[0] = '\0';
		}

		return err;
	}

	// Attempt to read the audio file from disk and load it into memory, so that
	// the contents of the audio file can be passed to RNBO
	void rnbo_data_loader_file(t_rnbo_data_loader *loader, const char *filename)
	{
		// If there were a mechanism to cancel an HTTP request, we could do it here
		loader->_last_requested = gensym(filename);

		// Locate the file
		char out_filename[MAX_PATH_CHARS];
		short out_path;
		t_max_err err = rnbo_data_locatefile(filename, &out_path, out_filename);

		if (err == MAX_ERR_NONE) {
			err = rnbo_data_loader_storefile(loader, filename, out_path, out_filename);
		} else {
			object_error(nullptr, "%s: can't open file", filename);
		}
	}

	// Attempt to download the audio file from a remote URL, then load it so that
	// the contents of the audio file can be passed to RNBO
	void rnbo_data_loader_url(t_rnbo_data_loader *loader, const char *url)
	{
		static t_symbol *sym_download = gensym("download");

		loader->_last_requested = gensym(url);

		if (!loader->_remote_resource) {
			loader->_remote_resource = (t_object *) object_new(CLASS_NOBOX, gensym("remote_resource"));
			object_attach_byptr_register(loader, loader->_remote_resource, CLASS_NOBOX);
		}

		object_method(loader->_remote_resource, sym_download, loader->_last_requested);
	}

	t_max_err rnbo_data_loader_notify(t_rnbo_data_loader *loader, t_symbol *s, t_symbol *msg, void *sender, void *data)
	{
		static t_symbol *sym_complete = gensym("complete");
		static t_symbol *sym_geturl = gensym("geturl");
		static t_symbol *sym_filename = gensym("filename");
		static t_symbol *sym_vol = gensym("vol");
		static t_symbol *sym_remote_resource = gensym("remote_resource");

		if (sender && object_classname_compare(sender, sym_remote_resource)) {
			t_symbol *url = (t_symbol *) object_method(sender, sym_geturl);

			bool showError = false;

			// If the url doesn't match _last_requested, it means that RNBO has changed
			// its mind about what resource should be loaded into the given data buffer,
			// and we can just move on
			if (url == loader->_last_requested) {
				if (msg == sym_complete) {
					t_symbol *filename = (t_symbol *) object_method(sender, sym_filename);
					const short vol = (t_atom_long) object_method(sender, sym_vol);

					if (filename != NULL && vol != -1) {
						rnbo_data_loader_storefile(loader, url->s_name, vol, filename->s_name);
					} else {
						showError = true;
					}
				} else if (msg == ps_error) {
					showError = true;
				}
			}

			if (showError) object_error(nullptr, "Couldn't download file from %s", url ? url->s_name : "unknown url");
		}

		return MAX_ERR_NONE;
	}

	t_symbol *rnbo_data_loader_get_last_requested(t_rnbo_data_loader *loader)
	{
		return loader->_last_requested;
	}


	bool rnbo_path_is_url(const char *pathOrURL) {
		return strstr(pathOrURL, "://") != nullptr;
	}

	void rnbo_data_loader_load(t_rnbo_data_loader *x, const char *pathorurl) {
		if (rnbo_path_is_url(pathorurl)) {
			rnbo_data_loader_url(x, pathorurl);
		} else {
			rnbo_data_loader_file(x, pathorurl);
		}
	}
}

namespace RNBO {
	void DataLoaderHandoffData(
			ExternalDataIndex dataRefIndex,
			const ExternalDataRef* ref,
			t_rnbo_data_loader *loader,
			UpdateRefCallback updateDataRef,
			ReleaseRefCallback releaseDataRef
	) {
		systhread_mutex_lock(loader->_mutex);
		if (rnbo_data_loader_ready(loader)) {
			if (loader->_data) {

				long sizeInBytes = loader->_info.channels * loader->_info.frames;
				if (loader->_type == DataType::Float64AudioBuffer) {
					Float64AudioBuffer newType(loader->_info.channels, loader->_info.samplerate);
					sizeInBytes *= sizeof(double);
					updateDataRef(dataRefIndex, loader->_data, sizeInBytes, newType);
				} else {
					Float32AudioBuffer newType(loader->_info.channels, loader->_info.samplerate);
					sizeInBytes *= sizeof(float);
					updateDataRef(dataRefIndex, loader->_data, sizeInBytes, newType);
				}

				loader->_data = nullptr; // RNBO owns the data now, so we just move on
				loader->_ready = false;
			}
		}
		systhread_mutex_unlock(loader->_mutex);
	}
};
