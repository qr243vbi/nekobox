package main

import (
	"context"
	"fmt"
	"log"
	"nekobox_core/gen"
	"nekobox_core/internal"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strconv"
	"syscall"
	"time"

	"kernel.org/pub/linux/libs/security/libcap/cap"
)

func (s *server) SetSystemDNS(ctx context.Context, in *gen.SetSystemDNSRequest) (*gen.EmptyResp, error) {
	out := new(gen.EmptyResp)
	return out, nil
}

func runAdmin() (int, error) {
	return 0, nil
}

func checkTaskScheduler(save bool) error {
	return nil
}

func doRun(_ string) error {
	return nil
}

func (s *server) IsPrivileged(ctx context.Context, in *gen.EmptyReq) (*gen.IsPrivilegedResponse, error) {
	out := new(gen.IsPrivilegedResponse)
	priv, err := isElevated()
	out.HasPrivilege = priv
	return out, err
}

func WaitForProcessExit(pid int) error {
	// Wait for the process to terminate
	for {
		// Send signal 0 to check if the process exists
		err := syscall.Kill(pid, syscall.Signal(0))
		if err != nil {
			if err == syscall.ESRCH {
				return nil
			}
			return fmt.Errorf("Error checking process status: %v", err)
		}
		time.Sleep(442 * time.Millisecond)
	}
}

func wsainit() {

}

func isElevated() (bool, error) {
	if os.Geteuid() == 0 {
		return true, nil
	}
	c := cap.GetProc()
	cp, err := c.GetFlag(cap.Effective, cap.NET_ADMIN)
	return cp, err
}

// Function to create a group if it does not exist
func createGroup(groupName string) error {
	// Create the group using the `groupadd` command
	cmd := exec.Command("groupadd", groupName)
	err := cmd.Run()
	if err != nil {
		return fmt.Errorf("failed to create group '%s': %v", groupName, err)
	}
	fmt.Printf("Group '%s' created successfully.\n", groupName)
	return nil
}

// Function to get the GID for the specified group
func getGroupInfo(groupName string) (int, error) {
	group, err := user.LookupGroup(groupName)
	if err != nil {
		return -1, err
	}
	gid, err := strconv.Atoi(group.Gid)
	if err != nil {
		return -1, err
	}
	return gid, nil
}

// Function to check if the group exists
func groupExists(groupName string) bool {
	_, err := getGroupInfo(groupName)
	return err == nil
}

// Function to set the group ownership of the program file
func setGroupOwnership(programPath string, groupName string) error {
	// Get the GID for the specified group
	gid, err := getGroupInfo(groupName)
	if err != nil {
		return fmt.Errorf("failed to get GID for group %s: %v", groupName, err)
	}

	// Change the group ownership of the program file
	err = syscall.Chown(programPath, -1, gid)
	if err != nil {
		return fmt.Errorf("failed to change group ownership for %s: %v", programPath, err)
	}

	fmt.Printf("Group ownership of '%s' set to '%s' (GID: %d).\n", programPath, groupName, gid)
	return nil
}

// Function to set the setgid bit on the program file
func setSetGidBit(programPath string) error {
	// Set the setgid bit to ensure the program runs with the group ownership
	err := syscall.Chmod(programPath, 0755|syscall.S_ISGID)
	if err != nil {
		return fmt.Errorf("failed to set setgid bit on %s: %v", programPath, err)
	}
	fmt.Printf("Setgid bit set for '%s'.\n", programPath)
	return nil
}

// Main function that combines all the steps
func ensureGroupAndSetOwnership(programPath string, groupName string) error {
	// Step 1: Check if the group exists
	if !groupExists(groupName) {
		// Step 2: Create the group if it doesn't exist
		err := createGroup(groupName)
		if err != nil {
			return fmt.Errorf("failed to ensure group exists: %v", err)
		}
	}

	// Step 3: Set the group ownership of the program file
	err := setGroupOwnership(programPath, groupName)
	if err != nil {
		return fmt.Errorf("failed to set group ownership: %v", err)
	}

	// Step 4: Set the setgid bit on the program file
	err = setSetGidBit(programPath)
	if err != nil {
		return fmt.Errorf("failed to set setgid bit: %v", err)
	}

	fmt.Println("Successfully ensured group and set ownership and special bits.")
	return nil
}

