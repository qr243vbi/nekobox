package cronet

// This file contains shared type definitions for both CGO and purego implementations.
// All pointer types are represented as uintptr for cross-implementation compatibility.

// Engine is an engine to process URLRequest, which uses the best HTTP stack
// available on the current platform. An instance of this class can be started
// using StartWithParams.
type Engine struct {
	ptr uintptr
}

// EngineParams contains parameters for initializing a Cronet Engine.
type EngineParams struct {
	ptr uintptr
}

// Buffer provided by the application to read and write data.
type Buffer struct {
	ptr uintptr
}

// BufferCallback is called when the Buffer is destroyed.
type BufferCallback struct {
	ptr uintptr
}

// Executor is an interface provided by the app to run commands asynchronously.
type Executor struct {
	ptr uintptr
}

// Runnable is a command to be executed by an Executor.
type Runnable struct {
	ptr uintptr
}

// URLRequest controls an HTTP request (GET, PUT, POST etc).
// Initialized by InitWithParams().
// Note: All methods must be called on the Executor passed to InitWithParams().
type URLRequest struct {
	ptr uintptr
}

// URLRequestParams contains parameters for initializing a URLRequest.
type URLRequestParams struct {
	ptr uintptr
}

// URLRequestCallback is used to receive callbacks from URLRequest.
type URLRequestCallback struct {
	ptr uintptr
}

// URLResponseInfo contains response information for a URLRequest.
type URLResponseInfo struct {
	ptr uintptr
}

// Error is the base error passed to URLRequestCallbackHandler.OnFailed().
type Error struct {
	ptr uintptr
}

// DateTime represents a date and time value from cronet.
type DateTime struct {
	ptr uintptr
}

// Metrics contains timing metrics for a URLRequest.
type Metrics struct {
	ptr uintptr
}

// RequestFinishedInfo contains information about a finished request.
type RequestFinishedInfo struct {
	ptr uintptr
}

// URLRequestFinishedInfoListener is called when a request finishes.
type URLRequestFinishedInfoListener struct {
	ptr uintptr
}

// URLRequestStatusListener receives status updates for a URLRequest.
type URLRequestStatusListener struct {
	ptr uintptr
}

// UploadDataProvider provides upload data to a URLRequest.
type UploadDataProvider struct {
	ptr uintptr
}

// UploadDataSink is used by UploadDataProvider to signal events.
type UploadDataSink struct {
	ptr uintptr
}

// StreamEngine is an opaque object representing a Bidirectional stream creating engine.
// Created and configured outside of this API to facilitate sharing with other components.
type StreamEngine struct {
	ptr uintptr
}

// BidirectionalStream is an opaque object representing a Bidirectional Stream.
type BidirectionalStream struct {
	ptr uintptr
}

// HTTPHeader represents an HTTP header key-value pair.
type HTTPHeader struct {
	ptr uintptr
}

// QuicHint contains hints for QUIC protocol usage.
type QuicHint struct {
	ptr uintptr
}

// PublicKeyPins contains public key pins for certificate validation.
type PublicKeyPins struct {
	ptr uintptr
}

