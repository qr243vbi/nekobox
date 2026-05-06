//go:build with_purego

package cronet

import (
	"runtime"
	"unsafe"
)

// CString allocates a null-terminated C string from a Go string.
// It returns both the pointer and the backing byte slice.
// The caller must keep the byte slice alive (via runtime.KeepAlive)
// until the pointer is no longer needed by C code.
func CString(s string) (uintptr, []byte) {
	b := make([]byte, len(s)+1)
	copy(b, s)
	b[len(s)] = 0
	return uintptr(unsafe.Pointer(&b[0])), b
}

// Engine API

func EngineCreate() uintptr {
	ensureLoaded()
	return cronetEngineCreate()
}

func EngineDestroy(engine uintptr) {
	cronetEngineDestroy(engine)
}

func EngineStartNetLogToFile(engine uintptr, fileName string, logAll bool) bool {
	return cronetEngineStartNetLogToFile(engine, fileName, logAll)
}

func EngineStopNetLog(engine uintptr) {
	cronetEngineStopNetLog(engine)
}

func EngineShutdown(engine uintptr) int32 {
	return cronetEngineShutdown(engine)
}

func EngineGetVersionString(engine uintptr) string {
	return GoString(cronetEngineGetVersionString(engine))
}

func EngineGetDefaultUserAgent(engine uintptr) string {
	return GoString(cronetEngineGetDefaultUserAgent(engine))
}

func EngineAddRequestFinishedListener(engine, listener, executor uintptr) {
	cronetEngineAddRequestFinishedListener(engine, listener, executor)
}

func EngineRemoveRequestFinishedListener(engine, listener uintptr) {
	cronetEngineRemoveRequestFinishedListener(engine, listener)
}

func EngineSetClientContext(engine, context uintptr) {
	cronetEngineSetClientContext(engine, context)
}

func EngineGetClientContext(engine uintptr) uintptr {
	return cronetEngineGetClientContext(engine)
}

func EngineSetMockCertVerifierForTesting(engine, certVerifier uintptr) {
	cronetEngineSetMockCertVerifierForTesting(engine, certVerifier)
}

func EngineCloseAllConnections(engine uintptr) {
	cronetEngineCloseAllConnections(engine)
}

func EngineSetDialer(engine, dialer, context uintptr) {
	cronetEngineSetDialer(engine, dialer, context)
}

func EngineSetUdpDialer(engine, dialer, context uintptr) {
	cronetEngineSetUdpDialer(engine, dialer, context)
}

func EngineGetStreamEngine(engine uintptr) uintptr {
	return cronetEngineGetStreamEngine(engine)
}

// EngineParams API

func EngineParamsCreate() uintptr {
	ensureLoaded()
	return cronetEngineParamsCreate()
}

func EngineParamsDestroy(params uintptr) {
	cronetEngineParamsDestroy(params)
}

func EngineParamsEnableCheckResultSet(params uintptr, value bool) {
	cronetEngineParamsEnableCheckResultSet(params, value)
}

func EngineParamsUserAgentSet(params uintptr, userAgent string) {
	cronetEngineParamsUserAgentSet(params, userAgent)
}

func EngineParamsAcceptLanguageSet(params uintptr, acceptLanguage string) {
	cronetEngineParamsAcceptLanguageSet(params, acceptLanguage)
}

func EngineParamsStoragePathSet(params uintptr, storagePath string) {
	cronetEngineParamsStoragePathSet(params, storagePath)
}

func EngineParamsEnableQuicSet(params uintptr, value bool) {
	cronetEngineParamsEnableQuicSet(params, value)
}

func EngineParamsEnableHttp2Set(params uintptr, value bool) {
	cronetEngineParamsEnableHttp2Set(params, value)
}

func EngineParamsEnableBrotliSet(params uintptr, value bool) {
	cronetEngineParamsEnableBrotliSet(params, value)
}

func EngineParamsHttpCacheModeSet(params uintptr, mode int32) {
	cronetEngineParamsHttpCacheModeSet(params, mode)
}

func EngineParamsHttpCacheMaxSizeSet(params uintptr, size int64) {
	cronetEngineParamsHttpCacheMaxSizeSet(params, size)
}

func EngineParamsQuicHintsAdd(params, quicHint uintptr) {
	cronetEngineParamsQuicHintsAdd(params, quicHint)
}

