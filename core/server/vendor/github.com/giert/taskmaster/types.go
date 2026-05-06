//go:build windows
// +build windows

package taskmaster

import (
	"fmt"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/go-ole/go-ole"
	"github.com/rickb777/date/period"
)

// DayOfWeek is a day of the week.
type DayOfWeek uint16

const (
	Sunday DayOfWeek = 1 << iota
	Monday
	Tuesday
	Wednesday
	Thursday
	Friday
	Saturday
	AllDays DayOfWeek = (1 << 7) - 1
)

func (d DayOfWeek) String() string {
	if d == 0 || d > AllDays {
		return "Invalid day of week"
	} else if d == AllDays {
		return "All days of the week"
	}

	var buf strings.Builder
	if Sunday&d == Sunday {
		buf.WriteString("Sunday, ")
	}
	if Monday&d == Monday {
		buf.WriteString("Monday, ")
	}
	if Tuesday&d == Tuesday {
		buf.WriteString("Tuesday, ")
	}
	if Wednesday&d == Wednesday {
		buf.WriteString("Wednesday, ")
	}
	if Thursday&d == Thursday {
		buf.WriteString("Thursday, ")
	}
	if Friday&d == Friday {
		buf.WriteString("Friday, ")
	}
	if Saturday&d == Saturday {
		buf.WriteString("Saturday, ")
	}

	s := buf.String()
	return s[:len(s)-2]
}

// DayInterval specifies if a task runs every day or every other day.
type DayInterval uint8

const (
	EveryDay      DayInterval = 1
	EveryOtherDay DayInterval = 2
)

func (d DayInterval) String() string {
	if d == EveryDay {
		return "Every day"
	}
	return fmt.Sprintf("Every %d days", d)
}

// DayOfMonth is a day of a month.
type DayOfMonth uint32

const (
	One DayOfMonth = 1 << iota
	Two
	Three
	Four
	Five
	Six
	Seven
	Eight
	Nine
	Ten
	Eleven
	Twelve
	Thirteen
	Fourteen
	Fifteen
	Sixteen
	Seventeen
	Eighteen
	Nineteen
	Twenty
	TwentyOne
	TwentyTwo
	TwentyThree
	TwentyFour
	TwentyFive
	TwentySix
	TwentySeven
	TwentyEight
	TwentyNine
	Thirty
	ThirtyOne
	LastDayOfMonth
	AllDaysOfMonth DayOfMonth = (1 << 31) - 1
)

func (d DayOfMonth) String() string {
	if d == 0 || d > LastDayOfMonth {
		return "Invalid day of month"
	} else if d == AllDaysOfMonth {
		return "All days of the month"
	}

	var buf strings.Builder
	for i, j := DayOfMonth(1), uint(1); i < LastDayOfMonth; i, j = (1<<j+1)-1, j+1 {
		if d&i == i {
			buf.WriteString(strconv.FormatInt(int64(j), 10))
			buf.WriteString(", ")
		}
	}

	if d&LastDayOfMonth == LastDayOfMonth {
		buf.WriteString("last day of month")
		return buf.String()
	}

	s := buf.String()
	return s[:len(s)-2]
}

// Month is one of the 12 months.
type Month uint16

const (
	January Month = 1 << iota
	February
	March
	April
	May
	June
	July
	August
	September
	October
	November
	December
	AllMonths Month = (1 << 12) - 1
)

func (m Month) String() string {
	if m == 0 || m > AllMonths {
		return "Invalid month"
	} else if m == AllMonths {
		return "All months"
	}

	var buf strings.Builder
	if m&January == January {
		buf.WriteString("January, ")
	}
	if m&February == February {
		buf.WriteString("February, ")
	}
	if m&March == March {
		buf.WriteString("March, ")
	}
	if m&April == April {
		buf.WriteString("April, ")
	}
	if m&May == May {
		buf.WriteString("May, ")
	}
	if m&June == June {
		buf.WriteString("June, ")
	}
	if m&July == July {
		buf.WriteString("July, ")
	}
	if m&August == August {
		buf.WriteString("August, ")
	}
	if m&September == September {
		buf.WriteString("September, ")
	}
	if m&October == October {
		buf.WriteString("October, ")
	}
	if m&November == November {
		buf.WriteString("November, ")
	}
	if m&December == December {
		buf.WriteString("December, ")
	}

	s := buf.String()
	return s[:len(s)-2]
}