// BidirectionalStreamCallback
// Set of callbacks used to receive callbacks from bidirectional stream.
type BidirectionalStreamCallback interface {
	// OnStreamReady
	// Invoked when the stream is ready for reading and writing.
	// Consumer may call BidirectionalStream.Read() to start reading data.
	// Consumer may call BidirectionalStream.Write() to start writing
	// data.
	OnStreamReady(stream BidirectionalStream)

	// OnResponseHeadersReceived
	// Invoked when initial response headers are received.
	// Consumer must call BidirectionalStream.Read() to start reading.
	// Consumer may call BidirectionalStream.Write() to start writing or
	// close the stream. Contents of |headers| is valid for duration of the call.
	///
	OnResponseHeadersReceived(stream BidirectionalStream, headers map[string]string, negotiatedProtocol string)

	// OnReadCompleted
	// Invoked when data is read into the buffer passed to
	// BidirectionalStream.Read(). Only part of the buffer may be
	// populated. To continue reading, call BidirectionalStream.Read().
	// It may be invoked after on_response_trailers_received()}, if there was
	// pending read data before trailers were received.
	//
	// If |bytesRead| is 0, it means the remote side has signaled that it will
	// send no more data; future calls to BidirectionalStream.Read()
	// will result in the OnReadCompleted() callback or OnSucceeded() callback if
	// BidirectionalStream.Write() was invoked with endOfStream set to
	// true.
	OnReadCompleted(stream BidirectionalStream, bytesRead int)

	// OnWriteCompleted
	// Invoked when all data passed to BidirectionalStream.Write() is
	// sent. To continue writing, call BidirectionalStream.Write().
	OnWriteCompleted(stream BidirectionalStream)

	// OnResponseTrailersReceived
	// Invoked when trailers are received before closing the stream. Only invoked
	// when server sends trailers, which it may not. May be invoked while there is
	// read data remaining in local buffer. Contents of |trailers| is valid for
	// duration of the call.
	OnResponseTrailersReceived(stream BidirectionalStream, trailers map[string]string)

	// OnSucceeded
	// Invoked when there is no data to be read or written and the stream is
	// closed successfully remotely and locally. Once invoked, no further callback
	// methods will be invoked.
	OnSucceeded(stream BidirectionalStream)

	// OnFailed
	// Invoked if the stream failed for any reason after
	// BidirectionalStream.Start(). HTTP/2 error codes are
	// mapped to chrome net error codes. Once invoked, no further callback methods
	// will be invoked.
	OnFailed(stream BidirectionalStream, netError int)

	// OnCanceled
	// Invoked if the stream was canceled via
	// BidirectionalStream.Cancel(). Once invoked, no further callback
	// methods will be invoked.
	OnCanceled(stream BidirectionalStream)
}

// BidirectionalStreamHeaderArray is used to pass headers to bidirectional streams.
type BidirectionalStreamHeaderArray struct {
	ptr uintptr
}

// EngineParamsHTTPCacheMode specifies HTTP cache mode.
type EngineParamsHTTPCacheMode int32

const (
	// HTTPCacheModeDisabled disables caching for the engine.
	HTTPCacheModeDisabled EngineParamsHTTPCacheMode = 0
	// HTTPCacheModeInMemory enables in-memory caching, including HTTP data.
	HTTPCacheModeInMemory EngineParamsHTTPCacheMode = 1
	// HTTPCacheModeDiskNoHTTP enables on-disk caching, excluding HTTP data.
	HTTPCacheModeDiskNoHTTP EngineParamsHTTPCacheMode = 2
	// HTTPCacheModeDisk enables on-disk caching, including HTTP data.
	HTTPCacheModeDisk EngineParamsHTTPCacheMode = 3
)

// URLRequestParamsRequestPriority specifies request priority level.
type URLRequestParamsRequestPriority int

const (
	// URLRequestParamsRequestPriorityIdle
	// Lowest request priority.
	URLRequestParamsRequestPriorityIdle URLRequestParamsRequestPriority = 0

	// URLRequestParamsRequestPriorityLowest
	// Very low request priority.
	URLRequestParamsRequestPriorityLowest URLRequestParamsRequestPriority = 1

	// URLRequestParamsRequestPriorityLow
	// Low request priority.
	URLRequestParamsRequestPriorityLow URLRequestParamsRequestPriority = 2

	// URLRequestParamsRequestPriorityMedium
	// Medium request priority. This is the default priority given to the request.
	URLRequestParamsRequestPriorityMedium URLRequestParamsRequestPriority = 3

	// URLRequestParamsRequestPriorityHighest
	// Highest request priority.
	URLRequestParamsRequestPriorityHighest URLRequestParamsRequestPriority = 4
)

// URLRequestParamsIdempotency specifies idempotency of a request.
type URLRequestParamsIdempotency int

const (
	URLRequestParamsIdempotencyDefaultIdempotency URLRequestParamsIdempotency = 0
	URLRequestParamsIdempotencyIdempotent         URLRequestParamsIdempotency = 1
	URLRequestParamsIdempotencyNotIdempotent      URLRequestParamsIdempotency = 2
)