func EngineParamsPublicKeyPinsAdd(params, publicKeyPins uintptr) {
	cronetEngineParamsPublicKeyPinsAdd(params, publicKeyPins)
}

func EngineParamsEnablePublicKeyPinningBypassForLocalTrustAnchorsSet(params uintptr, value bool) {
	cronetEngineParamsEnablePublicKeyPinningBypassForLocalTrustAnchorsSet(params, value)
}

func EngineParamsExperimentalOptionsSet(params uintptr, options string) {
	cronetEngineParamsExperimentalOptionsSet(params, options)
}

func EngineParamsEnableCheckResultGet(params uintptr) bool {
	return cronetEngineParamsEnableCheckResultGet(params)
}

func EngineParamsUserAgentGet(params uintptr) string {
	return GoString(cronetEngineParamsUserAgentGet(params))
}

func EngineParamsAcceptLanguageGet(params uintptr) string {
	return GoString(cronetEngineParamsAcceptLanguageGet(params))
}

func EngineParamsStoragePathGet(params uintptr) string {
	return GoString(cronetEngineParamsStoragePathGet(params))
}

func EngineParamsEnableQuicGet(params uintptr) bool {
	return cronetEngineParamsEnableQuicGet(params)
}

func EngineParamsEnableHttp2Get(params uintptr) bool {
	return cronetEngineParamsEnableHttp2Get(params)
}

func EngineParamsEnableBrotliGet(params uintptr) bool {
	return cronetEngineParamsEnableBrotliGet(params)
}

func EngineParamsHttpCacheModeGet(params uintptr) int32 {
	return cronetEngineParamsHttpCacheModeGet(params)
}

func EngineParamsHttpCacheMaxSizeGet(params uintptr) int64 {
	return cronetEngineParamsHttpCacheMaxSizeGet(params)
}

func EngineParamsQuicHintsSize(params uintptr) uint32 {
	return cronetEngineParamsQuicHintsSize(params)
}

func EngineParamsQuicHintsAt(params uintptr, index uint32) uintptr {
	return cronetEngineParamsQuicHintsAt(params, index)
}

func EngineParamsQuicHintsClear(params uintptr) {
	cronetEngineParamsQuicHintsClear(params)
}

func EngineParamsPublicKeyPinsSize(params uintptr) uint32 {
	return cronetEngineParamsPublicKeyPinsSize(params)
}

func EngineParamsPublicKeyPinsAt(params uintptr, index uint32) uintptr {
	return cronetEngineParamsPublicKeyPinsAt(params, index)
}

func EngineParamsPublicKeyPinsClear(params uintptr) {
	cronetEngineParamsPublicKeyPinsClear(params)
}

func EngineParamsEnablePublicKeyPinningBypassForLocalTrustAnchorsGet(params uintptr) bool {
	return cronetEngineParamsEnablePublicKeyPinningBypassForLocalTrustAnchorsGet(params)
}

func EngineParamsExperimentalOptionsGet(params uintptr) string {
	return GoString(cronetEngineParamsExperimentalOptionsGet(params))
}

// Certificate verifier functions

func CreateCertVerifierWithRootCerts(pemRootCerts string) uintptr {
	ensureLoaded()
	return cronetCreateCertVerifierWithRootCerts(pemRootCerts)
}

// Buffer API

func BufferCreate() uintptr {
	ensureLoaded()
	return cronetBufferCreate()
}

func BufferDestroy(buffer uintptr) {
	cronetBufferDestroy(buffer)
}

func BufferSetClientContext(buffer, context uintptr) {
	cronetBufferSetClientContext(buffer, context)
}

func BufferGetClientContext(buffer uintptr) uintptr {
	return cronetBufferGetClientContext(buffer)
}

func BufferInitWithDataAndCallback(buffer, data uintptr, size uint64, callback uintptr) {
	cronetBufferInitWithDataAndCallback(buffer, data, size, callback)
}

func BufferInitWithAlloc(buffer uintptr, size uint64) {
	cronetBufferInitWithAlloc(buffer, size)
}

func BufferGetSize(buffer uintptr) uint64 {
	return cronetBufferGetSize(buffer)
}

func BufferGetData(buffer uintptr) uintptr {
	return cronetBufferGetData(buffer)
}