// Week specifies what week of the month a task will run on.
type Week uint8

const (
	First Week = 1 << iota
	Second
	Third
	Fourth
	LastWeek
	AllWeeks Week = (1 << 5) - 1
)

func (w Week) String() string {
	if w == 0 || w > AllWeeks {
		return "Invalid week of the month"
	} else if w == AllWeeks {
		return "All weeks of the month"
	}

	var buf strings.Builder
	if First&w == First {
		buf.WriteString("First, ")
	}
	if Second&w == Second {
		buf.WriteString("Second, ")
	}
	if Third&w == Third {
		buf.WriteString("Third, ")
	}
	if Fourth&w == Fourth {
		buf.WriteString("Fourth, ")
	}
	if LastWeek&w == LastWeek {
		buf.WriteString("LastWeek, ")
	}

	s := buf.String()
	return s[:len(s)-2]
}

// WeekInterval specifies if a task runs every week or every other week.
type WeekInterval uint8

const (
	EveryWeek      WeekInterval = 1
	EveryOtherWeek WeekInterval = 2
)

func (w WeekInterval) String() string {
	if w == EveryWeek {
		return "Every week"
	}
	return fmt.Sprintf("Every %d weeks", w)
}

// TaskActionType specifies the type of a task action.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/ne-taskschd-task_action_type
type TaskActionType uint

const (
	TASK_ACTION_EXEC         TaskActionType = 0
	TASK_ACTION_COM_HANDLER  TaskActionType = 5
	TASK_ACTION_SEND_EMAIL   TaskActionType = 6
	TASK_ACTION_SHOW_MESSAGE TaskActionType = 7
)

func (t TaskActionType) String() string {
	switch t {
	case TASK_ACTION_EXEC:
		return "Exec"
	case TASK_ACTION_COM_HANDLER:
		return "COM Handler"
	case TASK_ACTION_SEND_EMAIL:
		return "Send Email"
	case TASK_ACTION_SHOW_MESSAGE:
		return "Show Message"
	default:
		return ""
	}
}

// TaskCompatibility specifies the compatibility of a registered task.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/ne-taskschd-task_compatibility
type TaskCompatibility uint

const (
	TASK_COMPATIBILITY_AT TaskCompatibility = iota
	TASK_COMPATIBILITY_V1
	TASK_COMPATIBILITY_V2
	TASK_COMPATIBILITY_V2_1
	TASK_COMPATIBILITY_V2_2
	TASK_COMPATIBILITY_V2_3
	TASK_COMPATIBILITY_V2_4
)

func (c TaskCompatibility) String() string {
	switch c {
	case TASK_COMPATIBILITY_AT:
		return "AT"
	case TASK_COMPATIBILITY_V1:
		return "v1.0"
	case TASK_COMPATIBILITY_V2:
		return "v2.0"
	case TASK_COMPATIBILITY_V2_1:
		return "v2.1"
	case TASK_COMPATIBILITY_V2_2:
		return "v2.2"
	case TASK_COMPATIBILITY_V2_3:
		return "v2.3"
	case TASK_COMPATIBILITY_V2_4:
		return "v2.4"
	default:
		return ""
	}
}

// TaskCreationFlags specifies how a task will be created.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/ne-taskschd-task_creation
type TaskCreationFlags uint

const (
	TASK_VALIDATE_ONLY                TaskCreationFlags = 0x01
	TASK_CREATE                       TaskCreationFlags = 0x02
	TASK_UPDATE                       TaskCreationFlags = 0x04
	TASK_CREATE_OR_UPDATE             TaskCreationFlags = 0x06
	TASK_DISABLE                      TaskCreationFlags = 0x08
	TASK_DONT_ADD_PRINCIPAL_ACE       TaskCreationFlags = 0x10
	TASK_IGNORE_REGISTRATION_TRIGGERS TaskCreationFlags = 0x20
)

// TaskEnumFlags specifies how tasks will be enumerated.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/ne-taskschd-task_enum_flags
type TaskEnumFlags uint

const (
	TASK_ENUM_HIDDEN TaskEnumFlags = 1 // enumerate all tasks, including tasks that are hidden
)

// TaskInstancesPolicy specifies what the Task Scheduler service will do when
// multiple instances of a task are triggered or operating at once.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/ne-taskschd-task_instances_policy
type TaskInstancesPolicy uint

