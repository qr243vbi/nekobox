package winpowrprof

const (
	EVENT_SUSPEND = iota
	EVENT_RESUME
	EVENT_RESUME_AUTOMATIC // Because the user is not present, most applications should do nothing.
)

type EventCallback = func(event int)

type EventListener interface {
	Start() error
	Close() error
}