// BufferCallback API

func BufferCallbackDestroy(callback uintptr) {
	cronetBufferCallbackDestroy(callback)
}

func BufferCallbackSetClientContext(callback, context uintptr) {
	cronetBufferCallbackSetClientContext(callback, context)
}

func BufferCallbackGetClientContext(callback uintptr) uintptr {
	return cronetBufferCallbackGetClientContext(callback)
}

func BufferCallbackCreateWith(onDestroy uintptr) uintptr {
	ensureLoaded()
	return cronetBufferCallbackCreateWith(onDestroy)
}

// Executor API

func ExecutorDestroy(executor uintptr) {
	cronetExecutorDestroy(executor)
}

func ExecutorSetClientContext(executor, context uintptr) {
	cronetExecutorSetClientContext(executor, context)
}

func ExecutorGetClientContext(executor uintptr) uintptr {
	return cronetExecutorGetClientContext(executor)
}

func ExecutorExecute(executor, runnable uintptr) {
	cronetExecutorExecute(executor, runnable)
}

func ExecutorCreateWith(execute uintptr) uintptr {
	ensureLoaded()
	return cronetExecutorCreateWith(execute)
}

// Runnable API

func RunnableDestroy(runnable uintptr) {
	cronetRunnableDestroy(runnable)
}

func RunnableSetClientContext(runnable, context uintptr) {
	cronetRunnableSetClientContext(runnable, context)
}

func RunnableGetClientContext(runnable uintptr) uintptr {
	return cronetRunnableGetClientContext(runnable)
}

func RunnableRun(runnable uintptr) {
	cronetRunnableRun(runnable)
}

func RunnableCreateWith(run uintptr) uintptr {
	ensureLoaded()
	return cronetRunnableCreateWith(run)
}

// URLRequest API

func UrlRequestCreate() uintptr {
	ensureLoaded()
	return cronetUrlRequestCreate()
}

func UrlRequestDestroy(request uintptr) {
	cronetUrlRequestDestroy(request)
}

func UrlRequestSetClientContext(request, context uintptr) {
	cronetUrlRequestSetClientContext(request, context)
}

func UrlRequestGetClientContext(request uintptr) uintptr {
	return cronetUrlRequestGetClientContext(request)
}

func UrlRequestInitWithParams(request, engine uintptr, url string, params, callback, executor uintptr) int32 {
	return cronetUrlRequestInitWithParams(request, engine, url, params, callback, executor)
}

func UrlRequestStart(request uintptr) int32 {
	return cronetUrlRequestStart(request)
}

func UrlRequestFollowRedirect(request uintptr) int32 {
	return cronetUrlRequestFollowRedirect(request)
}

func UrlRequestRead(request, buffer uintptr) int32 {
	return cronetUrlRequestRead(request, buffer)
}

func UrlRequestCancel(request uintptr) {
	cronetUrlRequestCancel(request)
}

func UrlRequestIsDone(request uintptr) bool {
	return cronetUrlRequestIsDone(request)
}

func UrlRequestGetStatus(request, listener uintptr) {
	cronetUrlRequestGetStatus(request, listener)
}

// URLRequestParams API

func UrlRequestParamsCreate() uintptr {
	ensureLoaded()
	return cronetUrlRequestParamsCreate()
}

func UrlRequestParamsDestroy(params uintptr) {
	cronetUrlRequestParamsDestroy(params)
}

func UrlRequestParamsHttpMethodSet(params uintptr, method string) {
	cronetUrlRequestParamsHttpMethodSet(params, method)
}

func UrlRequestParamsRequestHeadersAdd(params, header uintptr) {
	cronetUrlRequestParamsRequestHeadersAdd(params, header)
}

func UrlRequestParamsDisableCacheSet(params uintptr, value bool) {
	cronetUrlRequestParamsDisableCacheSet(params, value)
}

func UrlRequestParamsPrioritySet(params uintptr, priority int32) {
	cronetUrlRequestParamsPrioritySet(params, priority)
}

func UrlRequestParamsUploadDataProviderSet(params, provider uintptr) {
	cronetUrlRequestParamsUploadDataProviderSet(params, provider)
}

func UrlRequestParamsUploadDataProviderExecutorSet(params, executor uintptr) {
	cronetUrlRequestParamsUploadDataProviderExecutorSet(params, executor)
}

