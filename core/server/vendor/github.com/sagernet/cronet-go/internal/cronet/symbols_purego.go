//go:build with_purego

package cronet

import "unsafe"

// Cronet API function pointers loaded at runtime via purego.
// These are populated by the loader (loader_unix.go or loader_windows.go).

var (
	// Buffer functions
	cronetBufferCreate                  func() uintptr
	cronetBufferDestroy                 func(uintptr)
	cronetBufferSetClientContext        func(uintptr, uintptr)
	cronetBufferGetClientContext        func(uintptr) uintptr
	cronetBufferInitWithDataAndCallback func(uintptr, uintptr, uint64, uintptr)
	cronetBufferInitWithAlloc           func(uintptr, uint64)
	cronetBufferGetSize                 func(uintptr) uint64
	cronetBufferGetData                 func(uintptr) uintptr

	// BufferCallback functions
	cronetBufferCallbackDestroy          func(uintptr)
	cronetBufferCallbackSetClientContext func(uintptr, uintptr)
	cronetBufferCallbackGetClientContext func(uintptr) uintptr
	cronetBufferCallbackCreateWith       func(uintptr) uintptr

	// Runnable functions
	cronetRunnableDestroy          func(uintptr)
	cronetRunnableSetClientContext func(uintptr, uintptr)
	cronetRunnableGetClientContext func(uintptr) uintptr
	cronetRunnableRun              func(uintptr)
	cronetRunnableCreateWith       func(uintptr) uintptr

	// Executor functions
	cronetExecutorDestroy          func(uintptr)
	cronetExecutorSetClientContext func(uintptr, uintptr)
	cronetExecutorGetClientContext func(uintptr) uintptr
	cronetExecutorExecute          func(uintptr, uintptr)
	cronetExecutorCreateWith       func(uintptr) uintptr

	// Engine functions
	cronetEngineCreate                        func() uintptr
	cronetEngineDestroy                       func(uintptr)
	cronetEngineSetClientContext              func(uintptr, uintptr)
	cronetEngineGetClientContext              func(uintptr) uintptr
	cronetEngineStartWithParams               func(uintptr, uintptr) int32
	cronetEngineStartNetLogToFile             func(uintptr, string, bool) bool
	cronetEngineStopNetLog                    func(uintptr)
	cronetEngineShutdown                      func(uintptr) int32
	cronetEngineGetVersionString              func(uintptr) uintptr
	cronetEngineGetDefaultUserAgent           func(uintptr) uintptr
	cronetEngineAddRequestFinishedListener    func(uintptr, uintptr, uintptr)
	cronetEngineRemoveRequestFinishedListener func(uintptr, uintptr)
	cronetEngineGetStreamEngine               func(uintptr) uintptr
	cronetEngineSetMockCertVerifierForTesting func(uintptr, uintptr)
	cronetEngineCloseAllConnections           func(uintptr)
	cronetEngineSetDialer                     func(uintptr, uintptr, uintptr)
	cronetEngineSetUdpDialer                  func(uintptr, uintptr, uintptr)

	// EngineParams functions
	cronetEngineParamsCreate                                              func() uintptr
	cronetEngineParamsDestroy                                             func(uintptr)
	cronetEngineParamsEnableCheckResultSet                                func(uintptr, bool)
	cronetEngineParamsUserAgentSet                                        func(uintptr, string)
	cronetEngineParamsAcceptLanguageSet                                   func(uintptr, string)
	cronetEngineParamsStoragePathSet                                      func(uintptr, string)
	cronetEngineParamsEnableQuicSet                                       func(uintptr, bool)
	cronetEngineParamsEnableHttp2Set                                      func(uintptr, bool)
	cronetEngineParamsEnableBrotliSet                                     func(uintptr, bool)
	cronetEngineParamsHttpCacheModeSet                                    func(uintptr, int32)
	cronetEngineParamsHttpCacheMaxSizeSet                                 func(uintptr, int64)
	cronetEngineParamsQuicHintsAdd                                        func(uintptr, uintptr)
	cronetEngineParamsPublicKeyPinsAdd                                    func(uintptr, uintptr)
	cronetEngineParamsEnablePublicKeyPinningBypassForLocalTrustAnchorsSet func(uintptr, bool)
	cronetEngineParamsNetworkThreadPrioritySet                            func(uintptr, float64)
	cronetEngineParamsExperimentalOptionsSet                              func(uintptr, string)
	cronetEngineParamsEnableCheckResultGet                                func(uintptr) bool
	cronetEngineParamsUserAgentGet                                        func(uintptr) uintptr
	cronetEngineParamsAcceptLanguageGet                                   func(uintptr) uintptr
	cronetEngineParamsStoragePathGet                                      func(uintptr) uintptr
	cronetEngineParamsEnableQuicGet                                       func(uintptr) bool
	cronetEngineParamsEnableHttp2Get                                      func(uintptr) bool
	cronetEngineParamsEnableBrotliGet                                     func(uintptr) bool
	cronetEngineParamsHttpCacheModeGet                                    func(uintptr) int32
	cronetEngineParamsHttpCacheMaxSizeGet                                 func(uintptr) int64
	cronetEngineParamsQuicHintsSize                                       func(uintptr) uint32
	cronetEngineParamsQuicHintsAt                                         func(uintptr, uint32) uintptr
	cronetEngineParamsQuicHintsClear                                      func(uintptr)
	cronetEngineParamsPublicKeyPinsSize                                   func(uintptr) uint32
	cronetEngineParamsPublicKeyPinsAt                                     func(uintptr, uint32) uintptr
	cronetEngineParamsPublicKeyPinsClear                                  func(uintptr)
	cronetEngineParamsEnablePublicKeyPinningBypassForLocalTrustAnchorsGet func(uintptr) bool
	cronetEngineParamsNetworkThreadPriorityGet                            func(uintptr) float64
	cronetEngineParamsExperimentalOptionsGet                              func(uintptr) uintptr

	// UrlRequest functions
	cronetUrlRequestCreate           func() uintptr
	cronetUrlRequestDestroy          func(uintptr)
	cronetUrlRequestSetClientContext func(uintptr, uintptr)
	cronetUrlRequestGetClientContext func(uintptr) uintptr
	cronetUrlRequestInitWithParams   func(uintptr, uintptr, string, uintptr, uintptr, uintptr) int32
	cronetUrlRequestStart            func(uintptr) int32
	cronetUrlRequestFollowRedirect   func(uintptr) int32
	cronetUrlRequestRead             func(uintptr, uintptr) int32
	cronetUrlRequestCancel           func(uintptr)
	cronetUrlRequestIsDone           func(uintptr) bool
	cronetUrlRequestGetStatus        func(uintptr, uintptr)

	// UrlRequestParams functions
	cronetUrlRequestParamsCreate                        func() uintptr
	cronetUrlRequestParamsDestroy                       func(uintptr)
	cronetUrlRequestParamsHttpMethodSet                 func(uintptr, string)
	cronetUrlRequestParamsRequestHeadersAdd             func(uintptr, uintptr)
	cronetUrlRequestParamsDisableCacheSet               func(uintptr, bool)
	cronetUrlRequestParamsPrioritySet                   func(uintptr, int32)
	cronetUrlRequestParamsUploadDataProviderSet         func(uintptr, uintptr)
	cronetUrlRequestParamsUploadDataProviderExecutorSet func(uintptr, uintptr)
	cronetUrlRequestParamsAllowDirectExecutorSet        func(uintptr, bool)
	cronetUrlRequestParamsAnnotationsAdd                func(uintptr, uintptr)
	cronetUrlRequestParamsRequestFinishedListenerSet    func(uintptr, uintptr)
	cronetUrlRequestParamsRequestFinishedExecutorSet    func(uintptr, uintptr)
	cronetUrlRequestParamsIdempotencySet                func(uintptr, int32)
	cronetUrlRequestParamsHttpMethodGet                 func(uintptr) uintptr
	cronetUrlRequestParamsRequestHeadersSize            func(uintptr) uint32
	cronetUrlRequestParamsRequestHeadersAt              func(uintptr, uint32) uintptr
	cronetUrlRequestParamsRequestHeadersClear           func(uintptr)
	cronetUrlRequestParamsDisableCacheGet               func(uintptr) bool
	cronetUrlRequestParamsPriorityGet                   func(uintptr) int32
	cronetUrlRequestParamsUploadDataProviderGet         func(uintptr) uintptr
	cronetUrlRequestParamsUploadDataProviderExecutorGet func(uintptr) uintptr
	cronetUrlRequestParamsAllowDirectExecutorGet        func(uintptr) bool
	cronetUrlRequestParamsAnnotationsSize               func(uintptr) uint32
	cronetUrlRequestParamsAnnotationsAt                 func(uintptr, uint32) uintptr
	cronetUrlRequestParamsAnnotationsClear              func(uintptr)
	cronetUrlRequestParamsRequestFinishedListenerGet    func(uintptr) uintptr
	cronetUrlRequestParamsRequestFinishedExecutorGet    func(uintptr) uintptr
	cronetUrlRequestParamsIdempotencyGet                func(uintptr) int32

	// UrlRequestCallback functions
	cronetUrlRequestCallbackDestroy          func(uintptr)
	cronetUrlRequestCallbackSetClientContext func(uintptr, uintptr)
	cronetUrlRequestCallbackGetClientContext func(uintptr) uintptr
	cronetUrlRequestCallbackCreateWith       func(uintptr, uintptr, uintptr, uintptr, uintptr, uintptr) uintptr

	// UrlRequestStatusListener functions
	cronetUrlRequestStatusListenerDestroy          func(uintptr)
	cronetUrlRequestStatusListenerSetClientContext func(uintptr, uintptr)
	cronetUrlRequestStatusListenerGetClientContext func(uintptr) uintptr
	cronetUrlRequestStatusListenerCreateWith       func(uintptr) uintptr

	// UploadDataProvider functions
	cronetUploadDataProviderDestroy          func(uintptr)
	cronetUploadDataProviderSetClientContext func(uintptr, uintptr)
	cronetUploadDataProviderGetClientContext func(uintptr) uintptr
	cronetUploadDataProviderCreateWith       func(uintptr, uintptr, uintptr, uintptr) uintptr

	// UploadDataSink functions
	cronetUploadDataSinkDestroy           func(uintptr)
	cronetUploadDataSinkSetClientContext  func(uintptr, uintptr)
	cronetUploadDataSinkGetClientContext  func(uintptr) uintptr
	cronetUploadDataSinkOnReadSucceeded   func(uintptr, uint64, bool)
	cronetUploadDataSinkOnReadError       func(uintptr, string)
	cronetUploadDataSinkOnRewindSucceeded func(uintptr)
	cronetUploadDataSinkOnRewindError     func(uintptr, string)

	// UrlResponseInfo functions
	cronetUrlResponseInfoCreate                func() uintptr
	cronetUrlResponseInfoDestroy               func(uintptr)
	cronetUrlResponseInfoUrlSet                func(uintptr, uintptr)
	cronetUrlResponseInfoUrlChainAdd           func(uintptr, uintptr)
	cronetUrlResponseInfoHttpStatusCodeSet     func(uintptr, int32)
	cronetUrlResponseInfoHttpStatusTextSet     func(uintptr, uintptr)
	cronetUrlResponseInfoAllHeadersListAdd     func(uintptr, uintptr)
	cronetUrlResponseInfoWasCachedSet          func(uintptr, bool)
	cronetUrlResponseInfoNegotiatedProtocolSet func(uintptr, uintptr)
	cronetUrlResponseInfoProxyServerSet        func(uintptr, uintptr)
	cronetUrlResponseInfoReceivedByteCountSet  func(uintptr, int64)
	cronetUrlResponseInfoUrlGet                func(uintptr) uintptr
	cronetUrlResponseInfoUrlChainSize          func(uintptr) uint32
	cronetUrlResponseInfoUrlChainAt            func(uintptr, uint32) uintptr
	cronetUrlResponseInfoUrlChainClear         func(uintptr)
	cronetUrlResponseInfoHttpStatusCodeGet     func(uintptr) int32
	cronetUrlResponseInfoHttpStatusTextGet     func(uintptr) uintptr
	cronetUrlResponseInfoAllHeadersListSize    func(uintptr) uint32
	cronetUrlResponseInfoAllHeadersListAt      func(uintptr, uint32) uintptr
	cronetUrlResponseInfoAllHeadersListClear   func(uintptr)
	cronetUrlResponseInfoWasCachedGet          func(uintptr) bool
	cronetUrlResponseInfoNegotiatedProtocolGet func(uintptr) uintptr
	cronetUrlResponseInfoProxyServerGet        func(uintptr) uintptr
	cronetUrlResponseInfoReceivedByteCountGet  func(uintptr) int64

	// Error functions
	cronetErrorCreate                   func() uintptr
	cronetErrorDestroy                  func(uintptr)
	cronetErrorErrorCodeSet             func(uintptr, int32)
	cronetErrorMessageSet               func(uintptr, uintptr)
	cronetErrorInternalErrorCodeSet     func(uintptr, int32)
	cronetErrorImmediatelyRetryableSet  func(uintptr, bool)
	cronetErrorQuicDetailedErrorCodeSet func(uintptr, int32)
	cronetErrorErrorCodeGet             func(uintptr) int32
	cronetErrorMessageGet               func(uintptr) uintptr
	cronetErrorInternalErrorCodeGet     func(uintptr) int32
	cronetErrorImmediatelyRetryableGet  func(uintptr) bool
	cronetErrorQuicDetailedErrorCodeGet func(uintptr) int32

	// HttpHeader functions
	cronetHttpHeaderCreate   func() uintptr
	cronetHttpHeaderDestroy  func(uintptr)
	cronetHttpHeaderNameSet  func(uintptr, string)
	cronetHttpHeaderValueSet func(uintptr, string)
	cronetHttpHeaderNameGet  func(uintptr) uintptr
	cronetHttpHeaderValueGet func(uintptr) uintptr

	// QuicHint functions
	cronetQuicHintCreate           func() uintptr
	cronetQuicHintDestroy          func(uintptr)
	cronetQuicHintHostSet          func(uintptr, string)
	cronetQuicHintPortSet          func(uintptr, int32)
	cronetQuicHintAlternatePortSet func(uintptr, int32)
	cronetQuicHintHostGet          func(uintptr) uintptr
	cronetQuicHintPortGet          func(uintptr) int32
	cronetQuicHintAlternatePortGet func(uintptr) int32

	// PublicKeyPins functions
	cronetPublicKeyPinsCreate               func() uintptr
	cronetPublicKeyPinsDestroy              func(uintptr)
	cronetPublicKeyPinsHostSet              func(uintptr, string)
	cronetPublicKeyPinsPinsSha256Add        func(uintptr, string)
	cronetPublicKeyPinsIncludeSubdomainsSet func(uintptr, bool)
	cronetPublicKeyPinsExpirationDateSet    func(uintptr, int64)
	cronetPublicKeyPinsHostGet              func(uintptr) uintptr
	cronetPublicKeyPinsPinsSha256Size       func(uintptr) uint32
	cronetPublicKeyPinsPinsSha256At         func(uintptr, uint32) uintptr
	cronetPublicKeyPinsPinsSha256Clear      func(uintptr)
	cronetPublicKeyPinsIncludeSubdomainsGet func(uintptr) bool
	cronetPublicKeyPinsExpirationDateGet    func(uintptr) int64

	// DateTime functions
	cronetDateTimeCreate   func() uintptr
	cronetDateTimeDestroy  func(uintptr)
	cronetDateTimeValueSet func(uintptr, int64)
	cronetDateTimeValueGet func(uintptr) int64

	// Metrics functions
	cronetMetricsCreate               func() uintptr
	cronetMetricsDestroy              func(uintptr)
	cronetMetricsRequestStartSet      func(uintptr, uintptr)
	cronetMetricsDnsStartSet          func(uintptr, uintptr)
	cronetMetricsDnsEndSet            func(uintptr, uintptr)
	cronetMetricsConnectStartSet      func(uintptr, uintptr)
	cronetMetricsConnectEndSet        func(uintptr, uintptr)
	cronetMetricsSslStartSet          func(uintptr, uintptr)
	cronetMetricsSslEndSet            func(uintptr, uintptr)
	cronetMetricsSendingStartSet      func(uintptr, uintptr)
	cronetMetricsSendingEndSet        func(uintptr, uintptr)
	cronetMetricsPushStartSet         func(uintptr, uintptr)
	cronetMetricsPushEndSet           func(uintptr, uintptr)
	cronetMetricsResponseStartSet     func(uintptr, uintptr)
	cronetMetricsRequestEndSet        func(uintptr, uintptr)
	cronetMetricsSocketReusedSet      func(uintptr, bool)
	cronetMetricsSentByteCountSet     func(uintptr, int64)
	cronetMetricsReceivedByteCountSet func(uintptr, int64)
	cronetMetricsRequestStartGet      func(uintptr) uintptr
	cronetMetricsDnsStartGet          func(uintptr) uintptr
	cronetMetricsDnsEndGet            func(uintptr) uintptr
	cronetMetricsConnectStartGet      func(uintptr) uintptr
	cronetMetricsConnectEndGet        func(uintptr) uintptr
	cronetMetricsSslStartGet          func(uintptr) uintptr
	cronetMetricsSslEndGet            func(uintptr) uintptr
	cronetMetricsSendingStartGet      func(uintptr) uintptr
	cronetMetricsSendingEndGet        func(uintptr) uintptr
	cronetMetricsPushStartGet         func(uintptr) uintptr
	cronetMetricsPushEndGet           func(uintptr) uintptr
	cronetMetricsResponseStartGet     func(uintptr) uintptr
	cronetMetricsRequestEndGet        func(uintptr) uintptr
	cronetMetricsSocketReusedGet      func(uintptr) bool
	cronetMetricsSentByteCountGet     func(uintptr) int64
	cronetMetricsReceivedByteCountGet func(uintptr) int64

	// RequestFinishedInfo functions
	cronetRequestFinishedInfoCreate            func() uintptr
	cronetRequestFinishedInfoDestroy           func(uintptr)
	cronetRequestFinishedInfoMetricsSet        func(uintptr, uintptr)
	cronetRequestFinishedInfoAnnotationsAdd    func(uintptr, uintptr)
	cronetRequestFinishedInfoFinishedReasonSet func(uintptr, int32)
	cronetRequestFinishedInfoMetricsGet        func(uintptr) uintptr
	cronetRequestFinishedInfoAnnotationsSize   func(uintptr) uint32
	cronetRequestFinishedInfoAnnotationsAt     func(uintptr, uint32) uintptr
	cronetRequestFinishedInfoAnnotationsClear  func(uintptr)
	cronetRequestFinishedInfoFinishedReasonGet func(uintptr) int32

	// RequestFinishedInfoListener functions
	cronetRequestFinishedInfoListenerDestroy          func(uintptr)
	cronetRequestFinishedInfoListenerSetClientContext func(uintptr, uintptr)
	cronetRequestFinishedInfoListenerGetClientContext func(uintptr) uintptr
	cronetRequestFinishedInfoListenerCreateWith       func(uintptr) uintptr

	// Custom cert verifier functions
	cronetCreateCertVerifierWithRootCerts func(string) uintptr
)

