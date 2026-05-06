package observable

import (
	"os"
	"sync"
)

type Observable[T any] interface {
	Subscribe() (subscription Subscription[T], done <-chan struct{}, err error)
	UnSubscribe(subscription Subscription[T])
}

type Observer[T any] struct {
	subscriber   *Subscriber[T]
	listenerSize int
	listener     map[Subscription[T]]*Subscriber[T]
	mux          sync.Mutex
	done         bool
}

func NewObserver[T any](subscriber *Subscriber[T], listenerBufferSize int) *Observer[T] {
	observable := &Observer[T]{
		subscriber:   subscriber,
		listener:     map[Subscription[T]]*Subscriber[T]{},
		listenerSize: listenerBufferSize,
	}
	go observable.process()
	return observable
}

func (o *Observer[T]) process() {
	subscription, done := o.subscriber.Subscription()
process:
	for {
		select {
		case <-done:
			break process
		case entry := <-subscription:
			o.mux.Lock()
			for _, sub := range o.listener {
				sub.Emit(entry)
			}
			o.mux.Unlock()
		}
	}
	o.mux.Lock()
	defer o.mux.Unlock()
	for _, listener := range o.listener {
		listener.Close()
	}
}

func (o *Observer[T]) Subscribe() (subscription Subscription[T], done <-chan struct{}, err error) {
	o.mux.Lock()
	defer o.mux.Unlock()
	if o.done {
		return nil, nil, os.ErrClosed
	}
	subscriber := NewSubscriber[T](o.listenerSize)
	subscription, done = subscriber.Subscription()
	o.listener[subscription] = subscriber
	return
}

func (o *Observer[T]) UnSubscribe(subscription Subscription[T]) {
	o.mux.Lock()
	defer o.mux.Unlock()
	subscriber, exist := o.listener[subscription]
	if !exist {
		return
	}
	delete(o.listener, subscription)
	subscriber.Close()
}

func (o *Observer[T]) Emit(item T) {
	o.subscriber.Emit(item)
}

func (o *Observer[T]) Close() error {
	o.mux.Lock()
	defer o.mux.Unlock()
	if o.done {
		return os.ErrClosed
	}
	o.subscriber.Close()
	o.done = true
	return nil
}