func UrlRequestParamsAllowDirectExecutorSet(params uintptr, value bool) {
	cronetUrlRequestParamsAllowDirectExecutorSet(params, value)
}

func UrlRequestParamsAnnotationsAdd(params, annotation uintptr) {
	cronetUrlRequestParamsAnnotationsAdd(params, annotation)
}

func UrlRequestParamsRequestFinishedListenerSet(params, listener uintptr) {
	cronetUrlRequestParamsRequestFinishedListenerSet(params, listener)
}

func UrlRequestParamsRequestFinishedExecutorSet(params, executor uintptr) {
	cronetUrlRequestParamsRequestFinishedExecutorSet(params, executor)
}

func UrlRequestParamsIdempotencySet(params uintptr, idempotency int32) {
	cronetUrlRequestParamsIdempotencySet(params, idempotency)
}

func UrlRequestParamsHttpMethodGet(params uintptr) string {
	return GoString(cronetUrlRequestParamsHttpMethodGet(params))
}

func UrlRequestParamsRequestHeadersSize(params uintptr) uint32 {
	return cronetUrlRequestParamsRequestHeadersSize(params)
}

func UrlRequestParamsRequestHeadersAt(params uintptr, index uint32) uintptr {
	return cronetUrlRequestParamsRequestHeadersAt(params, index)
}

func UrlRequestParamsRequestHeadersClear(params uintptr) {
	cronetUrlRequestParamsRequestHeadersClear(params)
}

func UrlRequestParamsDisableCacheGet(params uintptr) bool {
	return cronetUrlRequestParamsDisableCacheGet(params)
}

func UrlRequestParamsPriorityGet(params uintptr) int32 {
	return cronetUrlRequestParamsPriorityGet(params)
}

func UrlRequestParamsUploadDataProviderGet(params uintptr) uintptr {
	return cronetUrlRequestParamsUploadDataProviderGet(params)
}

func UrlRequestParamsUploadDataProviderExecutorGet(params uintptr) uintptr {
	return cronetUrlRequestParamsUploadDataProviderExecutorGet(params)
}

func UrlRequestParamsAllowDirectExecutorGet(params uintptr) bool {
	return cronetUrlRequestParamsAllowDirectExecutorGet(params)
}

func UrlRequestParamsAnnotationsSize(params uintptr) uint32 {
	return cronetUrlRequestParamsAnnotationsSize(params)
}

func UrlRequestParamsAnnotationsAt(params uintptr, index uint32) uintptr {
	return cronetUrlRequestParamsAnnotationsAt(params, index)
}

func UrlRequestParamsAnnotationsClear(params uintptr) {
	cronetUrlRequestParamsAnnotationsClear(params)
}

func UrlRequestParamsRequestFinishedListenerGet(params uintptr) uintptr {
	return cronetUrlRequestParamsRequestFinishedListenerGet(params)
}

func UrlRequestParamsRequestFinishedExecutorGet(params uintptr) uintptr {
	return cronetUrlRequestParamsRequestFinishedExecutorGet(params)
}

func UrlRequestParamsIdempotencyGet(params uintptr) int32 {
	return cronetUrlRequestParamsIdempotencyGet(params)
}

// URLRequestCallback API

func UrlRequestCallbackDestroy(callback uintptr) {
	cronetUrlRequestCallbackDestroy(callback)
}

func UrlRequestCallbackSetClientContext(callback, context uintptr) {
	cronetUrlRequestCallbackSetClientContext(callback, context)
}

func UrlRequestCallbackGetClientContext(callback uintptr) uintptr {
	return cronetUrlRequestCallbackGetClientContext(callback)
}

func UrlRequestCallbackCreateWith(onRedirectReceived, onResponseStarted, onReadCompleted, onSucceeded, onFailed, onCanceled uintptr) uintptr {
	ensureLoaded()
	return cronetUrlRequestCallbackCreateWith(onRedirectReceived, onResponseStarted, onReadCompleted, onSucceeded, onFailed, onCanceled)
}

// URLRequestStatusListener API

func UrlRequestStatusListenerDestroy(listener uintptr) {
	cronetUrlRequestStatusListenerDestroy(listener)
}