const (
	TASK_INSTANCES_PARALLEL      TaskInstancesPolicy = iota // start new instance while an existing instance is running
	TASK_INSTANCES_QUEUE                                    // start a new instance of the task after all other instances of the task are complete
	TASK_INSTANCES_IGNORE_NEW                               // do not start a new instance if an existing instance of the task is running
	TASK_INSTANCES_STOP_EXISTING                            // stop an existing instance of the task before it starts a new instance
)

func (t TaskInstancesPolicy) String() string {
	switch t {
	case TASK_INSTANCES_PARALLEL:
		return "Run Parallel"
	case TASK_INSTANCES_QUEUE:
		return "Queue Instances"
	case TASK_INSTANCES_IGNORE_NEW:
		return "Ignore New"
	case TASK_INSTANCES_STOP_EXISTING:
		return "Stop Existing"
	default:
		return ""
	}
}

// TaskLogonType specifies how a registered task will authenticate when it executes.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/ne-taskschd-task_logon_type
type TaskLogonType uint

const (
	TASK_LOGON_NONE                          TaskLogonType = iota // the logon method is not specified. Used for non-NT credentials
	TASK_LOGON_PASSWORD                                           // use a password for logging on the user. The password must be supplied at registration time
	TASK_LOGON_S4U                                                // the service will log the user on using Service For User (S4U), and the task will run in a non-interactive desktop. When an S4U logon is used, no password is stored by the system and there is no access to either the network or to encrypted files
	TASK_LOGON_INTERACTIVE_TOKEN                                  // user must already be logged on. The task will be run only in an existing interactive session
	TASK_LOGON_GROUP                                              // group activation
	TASK_LOGON_SERVICE_ACCOUNT                                    // indicates that a Local System, Local Service, or Network Service account is being used as a security context to run the task
	TASK_LOGON_INTERACTIVE_TOKEN_OR_PASSWORD                      // first use the interactive token. If the user is not logged on (no interactive token is available), then the password is used. The password must be specified when a task is registered. This flag is not recommended for new tasks because it is less reliable than TASK_LOGON_PASSWORD
)

func (t TaskLogonType) String() string {
	switch t {
	case TASK_LOGON_NONE:
		return "None"
	case TASK_LOGON_PASSWORD:
		return "Password"
	case TASK_LOGON_S4U:
		return "S4u"
	case TASK_LOGON_INTERACTIVE_TOKEN:
		return "Interactive Token"
	case TASK_LOGON_GROUP:
		return "Group"
	case TASK_LOGON_SERVICE_ACCOUNT:
		return "Service Account"
	case TASK_LOGON_INTERACTIVE_TOKEN_OR_PASSWORD:
		return "Interactive Token or Password"
	default:
		return ""
	}
}

// TaskRunFlags specifies how a task will be executed.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/ne-taskschd-task_run_flags
type TaskRunFlags uint

const (
	TASK_RUN_NO_FLAGS           TaskRunFlags = iota // the task is run with all flags ignored
	TASK_RUN_AS_SELF                                // the task is run as the user who is calling the Run method
	TASK_RUN_IGNORE_CONSTRAINTS                     // the task is run regardless of constraints such as "do not run on batteries" or "run only if idle"
	TASK_RUN_USE_SESSION_ID                         // the task is run using a terminal server session identifier
	TASK_RUN_USER_SID                               // the task is run using a security identifier
)

// TaskRunLevel specifies whether the task will be run with full permissions or not.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/ne-taskschd-task_runlevel_type
type TaskRunLevel uint

const (
	TASK_RUNLEVEL_LUA     TaskRunLevel = iota // task will be run with the least privileges
	TASK_RUNLEVEL_HIGHEST                     // task will be run with the highest privileges
)

func (t TaskRunLevel) String() string {
	switch t {
	case TASK_RUNLEVEL_LUA:
		return "Least"
	case TASK_RUNLEVEL_HIGHEST:
		return "Highest"
	default:
		return ""
	}
}

// TaskSessionStateChangeType specifies the type of session state change that a
// SessionStateChange trigger will trigger on.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/ne-taskschd-task_session_state_change_type
type TaskSessionStateChangeType uint

