package cronet

// ErrorCode represents the error code returned by cronet.
type ErrorCode int

const (
	// ErrorCodeErrorCallback indicating the error returned by app callback.
	ErrorCodeErrorCallback ErrorCode = 0

	// ErrorCodeErrorHostnameNotResolved indicating the host being sent the request could not be resolved to an IP address.
	ErrorCodeErrorHostnameNotResolved ErrorCode = 1

	// ErrorCodeErrorInternetDisconnected indicating the device was not connected to any network.
	ErrorCodeErrorInternetDisconnected ErrorCode = 2

	// ErrorCodeErrorNetworkChanged indicating that as the request was processed the network configuration changed.
	ErrorCodeErrorNetworkChanged ErrorCode = 3

	// ErrorCodeErrorTimedOut indicating a timeout expired. Timeouts expiring while attempting to connect will
	// be reported as the more specific ErrorCodeErrorConnectionTimedOut.
	ErrorCodeErrorTimedOut ErrorCode = 4

	// ErrorCodeErrorConnectionClosed indicating the connection was closed unexpectedly.
	ErrorCodeErrorConnectionClosed ErrorCode = 5

	// ErrorCodeErrorConnectionTimedOut indicating the connection attempt timed out.
	ErrorCodeErrorConnectionTimedOut ErrorCode = 6

	// ErrorCodeErrorConnectionRefused indicating the connection attempt was refused.
	ErrorCodeErrorConnectionRefused ErrorCode = 7

	// ErrorCodeErrorConnectionReset indicating the connection was unexpectedly reset.
	ErrorCodeErrorConnectionReset ErrorCode = 8

	// ErrorCodeErrorAddressUnreachable indicating the IP address being contacted is unreachable,
	// meaning there is no route to the specified host or network.
	ErrorCodeErrorAddressUnreachable ErrorCode = 9

	// ErrorCodeErrorQuicProtocolFailed indicating an error related to the QUIC protocol.
	// When Error.ErrorCode() is this code, see Error.QuicDetailedErrorCode() for more information.
	ErrorCodeErrorQuicProtocolFailed ErrorCode = 10

	// ErrorCodeErrorOther indicating another type of error was encountered.
	// Error.InternalErrorCode() can be consulted to get a more specific cause.
	ErrorCodeErrorOther ErrorCode = 11
)
