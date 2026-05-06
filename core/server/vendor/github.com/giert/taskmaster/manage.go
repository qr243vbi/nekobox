//go:build windows
// +build windows

package taskmaster

import (
	"errors"
	"fmt"
	"os"
	"os/user"
	"strings"
	"time"

	ole "github.com/go-ole/go-ole"
	"github.com/go-ole/go-ole/oleutil"
	"github.com/rickb777/date/period"
)

// S_FALSE is returned by CoInitialize if it was already called on this thread.
const S_FALSE = 0x00000001

func (t *TaskService) initialize() error {
	var err error

	err = ole.CoInitializeEx(0, ole.COINIT_MULTITHREADED)
	if err != nil {
		code := err.(*ole.OleError).Code()
		if code != ole.S_OK && code != S_FALSE {
			return err
		}
	}

	schedClassID, err := ole.ClassIDFrom("Schedule.Service.1")
	if err != nil {
		ole.CoUninitialize()
		return getTaskSchedulerError(err)
	}
	taskSchedulerObj, err := ole.CreateInstance(schedClassID, nil)
	if err != nil {
		ole.CoUninitialize()
		return getTaskSchedulerError(err)
	}
	if taskSchedulerObj == nil {
		ole.CoUninitialize()
		return errors.New("Could not create ITaskService object")
	}
	defer taskSchedulerObj.Release()

	tskSchdlr := taskSchedulerObj.MustQueryInterface(ole.IID_IDispatch)
	t.taskServiceObj = tskSchdlr
	t.isInitialized = true

	return nil
}

// Connect connects to the local Task Scheduler service, using the current
// token for authentication. This function must run before any other functions
// in taskmaster can be used.
func Connect() (TaskService, error) {
	return ConnectWithOptions("", "", "", "")
}