const (
	TASK_CONSOLE_CONNECT    TaskSessionStateChangeType = 1 // Terminal Server console connection state change. For example, when you connect to a user session on the local computer by switching users on the computer
	TASK_CONSOLE_DISCONNECT TaskSessionStateChangeType = 2 // Terminal Server console disconnection state change. For example, when you disconnect to a user session on the local computer by switching users on the computer
	TASK_REMOTE_CONNECT     TaskSessionStateChangeType = 3 // Terminal Server remote connection state change. For example, when a user connects to a user session by using the Remote Desktop Connection program from a remote computer
	TASK_REMOTE_DISCONNECT  TaskSessionStateChangeType = 4 // Terminal Server remote disconnection state change. For example, when a user disconnects from a user session while using the Remote Desktop Connection program from a remote computer
	TASK_SESSION_LOCK       TaskSessionStateChangeType = 7 // Terminal Server session locked state change. For example, this state change causes the task to run when the computer is locked
	TASK_SESSION_UNLOCK     TaskSessionStateChangeType = 8 // Terminal Server session unlocked state change. For example, this state change causes the task to run when the computer is unlocked
)

func (t TaskSessionStateChangeType) String() string {
	switch t {
	case TASK_CONSOLE_CONNECT:
		return "Console Connect"
	case TASK_CONSOLE_DISCONNECT:
		return "Console Disconnect"
	case TASK_REMOTE_CONNECT:
		return "Remote Connect"
	case TASK_REMOTE_DISCONNECT:
		return "Remote Disconnect"
	case TASK_SESSION_LOCK:
		return "Session Lock"
	case TASK_SESSION_UNLOCK:
		return "Session Unlock"
	default:
		return ""
	}
}

// TaskState specifies the state of a running or registered task.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/ne-taskschd-task_state
type TaskState uint

const (
	TASK_STATE_UNKNOWN  TaskState = iota // the state of the task is unknown
	TASK_STATE_DISABLED                  // the task is registered but is disabled and no instances of the task are queued or running. The task cannot be run until it is enabled
	TASK_STATE_QUEUED                    // instances of the task are queued
	TASK_STATE_READY                     // the task is ready to be executed, but no instances are queued or running
	TASK_STATE_RUNNING                   // one or more instances of the task is running
)

func (t TaskState) String() string {
	switch t {
	case TASK_STATE_UNKNOWN:
		return "Unknown"
	case TASK_STATE_DISABLED:
		return "Disabled"
	case TASK_STATE_QUEUED:
		return "Queued"
	case TASK_STATE_READY:
		return "Ready"
	case TASK_STATE_RUNNING:
		return "Running"
	default:
		return ""
	}
}

// TaskTriggerType specifies the type of a task trigger.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/ne-taskschd-task_trigger_type2
type TaskTriggerType uint

const (
	TASK_TRIGGER_EVENT                TaskTriggerType = 0
	TASK_TRIGGER_TIME                 TaskTriggerType = 1
	TASK_TRIGGER_DAILY                TaskTriggerType = 2
	TASK_TRIGGER_WEEKLY               TaskTriggerType = 3
	TASK_TRIGGER_MONTHLY              TaskTriggerType = 4
	TASK_TRIGGER_MONTHLYDOW           TaskTriggerType = 5
	TASK_TRIGGER_IDLE                 TaskTriggerType = 6
	TASK_TRIGGER_REGISTRATION         TaskTriggerType = 7
	TASK_TRIGGER_BOOT                 TaskTriggerType = 8
	TASK_TRIGGER_LOGON                TaskTriggerType = 9
	TASK_TRIGGER_SESSION_STATE_CHANGE TaskTriggerType = 11
	TASK_TRIGGER_CUSTOM_TRIGGER_01    TaskTriggerType = 12
)

func (t TaskTriggerType) String() string {
	switch t {
	case TASK_TRIGGER_EVENT:
		return "Event"
	case TASK_TRIGGER_TIME:
		return "Time"
	case TASK_TRIGGER_DAILY:
		return "Daily"
	case TASK_TRIGGER_WEEKLY:
		return "Weekly"
	case TASK_TRIGGER_MONTHLY:
		return "Monthly"
	case TASK_TRIGGER_MONTHLYDOW:
		return "Monthly Day of the Week"
	case TASK_TRIGGER_IDLE:
		return "Idle"
	case TASK_TRIGGER_REGISTRATION:
		return "registration"
	case TASK_TRIGGER_BOOT:
		return "Boot"
	case TASK_TRIGGER_LOGON:
		return "Logon"
	case TASK_TRIGGER_SESSION_STATE_CHANGE:
		return "Session State Change"
	case TASK_TRIGGER_CUSTOM_TRIGGER_01:
		return "Custom"
	default:
		return ""
	}
}

