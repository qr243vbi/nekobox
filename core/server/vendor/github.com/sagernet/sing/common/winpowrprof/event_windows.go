package winpowrprof

// modify from https://github.com/golang/go/blob/b634f6fdcbebee23b7da709a243f3db217b64776/src/runtime/os_windows.go#L257

import (
	"unsafe"

	"golang.org/x/sys/windows"
)

type powerEventListener struct {
	pinner   myPinner
	callback EventCallback
	handle   uintptr
}

func NewEventListener(callback EventCallback) (EventListener, error) {
	err := procPowerRegisterSuspendResumeNotification.Find()
	if err != nil {
		return nil, err // Running on Windows 7, where we don't need it anyway.
	}
	err = procPowerUnregisterSuspendResumeNotification.Find()
	if err != nil {
		return nil, err // Running on Windows 7, where we don't need it anyway.
	}
	return &powerEventListener{
		callback: callback,
	}, nil
}

func (l *powerEventListener) Start() error {
	l.pinner.Pin(&l.callback)
	params := DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS{
		callback: windows.NewCallback(suspendResumeNotificationCallback),
		context:  unsafe.Pointer(&l.callback),
	}
	err := PowerRegisterSuspendResumeNotification(
		DEVICE_NOTIFY_CALLBACK,
		&params,
		&l.handle,
	)
	if err != nil {
		l.pinner.Unpin()
		return err
	}
	return nil
}

func (l *powerEventListener) Close() error {
	if l.handle == 0 {
		return nil
	}
	defer l.pinner.Unpin()
	err := PowerUnregisterSuspendResumeNotification(l.handle)
	if err != nil {
		return err
	}
	l.handle = 0
	return nil
}

func suspendResumeNotificationCallback(context *EventCallback, changeType uint32, setting uintptr) uintptr {
	callback := *context
	var event int
	switch changeType {
	case PBT_APMSUSPEND:
		event = EVENT_SUSPEND
	case PBT_APMRESUMESUSPEND:
		event = EVENT_RESUME
	case PBT_APMRESUMEAUTOMATIC:
		event = EVENT_RESUME_AUTOMATIC
	default:
		return 0
	}
	callback(event)
	return 0
}