func UrlRequestStatusListenerSetClientContext(listener, context uintptr) {
	cronetUrlRequestStatusListenerSetClientContext(listener, context)
}

func UrlRequestStatusListenerGetClientContext(listener uintptr) uintptr {
	return cronetUrlRequestStatusListenerGetClientContext(listener)
}

func UrlRequestStatusListenerCreateWith(onStatus uintptr) uintptr {
	ensureLoaded()
	return cronetUrlRequestStatusListenerCreateWith(onStatus)
}

// UploadDataProvider API

func UploadDataProviderDestroy(provider uintptr) {
	cronetUploadDataProviderDestroy(provider)
}

func UploadDataProviderSetClientContext(provider, context uintptr) {
	cronetUploadDataProviderSetClientContext(provider, context)
}

func UploadDataProviderGetClientContext(provider uintptr) uintptr {
	return cronetUploadDataProviderGetClientContext(provider)
}

func UploadDataProviderCreateWith(getLength, read, rewind, close uintptr) uintptr {
	ensureLoaded()
	return cronetUploadDataProviderCreateWith(getLength, read, rewind, close)
}

// UploadDataSink API

func UploadDataSinkDestroy(sink uintptr) {
	cronetUploadDataSinkDestroy(sink)
}

func UploadDataSinkSetClientContext(sink, context uintptr) {
	cronetUploadDataSinkSetClientContext(sink, context)
}

func UploadDataSinkGetClientContext(sink uintptr) uintptr {
	return cronetUploadDataSinkGetClientContext(sink)
}

func UploadDataSinkOnReadSucceeded(sink uintptr, bytesRead uint64, finalChunk bool) {
	cronetUploadDataSinkOnReadSucceeded(sink, bytesRead, finalChunk)
}

func UploadDataSinkOnReadError(sink uintptr, message string) {
	cronetUploadDataSinkOnReadError(sink, message)
}

func UploadDataSinkOnRewindSucceeded(sink uintptr) {
	cronetUploadDataSinkOnRewindSucceeded(sink)
}

func UploadDataSinkOnRewindError(sink uintptr, message string) {
	cronetUploadDataSinkOnRewindError(sink, message)
}

// URLResponseInfo API

func UrlResponseInfoCreate() uintptr {
	ensureLoaded()
	return cronetUrlResponseInfoCreate()
}

func UrlResponseInfoDestroy(info uintptr) {
	cronetUrlResponseInfoDestroy(info)
}

func UrlResponseInfoUrlGet(info uintptr) string {
	return GoString(cronetUrlResponseInfoUrlGet(info))
}

func UrlResponseInfoUrlChainSize(info uintptr) uint32 {
	return cronetUrlResponseInfoUrlChainSize(info)
}

func UrlResponseInfoUrlChainAt(info uintptr, index uint32) string {
	return GoString(cronetUrlResponseInfoUrlChainAt(info, index))
}

func UrlResponseInfoHttpStatusCodeGet(info uintptr) int32 {
	return cronetUrlResponseInfoHttpStatusCodeGet(info)
}

func UrlResponseInfoHttpStatusTextGet(info uintptr) string {
	return GoString(cronetUrlResponseInfoHttpStatusTextGet(info))
}

func UrlResponseInfoAllHeadersListSize(info uintptr) uint32 {
	return cronetUrlResponseInfoAllHeadersListSize(info)
}

func UrlResponseInfoAllHeadersListAt(info uintptr, index uint32) uintptr {
	return cronetUrlResponseInfoAllHeadersListAt(info, index)
}

func UrlResponseInfoWasCachedGet(info uintptr) bool {
	return cronetUrlResponseInfoWasCachedGet(info)
}

func UrlResponseInfoNegotiatedProtocolGet(info uintptr) string {
	return GoString(cronetUrlResponseInfoNegotiatedProtocolGet(info))
}

func UrlResponseInfoProxyServerGet(info uintptr) string {
	return GoString(cronetUrlResponseInfoProxyServerGet(info))
}

func UrlResponseInfoReceivedByteCountGet(info uintptr) int64 {
	return cronetUrlResponseInfoReceivedByteCountGet(info)
}

// Error API

func ErrorCreate() uintptr {
	ensureLoaded()
	return cronetErrorCreate()
}

func ErrorDestroy(err uintptr) {
	cronetErrorDestroy(err)
}