type TaskResult uint32

const (
	SCHED_S_SUCCESS    TaskResult = 0x0
	SCHED_S_TASK_READY TaskResult = iota + 0x00041300
	SCHED_S_TASK_RUNNING
	SCHED_S_TASK_DISABLED
	SCHED_S_TASK_HAS_NOT_RUN
	SCHED_S_TASK_NO_MORE_RUNS
	SCHED_S_TASK_NOT_SCHEDULED
	SCHED_S_TASK_TERMINATED
	SCHED_S_TASK_NO_VALID_TRIGGERS
	SCHED_S_EVENT_TRIGGER
	SCHED_S_SOME_TRIGGERS_FAILED TaskResult = 0x0004131B
	SCHED_S_BATCH_LOGON_PROBLEM  TaskResult = 0x0004131C
	SCHED_S_TASK_QUEUED          TaskResult = 0x00041325
)

func (r TaskResult) String() string {
	switch r {
	case SCHED_S_SUCCESS:
		return "Completed successfully"
	case SCHED_S_TASK_READY:
		return "Ready"
	case SCHED_S_TASK_RUNNING:
		return "Currently running"
	case SCHED_S_TASK_DISABLED:
		return "Disabled"
	case SCHED_S_TASK_HAS_NOT_RUN:
		return "Has not been run yet"
	case SCHED_S_TASK_NO_MORE_RUNS:
		return "No more runs scheduled"
	case SCHED_S_TASK_NOT_SCHEDULED:
		return "One or more of the properties that are needed to run this task on a schedule have not been set"
	case SCHED_S_TASK_TERMINATED:
		return "Terminated by user"
	case SCHED_S_TASK_NO_VALID_TRIGGERS:
		return "Either the task has no triggers or the existing triggers are disabled or not set"
	case SCHED_S_EVENT_TRIGGER:
		return "Event triggers do not have set run times"
	case SCHED_S_SOME_TRIGGERS_FAILED:
		return "Not all specified triggers will start the task"
	case SCHED_S_BATCH_LOGON_PROBLEM:
		return "May fail to start unless batch logon privilege is enabled for the task principal"
	case SCHED_S_TASK_QUEUED:
		return "Queued"
	default:
		return syscall.Errno(r).Error()
	}
}

type TaskService struct {
	taskServiceObj        *ole.IDispatch
	rootFolderObj         *ole.IDispatch
	isInitialized         bool
	isConnected           bool
	connectedDomain       string
	connectedComputerName string
	connectedUser         string
}

type TaskFolder struct {
	isReleased      bool
	Name            string
	Path            string
	SubFolders      []*TaskFolder
	RegisteredTasks RegisteredTaskCollection
}

// RunningTask is a task that is currently running.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-irunningtask
type RunningTask struct {
	taskObj       *ole.IDispatch
	isReleased    bool
	CurrentAction string    // the name of the current action that the running task is performing
	EnginePID     uint      // the process ID for the engine (process) which is running the task
	InstanceGUID  string    // the GUID identifier for this instance of the task
	Name          string    // the name of the task
	Path          string    // the path to where the task is stored
	State         TaskState // an identifier for the state of the running task
}

// RegisteredTask is a task that is registered in the Task Scheduler database.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-iregisteredtask
type RegisteredTask struct {
	taskObj        *ole.IDispatch
	isReleased     bool
	Name           string // the name of the registered task
	Path           string // the path to where the registered task is stored
	Definition     Definition
	Enabled        bool
	State          TaskState  // the operational state of the registered task
	MissedRuns     uint       // the number of times the registered task has missed a scheduled run
	NextRunTime    time.Time  // the time when the registered task is next scheduled to run
	LastRunTime    time.Time  // the time the registered task was last run
	LastTaskResult TaskResult // the results that were returned the last time the registered task was run
}

// Definition defines all the components of a task, such as the task settings, triggers, actions, and registration information
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-itaskdefinition
type Definition struct {
	Actions          []Action
	Context          string // specifies the security context under which the actions of the task are performed
	Data             string // the data that is associated with the task
	Principal        Principal
	RegistrationInfo RegistrationInfo
	Settings         TaskSettings
	Triggers         []Trigger
	XMLText          string // the XML-formatted definition of the task
}

