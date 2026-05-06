package shell

import (
	"os"
	"os/exec"
	"strings"

	E "github.com/sagernet/sing/common/exceptions"
)

type Shell struct {
	*exec.Cmd
}

func Exec(name string, args ...string) *Shell {
	command := exec.Command(name, args...)
	command.Env = os.Environ()
	return &Shell{command}
}

func (s *Shell) SetDir(path string) *Shell {
	s.Dir = path
	return s
}

func (s *Shell) Attach() *Shell {
	s.Stdin = os.Stdin
	s.Stdout = os.Stderr
	s.Stderr = os.Stderr
	return s
}

func (s *Shell) SetEnv(env []string) *Shell {
	s.Env = append(os.Environ(), env...)
	return s
}

func (s *Shell) Wait() error {
	return s.buildError(s.Cmd.Wait())
}

func (s *Shell) Run() error {
	return s.buildError(s.Cmd.Run())
}

func (s *Shell) Read() (string, error) {
	output, err := s.CombinedOutput()
	return string(output), s.buildError(err)
}

func (s *Shell) ReadOutput() (string, error) {
	output, err := s.Output()
	return strings.TrimSpace(string(output)), s.buildError(err)
}

func (s *Shell) buildError(err error) error {
	if err == nil {
		return nil
	}
	return E.Cause(err, "execute (", s.Path, ") ", strings.Join(s.Args, " "))
}
