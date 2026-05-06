//go:build with_purego

package cronet

import (
	"github.com/sagernet/cronet-go/internal/cronet"
)

func NewEngineParams() EngineParams {
	return EngineParams{cronet.EngineParamsCreate()}
}

func (p EngineParams) Destroy() {
	cronet.EngineParamsDestroy(p.ptr)
}

func (p EngineParams) SetEnableCheckResult(enable bool) {
	cronet.EngineParamsEnableCheckResultSet(p.ptr, enable)
}

func (p EngineParams) EnableCheckResult() bool {
	return cronet.EngineParamsEnableCheckResultGet(p.ptr)
}

func (p EngineParams) SetUserAgent(userAgent string) {
	cronet.EngineParamsUserAgentSet(p.ptr, userAgent)
}

func (p EngineParams) UserAgent() string {
	return cronet.EngineParamsUserAgentGet(p.ptr)
}

func (p EngineParams) SetAcceptLanguage(acceptLanguage string) {
	cronet.EngineParamsAcceptLanguageSet(p.ptr, acceptLanguage)
}

func (p EngineParams) AcceptLanguage() string {
	return cronet.EngineParamsAcceptLanguageGet(p.ptr)
}

func (p EngineParams) SetStoragePath(storagePath string) {
	cronet.EngineParamsStoragePathSet(p.ptr, storagePath)
}

func (p EngineParams) StoragePath() string {
	return cronet.EngineParamsStoragePathGet(p.ptr)
}

func (p EngineParams) SetEnableQuic(enable bool) {
	cronet.EngineParamsEnableQuicSet(p.ptr, enable)
}

func (p EngineParams) EnableQuic() bool {
	return cronet.EngineParamsEnableQuicGet(p.ptr)
}

func (p EngineParams) SetEnableHTTP2(enable bool) {
	cronet.EngineParamsEnableHttp2Set(p.ptr, enable)
}

func (p EngineParams) EnableHTTP2() bool {
	return cronet.EngineParamsEnableHttp2Get(p.ptr)
}

func (p EngineParams) SetEnableBrotli(enable bool) {
	cronet.EngineParamsEnableBrotliSet(p.ptr, enable)
}

func (p EngineParams) EnableBrotli() bool {
	return cronet.EngineParamsEnableBrotliGet(p.ptr)
}

func (p EngineParams) SetHTTPCacheMode(mode EngineParamsHTTPCacheMode) {
	cronet.EngineParamsHttpCacheModeSet(p.ptr, int32(mode))
}

func (p EngineParams) HTTPCacheMode() EngineParamsHTTPCacheMode {
	return EngineParamsHTTPCacheMode(cronet.EngineParamsHttpCacheModeGet(p.ptr))
}

func (p EngineParams) SetHTTPCacheMaxSize(size int64) {
	cronet.EngineParamsHttpCacheMaxSizeSet(p.ptr, size)
}

func (p EngineParams) HTTPCacheMaxSize() int64 {
	return cronet.EngineParamsHttpCacheMaxSizeGet(p.ptr)
}

func (p EngineParams) AddQuicHint(hint QuicHint) {
	cronet.EngineParamsQuicHintsAdd(p.ptr, hint.ptr)
}

func (p EngineParams) QuicHintSize() int {
	return int(cronet.EngineParamsQuicHintsSize(p.ptr))
}

func (p EngineParams) QuicHintAt(index int) QuicHint {
	return QuicHint{cronet.EngineParamsQuicHintsAt(p.ptr, uint32(index))}
}

func (p EngineParams) ClearQuicHints() {
	cronet.EngineParamsQuicHintsClear(p.ptr)
}

func (p EngineParams) AddPublicKeyPins(pins PublicKeyPins) {
	cronet.EngineParamsPublicKeyPinsAdd(p.ptr, pins.ptr)
}

func (p EngineParams) PublicKeyPinsSize() int {
	return int(cronet.EngineParamsPublicKeyPinsSize(p.ptr))
}

func (p EngineParams) PublicKeyPinsAt(index int) PublicKeyPins {
	return PublicKeyPins{cronet.EngineParamsPublicKeyPinsAt(p.ptr, uint32(index))}
}

func (p EngineParams) ClearPublicKeyPins() {
	cronet.EngineParamsPublicKeyPinsClear(p.ptr)
}

func (p EngineParams) SetEnablePublicKeyPinningBypassForLocalTrustAnchors(enable bool) {
	cronet.EngineParamsEnablePublicKeyPinningBypassForLocalTrustAnchorsSet(p.ptr, enable)
}

func (p EngineParams) EnablePublicKeyPinningBypassForLocalTrustAnchors() bool {
	return cronet.EngineParamsEnablePublicKeyPinningBypassForLocalTrustAnchorsGet(p.ptr)
}

func (p EngineParams) SetNetworkThreadPriority(priority float64) {
	cronet.EngineParamsNetworkThreadPrioritySet(p.ptr, priority)
}

func (p EngineParams) NetworkThreadPriority() float64 {
	return cronet.EngineParamsNetworkThreadPriorityGet(p.ptr)
}

func (p EngineParams) SetExperimentalOptions(options string) {
	cronet.EngineParamsExperimentalOptionsSet(p.ptr, options)
}

func (p EngineParams) ExperimentalOptions() string {
	return cronet.EngineParamsExperimentalOptionsGet(p.ptr)
}