type Action interface {
	GetID() string
	GetType() TaskActionType
}

// ExecAction is an action that performs a command-line operation. The args
// field can have up to 32 $(ArgX) values, such as '/c $(Arg0) $(Arg1)'.
// This will allow the arguments to be dynamically entered when the task is run.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-iexecaction
type ExecAction struct {
	ID         string
	Path       string
	Args       string
	WorkingDir string
}

// ComHandlerAction is an action that fires a COM handler. Can only be used if TASK_COMPATIBILITY_V2 or above is set.
// The clisd parameter is the CLSID of the COM object that will get instantiated when the action executes, and the
// data parameter is the arguments passed to the COM object.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-icomhandleraction
type ComHandlerAction struct {
	ID      string
	ClassID string
	Data    string
}

// Principal provides security credentials that define the security context for the tasks that are associated with it.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-iprincipal
type Principal struct {
	Name      string        // the name of the principal
	GroupID   string        // the identifier of the user group that is required to run the tasks
	ID        string        // the identifier of the principal
	LogonType TaskLogonType // the security logon method that is required to run the tasks
	RunLevel  TaskRunLevel  // the identifier that is used to specify the privilege level that is required to run the tasks
	UserID    string        // the user identifier that is required to run the tasks
}

// RegistrationInfo provides the administrative information that can be used to describe the task
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-iregistrationinfo
type RegistrationInfo struct {
	Author             string
	Date               time.Time
	Description        string
	Documentation      string
	SecurityDescriptor string
	Source             string
	URI                string
	Version            string
}

// TaskSettings provides the settings that the Task Scheduler service uses to perform the task
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-itasksettings
type TaskSettings struct {
	AllowDemandStart       bool              // indicates that the task can be started by using either the Run command or the Context menu
	AllowHardTerminate     bool              // indicates that the task may be terminated by the Task Scheduler service using TerminateProcess
	Compatibility          TaskCompatibility // indicates which version of Task Scheduler a task is compatible with
	DeleteExpiredTaskAfter string            // the amount of time that the Task Scheduler will wait before deleting the task after it expires
	DontStartOnBatteries   bool              // indicates that the task will not be started if the computer is running on batteries
	Enabled                bool              // indicates that the task is enabled
	TimeLimit              period.Period     // the amount of time that is allowed to complete the task
	Hidden                 bool              // indicates that the task will not be visible in the UI
	IdleSettings
	MultipleInstances TaskInstancesPolicy // defines how the Task Scheduler deals with multiple instances of the task
	NetworkSettings
	Priority                  uint          // the priority level of the task, ranging from 0 - 10, where 0 is the highest priority, and 10 is the lowest. Only applies to ComHandler, Email, and MessageBox actions
	RestartCount              uint          // the number of times that the Task Scheduler will attempt to restart the task
	RestartInterval           period.Period // specifies how long the Task Scheduler will attempt to restart the task
	RunOnlyIfIdle             bool          // indicates that the Task Scheduler will run the task only if the computer is in an idle condition
	RunOnlyIfNetworkAvailable bool          // indicates that the Task Scheduler will run the task only when a network is available
	StartWhenAvailable        bool          // indicates that the Task Scheduler can start the task at any time after its scheduled time has passed
	StopIfGoingOnBatteries    bool          // indicates that the task will be stopped if the computer is going onto batteries
	WakeToRun                 bool          // indicates that the Task Scheduler will wake the computer when it is time to run the task, and keep the computer awake until the task is completed
}

// IdleSettings specifies how the Task Scheduler performs tasks when the computer is in an idle condition.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-iidlesettings
type IdleSettings struct {
	IdleDuration  period.Period // the amount of time that the computer must be in an idle state before the task is run
	RestartOnIdle bool          // whether the task is restarted when the computer cycles into an idle condition more than once
	StopOnIdleEnd bool          // indicates that the Task Scheduler will terminate the task if the idle condition ends before the task is completed
	WaitTimeout   period.Period // the amount of time that the Task Scheduler will wait for an idle condition to occur
}

// NetworkSettings provides the settings that the Task Scheduler service uses to obtain a network profile.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-inetworksettings
type NetworkSettings struct {
	ID   string // a GUID value that identifies a network profile
	Name string // the name of a network profile
}

