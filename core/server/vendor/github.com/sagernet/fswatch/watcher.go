package fswatch

import (
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"

	"github.com/fsnotify/fsnotify"
)

const DefaultWaitTimeout = 100 * time.Millisecond

// Watcher is a simple fsnotify wrapper to watch updates correctly.
type Watcher struct {
	watchDirect bool
	watchTarget []string
	watchPath   []string
	callback    func(path string)
	waitTimeout time.Duration
	logger      logger.Logger
	watcher     *fsnotify.Watcher
}

type Options struct {
	// Path is the list of files or directories to watch
	// It is the caller's responsibility to ensure that paths are absolute.
	Path []string

	// Direct is the flag to watch the file directly if file will never be removed
	Direct bool

	// Callback is the function to call when a file is updated
	Callback func(path string)

	// WaitTimeout is the time to wait write events before calling the callback
	// DefaultWaitTimeout is used by default
	WaitTimeout time.Duration

	// Logger is the logger to log errors
	// optional
	Logger logger.Logger
}

func NewWatcher(options Options) (*Watcher, error) {
	if len(options.Path) == 0 || options.Callback == nil {
		return nil, os.ErrInvalid
	}
	waitTimeout := options.WaitTimeout
	if waitTimeout == 0 {
		waitTimeout = DefaultWaitTimeout
	}
	var watchTarget []string
	if options.Direct {
		watchTarget = options.Path
	} else {
		watchTarget = common.Uniq(common.Map(options.Path, filepath.Dir))
		// TODO: update sing to use common.Remove when it's stable
		watchTarget = common.Filter(watchTarget, func(it string) bool {
			return !common.Any(watchTarget, func(path string) bool {
				return len(path) > len(it) && strings.HasPrefix(path, it)
			})
		})
	}
	return &Watcher{
		watchDirect: options.Direct,
		watchTarget: watchTarget,
		watchPath:   options.Path,
		callback:    options.Callback,
		waitTimeout: waitTimeout,
		logger:      options.Logger,
	}, nil
}

func (w *Watcher) Start() error {
	watcher, err := fsnotify.NewWatcher()
	if err != nil {
		return E.Cause(err, "fswatch: create fsnotify watcher")
	}
	for _, target := range w.watchTarget {
		err = watcher.Add(target)
		if err != nil {
			return E.Cause(err, "fswatch: watch ", target)
		}
	}
	w.watcher = watcher
	go w.loopUpdate()
	return nil
}

func (w *Watcher) Close() error {
	return common.Close(common.PtrOrNil(w.watcher))
}

func (w *Watcher) loopUpdate() {
	var timerAccess sync.Mutex
	timerMap := make(map[string]*time.Timer)
	for {
		select {
		case event, loaded := <-w.watcher.Events:
			if !loaded {
				return
			}
			if common.Contains(w.watchTarget, event.Name) && (event.Has(fsnotify.Rename) || event.Has(fsnotify.Remove)) {
				if w.logger != nil {
					w.logger.Error("fswatch: watcher removed: ", event.Name)
				}
			} else if common.Contains(w.watchPath, event.Name) && (event.Has(fsnotify.Create) || event.Has(fsnotify.Write)) {
				timerAccess.Lock()
				timer := timerMap[event.Name]
				if timer != nil {
					timer.Reset(w.waitTimeout)
				} else {
					timerMap[event.Name] = time.AfterFunc(w.waitTimeout, func() {
						w.callback(event.Name)
						timerAccess.Lock()
						delete(timerMap, event.Name)
						timerAccess.Unlock()
					})
				}
				timerAccess.Unlock()
			}
		case err, loaded := <-w.watcher.Errors:
			if !loaded {
				return
			}
			if w.logger != nil {
				w.logger.Error("fswatch: ", err)
			}
		}
	}
}
