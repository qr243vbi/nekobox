// +build windows

package taskmaster

import (
	"errors"
	"fmt"

	"github.com/go-ole/go-ole"
	"github.com/go-ole/go-ole/oleutil"
)

func (d *Definition) AddAction(action Action) {
	d.Actions = append(d.Actions, action)
}

func (d *Definition) AddTrigger(trigger Trigger) {
	d.Triggers = append(d.Triggers, trigger)
}

// Refresh refreshes all of the local instance variables of the running task.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nf-taskschd-irunningtask-refresh
func (r RunningTask) Refresh() error {
	_, err := oleutil.CallMethod(r.taskObj, "Refresh")
	if err != nil {
		return fmt.Errorf("error refreshing running task %s: %v", r.Path, getTaskSchedulerError(err))
	}

	return nil
}

// Stop kills and releases a running task.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nf-taskschd-irunningtask-stop
func (r *RunningTask) Stop() error {
	_, err := oleutil.CallMethod(r.taskObj, "Stop")
	if err != nil {
		return fmt.Errorf("error stopping running task %s: %v", r.Path, getTaskSchedulerError(err))
	}

	r.Release()

	return nil
}

// Release frees the running task COM object. Must be called before
// program termination to avoid memory leaks.
func (r *RunningTask) Release() {
	if !r.isReleased && r.taskObj != nil {
		r.taskObj.Release()
		r.isReleased = true
	}
}

// Run starts an instance of a registered task. If the task was started successfully,
// a pointer to a running task will be returned.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nf-taskschd-iregisteredtask-run
func (r *RegisteredTask) Run(args ...string) (RunningTask, error) {
	return r.RunEx(args, TASK_RUN_NO_FLAGS, 0, "")
}

// RunEx starts an instance of a registered task. If the task was started successfully,
// a pointer to a running task will be returned.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nf-taskschd-iregisteredtask-runex
func (r *RegisteredTask) RunEx(args []string, flags TaskRunFlags, sessionID int, user string) (RunningTask, error) {
	if !r.Enabled {
		return RunningTask{}, fmt.Errorf("error running registered task %s: cannot run a disabled task", r.Path)
	}

	runningTaskObj, err := oleutil.CallMethod(r.taskObj, "RunEx", args, int(flags), sessionID, user)
	if err != nil {
		return RunningTask{}, fmt.Errorf("error running registered task %s: %v", r.Path, getTaskSchedulerError(err))
	}

	return parseRunningTask(runningTaskObj.ToIDispatch())
}

// GetInstances returns all of the currently running instances of a registered task.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nf-taskschd-iregisteredtask-getinstances
func (r *RegisteredTask) GetInstances() (RunningTaskCollection, error) {
	runningTasks, err := oleutil.CallMethod(r.taskObj, "GetInstances", 0)
	if err != nil {
		return nil, fmt.Errorf("error getting instances of registered task %s: %v", r.Path, getTaskSchedulerError(err))
	}

	runningTasksObj := runningTasks.ToIDispatch()
	defer runningTasksObj.Release()
	var parsedRunningTasks RunningTaskCollection

	err = oleutil.ForEach(runningTasksObj, func(v *ole.VARIANT) error {
		runningTaskObj := v.ToIDispatch()

		parsedRunningTask, err := parseRunningTask(runningTaskObj)
		if err != nil {
			if errors.Is(err, ErrRunningTaskCompleted) {
				return nil
			}
			return fmt.Errorf("error parsing running task: %v", err)
		}

		parsedRunningTasks = append(parsedRunningTasks, parsedRunningTask)

		return nil
	})
	if err != nil {
		return nil, err
	}

	return parsedRunningTasks, nil
}

// Stop kills all running instances of the registered task that the current
// user has access to. If all instances were killed, Stop returns true,
// otherwise Stop returns false.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nf-taskschd-iregisteredtask-stop
func (r *RegisteredTask) Stop() error {
	_, err := oleutil.CallMethod(r.taskObj, "Stop", 0)
	if err != nil {
		return fmt.Errorf("error stopping registered task %s: %v", r.Path, getTaskSchedulerError(err))
	}

	return nil
}

// Release frees the registered task COM object. Must be called before
// program termination to avoid memory leaks.
func (r *RegisteredTask) Release() {
	if !r.isReleased && r.taskObj != nil {
		r.taskObj.Release()
		r.isReleased = true
	}
}

// RunningTaskCollection is a collection of running tasks.
type RunningTaskCollection []RunningTask

// Stop kills and frees all the running tasks COM objects in the
// collection. If an error is encountered while stopping a running
// task, Stop returns the error without attempting to stop any
// other running tasks in the collection.
func (r RunningTaskCollection) Stop() error {
	for _, runningTask := range r {
		if err := runningTask.Stop(); err != nil {
			return err
		}
	}

	return nil
}

// Release frees all the running task COM objects in the collection.
// Must be called before program termination to avoid memory leaks.
func (r RunningTaskCollection) Release() {
	for _, runningTask := range r {
		runningTask.Release()
	}
}

// RegisteredTaskCollection is a collection of registered tasks.
type RegisteredTaskCollection []RegisteredTask

// Release frees all the registered task COM objects in the collection.
// Must be called before program termination to avoid memory leaks.
func (r RegisteredTaskCollection) Release() {
	for _, registeredTask := range r {
		registeredTask.Release()
	}
}

// Release frees all the registered task COM objects in the folder and
// all subfolders. Must be called before program termination to avoid
// memory leaks.
func (f *TaskFolder) Release() {
	if !f.isReleased {
		f.RegisteredTasks.Release()
		for _, subFolder := range f.SubFolders {
			subFolder.RegisteredTasks.Release()
		}

		f.isReleased = true
	}
}