type Trigger interface {
	GetEnabled() bool
	GetEndBoundary() time.Time
	GetExecutionTimeLimit() period.Period
	GetID() string
	GetRepetitionDuration() period.Period
	GetRepetitionInterval() period.Period
	GetStartBoundary() time.Time
	GetStopAtDurationEnd() bool
	GetType() TaskTriggerType
}

// TaskTrigger provides the common properties that are inherited by all trigger objects.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-itrigger
type TaskTrigger struct {
	Enabled            bool          // indicates whether the trigger is enabled
	EndBoundary        time.Time     // the date and time when the trigger is deactivated
	ExecutionTimeLimit period.Period // the maximum amount of time that the task launched by this trigger is allowed to run
	ID                 string        // the identifier for the trigger
	RepetitionPattern
	StartBoundary time.Time // the date and time when the trigger is activated
}

// RepetitionPattern defines how often the task is run and how long the repetition pattern is repeated after the task is started.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-irepetitionpattern
type RepetitionPattern struct {
	RepetitionDuration period.Period // how long the pattern is repeated
	RepetitionInterval period.Period // the amount of time between each restart of the task. Required if RepetitionDuration is specified. Minimum time is one minute
	StopAtDurationEnd  bool          // indicates if a running instance of the task is stopped at the end of the repetition pattern duration
}

// BootTrigger triggers the task when the computer boots. Only Administrators can create tasks with a BootTrigger.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-iboottrigger
type BootTrigger struct {
	TaskTrigger
	Delay period.Period // indicates the amount of time between when the system is booted and when the task is started
}

// DailyTrigger triggers the task on a daily schedule. For example, the task starts at a specific time every day, every other day, or every third day. The time of day that the task is started is set by StartBoundary, which must be set.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-idailytrigger
type DailyTrigger struct {
	TaskTrigger
	DayInterval DayInterval   // the interval between the days in the schedule
	RandomDelay period.Period // a delay time that is randomly added to the start time of the trigger
}

// EventTrigger triggers the task when a specific event occurs. A maximum of 500 tasks with event subscriptions can be created.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-ieventtrigger
type EventTrigger struct {
	TaskTrigger
	Delay        period.Period     // indicates the amount of time between when the event occurs and when the task is started
	Subscription string            // a query string that identifies the event that fires the trigger
	ValueQueries map[string]string // a collection of named XPath queries. Each query in the collection is applied to the last matching event XML returned from the subscription query
}

// IdleTrigger triggers the task when the computer goes into an idle state. An IdleTrigger will only trigger a task action if the computer goes into an idle state after the start boundary of the trigger
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-iidletrigger
type IdleTrigger struct {
	TaskTrigger
}

// LogonTrigger triggers the task when a specific user logs on. When the Task Scheduler service starts, all logged-on users are enumerated and any tasks registered with logon triggers that match the logged on user are run.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-ilogontrigger
type LogonTrigger struct {
	TaskTrigger
	Delay  period.Period // indicates the amount of time between when the user logs on and when the task is started
	UserID string        // the identifier of the user. If left empty, the trigger will fire when any user logs on
}

// MonthlyDOWTrigger triggers the task on a monthly day-of-week schedule. For example, the task starts on a specific days of the week, weeks of the month, and months of the year. The time of day that the task is started is set by StartBoundary, which must be set.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-imonthlydowtrigger
type MonthlyDOWTrigger struct {
	TaskTrigger
	DaysOfWeek           DayOfWeek     // the days of the week during which the task runs
	MonthsOfYear         Month         // the months of the year during which the task runs
	RandomDelay          period.Period // a delay time that is randomly added to the start time of the trigger
	RunOnLastWeekOfMonth bool          // indicates that the task runs on the last week of the month
	WeeksOfMonth         Week          // the weeks of the month during which the task runs
}

// MonthlyTrigger triggers the task on a monthly schedule. For example, the task starts on specific days of specific months.
// The time of day that the task is started is set by StartBoundary, which must be set.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-imonthlytrigger
type MonthlyTrigger struct {
	TaskTrigger
	DaysOfMonth         DayOfMonth    // the days of the month during which the task runs
	MonthsOfYear        Month         // the months of the year during which the task runs
	RandomDelay         period.Period // a delay time that is randomly added to the start time of the trigger
	RunOnLastDayOfMonth bool          // indicates that the task runs on the last day of the month
}

