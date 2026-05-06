package cronet

// ExecutorExecuteFunc takes ownership of |command| and runs it synchronously or asynchronously.
// Destroys the |command| after execution, or if executor is shutting down.
type ExecutorExecuteFunc func(executor Executor, command Runnable)

// RunnableRunFunc is the function type for Runnable.Run callback.
type RunnableRunFunc func(self Runnable)

// BufferCallbackFunc is called when the Buffer is destroyed.
type BufferCallbackFunc func(callback BufferCallback, buffer Buffer)

// URLRequestStatusListenerOnStatusFunc is called with the status of a URL request.
type URLRequestStatusListenerOnStatusFunc func(self URLRequestStatusListener, status URLRequestStatusListenerStatus)

// URLRequestFinishedInfoListenerOnRequestFinishedFunc is called when a request finishes.
type URLRequestFinishedInfoListenerOnRequestFinishedFunc func(listener URLRequestFinishedInfoListener, requestInfo RequestFinishedInfo, responseInfo URLResponseInfo, error Error)

// URLRequestCallbackHandler handles callbacks from URLRequest.
type URLRequestCallbackHandler interface {
	// OnRedirectReceived is invoked whenever a redirect is encountered.
	OnRedirectReceived(self URLRequestCallback, request URLRequest, info URLResponseInfo, newLocationUrl string)

	// OnResponseStarted is invoked when the final set of headers, after all redirects, is received.
	OnResponseStarted(self URLRequestCallback, request URLRequest, info URLResponseInfo)

	// OnReadCompleted is invoked whenever part of the response body has been read.
	OnReadCompleted(self URLRequestCallback, request URLRequest, info URLResponseInfo, buffer Buffer, bytesRead int64)

	// OnSucceeded is invoked when request is completed successfully.
	OnSucceeded(self URLRequestCallback, request URLRequest, info URLResponseInfo)

	// OnFailed is invoked if request failed for any reason after URLRequest.Start().
	OnFailed(self URLRequestCallback, request URLRequest, info URLResponseInfo, error Error)

	// OnCanceled is invoked if request was canceled via URLRequest.Cancel().
	OnCanceled(self URLRequestCallback, request URLRequest, info URLResponseInfo)
}
