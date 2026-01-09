package list

import "github.com/sagernet/sing/common"

func (l List[T]) Size() int {
	return l.len
}

func (l List[T]) IsEmpty() bool {
	return l.len == 0
}

func (l *List[T]) PopBack() T {
	if l.len == 0 {
		return common.DefaultValue[T]()
	}
	entry := l.root.prev
	l.remove(entry)
	return entry.Value
}

func (l *List[T]) PopFront() T {
	if l.len == 0 {
		return common.DefaultValue[T]()
	}
	entry := l.root.next
	l.remove(entry)
	return entry.Value
}

func (l *List[T]) Array() []T {
	if l.len == 0 {
		return nil
	}
	array := make([]T, 0, l.len)
	for element := l.Front(); element != nil; element = element.Next() {
		array = append(array, element.Value)
	}
	return array
}

func (e *Element[T]) List() *List[T] {
	return e.list
}