// URLRequestStatusListenerStatus specifies the status of a URL request.
type URLRequestStatusListenerStatus int

const (
	URLRequestStatusListenerStatusInvalid                     URLRequestStatusListenerStatus = -1
	URLRequestStatusListenerStatusIdle                        URLRequestStatusListenerStatus = 0
	URLRequestStatusListenerStatusWaitingForStalledSocketPool URLRequestStatusListenerStatus = 1
	URLRequestStatusListenerStatusWaitingForAvailableSocket   URLRequestStatusListenerStatus = 2
	URLRequestStatusListenerStatusWaitingForDelegate          URLRequestStatusListenerStatus = 3
	URLRequestStatusListenerStatusWaitingForCache             URLRequestStatusListenerStatus = 4
	URLRequestStatusListenerStatusDownloadingPacFile          URLRequestStatusListenerStatus = 5
	URLRequestStatusListenerStatusResolvingProxyForURL        URLRequestStatusListenerStatus = 6
	URLRequestStatusListenerStatusResolvingHostInPacFile      URLRequestStatusListenerStatus = 7
	URLRequestStatusListenerStatusEstablishingProxyTunnel     URLRequestStatusListenerStatus = 8
	URLRequestStatusListenerStatusResolvingHost               URLRequestStatusListenerStatus = 9
	URLRequestStatusListenerStatusConnecting                  URLRequestStatusListenerStatus = 10
	URLRequestStatusListenerStatusSSLHandshake                URLRequestStatusListenerStatus = 11
	URLRequestStatusListenerStatusSendingRequest              URLRequestStatusListenerStatus = 12
	URLRequestStatusListenerStatusWaitingForResponse          URLRequestStatusListenerStatus = 13
	URLRequestStatusListenerStatusReadingResponse             URLRequestStatusListenerStatus = 14
)

// URLRequestFinishedInfoFinishedReason
// The reason why the request finished.
type URLRequestFinishedInfoFinishedReason int

const (
	// URLRequestFinishedInfoFinishedReasonSucceeded
	// The request succeeded.
	URLRequestFinishedInfoFinishedReasonSucceeded URLRequestFinishedInfoFinishedReason = 0

	// URLRequestFinishedInfoFinishedReasonFailed
	// The request failed or returned an error.
	URLRequestFinishedInfoFinishedReasonFailed URLRequestFinishedInfoFinishedReason = 1

	// URLRequestFinishedInfoFinishedReasonCanceled
	// The request was canceled.
	URLRequestFinishedInfoFinishedReasonCanceled URLRequestFinishedInfoFinishedReason = 2
)

// Dialer is a callback function for custom TCP connection establishment.
// address: IP address string (e.g. "1.2.3.4" or "::1")
// port: Port number
// Returns: connected socket fd on success, negative net error code on failure.
// Common error codes:
//
//	ERR_CONNECTION_REFUSED (-102)
//	ERR_CONNECTION_FAILED (-104)
//	ERR_ADDRESS_UNREACHABLE (-109)
//	ERR_CONNECTION_TIMED_OUT (-118)
type Dialer func(address string, port uint16) int

// UDPDialer is a callback function for custom UDP socket creation.
// address: IP address string (e.g. "1.2.3.4" or "::1")
// port: Port number
// Returns:
//   - fd: socket fd on success, negative net error code on failure
//   - localAddress: local IP address string (may be empty)
//   - localPort: local port number
//
// The returned socket can be:
//   - AF_INET/AF_INET6 SOCK_DGRAM: Standard UDP socket (may be connected)
//   - AF_UNIX SOCK_DGRAM: Unix domain datagram socket (Unix/macOS/Linux)
//   - AF_UNIX SOCK_STREAM: Unix domain stream socket (Windows, with framing)
//
// Cronet will NOT call connect() on the returned socket.
type UDPDialer func(address string, port uint16) (fd int, localAddress string, localPort uint16)