func ErrorErrorCodeGet(err uintptr) int32 {
	return cronetErrorErrorCodeGet(err)
}

func ErrorMessageGet(err uintptr) string {
	return GoString(cronetErrorMessageGet(err))
}

func ErrorInternalErrorCodeGet(err uintptr) int32 {
	return cronetErrorInternalErrorCodeGet(err)
}

func ErrorImmediatelyRetryableGet(err uintptr) bool {
	return cronetErrorImmediatelyRetryableGet(err)
}

func ErrorQuicDetailedErrorCodeGet(err uintptr) int32 {
	return cronetErrorQuicDetailedErrorCodeGet(err)
}

func ErrorErrorCodeSet(err uintptr, code int32) {
	cronetErrorErrorCodeSet(err, code)
}

func ErrorMessageSet(err uintptr, message string) {
	ptr, backing := CString(message)
	cronetErrorMessageSet(err, ptr)
	runtime.KeepAlive(backing)
}

func ErrorInternalErrorCodeSet(err uintptr, code int32) {
	cronetErrorInternalErrorCodeSet(err, code)
}

func ErrorImmediatelyRetryableSet(err uintptr, retryable bool) {
	cronetErrorImmediatelyRetryableSet(err, retryable)
}

func ErrorQuicDetailedErrorCodeSet(err uintptr, code int32) {
	cronetErrorQuicDetailedErrorCodeSet(err, code)
}

// HttpHeader API

func HttpHeaderCreate() uintptr {
	ensureLoaded()
	return cronetHttpHeaderCreate()
}

func HttpHeaderDestroy(header uintptr) {
	cronetHttpHeaderDestroy(header)
}

func HttpHeaderNameSet(header uintptr, name string) {
	cronetHttpHeaderNameSet(header, name)
}

func HttpHeaderValueSet(header uintptr, value string) {
	cronetHttpHeaderValueSet(header, value)
}

func HttpHeaderNameGet(header uintptr) string {
	return GoString(cronetHttpHeaderNameGet(header))
}

func HttpHeaderValueGet(header uintptr) string {
	return GoString(cronetHttpHeaderValueGet(header))
}

// QuicHint API

func QuicHintCreate() uintptr {
	ensureLoaded()
	return cronetQuicHintCreate()
}

func QuicHintDestroy(hint uintptr) {
	cronetQuicHintDestroy(hint)
}

func QuicHintHostSet(hint uintptr, host string) {
	cronetQuicHintHostSet(hint, host)
}

func QuicHintPortSet(hint uintptr, port int32) {
	cronetQuicHintPortSet(hint, port)
}

func QuicHintAlternatePortSet(hint uintptr, port int32) {
	cronetQuicHintAlternatePortSet(hint, port)
}

func QuicHintHostGet(hint uintptr) string {
	return GoString(cronetQuicHintHostGet(hint))
}

func QuicHintPortGet(hint uintptr) int32 {
	return cronetQuicHintPortGet(hint)
}

func QuicHintAlternatePortGet(hint uintptr) int32 {
	return cronetQuicHintAlternatePortGet(hint)
}

// PublicKeyPins API

func PublicKeyPinsCreate() uintptr {
	ensureLoaded()
	return cronetPublicKeyPinsCreate()
}

func PublicKeyPinsDestroy(pins uintptr) {
	cronetPublicKeyPinsDestroy(pins)
}

func PublicKeyPinsHostSet(pins uintptr, host string) {
	cronetPublicKeyPinsHostSet(pins, host)
}

func PublicKeyPinsPinsSha256Add(pins uintptr, pinSha256 string) {
	cronetPublicKeyPinsPinsSha256Add(pins, pinSha256)
}

func PublicKeyPinsIncludeSubdomainsSet(pins uintptr, value bool) {
	cronetPublicKeyPinsIncludeSubdomainsSet(pins, value)
}

func PublicKeyPinsExpirationDateSet(pins uintptr, date int64) {
	cronetPublicKeyPinsExpirationDateSet(pins, date)
}

func PublicKeyPinsHostGet(pins uintptr) string {
	return GoString(cronetPublicKeyPinsHostGet(pins))
}

func PublicKeyPinsPinsSha256Size(pins uintptr) uint32 {
	return cronetPublicKeyPinsPinsSha256Size(pins)
}

