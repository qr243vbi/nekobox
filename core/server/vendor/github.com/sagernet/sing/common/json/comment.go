package json

import (
	"bufio"
	"io"
)

// kanged from v2ray

type commentFilterState = byte

const (
	commentFilterStateContent commentFilterState = iota
	commentFilterStateEscape
	commentFilterStateDoubleQuote
	commentFilterStateDoubleQuoteEscape
	commentFilterStateSingleQuote
	commentFilterStateSingleQuoteEscape
	commentFilterStateComment
	commentFilterStateSlash
	commentFilterStateMultilineComment
	commentFilterStateMultilineCommentStar
)

type CommentFilter struct {
	br       *bufio.Reader
	state    commentFilterState
	pending  [2]byte
	pendingN int
}

func NewCommentFilter(reader io.Reader) io.Reader {
	return &CommentFilter{br: bufio.NewReader(reader)}
}

func (v *CommentFilter) Read(b []byte) (int, error) {
	if len(b) == 0 {
		return 0, nil
	}

	n := 0
	if v.pendingN > 0 {
		copied := copy(b, v.pending[:v.pendingN])
		n += copied
		if copied < v.pendingN {
			copy(v.pending[:], v.pending[copied:v.pendingN])
			v.pendingN -= copied
			return n, nil
		}
		v.pendingN = 0
	}

	emit := func(bs ...byte) bool {
		remaining := len(b) - n
		if remaining <= 0 {
			copy(v.pending[:], bs)
			v.pendingN = len(bs)
			return false
		}
		if len(bs) <= remaining {
			copy(b[n:], bs)
			n += len(bs)
			return true
		}
		copy(b[n:], bs[:remaining])
		n += remaining
		bs = bs[remaining:]
		copy(v.pending[:], bs)
		v.pendingN = len(bs)
		return false
	}

	emitWithState := func(next commentFilterState, bs ...byte) bool {
		v.state = next
		return emit(bs...)
	}

	for n < len(b) {
		x, err := v.br.ReadByte()
		if err != nil {
			// Handle pending slash at EOF
			if v.state == commentFilterStateSlash {
				v.state = commentFilterStateContent
				if !emit('/') {
					return n, nil
				}
			}
			if n == 0 {
				return 0, err
			}
			return n, nil
		}
		switch v.state {
		case commentFilterStateContent:
			switch x {
			case '"':
				if !emitWithState(commentFilterStateDoubleQuote, x) {
					return n, nil
				}
			case '\'':
				if !emitWithState(commentFilterStateSingleQuote, x) {
					return n, nil
				}
			case '\\':
				v.state = commentFilterStateEscape
			case '#':
				v.state = commentFilterStateComment
			case '/':
				v.state = commentFilterStateSlash
			default:
				if !emit(x) {
					return n, nil
				}
			}
		case commentFilterStateEscape:
			if !emitWithState(commentFilterStateContent, '\\', x) {
				return n, nil
			}
		case commentFilterStateDoubleQuote:
			switch x {
			case '"':
				if !emitWithState(commentFilterStateContent, x) {
					return n, nil
				}
			case '\\':
				v.state = commentFilterStateDoubleQuoteEscape
			default:
				if !emit(x) {
					return n, nil
				}
			}
		case commentFilterStateDoubleQuoteEscape:
			if !emitWithState(commentFilterStateDoubleQuote, '\\', x) {
				return n, nil
			}
		case commentFilterStateSingleQuote:
			switch x {
			case '\'':
				if !emitWithState(commentFilterStateContent, x) {
					return n, nil
				}
			case '\\':
				v.state = commentFilterStateSingleQuoteEscape
			default:
				if !emit(x) {
					return n, nil
				}
			}
		case commentFilterStateSingleQuoteEscape:
			if !emitWithState(commentFilterStateSingleQuote, '\\', x) {
				return n, nil
			}
		case commentFilterStateComment:
			if x == '\n' {
				if !emitWithState(commentFilterStateContent, '\n') {
					return n, nil
				}
			}
		case commentFilterStateSlash:
			switch x {
			case '/':
				v.state = commentFilterStateComment
			case '*':
				v.state = commentFilterStateMultilineComment
			default:
				if !emitWithState(commentFilterStateContent, '/', x) {
					return n, nil
				}
			}
		case commentFilterStateMultilineComment:
			switch x {
			case '*':
				v.state = commentFilterStateMultilineCommentStar
			case '\n':
				if !emit('\n') {
					return n, nil
				}
			}
		case commentFilterStateMultilineCommentStar:
			switch x {
			case '/':
				v.state = commentFilterStateContent
			case '*':
				// Stay in star state
			case '\n':
				if !emitWithState(commentFilterStateMultilineComment, '\n') {
					return n, nil
				}
			default:
				v.state = commentFilterStateMultilineComment
			}
		default:
			panic("Unknown state.")
		}
	}
	return n, nil
}
