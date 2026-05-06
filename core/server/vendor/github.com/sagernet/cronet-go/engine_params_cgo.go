//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import (
	"unsafe"
)

func NewEngineParams() EngineParams {
	return EngineParams{uintptr(unsafe.Pointer(C.Cronet_EngineParams_Create()))}
}

func (p EngineParams) Destroy() {
	C.Cronet_EngineParams_Destroy(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)))
}

// SetEnableCheckResult override strict result checking for all operations that return RESULT.
// If set to true, then failed result will cause native crash via SIGABRT.
func (p EngineParams) SetEnableCheckResult(enable bool) {
	C.Cronet_EngineParams_enable_check_result_set(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), C.bool(enable))
}

func (p EngineParams) EnableCheckResult() bool {
	return bool(C.Cronet_EngineParams_enable_check_result_get(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

// SetUserAgent override of the User-Agent header for all requests. An explicitly
// set User-Agent header will override a value set using this param.
func (p EngineParams) SetUserAgent(userAgent string) {
	cUserAgent := C.CString(userAgent)
	C.Cronet_EngineParams_user_agent_set(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), cUserAgent)
	C.free(unsafe.Pointer(cUserAgent))
}

func (p EngineParams) UserAgent() string {
	return C.GoString(C.Cronet_EngineParams_user_agent_get(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

// SetAcceptLanguage sets a default value for the Accept-Language header value for UrlRequests
// created by this engine. Explicitly setting the Accept-Language header
// value for individual UrlRequests will override this value.
func (p EngineParams) SetAcceptLanguage(acceptLanguage string) {
	cAcceptLanguage := C.CString(acceptLanguage)
	C.Cronet_EngineParams_accept_language_set(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), cAcceptLanguage)
	C.free(unsafe.Pointer(cAcceptLanguage))
}

func (p EngineParams) AcceptLanguage() string {
	return C.GoString(C.Cronet_EngineParams_accept_language_get(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

// SetStoragePath sets directory for HTTP Cache and Prefs Storage. The directory must exist.
func (p EngineParams) SetStoragePath(storagePath string) {
	cStoragePath := C.CString(storagePath)
	C.Cronet_EngineParams_storage_path_set(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), cStoragePath)
	C.free(unsafe.Pointer(cStoragePath))
}

func (p EngineParams) StoragePath() string {
	return C.GoString(C.Cronet_EngineParams_storage_path_get(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

// SetEnableQuic sets whether <a href="https://www.chromium.org/quic">QUIC</a> protocol
// is enabled. If QUIC is enabled, then QUIC User Agent Id
// containing application name and Cronet version is sent to the server.
func (p EngineParams) SetEnableQuic(enable bool) {
	C.Cronet_EngineParams_enable_quic_set(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), C.bool(enable))
}

func (p EngineParams) EnableQuic() bool {
	return bool(C.Cronet_EngineParams_enable_quic_get(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

// SetEnableHTTP2 sets whether <a href="https://tools.ietf.org/html/rfc7540">HTTP/2</a>
// protocol is enabled.
func (p EngineParams) SetEnableHTTP2(enable bool) {
	C.Cronet_EngineParams_enable_http2_set(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), C.bool(enable))
}

func (p EngineParams) EnableHTTP2() bool {
	return bool(C.Cronet_EngineParams_enable_http2_get(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

// SetEnableBrotli sets whether <a href="https://tools.ietf.org/html/rfc7932">Brotli</a> compression is
// enabled. If enabled, Brotli will be advertised in Accept-Encoding request headers.
func (p EngineParams) SetEnableBrotli(enable bool) {
	C.Cronet_EngineParams_enable_brotli_set(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), C.bool(enable))
}

func (p EngineParams) EnableBrotli() bool {
	return bool(C.Cronet_EngineParams_enable_brotli_get(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

// SetHTTPCacheMode enables or disables caching of HTTP data and other information like QUIC
// server information.
func (p EngineParams) SetHTTPCacheMode(mode EngineParamsHTTPCacheMode) {
	C.Cronet_EngineParams_http_cache_mode_set(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), C.Cronet_EngineParams_HTTP_CACHE_MODE(mode))
}

func (p EngineParams) HTTPCacheMode() EngineParamsHTTPCacheMode {
	return EngineParamsHTTPCacheMode(C.Cronet_EngineParams_http_cache_mode_get(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

// SetHTTPCacheMaxSize sets Maximum size in bytes used to cache data (advisory and maybe exceeded at
// times)
func (p EngineParams) SetHTTPCacheMaxSize(maxSize int64) {
	C.Cronet_EngineParams_http_cache_max_size_set(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), C.int64_t(maxSize))
}

func (p EngineParams) HTTPCacheMaxSize() int64 {
	return int64(C.Cronet_EngineParams_http_cache_max_size_get(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

// AddQuicHint add hints that hosts support QUIC.
func (p EngineParams) AddQuicHint(element QuicHint) {
	C.Cronet_EngineParams_quic_hints_add(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), C.Cronet_QuicHintPtr(unsafe.Pointer(element.ptr)))
}

func (p EngineParams) QuicHintSize() int {
	return int(C.Cronet_EngineParams_quic_hints_size(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

func (p EngineParams) QuicHintAt(index int) QuicHint {
	return QuicHint{uintptr(unsafe.Pointer(C.Cronet_EngineParams_quic_hints_at(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), C.uint32_t(index))))}
}

func (p EngineParams) ClearQuicHints() {
	C.Cronet_EngineParams_quic_hints_clear(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)))
}

// AddPublicKeyPins pins a set of public keys for given hosts. See PublicKeyPins for explanation.
func (p EngineParams) AddPublicKeyPins(element PublicKeyPins) {
	C.Cronet_EngineParams_public_key_pins_add(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), C.Cronet_PublicKeyPinsPtr(unsafe.Pointer(element.ptr)))
}

func (p EngineParams) PublicKeyPinsSize() int {
	return int(C.Cronet_EngineParams_public_key_pins_size(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

func (p EngineParams) PublicKeyPinsAt(index int) PublicKeyPins {
	return PublicKeyPins{uintptr(unsafe.Pointer(C.Cronet_EngineParams_public_key_pins_at(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), C.uint32_t(index))))}
}

func (p EngineParams) ClearPublicKeyPins() {
	C.Cronet_EngineParams_public_key_pins_clear(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)))
}

// SetEnablePublicKeyPinningBypassForLocalTrustAnchors enables or disables public key pinning bypass for local trust anchors. Disabling the
// bypass for local trust anchors is highly discouraged since it may prohibit the app
// from communicating with the pinned hosts. E.g., a user may want to send all traffic
// through an SSL enabled proxy by changing the device proxy settings and adding the
// proxy certificate to the list of local trust anchor. Disabling the bypass will most
// likely prevent the app from sending any traffic to the pinned hosts. For more
// information see 'How does key pinning interact with local proxies and filters?' at
// https://www.chromium.org/Home/chromium-security/security-faq
func (p EngineParams) SetEnablePublicKeyPinningBypassForLocalTrustAnchors(enable bool) {
	C.Cronet_EngineParams_enable_public_key_pinning_bypass_for_local_trust_anchors_set(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), C.bool(enable))
}

func (p EngineParams) EnablePublicKeyPinningBypassForLocalTrustAnchors() bool {
	return bool(C.Cronet_EngineParams_enable_public_key_pinning_bypass_for_local_trust_anchors_get(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

// SetNetworkThreadPriority set optional network thread priority. NAN indicates unset, use default.
// On Android, corresponds to android.os.Process.setThreadPriority() values.
// On iOS, corresponds to NSThread::setThreadPriority values.
// Do not specify for other platforms.
func (p EngineParams) SetNetworkThreadPriority(priority float64) {
	C.Cronet_EngineParams_network_thread_priority_set(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), C.double(priority))
}

func (p EngineParams) NetworkThreadPriority() float64 {
	return float64(C.Cronet_EngineParams_network_thread_priority_get(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}

// SetExperimentalOptions set JSON formatted experimental options to be used in Cronet Engine.
func (p EngineParams) SetExperimentalOptions(options string) {
	cOptions := C.CString(options)
	C.Cronet_EngineParams_experimental_options_set(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr)), cOptions)
	C.free(unsafe.Pointer(cOptions))
}

func (p EngineParams) ExperimentalOptions() string {
	return C.GoString(C.Cronet_EngineParams_experimental_options_get(C.Cronet_EngineParamsPtr(unsafe.Pointer(p.ptr))))
}
