package common

import "strings"

func SubstringAfter(s string, substr string) string {
	index := strings.Index(s, substr)
	if index == -1 {
		return s
	}
	return s[index+len(substr):]
}

func SubstringAfterLast(s string, substr string) string {
	index := strings.LastIndex(s, substr)
	if index == -1 {
		return s
	}
	return s[index+len(substr):]
}

func SubstringBefore(s string, substr string) string {
	index := strings.Index(s, substr)
	if index == -1 {
		return s
	}
	return s[:index]
}

func SubstringBeforeLast(s string, substr string) string {
	index := strings.LastIndex(s, substr)
	if index == -1 {
		return s
	}
	return s[:index]
}

func SubstringBetween(s string, after string, before string) string {
	return SubstringBefore(SubstringAfter(s, after), before)
}