// ConnectWithOptions connects to a local or remote Task Scheduler service. This
// function must run before any other functions in taskmaster can be used. If the
// serverName parameter is empty, a connection to the local Task Scheduler service
// will be attempted. If the user and password parameters are empty, the current
// token will be used for authentication.
func ConnectWithOptions(serverName, domain, username, password string) (TaskService, error) {
	var err error
	var taskService TaskService

	if !taskService.isInitialized {
		err = taskService.initialize()
		if err != nil {
			return TaskService{}, fmt.Errorf("error initializing ITaskService object: %w", err)
		}
	}

	_, err = oleutil.CallMethod(taskService.taskServiceObj, "Connect", serverName, username, domain, password)
	if err != nil {
		return TaskService{}, fmt.Errorf("error connecting to Task Scheduler service: %w", getTaskSchedulerError(err))
	}

	if serverName == "" {
		serverName, err = os.Hostname()
		if err != nil {
			return TaskService{}, err
		}
	}
	if domain == "" {
		domain = serverName
	}
	if username == "" {
		currentUser, err := user.Current()
		if err != nil {
			return TaskService{}, err
		}
		username = strings.Split(currentUser.Username, `\`)[1]
	}
	taskService.connectedDomain = domain
	taskService.connectedComputerName = serverName
	taskService.connectedUser = username

	res, err := oleutil.CallMethod(taskService.taskServiceObj, "GetFolder", `\`)
	if err != nil {
		return TaskService{}, fmt.Errorf("error getting the root folder: %w", getTaskSchedulerError(err))
	}
	taskService.rootFolderObj = res.ToIDispatch()
	taskService.isConnected = true

	return taskService, nil
}

// Disconnect frees all the Task Scheduler COM objects that have been created.
// If this function is not called before the parent program terminates,
// memory leaks will occur.
func (t *TaskService) Disconnect() {
	if t.isConnected {
		t.taskServiceObj.Release()
		t.rootFolderObj.Release()
	}
	if t.isInitialized {
		ole.CoUninitialize()
	}

	t.isInitialized = false
	t.isConnected = false
}

// GetRunningTasks enumerates the Task Scheduler database for all currently running tasks.
func (t *TaskService) GetRunningTasks() (RunningTaskCollection, error) {
	var runningTasks RunningTaskCollection

	res, err := oleutil.CallMethod(t.taskServiceObj, "GetRunningTasks", int(TASK_ENUM_HIDDEN))
	if err != nil {
		return nil, fmt.Errorf("error getting running tasks: %w", getTaskSchedulerError(err))
	}

	runningTasksObj := res.ToIDispatch()
	defer runningTasksObj.Release()
	err = oleutil.ForEach(runningTasksObj, func(v *ole.VARIANT) error {
		task := v.ToIDispatch()

		runningTask, err := parseRunningTask(task)
		if err != nil {
			return fmt.Errorf("error parsing running task: %w", err)
		}
		runningTasks = append(runningTasks, runningTask)

		return nil
	})
	if err != nil {
		return nil, err
	}

	return runningTasks, nil
}

// GetRegisteredTasks enumerates the Task Scheduler database for all currently registered tasks.
func (t *TaskService) GetRegisteredTasks() (RegisteredTaskCollection, error) {
	var (
		err             error
		registeredTasks RegisteredTaskCollection
	)

	// get tasks from root folder
	res, err := oleutil.CallMethod(t.rootFolderObj, "GetTasks", int(TASK_ENUM_HIDDEN))
	if err != nil {
		return nil, fmt.Errorf("error getting tasks of root folder: %w", getTaskSchedulerError(err))
	}
	rootTaskCollection := res.ToIDispatch()
	defer rootTaskCollection.Release()
	err = oleutil.ForEach(rootTaskCollection, func(v *ole.VARIANT) error {
		task := v.ToIDispatch()
		defer task.Release()

		registeredTask, path, err := parseRegisteredTask(task)
		if err != nil {
			return fmt.Errorf("error parsing registered task %s: %w", path, err)
		}
		registeredTasks = append(registeredTasks, registeredTask)

		return nil
	})
	if err != nil {
		return nil, err
	}

	res, err = oleutil.CallMethod(t.rootFolderObj, "GetFolders", 0)
	if err != nil {
		return nil, fmt.Errorf("error getting task folders of root folder: %w", getTaskSchedulerError(err))
	}
	taskFolderList := res.ToIDispatch()
	defer taskFolderList.Release()

	// recursively enumerate folders and tasks
	var enumTaskFolders func(*ole.VARIANT) error
	enumTaskFolders = func(v *ole.VARIANT) error {
		taskFolder := v.ToIDispatch()
		defer taskFolder.Release()

		res, err := oleutil.CallMethod(taskFolder, "GetTasks", int(TASK_ENUM_HIDDEN))
		if err != nil {
			return fmt.Errorf("error getting tasks of folder: %w", getTaskSchedulerError(err))
		}
		taskCollection := res.ToIDispatch()
		defer taskCollection.Release()

		err = oleutil.ForEach(taskCollection, func(v *ole.VARIANT) error {
			task := v.ToIDispatch()

			registeredTask, path, err := parseRegisteredTask(task)
			if err != nil {
				return fmt.Errorf("error parsing registered task %s: %w", path, err)
			}
			registeredTasks = append(registeredTasks, registeredTask)

			return nil
		})
		if err != nil {
			return err
		}

		res, err = oleutil.CallMethod(taskFolder, "GetFolders", 0)
		if err != nil {
			return fmt.Errorf("error getting subfolders of folder: %w", getTaskSchedulerError(err))
		}
		taskFolderList := res.ToIDispatch()
		defer taskFolderList.Release()

		err = oleutil.ForEach(taskFolderList, enumTaskFolders)
		if err != nil {
			return err
		}

		return nil
	}

	err = oleutil.ForEach(taskFolderList, enumTaskFolders)
	if err != nil {
		return nil, err
	}

	return registeredTasks, nil
}

// GetRegisteredTask attempts to find the specified registered task and returns a
// pointer to it if it exists. If it doesn't exist, nil will be returned in place of
// the registered task.
func (t *TaskService) GetRegisteredTask(path string) (RegisteredTask, error) {
	if path[0] != '\\' {
		return RegisteredTask{}, ErrInvalidPath
	}

	taskObj, err := oleutil.CallMethod(t.rootFolderObj, "GetTask", path)
	if err != nil {
		return RegisteredTask{}, fmt.Errorf("error getting registered task %s: %w", path, getTaskSchedulerError(err))
	}

	task, _, err := parseRegisteredTask(taskObj.ToIDispatch())
	if err != nil {
		return RegisteredTask{}, fmt.Errorf("error parsing registered task %s: %w", path, err)
	}

	return task, nil
}

// GetTaskFolders enumerates the Task Schedule database for all task folders and currently
// registered tasks.
func (t TaskService) GetTaskFolders() (TaskFolder, error) {
	return t.GetTaskFolder(`\`)
}

// GetTaskFolder enumerates the Task Schedule database for all task sub folders and currently
// registered tasks under the folder specified, if it exists. If it doesn't exist, nil will be
// returned in place of the task folder.
func (t TaskService) GetTaskFolder(path string) (TaskFolder, error) {
	if path[0] != '\\' {
		return TaskFolder{}, ErrInvalidPath
	}

	var topFolderObj *ole.IDispatch
	if path == `\` {
		topFolderObj = t.rootFolderObj
	} else {
		topFolder, err := oleutil.CallMethod(t.taskServiceObj, "GetFolder", path)
		if err != nil {
			return TaskFolder{}, fmt.Errorf("error getting folder %s: %w", path, getTaskSchedulerError(err))
		}
		topFolderObj = topFolder.ToIDispatch()
		defer topFolderObj.Release()
	}

	// get tasks from the top folder
	res, err := oleutil.CallMethod(topFolderObj, "GetTasks", int(TASK_ENUM_HIDDEN))
	if err != nil {
		return TaskFolder{}, fmt.Errorf("error getting tasks of folder %s: %w", path, getTaskSchedulerError(err))
	}
	topFolderTaskCollection := res.ToIDispatch()
	defer topFolderTaskCollection.Release()
	topFolder := TaskFolder{Path: `\`}
	err = oleutil.ForEach(topFolderTaskCollection, func(v *ole.VARIANT) error {
		task := v.ToIDispatch()

		registeredTask, path, err := parseRegisteredTask(task)
		if err != nil {
			return fmt.Errorf("error parsing registered task %s: %w", path, err)
		}
		topFolder.RegisteredTasks = append(topFolder.RegisteredTasks, registeredTask)

		return nil
	})
	if err != nil {
		return TaskFolder{}, err
	}

	res, err = oleutil.CallMethod(topFolderObj, "GetFolders", 0)
	if err != nil {
		return TaskFolder{}, fmt.Errorf("error getting subfolders of folder %s: %w", path, getTaskSchedulerError(err))
	}
	taskFolderList := res.ToIDispatch()
	defer taskFolderList.Release()

	// recursively enumerate folders and tasks
	var initEnumTaskFolders func(*TaskFolder) func(*ole.VARIANT) error
	initEnumTaskFolders = func(parentFolder *TaskFolder) func(*ole.VARIANT) error {
		var enumTaskFolders func(*ole.VARIANT) error
		enumTaskFolders = func(v *ole.VARIANT) error {
			taskFolder := v.ToIDispatch()
			defer taskFolder.Release()

			name := oleutil.MustGetProperty(taskFolder, "Name").ToString()
			path := oleutil.MustGetProperty(taskFolder, "Path").ToString()
			res, err := oleutil.CallMethod(taskFolder, "GetTasks", int(TASK_ENUM_HIDDEN))
			if err != nil {
				return fmt.Errorf("error getting tasks of folder %s: %w", path, getTaskSchedulerError(err))
			}
			taskCollection := res.ToIDispatch()
			defer taskCollection.Release()

			taskSubFolder := &TaskFolder{
				Name: name,
				Path: path,
			}

			err = oleutil.ForEach(taskCollection, func(v *ole.VARIANT) error {
				task := v.ToIDispatch()

				registeredTask, path, err := parseRegisteredTask(task)
				if err != nil {
					return fmt.Errorf("error parsing registered task %s: %w", path, err)
				}
				taskSubFolder.RegisteredTasks = append(taskSubFolder.RegisteredTasks, registeredTask)

				return nil
			})
			if err != nil {
				return err
			}

			parentFolder.SubFolders = append(parentFolder.SubFolders, taskSubFolder)

			res, err = oleutil.CallMethod(taskFolder, "GetFolders", 0)
			if err != nil {
				return fmt.Errorf("error getting subfolders of folder %s: %w", path, getTaskSchedulerError(err))
			}
			taskFolderList := res.ToIDispatch()
			defer taskFolderList.Release()

			err = oleutil.ForEach(taskFolderList, initEnumTaskFolders(taskSubFolder))
			if err != nil {
				return err
			}

			return nil
		}

		return enumTaskFolders
	}

	err = oleutil.ForEach(taskFolderList, initEnumTaskFolders(&topFolder))
	if err != nil {
		return TaskFolder{}, err
	}

	return topFolder, nil
}

// NewTaskDefinition returns a new task definition that can be used to register a new task.
// Task settings and properties are set to Task Scheduler default values.
func (t TaskService) NewTaskDefinition() Definition {
	var newDef Definition

	newDef.Principal.LogonType = TASK_LOGON_INTERACTIVE_TOKEN
	newDef.Principal.RunLevel = TASK_RUNLEVEL_LUA

	newDef.RegistrationInfo.Author = t.connectedDomain + `\` + t.connectedUser
	newDef.RegistrationInfo.Date = time.Now()

	newDef.Settings.AllowDemandStart = true
	newDef.Settings.AllowHardTerminate = true
	newDef.Settings.Compatibility = TASK_COMPATIBILITY_V2
	newDef.Settings.DontStartOnBatteries = true
	newDef.Settings.Enabled = true
	newDef.Settings.Hidden = false
	newDef.Settings.IdleSettings.IdleDuration = period.NewHMS(0, 10, 0) // PT10M
	newDef.Settings.IdleSettings.WaitTimeout = period.NewHMS(1, 0, 0)   // PT1H
	newDef.Settings.MultipleInstances = TASK_INSTANCES_IGNORE_NEW
	newDef.Settings.Priority = 7
	newDef.Settings.RestartCount = 0
	newDef.Settings.RestartOnIdle = false
	newDef.Settings.RunOnlyIfIdle = false
	newDef.Settings.RunOnlyIfNetworkAvailable = false
	newDef.Settings.StartWhenAvailable = false
	newDef.Settings.StopIfGoingOnBatteries = true
	newDef.Settings.StopOnIdleEnd = true
	newDef.Settings.TimeLimit = period.NewHMS(72, 0, 0) // PT72H
	newDef.Settings.WakeToRun = false

	return newDef
}

// CreateTask creates a registered task on the connected computer. CreateTask returns
// true if the task was successfully registered, and false if the overwrite parameter
// is false and a task at the specified path already exists.
func (t *TaskService) CreateTask(path string, newTaskDef Definition, overwrite bool) (RegisteredTask, bool, error) {
	return t.CreateTaskEx(path, newTaskDef, "", "", newTaskDef.Principal.LogonType, overwrite)
}

// CreateTaskEx creates a registered task on the connected computer. CreateTaskEx returns
// true if the task was successfully registered, and false if the overwrite parameter
// is false and a task at the specified path already exists.
func (t *TaskService) CreateTaskEx(path string, newTaskDef Definition, username, password string, logonType TaskLogonType, overwrite bool) (RegisteredTask, bool, error) {
	var err error

	if path[0] != '\\' {
		return RegisteredTask{}, false, ErrInvalidPath
	} else if err = validateDefinition(newTaskDef); err != nil {
		return RegisteredTask{}, false, err
	}

	nameIndex := strings.LastIndex(path, `\`)
	folderPath := path[:nameIndex]

	if !t.taskFolderExist(folderPath) {
		_, err = oleutil.CallMethod(t.rootFolderObj, "CreateFolder", folderPath, "")
		if err != nil {
			return RegisteredTask{}, false, fmt.Errorf("error creating folder %s: %w", path, getTaskSchedulerError(err))
		}
	} else {
		if t.registeredTaskExist(path) {
			if !overwrite {
				task, err := t.GetRegisteredTask(path)
				if err != nil {
					return RegisteredTask{}, false, err
				}

				return task, false, nil
			}
			_, err = oleutil.CallMethod(t.rootFolderObj, "DeleteTask", path, 0)
			if err != nil {
				return RegisteredTask{}, false, fmt.Errorf("error deleting registered task %s: %w", path, getTaskSchedulerError(err))
			}
		}
	}

	newTaskObj, err := t.modifyTask(path, newTaskDef, username, password, logonType, TASK_CREATE)
	if err != nil {
		return RegisteredTask{}, false, fmt.Errorf("error creating registered task %s: %w", path, err)
	}

	newTask, _, err := parseRegisteredTask(newTaskObj)
	if err != nil {
		return RegisteredTask{}, false, fmt.Errorf("error parsing registered task %s: %w", path, err)
	}

	return newTask, true, nil
}

// UpdateTask updates a registered task.
func (t *TaskService) UpdateTask(path string, newTaskDef Definition) (RegisteredTask, error) {
	return t.UpdateTaskEx(path, newTaskDef, "", "", newTaskDef.Principal.LogonType)
}

// UpdateTaskEx updates a registered task.
func (t *TaskService) UpdateTaskEx(path string, newTaskDef Definition, username, password string, logonType TaskLogonType) (RegisteredTask, error) {
	var err error

	if path[0] != '\\' {
		return RegisteredTask{}, ErrInvalidPath
	} else if err = validateDefinition(newTaskDef); err != nil {
		return RegisteredTask{}, err
	}

	newTaskObj, err := t.modifyTask(path, newTaskDef, username, password, logonType, TASK_UPDATE)
	if err != nil {
		return RegisteredTask{}, fmt.Errorf("error updating %s task: %w", path, err)
	}

	// update the internal database of registered tasks
	newTask, _, err := parseRegisteredTask(newTaskObj)
	if err != nil {
		return RegisteredTask{}, fmt.Errorf("error parsing registered task %s: %w", path, err)
	}

	return newTask, nil
}

func (t *TaskService) modifyTask(path string, newTaskDef Definition, username, password string, logonType TaskLogonType, flags TaskCreationFlags) (*ole.IDispatch, error) {
	// set default UserID if UserID and GroupID both aren't set
	if newTaskDef.Principal.UserID == "" && newTaskDef.Principal.GroupID == "" {
		newTaskDef.Principal.UserID = t.connectedDomain + `\` + t.connectedUser
	}

	res, err := oleutil.CallMethod(t.taskServiceObj, "NewTask", 0)
	if err != nil {
		return nil, fmt.Errorf("error creating new task: %w", getTaskSchedulerError(err))
	}
	newTaskDefObj := res.ToIDispatch()
	defer newTaskDefObj.Release()

	err = fillDefinitionObj(newTaskDef, newTaskDefObj)
	if err != nil {
		return nil, fmt.Errorf("error filling ITaskDefinition: %w", err)
	}

	newTaskObj, err := oleutil.CallMethod(t.rootFolderObj, "RegisterTaskDefinition", path, newTaskDefObj, int(flags), username, password, int(logonType), "")
	if err != nil {
		return nil, fmt.Errorf("error registering task: %w", getTaskSchedulerError(err))
	}

	return newTaskObj.ToIDispatch(), nil
}

// DeleteFolder removes a task folder from the connected computer. If the deleteRecursively parameter
// is set to true, all tasks and subfolders will be removed recursively. If it's set to false, DeleteFolder
// will return true if the folder was empty and deleted successfully, and false otherwise.
func (t *TaskService) DeleteFolder(path string, deleteRecursively bool) (bool, error) {
	var err error

	if path[0] != '\\' {
		return false, ErrInvalidPath
	}

	taskFolder, err := oleutil.CallMethod(t.taskServiceObj, "GetFolder", path)
	if err != nil {
		return false, fmt.Errorf("error getting folder: %w", getTaskSchedulerError(err))
	}

	taskFolderObj := taskFolder.ToIDispatch()
	defer taskFolderObj.Release()
	res, err := oleutil.CallMethod(taskFolderObj, "GetTasks", int(TASK_ENUM_HIDDEN))
	if err != nil {
		return false, fmt.Errorf("error getting tasks of folder: %w", getTaskSchedulerError(err))
	}
	taskCollection := res.ToIDispatch()
	defer taskCollection.Release()
	if !deleteRecursively && oleutil.MustGetProperty(taskCollection, "Count").Val > 0 {
		return false, nil
	}

	res, err = oleutil.CallMethod(taskFolderObj, "GetFolders", int(TASK_ENUM_HIDDEN))
	if err != nil {
		return false, fmt.Errorf("error getting the subfolders: %w", getTaskSchedulerError(err))
	}
	folderCollection := res.ToIDispatch()
	defer folderCollection.Release()
	if !deleteRecursively && oleutil.MustGetProperty(folderCollection, "Count").Val > 0 {
		return false, nil
	}

	if deleteRecursively {
		// delete tasks in parent folder
		deleteAllTasks := func(v *ole.VARIANT) error {
			taskObj := v.ToIDispatch()
			defer taskObj.Release()

			name := oleutil.MustGetProperty(taskObj, "Path").ToString()

			return t.DeleteTask(name)
		}
		err = oleutil.ForEach(taskCollection, deleteAllTasks)
		if err != nil {
			return false, err
		}

		var deleteTasksRecursively func(*ole.VARIANT) error
		deleteTasksRecursively = func(v *ole.VARIANT) error {
			var err error

			folderObj := v.ToIDispatch()
			defer folderObj.Release()

			res, err := oleutil.CallMethod(folderObj, "GetTasks", int(TASK_ENUM_HIDDEN))
			if err != nil {
				return fmt.Errorf("error getting tasks of folder: %w", getTaskSchedulerError(err))
			}
			tasks := res.ToIDispatch()
			defer tasks.Release()

			err = oleutil.ForEach(tasks, deleteAllTasks)
			if err != nil {
				return err
			}

			res, err = oleutil.CallMethod(folderObj, "GetFolders", int(TASK_ENUM_HIDDEN))
			if err != nil {
				return fmt.Errorf("error getting subfolders: %w", getTaskSchedulerError(err))
			}
			subFolders := res.ToIDispatch()
			defer subFolders.Release()

			err = oleutil.ForEach(subFolders, deleteTasksRecursively)
			if err != nil {
				return err
			}

			currentFolderPath := oleutil.MustGetProperty(folderObj, "Path").ToString()
			_, err = oleutil.CallMethod(t.rootFolderObj, "DeleteFolder", currentFolderPath, 0)
			if err != nil {
				return fmt.Errorf("error deleting task folder %s: %w", path, getTaskSchedulerError(err))
			}

			return nil
		}

		// delete all subfolders and tasks recursively
		err = oleutil.ForEach(folderCollection, deleteTasksRecursively)
		if err != nil {
			return false, err
		}
	}

	// delete parent folder
	_, err = oleutil.CallMethod(t.rootFolderObj, "DeleteFolder", path, 0)
	if err != nil {
		return false, fmt.Errorf("error deleting task folder %s: %w", path, getTaskSchedulerError(err))
	}

	return true, nil
}

// DeleteTask removes a registered task from the connected computer.
func (t *TaskService) DeleteTask(path string) error {
	var err error

	if path[0] != '\\' {
		return ErrInvalidPath
	}

	_, err = oleutil.CallMethod(t.rootFolderObj, "DeleteTask", path, 0)
	if err != nil {
		return fmt.Errorf("error deleting task %s: %w", path, getTaskSchedulerError(err))
	}

	return nil
}

func (t *TaskService) registeredTaskExist(path string) bool {
	_, err := oleutil.CallMethod(t.rootFolderObj, "GetTask", path)
	if err != nil {
		return false
	}

	return true
}

func (t *TaskService) taskFolderExist(path string) bool {
	_, err := oleutil.CallMethod(t.taskServiceObj, "GetFolder", path)
	if err != nil {
		return false
	}

	return true
}
