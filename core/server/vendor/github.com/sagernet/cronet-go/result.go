package cronet

// Result is runtime result code returned by Engine and URLRequest. Equivalent to
// runtime exceptions in Android Java API. All results except SUCCESS trigger
// native crash (via SIGABRT triggered by CHECK failure) unless
// EngineParams.EnableCheckResult is set to false.
type Result int

const (
	// ResultSuccess Operation completed successfully
	ResultSuccess Result = 0

	// ResultIllegalArgument Illegal argument
	ResultIllegalArgument Result = -100

	// ResultIllegalArgumentStoragePathMustExist Storage path must be set to existing directory
	ResultIllegalArgumentStoragePathMustExist Result = -101

	// ResultIllegalArgumentInvalidPin Public key pin is invalid
	ResultIllegalArgumentInvalidPin Result = -102

	// ResultIllegalArgumentInvalidHostname Host name is invalid
	ResultIllegalArgumentInvalidHostname Result = -103

	// ResultIllegalArgumentInvalidHttpMethod Invalid http method
	ResultIllegalArgumentInvalidHttpMethod Result = -104

	// ResultIllegalArgumentInvalidHttpHeader Invalid http header
	ResultIllegalArgumentInvalidHttpHeader Result = -105

	// ResultIllegalState Illegal state
	ResultIllegalState Result = -200

	// ResultIllegalStateStoragePathInUse Storage path is used by another engine
	ResultIllegalStateStoragePathInUse Result = -201

	// ResultIllegalStateCannotShutdownEngineFromNetworkThread Cannot shutdown engine from network thread
	ResultIllegalStateCannotShutdownEngineFromNetworkThread Result = -202

	// ResultIllegalStateEngineAlreadyStarted The engine has already started
	ResultIllegalStateEngineAlreadyStarted Result = -203

	// ResultIllegalStateRequestAlreadyStarted The request has already started
	ResultIllegalStateRequestAlreadyStarted Result = -204

	// ResultIllegalStateRequestNotInitialized The request is not initialized
	ResultIllegalStateRequestNotInitialized Result = -205

	// ResultIllegalStateRequestAlreadyInitialized The request is already initialized
	ResultIllegalStateRequestAlreadyInitialized Result = -206

	// ResultIllegalStateRequestNotStarted The request is not started
	ResultIllegalStateRequestNotStarted Result = -207

	// ResultIllegalStateUnexpectedRedirect No redirect to follow
	ResultIllegalStateUnexpectedRedirect Result = -208

	// ResultIllegalStateUnexpectedRead Unexpected read attempt
	ResultIllegalStateUnexpectedRead Result = -209

	// ResultIllegalStateReadFailed Unexpected read failure
	ResultIllegalStateReadFailed Result = -210

	// ResultNullPointer Null pointer or empty data
	ResultNullPointer Result = -300

	// ResultNullPointerHostname The hostname cannot be null
	ResultNullPointerHostname Result = -301

	// ResultNullPointerSha256Pins The set of SHA256 pins cannot be null
	ResultNullPointerSha256Pins Result = -302

	// ResultNullPointerExpirationDate The pin expiration date cannot be null
	ResultNullPointerExpirationDate Result = -303

	// ResultNullPointerEngine Engine is required
	ResultNullPointerEngine Result = -304

	// ResultNullPointerURL URL is required
	ResultNullPointerURL Result = -305

	// ResultNullPointerCallback Callback is required
	ResultNullPointerCallback Result = -306

	// ResultNullPointerExecutor Executor is required
	ResultNullPointerExecutor Result = -307

	// ResultNullPointerMethod Method is required
	ResultNullPointerMethod Result = -308

	// ResultNullPointerHeaderName Invalid header name
	ResultNullPointerHeaderName Result = -309

	// ResultNullPointerHeaderValue Invalid header value
	ResultNullPointerHeaderValue Result = -310

	// ResultNullPointerParams Params is required
	ResultNullPointerParams Result = -311

	// ResultNullPointerRequestFinishedInfoListenerExecutor Executor for RequestFinishedInfoListener is required
	ResultNullPointerRequestFinishedInfoListenerExecutor Result = -312
)