// RegistrationTrigger triggers the task when the task is registered.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-iregistrationtrigger
type RegistrationTrigger struct {
	TaskTrigger
	Delay period.Period // the amount of time between when the task is registered and when the task is started
}

// SessionStateChangeTrigger triggers the task when a specific user session state changes.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-isessionstatechangetrigger
type SessionStateChangeTrigger struct {
	TaskTrigger
	Delay       period.Period              // indicates how long of a delay takes place before a task is started after a Terminal Server session state change is detected
	StateChange TaskSessionStateChangeType // the kind of Terminal Server session change that would trigger a task launch
	UserId      string                     // the user for the Terminal Server session. When a session state change is detected for this user, a task is started
}

// TimeTrigger triggers the task at a specific time of day. StartBoundary determines when the trigger fires.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-itimetrigger
type TimeTrigger struct {
	TaskTrigger
	RandomDelay period.Period // a delay time that is randomly added to the start time of the trigger
}

// WeeklyTrigger triggers the task on a weekly schedule. The time of day that the task is started is set by StartBoundary, which must be set.
// https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/nn-taskschd-iweeklytrigger
type WeeklyTrigger struct {
	TaskTrigger
	DaysOfWeek   DayOfWeek     // the days of the week in which the task runs
	RandomDelay  period.Period // a delay time that is randomly added to the start time of the trigger
	WeekInterval WeekInterval  // the interval between the weeks in the schedule
}

type CustomTrigger struct {
	TaskTrigger
}

func (t TaskService) IsConnected() bool {
	return t.isConnected
}

func (t TaskService) GetConnectedDomain() string {
	return t.connectedDomain
}

func (t TaskService) GetConnectedComputerName() string {
	return t.connectedComputerName
}

func (t TaskService) GetConnectedUser() string {
	return t.connectedUser
}

func (e ExecAction) GetID() string {
	return e.ID
}

func (ExecAction) GetType() TaskActionType {
	return TASK_ACTION_EXEC
}

func (c ComHandlerAction) GetID() string {
	return c.ID
}

func (ComHandlerAction) GetType() TaskActionType {
	return TASK_ACTION_COM_HANDLER
}

func (t TaskTrigger) GetRepetitionDuration() period.Period {
	return t.RepetitionDuration
}

func (t TaskTrigger) GetEnabled() bool {
	return t.Enabled
}

func (t TaskTrigger) GetEndBoundary() time.Time {
	return t.EndBoundary
}

func (t TaskTrigger) GetExecutionTimeLimit() period.Period {
	return t.ExecutionTimeLimit
}

func (t TaskTrigger) GetID() string {
	return t.ID
}

func (t TaskTrigger) GetRepetitionInterval() period.Period {
	return t.RepetitionInterval
}

func (t TaskTrigger) GetStartBoundary() time.Time {
	return t.StartBoundary
}

func (t TaskTrigger) GetStopAtDurationEnd() bool {
	return t.StopAtDurationEnd
}

func (BootTrigger) GetType() TaskTriggerType {
	return TASK_TRIGGER_BOOT
}

func (DailyTrigger) GetType() TaskTriggerType {
	return TASK_TRIGGER_DAILY
}

func (EventTrigger) GetType() TaskTriggerType {
	return TASK_TRIGGER_EVENT
}

func (IdleTrigger) GetType() TaskTriggerType {
	return TASK_TRIGGER_IDLE
}

func (LogonTrigger) GetType() TaskTriggerType {
	return TASK_TRIGGER_LOGON
}

func (MonthlyDOWTrigger) GetType() TaskTriggerType {
	return TASK_TRIGGER_MONTHLYDOW
}

func (MonthlyTrigger) GetType() TaskTriggerType {
	return TASK_TRIGGER_MONTHLY
}

func (RegistrationTrigger) GetType() TaskTriggerType {
	return TASK_TRIGGER_REGISTRATION
}

func (SessionStateChangeTrigger) GetType() TaskTriggerType {
	return TASK_TRIGGER_SESSION_STATE_CHANGE
}

func (TimeTrigger) GetType() TaskTriggerType {
	return TASK_TRIGGER_TIME
}

func (WeeklyTrigger) GetType() TaskTriggerType {
	return TASK_TRIGGER_WEEKLY
}

func (CustomTrigger) GetType() TaskTriggerType {
	return TASK_TRIGGER_CUSTOM_TRIGGER_01
}
