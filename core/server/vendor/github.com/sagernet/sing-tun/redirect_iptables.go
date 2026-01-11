//go:build linux

package tun

import (
	"os/exec"
	"strings"

	E "github.com/sagernet/sing/common/exceptions"
	F "github.com/sagernet/sing/common/format"
)

func (r *autoRedirect) setupIPTables() error {
	if r.enableIPv4 {
		err := r.setupIPTablesForFamily(r.iptablesPath)
		if err != nil {
			return err
		}
	}
	if r.enableIPv6 {
		err := r.setupIPTablesForFamily(r.ip6tablesPath)
		if err != nil {
			return err
		}
	}
	return nil
}

func (r *autoRedirect) setupIPTablesForFamily(iptablesPath string) error {
	tableNameOutput := r.tableName + "-output"
	redirectPort := r.redirectPort()
	// OUTPUT
	err := r.runShell(iptablesPath, "-t nat -N", tableNameOutput)
	if err != nil {
		return err
	}
	err = r.runShell(iptablesPath, "-t nat -A", tableNameOutput,
		"-p tcp -o", r.tunOptions.Name,
		"-j REDIRECT --to-ports", redirectPort)
	if err != nil {
		return err
	}
	err = r.runShell(iptablesPath, "-t nat -I OUTPUT -j", tableNameOutput)
	if err != nil {
		return err
	}
	return nil
}

func (r *autoRedirect) cleanupIPTables() {
	if r.enableIPv4 {
		r.cleanupIPTablesForFamily(r.iptablesPath)
	}
	if r.enableIPv6 {
		r.cleanupIPTablesForFamily(r.ip6tablesPath)
	}
}

func (r *autoRedirect) cleanupIPTablesForFamily(iptablesPath string) {
	tableNameOutput := r.tableName + "-output"

	_ = r.runShell(iptablesPath, "-t nat -D OUTPUT -j", tableNameOutput)
	_ = r.runShell(iptablesPath, "-t nat -F", tableNameOutput)
	_ = r.runShell(iptablesPath, "-t nat -X", tableNameOutput)
}

func (r *autoRedirect) runShell(commands ...any) error {
	commandStr := strings.Join(F.MapToString(commands), " ")
	var command *exec.Cmd
	if r.androidSu {
		command = exec.Command(r.suPath, "-c", commandStr)
	} else {
		commandArray := strings.Split(commandStr, " ")
		command = exec.Command(commandArray[0], commandArray[1:]...)
	}
	combinedOutput, err := command.CombinedOutput()
	if err != nil {
		return E.Extend(err, F.ToString(commandStr, ": ", string(combinedOutput)))
	}
	return nil
}