func PublicKeyPinsPinsSha256At(pins uintptr, index uint32) string {
	return GoString(cronetPublicKeyPinsPinsSha256At(pins, index))
}

func PublicKeyPinsIncludeSubdomainsGet(pins uintptr) bool {
	return cronetPublicKeyPinsIncludeSubdomainsGet(pins)
}

func PublicKeyPinsExpirationDateGet(pins uintptr) int64 {
	return cronetPublicKeyPinsExpirationDateGet(pins)
}

// DateTime API

func DateTimeCreate() uintptr {
	ensureLoaded()
	return cronetDateTimeCreate()
}

func DateTimeDestroy(dt uintptr) {
	cronetDateTimeDestroy(dt)
}

func DateTimeValueSet(dt uintptr, value int64) {
	cronetDateTimeValueSet(dt, value)
}

func DateTimeValueGet(dt uintptr) int64 {
	return cronetDateTimeValueGet(dt)
}

// Metrics API

func MetricsCreate() uintptr {
	ensureLoaded()
	return cronetMetricsCreate()
}

func MetricsDestroy(metrics uintptr) {
	cronetMetricsDestroy(metrics)
}

func MetricsRequestStartGet(metrics uintptr) uintptr {
	return cronetMetricsRequestStartGet(metrics)
}

func MetricsDnsStartGet(metrics uintptr) uintptr {
	return cronetMetricsDnsStartGet(metrics)
}

func MetricsDnsEndGet(metrics uintptr) uintptr {
	return cronetMetricsDnsEndGet(metrics)
}

func MetricsConnectStartGet(metrics uintptr) uintptr {
	return cronetMetricsConnectStartGet(metrics)
}

func MetricsConnectEndGet(metrics uintptr) uintptr {
	return cronetMetricsConnectEndGet(metrics)
}

func MetricsSslStartGet(metrics uintptr) uintptr {
	return cronetMetricsSslStartGet(metrics)
}

func MetricsSslEndGet(metrics uintptr) uintptr {
	return cronetMetricsSslEndGet(metrics)
}

func MetricsSendingStartGet(metrics uintptr) uintptr {
	return cronetMetricsSendingStartGet(metrics)
}

func MetricsSendingEndGet(metrics uintptr) uintptr {
	return cronetMetricsSendingEndGet(metrics)
}

func MetricsPushStartGet(metrics uintptr) uintptr {
	return cronetMetricsPushStartGet(metrics)
}

func MetricsPushEndGet(metrics uintptr) uintptr {
	return cronetMetricsPushEndGet(metrics)
}

func MetricsResponseStartGet(metrics uintptr) uintptr {
	return cronetMetricsResponseStartGet(metrics)
}

func MetricsRequestEndGet(metrics uintptr) uintptr {
	return cronetMetricsRequestEndGet(metrics)
}

func MetricsSocketReusedGet(metrics uintptr) bool {
	return cronetMetricsSocketReusedGet(metrics)
}

func MetricsSentByteCountGet(metrics uintptr) int64 {
	return cronetMetricsSentByteCountGet(metrics)
}

func MetricsReceivedByteCountGet(metrics uintptr) int64 {
	return cronetMetricsReceivedByteCountGet(metrics)
}

func MetricsRequestStartSet(metrics, dateTime uintptr) {
	cronetMetricsRequestStartSet(metrics, dateTime)
}

func MetricsDnsStartSet(metrics, dateTime uintptr) {
	cronetMetricsDnsStartSet(metrics, dateTime)
}

func MetricsDnsEndSet(metrics, dateTime uintptr) {
	cronetMetricsDnsEndSet(metrics, dateTime)
}

func MetricsConnectStartSet(metrics, dateTime uintptr) {
	cronetMetricsConnectStartSet(metrics, dateTime)
}

func MetricsConnectEndSet(metrics, dateTime uintptr) {
	cronetMetricsConnectEndSet(metrics, dateTime)
}

func MetricsSslStartSet(metrics, dateTime uintptr) {
	cronetMetricsSslStartSet(metrics, dateTime)
}

func MetricsSslEndSet(metrics, dateTime uintptr) {
	cronetMetricsSslEndSet(metrics, dateTime)
}

