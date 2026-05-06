//go:build with_purego && windows

package cronet

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"sync"
	"syscall"

	"github.com/ebitengine/purego"
)

var (
	loadOnce  sync.Once
	loadError error
	libHandle uintptr
)

func LoadLibrary(path string) error {
	loadOnce.Do(func() {
		loadError = doLoadLibrary(path)
	})
	return loadError
}

func ensureLoaded() {
	err := LoadLibrary("")
	if err != nil {
		panic(err)
	}
}

func doLoadLibrary(path string) error {
	if path == "" {
		path = findLibrary()
	}

	if path == "" {
		return errors.New("cronet: library not found. Place libcronet.dll in the executable directory or PATH")
	}

	handle, err := syscall.LoadLibrary(path)
	if err != nil {
		return fmt.Errorf("cronet: failed to load library %s: %w", path, err)
	}

	libHandle = uintptr(handle)
	return registerSymbols()
}

func findLibrary() string {
	libName := "libcronet.dll"

	var searchPaths []string
	executablePath, err := os.Executable()
	if err == nil {
		searchPaths = append(searchPaths, filepath.Dir(executablePath))
	}

	if pathEnv := os.Getenv("PATH"); pathEnv != "" {
		paths := filepath.SplitList(pathEnv)
		searchPaths = append(searchPaths, paths...)
	}

	for _, searchPath := range searchPaths {
		fullPath := filepath.Join(searchPath, libName)
		if _, err := os.Stat(fullPath); err == nil {
			return fullPath
		}
	}

	return ""
}

func lookupSymbol(name string) (uintptr, error) {
	if libHandle == 0 {
		return 0, errors.New("cronet: library not loaded")
	}

	proc, err := syscall.GetProcAddress(syscall.Handle(libHandle), name)
	if err != nil {
		return 0, fmt.Errorf("cronet: symbol %s not found: %w", name, err)
	}
	return proc, nil
}

func registerFunc(fnPtr interface{}, name string) error {
	sym, err := lookupSymbol(name)
	if err != nil {
		return err
	}
	purego.RegisterFunc(fnPtr, sym)
	return nil
}

