package cronet

// UploadDataProviderHandler is the interface that must be implemented to provide upload data.
type UploadDataProviderHandler interface {
	// Length
	// If this is a non-chunked upload, returns the length of the upload. Must
	// always return -1 if this is a chunked upload.
	Length(self UploadDataProvider) int64

	// Read
	// Reads upload data into |buffer|. Each call of this method must be followed be a
	// single call, either synchronous or asynchronous, to
	// UploadDataSink.OnReadSucceeded() on success
	// or UploadDataSink.OnReadError() on failure. Neither read nor rewind
	// will be called until one of those methods or the other is called. Even if
	// the associated UrlRequest is canceled, one or the other must
	// still be called before resources can be safely freed.
	//
	// @param sink The object to notify when the read has completed,
	//            successfully or otherwise.
	// @param buffer The buffer to copy the read bytes into.
	Read(self UploadDataProvider, sink UploadDataSink, buffer Buffer)

	// Rewind
	// Rewinds upload data. Each call must be followed be a single
	// call, either synchronous or asynchronous, to
	// UploadDataSink.OnRewindSucceeded() on success or
	// UploadDataSink.OnRewindError() on failure. Neither read nor rewind
	// will be called until one of those methods or the other is called.
	// Even if the associated UrlRequest is canceled, one or the other
	// must still be called before resources can be safely freed.
	//
	// If rewinding is not supported, this should call
	// UploadDataSink.OnRewindError(). Note that rewinding is required to
	// follow redirects that preserve the upload body, and for retrying when the
	// server times out stale sockets.
	//
	// @param sink The object to notify when the rewind operation has
	//         completed, successfully or otherwise.
	Rewind(self UploadDataProvider, sink UploadDataSink)

	// Close
	// Called when this UploadDataProvider is no longer needed by a request, so that resources
	// (like a file) can be explicitly released.
	Close(self UploadDataProvider)
}