func MetricsSendingStartSet(metrics, dateTime uintptr) {
	cronetMetricsSendingStartSet(metrics, dateTime)
}

func MetricsSendingEndSet(metrics, dateTime uintptr) {
	cronetMetricsSendingEndSet(metrics, dateTime)
}

func MetricsPushStartSet(metrics, dateTime uintptr) {
	cronetMetricsPushStartSet(metrics, dateTime)
}

func MetricsPushEndSet(metrics, dateTime uintptr) {
	cronetMetricsPushEndSet(metrics, dateTime)
}

func MetricsResponseStartSet(metrics, dateTime uintptr) {
	cronetMetricsResponseStartSet(metrics, dateTime)
}

func MetricsRequestEndSet(metrics, dateTime uintptr) {
	cronetMetricsRequestEndSet(metrics, dateTime)
}

func MetricsSocketReusedSet(metrics uintptr, reused bool) {
	cronetMetricsSocketReusedSet(metrics, reused)
}

func MetricsSentByteCountSet(metrics uintptr, count int64) {
	cronetMetricsSentByteCountSet(metrics, count)
}

func MetricsReceivedByteCountSet(metrics uintptr, count int64) {
	cronetMetricsReceivedByteCountSet(metrics, count)
}

// RequestFinishedInfo API

func RequestFinishedInfoCreate() uintptr {
	ensureLoaded()
	return cronetRequestFinishedInfoCreate()
}

func RequestFinishedInfoDestroy(info uintptr) {
	cronetRequestFinishedInfoDestroy(info)
}

func RequestFinishedInfoMetricsGet(info uintptr) uintptr {
	return cronetRequestFinishedInfoMetricsGet(info)
}

func RequestFinishedInfoAnnotationsSize(info uintptr) uint32 {
	return cronetRequestFinishedInfoAnnotationsSize(info)
}

func RequestFinishedInfoAnnotationsAt(info uintptr, index uint32) uintptr {
	return cronetRequestFinishedInfoAnnotationsAt(info, index)
}

func RequestFinishedInfoFinishedReasonGet(info uintptr) int32 {
	return cronetRequestFinishedInfoFinishedReasonGet(info)
}

// RequestFinishedInfoListener API

func RequestFinishedInfoListenerDestroy(listener uintptr) {
	cronetRequestFinishedInfoListenerDestroy(listener)
}

func RequestFinishedInfoListenerSetClientContext(listener, context uintptr) {
	cronetRequestFinishedInfoListenerSetClientContext(listener, context)
}

func RequestFinishedInfoListenerGetClientContext(listener uintptr) uintptr {
	return cronetRequestFinishedInfoListenerGetClientContext(listener)
}

func RequestFinishedInfoListenerCreateWith(onRequestFinished uintptr) uintptr {
	ensureLoaded()
	return cronetRequestFinishedInfoListenerCreateWith(onRequestFinished)
}

// BidirectionalStream API

func BidirectionalStreamCreate(engine, annotation, callback uintptr) uintptr {
	ensureLoaded()
	return bidirectionalStreamCreate(engine, annotation, callback)
}

func BidirectionalStreamDestroy(stream uintptr) int32 {
	return bidirectionalStreamDestroy(stream)
}

func BidirectionalStreamDisableAutoFlush(stream uintptr, disable bool) {
	bidirectionalStreamDisableAutoFlush(stream, disable)
}

func BidirectionalStreamDelayRequestHeadersUntilFlush(stream uintptr, delay bool) {
	bidirectionalStreamDelayRequestHeadersUntilFlush(stream, delay)
}

func BidirectionalStreamStart(stream uintptr, url string, priority int32, method string, headers uintptr, endOfStream bool) int32 {
	return bidirectionalStreamStart(stream, url, priority, method, headers, endOfStream)
}

func BidirectionalStreamRead(stream, buffer uintptr, capacity int32) int32 {
	return bidirectionalStreamRead(stream, buffer, capacity)
}

func BidirectionalStreamWrite(stream, buffer uintptr, count int32, endOfStream bool) int32 {
	return bidirectionalStreamWrite(stream, buffer, count, endOfStream)
}

func BidirectionalStreamFlush(stream uintptr) {
	bidirectionalStreamFlush(stream)
}

func BidirectionalStreamCancel(stream uintptr) {
	bidirectionalStreamCancel(stream)
}