// BidirectionalStream API function pointers
var (
	bidirectionalStreamCreate                        func(uintptr, uintptr, uintptr) uintptr
	bidirectionalStreamDestroy                       func(uintptr) int32
	bidirectionalStreamDisableAutoFlush              func(uintptr, bool)
	bidirectionalStreamDelayRequestHeadersUntilFlush func(uintptr, bool)
	bidirectionalStreamStart                         func(uintptr, string, int32, string, uintptr, bool) int32
	bidirectionalStreamRead                          func(uintptr, uintptr, int32) int32
	bidirectionalStreamWrite                         func(uintptr, uintptr, int32, bool) int32
	bidirectionalStreamFlush                         func(uintptr)
	bidirectionalStreamCancel                        func(uintptr)
)

// GoString converts a C string pointer to a Go string.
// Maximum string length is 64KB to prevent infinite loops on corrupted strings.
func GoString(ptr uintptr) string {
	if ptr == 0 {
		return ""
	}
	const maxLength = 65536 // 64KB max string length
	var length int
	for length < maxLength {
		b := *(*byte)(unsafe.Pointer(ptr + uintptr(length)))
		if b == 0 {
			break
		}
		length++
	}
	if length == 0 {
		return ""
	}
	return string(unsafe.Slice((*byte)(unsafe.Pointer(ptr)), length))
}
