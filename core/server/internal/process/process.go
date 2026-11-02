package process

import (
	"fmt"
	"github.com/sagernet/sing/common/atomic"
	"os/exec"
	"time"
)

type Process struct {
	path    string
	args    []string
	noOut   bool
	cmd     *exec.Cmd
	stopped atomic.Bool
	done    chan struct{}
}

func NewProcess(path string, args []string, noOut bool) *Process {
	return &Process{path: path, args: args, noOut: noOut}
}

func (p *Process) Start() error {
	p.cmd = exec.Command(p.path, p.args...)

	p.cmd.Stdout = &pipeLogger{prefix: "Extra Core", noOut: p.noOut}
	p.cmd.Stderr = &pipeLogger{prefix: "Extra Core", noOut: p.noOut}

	err := p.cmd.Start()
	if err != nil {
		return err
	}
	p.stopped.Store(false)
	p.done = make(chan struct{})

	go func() {
		fmt.Println(p.path, ":", "process started, waiting for it to end")
		_ = p.cmd.Wait()
		close(p.done)
		if !p.stopped.Load() {
			fmt.Println("Extra process exited unexpectedly")
		}
	}()
	return nil
}

// Done is closed once the process has exited.
func (p *Process) Done() <-chan struct{} {
	return p.done
}

func (p *Process) Stop() {
	p.stopped.Store(true)
	if p.cmd == nil || p.cmd.Process == nil {
		return
	}
	_ = p.cmd.Process.Kill()
	if p.done != nil {
		select {
		case <-p.done:
		case <-time.After(2 * time.Second):
		}
	}
}