func registerSymbols() error {
	// Buffer
	if err := registerFunc(&cronetBufferCreate, "Cronet_Buffer_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetBufferDestroy, "Cronet_Buffer_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetBufferSetClientContext, "Cronet_Buffer_SetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetBufferGetClientContext, "Cronet_Buffer_GetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetBufferInitWithDataAndCallback, "Cronet_Buffer_InitWithDataAndCallback"); err != nil {
		return err
	}
	if err := registerFunc(&cronetBufferInitWithAlloc, "Cronet_Buffer_InitWithAlloc"); err != nil {
		return err
	}
	if err := registerFunc(&cronetBufferGetSize, "Cronet_Buffer_GetSize"); err != nil {
		return err
	}
	if err := registerFunc(&cronetBufferGetData, "Cronet_Buffer_GetData"); err != nil {
		return err
	}

	// BufferCallback
	if err := registerFunc(&cronetBufferCallbackDestroy, "Cronet_BufferCallback_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetBufferCallbackSetClientContext, "Cronet_BufferCallback_SetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetBufferCallbackGetClientContext, "Cronet_BufferCallback_GetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetBufferCallbackCreateWith, "Cronet_BufferCallback_CreateWith"); err != nil {
		return err
	}

	// Runnable
	if err := registerFunc(&cronetRunnableDestroy, "Cronet_Runnable_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRunnableSetClientContext, "Cronet_Runnable_SetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRunnableGetClientContext, "Cronet_Runnable_GetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRunnableRun, "Cronet_Runnable_Run"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRunnableCreateWith, "Cronet_Runnable_CreateWith"); err != nil {
		return err
	}

	// Executor
	if err := registerFunc(&cronetExecutorDestroy, "Cronet_Executor_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetExecutorSetClientContext, "Cronet_Executor_SetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetExecutorGetClientContext, "Cronet_Executor_GetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetExecutorExecute, "Cronet_Executor_Execute"); err != nil {
		return err
	}
	if err := registerFunc(&cronetExecutorCreateWith, "Cronet_Executor_CreateWith"); err != nil {
		return err
	}

	// Engine
	if err := registerFunc(&cronetEngineCreate, "Cronet_Engine_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineDestroy, "Cronet_Engine_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineSetClientContext, "Cronet_Engine_SetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineGetClientContext, "Cronet_Engine_GetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineStartWithParams, "Cronet_Engine_StartWithParams"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineStartNetLogToFile, "Cronet_Engine_StartNetLogToFile"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineStopNetLog, "Cronet_Engine_StopNetLog"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineShutdown, "Cronet_Engine_Shutdown"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineGetVersionString, "Cronet_Engine_GetVersionString"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineGetDefaultUserAgent, "Cronet_Engine_GetDefaultUserAgent"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineAddRequestFinishedListener, "Cronet_Engine_AddRequestFinishedListener"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineRemoveRequestFinishedListener, "Cronet_Engine_RemoveRequestFinishedListener"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineGetStreamEngine, "Cronet_Engine_GetStreamEngine"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineSetMockCertVerifierForTesting, "Cronet_Engine_SetMockCertVerifierForTesting"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineCloseAllConnections, "Cronet_Engine_CloseAllConnections"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineSetDialer, "Cronet_Engine_SetDialer"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineSetUdpDialer, "Cronet_Engine_SetUdpDialer"); err != nil {
		return err
	}

	// EngineParams
	if err := registerFunc(&cronetEngineParamsCreate, "Cronet_EngineParams_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsDestroy, "Cronet_EngineParams_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsEnableCheckResultSet, "Cronet_EngineParams_enable_check_result_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsUserAgentSet, "Cronet_EngineParams_user_agent_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsAcceptLanguageSet, "Cronet_EngineParams_accept_language_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsStoragePathSet, "Cronet_EngineParams_storage_path_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsEnableQuicSet, "Cronet_EngineParams_enable_quic_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsEnableHttp2Set, "Cronet_EngineParams_enable_http2_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsEnableBrotliSet, "Cronet_EngineParams_enable_brotli_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsHttpCacheModeSet, "Cronet_EngineParams_http_cache_mode_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsHttpCacheMaxSizeSet, "Cronet_EngineParams_http_cache_max_size_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsQuicHintsAdd, "Cronet_EngineParams_quic_hints_add"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsPublicKeyPinsAdd, "Cronet_EngineParams_public_key_pins_add"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsEnablePublicKeyPinningBypassForLocalTrustAnchorsSet, "Cronet_EngineParams_enable_public_key_pinning_bypass_for_local_trust_anchors_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsExperimentalOptionsSet, "Cronet_EngineParams_experimental_options_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsEnableCheckResultGet, "Cronet_EngineParams_enable_check_result_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsUserAgentGet, "Cronet_EngineParams_user_agent_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsAcceptLanguageGet, "Cronet_EngineParams_accept_language_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsStoragePathGet, "Cronet_EngineParams_storage_path_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsEnableQuicGet, "Cronet_EngineParams_enable_quic_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsEnableHttp2Get, "Cronet_EngineParams_enable_http2_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsEnableBrotliGet, "Cronet_EngineParams_enable_brotli_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsHttpCacheModeGet, "Cronet_EngineParams_http_cache_mode_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsHttpCacheMaxSizeGet, "Cronet_EngineParams_http_cache_max_size_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsQuicHintsSize, "Cronet_EngineParams_quic_hints_size"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsQuicHintsAt, "Cronet_EngineParams_quic_hints_at"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsQuicHintsClear, "Cronet_EngineParams_quic_hints_clear"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsPublicKeyPinsSize, "Cronet_EngineParams_public_key_pins_size"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsPublicKeyPinsAt, "Cronet_EngineParams_public_key_pins_at"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsPublicKeyPinsClear, "Cronet_EngineParams_public_key_pins_clear"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsEnablePublicKeyPinningBypassForLocalTrustAnchorsGet, "Cronet_EngineParams_enable_public_key_pinning_bypass_for_local_trust_anchors_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsExperimentalOptionsGet, "Cronet_EngineParams_experimental_options_get"); err != nil {
		return err
	}

	// UrlRequest
	if err := registerFunc(&cronetUrlRequestCreate, "Cronet_UrlRequest_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestDestroy, "Cronet_UrlRequest_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestSetClientContext, "Cronet_UrlRequest_SetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestGetClientContext, "Cronet_UrlRequest_GetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestInitWithParams, "Cronet_UrlRequest_InitWithParams"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestStart, "Cronet_UrlRequest_Start"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestFollowRedirect, "Cronet_UrlRequest_FollowRedirect"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestRead, "Cronet_UrlRequest_Read"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestCancel, "Cronet_UrlRequest_Cancel"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestIsDone, "Cronet_UrlRequest_IsDone"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestGetStatus, "Cronet_UrlRequest_GetStatus"); err != nil {
		return err
	}

	// UrlRequestParams
	if err := registerFunc(&cronetUrlRequestParamsCreate, "Cronet_UrlRequestParams_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsDestroy, "Cronet_UrlRequestParams_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsHttpMethodSet, "Cronet_UrlRequestParams_http_method_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsRequestHeadersAdd, "Cronet_UrlRequestParams_request_headers_add"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsDisableCacheSet, "Cronet_UrlRequestParams_disable_cache_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsPrioritySet, "Cronet_UrlRequestParams_priority_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsUploadDataProviderSet, "Cronet_UrlRequestParams_upload_data_provider_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsUploadDataProviderExecutorSet, "Cronet_UrlRequestParams_upload_data_provider_executor_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsAllowDirectExecutorSet, "Cronet_UrlRequestParams_allow_direct_executor_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsAnnotationsAdd, "Cronet_UrlRequestParams_annotations_add"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsRequestFinishedListenerSet, "Cronet_UrlRequestParams_request_finished_listener_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsRequestFinishedExecutorSet, "Cronet_UrlRequestParams_request_finished_executor_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsIdempotencySet, "Cronet_UrlRequestParams_idempotency_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsHttpMethodGet, "Cronet_UrlRequestParams_http_method_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsRequestHeadersSize, "Cronet_UrlRequestParams_request_headers_size"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsRequestHeadersAt, "Cronet_UrlRequestParams_request_headers_at"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsRequestHeadersClear, "Cronet_UrlRequestParams_request_headers_clear"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsDisableCacheGet, "Cronet_UrlRequestParams_disable_cache_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsPriorityGet, "Cronet_UrlRequestParams_priority_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsUploadDataProviderGet, "Cronet_UrlRequestParams_upload_data_provider_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsUploadDataProviderExecutorGet, "Cronet_UrlRequestParams_upload_data_provider_executor_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsAllowDirectExecutorGet, "Cronet_UrlRequestParams_allow_direct_executor_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsAnnotationsSize, "Cronet_UrlRequestParams_annotations_size"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsAnnotationsAt, "Cronet_UrlRequestParams_annotations_at"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsAnnotationsClear, "Cronet_UrlRequestParams_annotations_clear"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsRequestFinishedListenerGet, "Cronet_UrlRequestParams_request_finished_listener_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsRequestFinishedExecutorGet, "Cronet_UrlRequestParams_request_finished_executor_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestParamsIdempotencyGet, "Cronet_UrlRequestParams_idempotency_get"); err != nil {
		return err
	}

	// UrlRequestCallback
	if err := registerFunc(&cronetUrlRequestCallbackDestroy, "Cronet_UrlRequestCallback_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestCallbackSetClientContext, "Cronet_UrlRequestCallback_SetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestCallbackGetClientContext, "Cronet_UrlRequestCallback_GetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestCallbackCreateWith, "Cronet_UrlRequestCallback_CreateWith"); err != nil {
		return err
	}

	// UrlRequestStatusListener
	if err := registerFunc(&cronetUrlRequestStatusListenerDestroy, "Cronet_UrlRequestStatusListener_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestStatusListenerSetClientContext, "Cronet_UrlRequestStatusListener_SetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestStatusListenerGetClientContext, "Cronet_UrlRequestStatusListener_GetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlRequestStatusListenerCreateWith, "Cronet_UrlRequestStatusListener_CreateWith"); err != nil {
		return err
	}

	// UploadDataProvider
	if err := registerFunc(&cronetUploadDataProviderDestroy, "Cronet_UploadDataProvider_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUploadDataProviderSetClientContext, "Cronet_UploadDataProvider_SetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUploadDataProviderGetClientContext, "Cronet_UploadDataProvider_GetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUploadDataProviderCreateWith, "Cronet_UploadDataProvider_CreateWith"); err != nil {
		return err
	}

	// UploadDataSink
	if err := registerFunc(&cronetUploadDataSinkDestroy, "Cronet_UploadDataSink_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUploadDataSinkSetClientContext, "Cronet_UploadDataSink_SetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUploadDataSinkGetClientContext, "Cronet_UploadDataSink_GetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUploadDataSinkOnReadSucceeded, "Cronet_UploadDataSink_OnReadSucceeded"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUploadDataSinkOnReadError, "Cronet_UploadDataSink_OnReadError"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUploadDataSinkOnRewindSucceeded, "Cronet_UploadDataSink_OnRewindSucceeded"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUploadDataSinkOnRewindError, "Cronet_UploadDataSink_OnRewindError"); err != nil {
		return err
	}

	// UrlResponseInfo
	if err := registerFunc(&cronetUrlResponseInfoCreate, "Cronet_UrlResponseInfo_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoDestroy, "Cronet_UrlResponseInfo_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoUrlSet, "Cronet_UrlResponseInfo_url_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoUrlChainAdd, "Cronet_UrlResponseInfo_url_chain_add"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoHttpStatusCodeSet, "Cronet_UrlResponseInfo_http_status_code_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoHttpStatusTextSet, "Cronet_UrlResponseInfo_http_status_text_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoAllHeadersListAdd, "Cronet_UrlResponseInfo_all_headers_list_add"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoWasCachedSet, "Cronet_UrlResponseInfo_was_cached_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoNegotiatedProtocolSet, "Cronet_UrlResponseInfo_negotiated_protocol_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoProxyServerSet, "Cronet_UrlResponseInfo_proxy_server_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoReceivedByteCountSet, "Cronet_UrlResponseInfo_received_byte_count_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoUrlGet, "Cronet_UrlResponseInfo_url_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoUrlChainSize, "Cronet_UrlResponseInfo_url_chain_size"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoUrlChainAt, "Cronet_UrlResponseInfo_url_chain_at"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoUrlChainClear, "Cronet_UrlResponseInfo_url_chain_clear"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoHttpStatusCodeGet, "Cronet_UrlResponseInfo_http_status_code_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoHttpStatusTextGet, "Cronet_UrlResponseInfo_http_status_text_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoAllHeadersListSize, "Cronet_UrlResponseInfo_all_headers_list_size"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoAllHeadersListAt, "Cronet_UrlResponseInfo_all_headers_list_at"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoAllHeadersListClear, "Cronet_UrlResponseInfo_all_headers_list_clear"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoWasCachedGet, "Cronet_UrlResponseInfo_was_cached_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoNegotiatedProtocolGet, "Cronet_UrlResponseInfo_negotiated_protocol_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoProxyServerGet, "Cronet_UrlResponseInfo_proxy_server_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetUrlResponseInfoReceivedByteCountGet, "Cronet_UrlResponseInfo_received_byte_count_get"); err != nil {
		return err
	}

	// Error
	if err := registerFunc(&cronetErrorCreate, "Cronet_Error_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetErrorDestroy, "Cronet_Error_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetErrorErrorCodeSet, "Cronet_Error_error_code_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetErrorMessageSet, "Cronet_Error_message_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetErrorInternalErrorCodeSet, "Cronet_Error_internal_error_code_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetErrorImmediatelyRetryableSet, "Cronet_Error_immediately_retryable_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetErrorQuicDetailedErrorCodeSet, "Cronet_Error_quic_detailed_error_code_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetErrorErrorCodeGet, "Cronet_Error_error_code_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetErrorMessageGet, "Cronet_Error_message_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetErrorInternalErrorCodeGet, "Cronet_Error_internal_error_code_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetErrorImmediatelyRetryableGet, "Cronet_Error_immediately_retryable_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetErrorQuicDetailedErrorCodeGet, "Cronet_Error_quic_detailed_error_code_get"); err != nil {
		return err
	}

	// HttpHeader
	if err := registerFunc(&cronetHttpHeaderCreate, "Cronet_HttpHeader_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetHttpHeaderDestroy, "Cronet_HttpHeader_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetHttpHeaderNameSet, "Cronet_HttpHeader_name_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetHttpHeaderValueSet, "Cronet_HttpHeader_value_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetHttpHeaderNameGet, "Cronet_HttpHeader_name_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetHttpHeaderValueGet, "Cronet_HttpHeader_value_get"); err != nil {
		return err
	}

	// QuicHint
	if err := registerFunc(&cronetQuicHintCreate, "Cronet_QuicHint_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetQuicHintDestroy, "Cronet_QuicHint_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetQuicHintHostSet, "Cronet_QuicHint_host_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetQuicHintPortSet, "Cronet_QuicHint_port_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetQuicHintAlternatePortSet, "Cronet_QuicHint_alternate_port_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetQuicHintHostGet, "Cronet_QuicHint_host_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetQuicHintPortGet, "Cronet_QuicHint_port_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetQuicHintAlternatePortGet, "Cronet_QuicHint_alternate_port_get"); err != nil {
		return err
	}

	// PublicKeyPins
	if err := registerFunc(&cronetPublicKeyPinsCreate, "Cronet_PublicKeyPins_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetPublicKeyPinsDestroy, "Cronet_PublicKeyPins_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetPublicKeyPinsHostSet, "Cronet_PublicKeyPins_host_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetPublicKeyPinsPinsSha256Add, "Cronet_PublicKeyPins_pins_sha256_add"); err != nil {
		return err
	}
	if err := registerFunc(&cronetPublicKeyPinsIncludeSubdomainsSet, "Cronet_PublicKeyPins_include_subdomains_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetPublicKeyPinsExpirationDateSet, "Cronet_PublicKeyPins_expiration_date_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetPublicKeyPinsHostGet, "Cronet_PublicKeyPins_host_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetPublicKeyPinsPinsSha256Size, "Cronet_PublicKeyPins_pins_sha256_size"); err != nil {
		return err
	}
	if err := registerFunc(&cronetPublicKeyPinsPinsSha256At, "Cronet_PublicKeyPins_pins_sha256_at"); err != nil {
		return err
	}
	if err := registerFunc(&cronetPublicKeyPinsPinsSha256Clear, "Cronet_PublicKeyPins_pins_sha256_clear"); err != nil {
		return err
	}
	if err := registerFunc(&cronetPublicKeyPinsIncludeSubdomainsGet, "Cronet_PublicKeyPins_include_subdomains_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetPublicKeyPinsExpirationDateGet, "Cronet_PublicKeyPins_expiration_date_get"); err != nil {
		return err
	}

	// DateTime
	if err := registerFunc(&cronetDateTimeCreate, "Cronet_DateTime_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetDateTimeDestroy, "Cronet_DateTime_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetDateTimeValueSet, "Cronet_DateTime_value_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetDateTimeValueGet, "Cronet_DateTime_value_get"); err != nil {
		return err
	}

	// Metrics
	if err := registerFunc(&cronetMetricsCreate, "Cronet_Metrics_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsDestroy, "Cronet_Metrics_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsRequestStartSet, "Cronet_Metrics_request_start_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsDnsStartSet, "Cronet_Metrics_dns_start_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsDnsEndSet, "Cronet_Metrics_dns_end_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsConnectStartSet, "Cronet_Metrics_connect_start_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsConnectEndSet, "Cronet_Metrics_connect_end_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsSslStartSet, "Cronet_Metrics_ssl_start_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsSslEndSet, "Cronet_Metrics_ssl_end_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsSendingStartSet, "Cronet_Metrics_sending_start_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsSendingEndSet, "Cronet_Metrics_sending_end_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsPushStartSet, "Cronet_Metrics_push_start_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsPushEndSet, "Cronet_Metrics_push_end_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsResponseStartSet, "Cronet_Metrics_response_start_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsRequestEndSet, "Cronet_Metrics_request_end_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsSocketReusedSet, "Cronet_Metrics_socket_reused_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsSentByteCountSet, "Cronet_Metrics_sent_byte_count_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsReceivedByteCountSet, "Cronet_Metrics_received_byte_count_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsRequestStartGet, "Cronet_Metrics_request_start_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsDnsStartGet, "Cronet_Metrics_dns_start_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsDnsEndGet, "Cronet_Metrics_dns_end_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsConnectStartGet, "Cronet_Metrics_connect_start_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsConnectEndGet, "Cronet_Metrics_connect_end_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsSslStartGet, "Cronet_Metrics_ssl_start_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsSslEndGet, "Cronet_Metrics_ssl_end_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsSendingStartGet, "Cronet_Metrics_sending_start_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsSendingEndGet, "Cronet_Metrics_sending_end_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsPushStartGet, "Cronet_Metrics_push_start_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsPushEndGet, "Cronet_Metrics_push_end_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsResponseStartGet, "Cronet_Metrics_response_start_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsRequestEndGet, "Cronet_Metrics_request_end_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsSocketReusedGet, "Cronet_Metrics_socket_reused_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsSentByteCountGet, "Cronet_Metrics_sent_byte_count_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetMetricsReceivedByteCountGet, "Cronet_Metrics_received_byte_count_get"); err != nil {
		return err
	}

	// RequestFinishedInfo
	if err := registerFunc(&cronetRequestFinishedInfoCreate, "Cronet_RequestFinishedInfo_Create"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRequestFinishedInfoDestroy, "Cronet_RequestFinishedInfo_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRequestFinishedInfoMetricsSet, "Cronet_RequestFinishedInfo_metrics_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRequestFinishedInfoAnnotationsAdd, "Cronet_RequestFinishedInfo_annotations_add"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRequestFinishedInfoFinishedReasonSet, "Cronet_RequestFinishedInfo_finished_reason_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRequestFinishedInfoMetricsGet, "Cronet_RequestFinishedInfo_metrics_get"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRequestFinishedInfoAnnotationsSize, "Cronet_RequestFinishedInfo_annotations_size"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRequestFinishedInfoAnnotationsAt, "Cronet_RequestFinishedInfo_annotations_at"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRequestFinishedInfoAnnotationsClear, "Cronet_RequestFinishedInfo_annotations_clear"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRequestFinishedInfoFinishedReasonGet, "Cronet_RequestFinishedInfo_finished_reason_get"); err != nil {
		return err
	}

	// RequestFinishedInfoListener
	if err := registerFunc(&cronetRequestFinishedInfoListenerDestroy, "Cronet_RequestFinishedInfoListener_Destroy"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRequestFinishedInfoListenerSetClientContext, "Cronet_RequestFinishedInfoListener_SetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRequestFinishedInfoListenerGetClientContext, "Cronet_RequestFinishedInfoListener_GetClientContext"); err != nil {
		return err
	}
	if err := registerFunc(&cronetRequestFinishedInfoListenerCreateWith, "Cronet_RequestFinishedInfoListener_CreateWith"); err != nil {
		return err
	}

	// Custom cert verifier
	if err := registerFunc(&cronetCreateCertVerifierWithRootCerts, "Cronet_CreateCertVerifierWithRootCerts"); err != nil {
		return err
	}

	// BidirectionalStream
	if err := registerFunc(&bidirectionalStreamCreate, "bidirectional_stream_create"); err != nil {
		return err
	}
	if err := registerFunc(&bidirectionalStreamDestroy, "bidirectional_stream_destroy"); err != nil {
		return err
	}
	if err := registerFunc(&bidirectionalStreamDisableAutoFlush, "bidirectional_stream_disable_auto_flush"); err != nil {
		return err
	}
	if err := registerFunc(&bidirectionalStreamDelayRequestHeadersUntilFlush, "bidirectional_stream_delay_request_headers_until_flush"); err != nil {
		return err
	}
	if err := registerFunc(&bidirectionalStreamStart, "bidirectional_stream_start"); err != nil {
		return err
	}
	if err := registerFunc(&bidirectionalStreamRead, "bidirectional_stream_read"); err != nil {
		return err
	}
	if err := registerFunc(&bidirectionalStreamWrite, "bidirectional_stream_write"); err != nil {
		return err
	}
	if err := registerFunc(&bidirectionalStreamFlush, "bidirectional_stream_flush"); err != nil {
		return err
	}
	if err := registerFunc(&bidirectionalStreamCancel, "bidirectional_stream_cancel"); err != nil {
		return err
	}

	// Register float functions (platform-specific, see loader_windows_float*.go)
	if err := registerFloatFuncs(); err != nil {
		return err
	}

	return nil
}