const polkitRuleDir = "/etc/polkit-1/rules.d/"
const polkitRuleFile = "99_nekobox_core.rules"
const polkitRuleContent = `
polkit.addRule(function(action, subject) {
    if ((action.id == "org.freedesktop.resolve1.set-domains" ||
         action.id == "org.freedesktop.resolve1.set-default-route" ||
         action.id == "org.freedesktop.resolve1.revert" ||
         action.id == "org.freedesktop.resolve1.set-dns-servers") &&
        subject.isInGroup("sing-box")) {
        return polkit.Result.YES;
    }
});
`

func restartAsAdmin(save bool) {
	elevated, err := isElevated()
	if elevated {
		caps := cap.NewSet()
		cap_value := []cap.Value{
			cap.NET_ADMIN,
			cap.NET_RAW,
			cap.NET_BIND_SERVICE,
			cap.SYS_PTRACE,
			cap.DAC_READ_SEARCH,
		}
		err := caps.SetFlag(cap.Inheritable, true, cap_value...)
		if err != nil {
			panic(err)
		}
		err = caps.SetFlag(cap.Effective, true, cap_value...)
		if err != nil {
			panic(err)
		}
		err = caps.SetFlag(cap.Permitted, true, cap_value...)
		if err != nil {
			panic(err)
		}
		{
			// Combine directory and file name to get the full path
			filePath := polkitRuleDir + polkitRuleFile

			// Check if the directory exists, create if not
			if _, err := os.Stat(polkitRuleDir); os.IsNotExist(err) {
				log.Fatalf("Directory does not exist: %v", polkitRuleDir)
			}

			// Create the rule file
			file, err := os.Create(filePath)
			if err != nil {
				log.Fatalf("Error creating rule file: %v", err)
			}
			defer file.Close()

			// Write the rule content to the file
			_, err = file.WriteString(polkitRuleContent)
			if err != nil {
				log.Fatalf("Error writing to rule file: %v", err)
			}

			// Inform the user
			fmt.Printf("Polkit rule written to: %s\n", filePath)
		}

		if save {
			file, err := filepath.Abs(os.Args[0])
			if err != nil {
				panic(err)
			}
			err = ensureGroupAndSetOwnership(file, "sing-box")
			if err != nil {
				panic(err)
			}
			err = caps.SetFile(file)
			if err != nil {
				panic(err)
			}
		}
		/*
				if gid > 0 {
					syscall.Setgid(gid)
				}

				if uid > 0 {
					syscall.Seteuid(uid)
				}

			// 1. Get the capability set for the current process
			// This captures Permitted, Effective, and Inheritable sets.
			c := cap.GetProc()

			// 2. Define the capability we want to enable
			netAdmin := cap.NET_ADMIN

			// 3. Set the NET_ADMIN bit in the Effective set
			// Since we are root, it is already in our Permitted set.
			if err := c.SetFlag(cap.Effective, true, netAdmin); err != nil {
				log.Fatalf("failed to set flag: %v", err)
			}

			if err := c.SetFlag(cap.Permitted, true, netAdmin); err != nil {
				log.Fatalf("failed to set flag: %v", err)
			}

			// 4. Apply these capabilities to EVERY thread in the Go process
			// The libcap wrapper uses the nptl:setxid mechanism to sync threads.
			if err := c.SetProc(); err != nil {
				log.Fatalf("failed to apply capabilities: %v (Are you root?)", err)
			}
		*/
		return
	}
	var args []string
	pkexecPath, err := exec.LookPath("pkexec")
	if err != nil {
		// exec.ErrNotFound is returned if the executable cannot be found.
		if err == exec.ErrNotFound {
			log.Fatalf("pkexec executable not found in PATH: %v", err)
		} else {
			log.Fatalf("Error finding pkexec executable: %v", err)
		}
	}

	executablePath, err := filepath.Abs(os.Args[0])
	args = append(args, pkexecPath, "sh", "-c", "exec sudo \"${0}\" \"${@}\"", "env", "NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE=nekobox_core", executablePath, "-ruleset-cache-directory", internal.GetRulesetCachedir())

	args = append(args, os.Args[1:]...)

	err = syscall.Exec(pkexecPath, args, os.Environ())
	if err != nil {
		// This part of the code will only be reached if syscall.Exec fails
		fmt.Println("Error executing 'pkexec':", err)
		os.Exit(1)
	}
}

func InstallerMode() {

}
