//go:build windows
// +build windows

package taskmaster

import (
	"errors"
	"fmt"
	"math"
	"time"

	ole "github.com/go-ole/go-ole"
	"github.com/go-ole/go-ole/oleutil"
)

func parseRunningTask(task *ole.IDispatch) (RunningTask, error) {
	var err error

	currentAction, err := oleutil.GetProperty(task, "CurrentAction")
	if err != nil {
		return RunningTask{}, getRunningTaskError(err)
	}
	enginePID, err := oleutil.GetProperty(task, "EnginePid")
	if err != nil {
		return RunningTask{}, getRunningTaskError(err)
	}
	instanceGUID, err := oleutil.GetProperty(task, "InstanceGuid")
	if err != nil {
		return RunningTask{}, getRunningTaskError(err)
	}
	name, err := oleutil.GetProperty(task, "Name")
	if err != nil {
		return RunningTask{}, getRunningTaskError(err)
	}
	path, err := oleutil.GetProperty(task, "Path")
	if err != nil {
		return RunningTask{}, getRunningTaskError(err)
	}
	state, err := oleutil.GetProperty(task, "State")
	if err != nil {
		return RunningTask{}, getRunningTaskError(err)
	}

	runningTask := RunningTask{
		taskObj:       task,
		CurrentAction: currentAction.ToString(),
		EnginePID:     uint(enginePID.Val),
		InstanceGUID:  instanceGUID.ToString(),
		Name:          name.ToString(),
		Path:          path.ToString(),
		State:         TaskState(state.Val),
	}

	return runningTask, nil
}

func parseRegisteredTask(task *ole.IDispatch) (RegisteredTask, string, error) {
	var err error

	nameVar, err := oleutil.GetProperty(task, "Name")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	name := nameVar.ToString()

	pathVar, err := oleutil.GetProperty(task, "Path")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	path := pathVar.ToString()

	enabledVar, err := oleutil.GetProperty(task, "Enabled")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	enabled := enabledVar.Value().(bool)

	stateVar, err := oleutil.GetProperty(task, "State")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	state := TaskState(stateVar.Val)

	missedRunsVar, err := oleutil.GetProperty(task, "NumberOfMissedRuns")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	missedRuns := uint(missedRunsVar.Val)

	nextRunTimeVar, err := oleutil.GetProperty(task, "NextRunTime")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	nextRunTime := variantTimeOrZero(nextRunTimeVar)

	lastRunTimeVar, err := oleutil.GetProperty(task, "LastRunTime")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	lastRunTime := variantTimeOrZero(lastRunTimeVar)

	lastTaskResultVar, err := oleutil.GetProperty(task, "LastTaskResult")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	lastTaskResult := TaskResult(lastTaskResultVar.Val)

	definitionVar, err := oleutil.GetProperty(task, "Definition")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	definition := definitionVar.ToIDispatch()
	defer definition.Release()
	actionsVar, err := oleutil.GetProperty(definition, "Actions")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	actions := actionsVar.ToIDispatch()
	defer actions.Release()

	contextVar, err := oleutil.GetProperty(actions, "Context")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	context := contextVar.ToString()

	var taskActions []Action
	err = oleutil.ForEach(actions, func(v *ole.VARIANT) error {
		action := v.ToIDispatch()
		defer action.Release()

		taskAction, err := parseTaskAction(action)
		if err != nil {
			return err
		}

		taskActions = append(taskActions, taskAction)

		return nil
	})
	if err != nil {
		return RegisteredTask{}, path, fmt.Errorf("error parsing IAction object: %v", err)
	}

	principalVar, err := oleutil.GetProperty(definition, "Principal")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	principal := principalVar.ToIDispatch()

	xmlTextVar, err := oleutil.GetProperty(definition, "XmlText")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	xmlText := xmlTextVar.ToString()

	defer principal.Release()
	taskPrincipal := parsePrincipal(principal)

	regInfoVar, err := oleutil.GetProperty(definition, "RegistrationInfo")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	regInfo := regInfoVar.ToIDispatch()
	defer regInfo.Release()
	registrationInfo, err := parseRegistrationInfo(regInfo)
	if err != nil {
		return RegisteredTask{}, path, fmt.Errorf("error parsing IRegistrationInfo object: %v", err)
	}

	settingsVar, err := oleutil.GetProperty(definition, "Settings")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	settings := settingsVar.ToIDispatch()
	defer settings.Release()
	taskSettings, err := parseTaskSettings(settings)
	if err != nil {
		return RegisteredTask{}, path, fmt.Errorf("error parsing ITaskSettings object: %v", err)
	}

	triggersVar, err := oleutil.GetProperty(definition, "Triggers")
	if err != nil {
		return RegisteredTask{}, "", err
	}
	triggers := triggersVar.ToIDispatch()
	defer triggers.Release()

	var taskTriggers []Trigger
	err = oleutil.ForEach(triggers, func(v *ole.VARIANT) error {
		trigger := v.ToIDispatch()
		defer trigger.Release()

		taskTrigger, err := parseTaskTrigger(trigger)
		if err != nil {
			return err
		}
		taskTriggers = append(taskTriggers, taskTrigger)

		return nil
	})
	if err != nil {
		return RegisteredTask{}, path, fmt.Errorf("error parsing ITrigger object: %v", err)
	}

	taskDef := Definition{
		Actions:          taskActions,
		Context:          context,
		Principal:        taskPrincipal,
		Settings:         *taskSettings,
		RegistrationInfo: *registrationInfo,
		Triggers:         taskTriggers,
		XMLText:          xmlText,
	}

	registeredTask := RegisteredTask{
		taskObj:        task,
		Name:           name,
		Path:           path,
		Definition:     taskDef,
		Enabled:        enabled,
		State:          state,
		MissedRuns:     missedRuns,
		NextRunTime:    nextRunTime,
		LastRunTime:    lastRunTime,
		LastTaskResult: lastTaskResult,
	}

	return registeredTask, path, nil
}

func parseTaskAction(action *ole.IDispatch) (Action, error) {
	id := oleutil.MustGetProperty(action, "Id").ToString()
	actionType := TaskActionType(oleutil.MustGetProperty(action, "Type").Val)

	switch actionType {
	case TASK_ACTION_EXEC:
		args := oleutil.MustGetProperty(action, "Arguments").ToString()
		path := oleutil.MustGetProperty(action, "Path").ToString()
		workingDir := oleutil.MustGetProperty(action, "WorkingDirectory").ToString()

		execAction := ExecAction{
			ID:         id,
			Path:       path,
			Args:       args,
			WorkingDir: workingDir,
		}

		return execAction, nil
	case TASK_ACTION_COM_HANDLER:
		classID := oleutil.MustGetProperty(action, "ClassId").ToString()
		data := oleutil.MustGetProperty(action, "Data").ToString()

		comHandlerAction := ComHandlerAction{
			ID:      id,
			ClassID: classID,
			Data:    data,
		}

		return comHandlerAction, nil
	default:
		return nil, errors.New("unsupported IAction type")
	}
}

func parsePrincipal(principleObj *ole.IDispatch) Principal {
	name := oleutil.MustGetProperty(principleObj, "DisplayName").ToString()
	groupID := oleutil.MustGetProperty(principleObj, "GroupId").ToString()
	id := oleutil.MustGetProperty(principleObj, "Id").ToString()
	logonType := TaskLogonType(oleutil.MustGetProperty(principleObj, "LogonType").Val)
	runLevel := TaskRunLevel(oleutil.MustGetProperty(principleObj, "RunLevel").Val)
	userID := oleutil.MustGetProperty(principleObj, "UserId").ToString()

	principle := Principal{
		Name:      name,
		GroupID:   groupID,
		ID:        id,
		LogonType: logonType,
		RunLevel:  runLevel,
		UserID:    userID,
	}

	return principle
}

func parseRegistrationInfo(regInfo *ole.IDispatch) (*RegistrationInfo, error) {
	author := oleutil.MustGetProperty(regInfo, "Author").ToString()
	date, err := TaskDateToTime(oleutil.MustGetProperty(regInfo, "Date").ToString())
	if err != nil {
		return nil, fmt.Errorf("error parsing Date field: %v", err)
	}
	description := oleutil.MustGetProperty(regInfo, "Description").ToString()
	documentation := oleutil.MustGetProperty(regInfo, "Documentation").ToString()
	securityDescriptor := oleutil.MustGetProperty(regInfo, "SecurityDescriptor").ToString()
	source := oleutil.MustGetProperty(regInfo, "Source").ToString()
	uri := oleutil.MustGetProperty(regInfo, "URI").ToString()
	version := oleutil.MustGetProperty(regInfo, "Version").ToString()

	registrationInfo := &RegistrationInfo{
		Author:             author,
		Date:               date,
		Description:        description,
		Documentation:      documentation,
		SecurityDescriptor: securityDescriptor,
		Source:             source,
		URI:                uri,
		Version:            version,
	}

	return registrationInfo, nil
}

func parseTaskSettings(settings *ole.IDispatch) (*TaskSettings, error) {
	allowDemandStart := oleutil.MustGetProperty(settings, "AllowDemandStart").Value().(bool)
	allowHardTerminate := oleutil.MustGetProperty(settings, "AllowHardTerminate").Value().(bool)
	compatibility := TaskCompatibility(oleutil.MustGetProperty(settings, "Compatibility").Val)
	deleteExpiredTaskAfter := oleutil.MustGetProperty(settings, "DeleteExpiredTaskAfter").ToString()
	dontStartOnBatteries := oleutil.MustGetProperty(settings, "DisallowStartIfOnBatteries").Value().(bool)
	enabled := oleutil.MustGetProperty(settings, "Enabled").Value().(bool)
	timeLimit, err := StringToPeriod(oleutil.MustGetProperty(settings, "ExecutionTimeLimit").ToString())
	if err != nil {
		return nil, fmt.Errorf("error parsing ExecutionTimeLimit field: %v", err)
	}
	hidden := oleutil.MustGetProperty(settings, "Hidden").Value().(bool)

	idleSettings := oleutil.MustGetProperty(settings, "IdleSettings").ToIDispatch()
	defer idleSettings.Release()
	idleDuration, err := StringToPeriod(oleutil.MustGetProperty(idleSettings, "IdleDuration").ToString())
	if err != nil {
		return nil, fmt.Errorf("error parsing IdleDuration field: %v", err)
	}
	restartOnIdle := oleutil.MustGetProperty(idleSettings, "RestartOnIdle").Value().(bool)
	stopOnIdleEnd := oleutil.MustGetProperty(idleSettings, "StopOnIdleEnd").Value().(bool)
	waitTimeOut, err := StringToPeriod(oleutil.MustGetProperty(idleSettings, "WaitTimeout").ToString())
	if err != nil {
		return nil, fmt.Errorf("error parsing WaitTimeout field: %v", err)
	}

	multipleInstances := TaskInstancesPolicy(oleutil.MustGetProperty(settings, "MultipleInstances").Val)

	networkSettings := oleutil.MustGetProperty(settings, "NetworkSettings").ToIDispatch()
	defer networkSettings.Release()
	id := oleutil.MustGetProperty(networkSettings, "Id").ToString()
	name := oleutil.MustGetProperty(networkSettings, "Name").ToString()

	priority := uint(oleutil.MustGetProperty(settings, "Priority").Val)
	restartCount := uint(oleutil.MustGetProperty(settings, "RestartCount").Val)
	restartInterval, err := StringToPeriod(oleutil.MustGetProperty(settings, "RestartInterval").ToString())
	if err != nil {
		return nil, fmt.Errorf("error parsing RestartInterval field: %v", err)
	}
	runOnlyIfIdle := oleutil.MustGetProperty(settings, "RunOnlyIfIdle").Value().(bool)
	runOnlyIfNetworkAvailable := oleutil.MustGetProperty(settings, "RunOnlyIfNetworkAvailable").Value().(bool)
	startWhenAvailable := oleutil.MustGetProperty(settings, "StartWhenAvailable").Value().(bool)
	stopIfGoingOnBatteries := oleutil.MustGetProperty(settings, "StopIfGoingOnBatteries").Value().(bool)
	wakeToRun := oleutil.MustGetProperty(settings, "WakeToRun").Value().(bool)

	idleTaskSettings := IdleSettings{
		IdleDuration:  idleDuration,
		RestartOnIdle: restartOnIdle,
		StopOnIdleEnd: stopOnIdleEnd,
		WaitTimeout:   waitTimeOut,
	}

	networkTaskSettings := NetworkSettings{
		ID:   id,
		Name: name,
	}

	taskSettings := &TaskSettings{
		AllowDemandStart:          allowDemandStart,
		AllowHardTerminate:        allowHardTerminate,
		Compatibility:             compatibility,
		DeleteExpiredTaskAfter:    deleteExpiredTaskAfter,
		DontStartOnBatteries:      dontStartOnBatteries,
		Enabled:                   enabled,
		TimeLimit:                 timeLimit,
		Hidden:                    hidden,
		IdleSettings:              idleTaskSettings,
		MultipleInstances:         multipleInstances,
		NetworkSettings:           networkTaskSettings,
		Priority:                  priority,
		RestartCount:              restartCount,
		RestartInterval:           restartInterval,
		RunOnlyIfIdle:             runOnlyIfIdle,
		RunOnlyIfNetworkAvailable: runOnlyIfNetworkAvailable,
		StartWhenAvailable:        startWhenAvailable,
		StopIfGoingOnBatteries:    stopIfGoingOnBatteries,
		WakeToRun:                 wakeToRun,
	}

	return taskSettings, nil
}

func parseTaskTrigger(trigger *ole.IDispatch) (Trigger, error) {
	enabled := oleutil.MustGetProperty(trigger, "Enabled").Value().(bool)
	endBoundary, err := TaskDateToTime(oleutil.MustGetProperty(trigger, "EndBoundary").ToString())
	if err != nil {
		return nil, fmt.Errorf("error parsing EndBoundary field: %v", err)
	}
	executionTimeLimit, err := StringToPeriod(oleutil.MustGetProperty(trigger, "ExecutionTimeLimit").ToString())
	if err != nil {
		return nil, fmt.Errorf("error parsing ExecutionTimeLimit field: %v", err)
	}
	id := oleutil.MustGetProperty(trigger, "Id").ToString()

	repetition := oleutil.MustGetProperty(trigger, "Repetition").ToIDispatch()
	defer repetition.Release()
	duration, err := StringToPeriod(oleutil.MustGetProperty(repetition, "Duration").ToString())
	if err != nil {
		return nil, fmt.Errorf("error parsing Duration field: %v", err)
	}
	interval, err := StringToPeriod(oleutil.MustGetProperty(repetition, "Interval").ToString())
	if err != nil {
		return nil, fmt.Errorf("error parsing Interval field: %v", err)
	}
	stopAtDurationEnd := oleutil.MustGetProperty(repetition, "StopAtDurationEnd").Value().(bool)

	startBoundary, err := TaskDateToTime(oleutil.MustGetProperty(trigger, "StartBoundary").ToString())
	if err != nil {
		return nil, fmt.Errorf("error parsing StartBoundary field: %v", err)
	}
	triggerType := TaskTriggerType(oleutil.MustGetProperty(trigger, "Type").Val)

	taskTriggerObj := TaskTrigger{
		Enabled:            enabled,
		EndBoundary:        endBoundary,
		ExecutionTimeLimit: executionTimeLimit,
		ID:                 id,
		RepetitionPattern: RepetitionPattern{
			RepetitionDuration: duration,
			RepetitionInterval: interval,
			StopAtDurationEnd:  stopAtDurationEnd,
		},
		StartBoundary: startBoundary,
	}

	switch triggerType {
	case TASK_TRIGGER_BOOT:
		delay, err := StringToPeriod(oleutil.MustGetProperty(trigger, "Delay").ToString())
		if err != nil {
			return nil, fmt.Errorf("error parsing IBootTrigger object: error parsing Delay field: %v", err)
		}

		bootTrigger := BootTrigger{
			TaskTrigger: taskTriggerObj,
			Delay:       delay,
		}

		return bootTrigger, nil
	case TASK_TRIGGER_DAILY:
		daysInterval := DayInterval(oleutil.MustGetProperty(trigger, "DaysInterval").Val)
		randomDelay, err := StringToPeriod(oleutil.MustGetProperty(trigger, "RandomDelay").ToString())
		if err != nil {
			return nil, fmt.Errorf("error parsing IDailyTrigger object: error parsing RandomDelay field: %v", err)
		}

		dailyTrigger := DailyTrigger{
			TaskTrigger: taskTriggerObj,
			DayInterval: daysInterval,
			RandomDelay: randomDelay,
		}

		return dailyTrigger, nil
	case TASK_TRIGGER_EVENT:
		delay, err := StringToPeriod(oleutil.MustGetProperty(trigger, "Delay").ToString())
		if err != nil {
			return nil, fmt.Errorf("error parsing IEventTrigger object: error parsing Delay field: %v", err)
		}
		subscription := oleutil.MustGetProperty(trigger, "Subscription").ToString()
		valueQueriesObj := oleutil.MustGetProperty(trigger, "ValueQueries").ToIDispatch()
		defer valueQueriesObj.Release()

		valQueryMap := make(map[string]string)
		oleutil.ForEach(valueQueriesObj, func(v *ole.VARIANT) error {
			valueQuery := v.ToIDispatch()
			defer valueQuery.Release()

			name := oleutil.MustGetProperty(valueQuery, "Name").ToString()
			value := oleutil.MustGetProperty(valueQuery, "Value").ToString()

			valQueryMap[name] = value

			return nil
		})

		eventTrigger := EventTrigger{
			TaskTrigger:  taskTriggerObj,
			Delay:        delay,
			Subscription: subscription,
			ValueQueries: valQueryMap,
		}

		return eventTrigger, nil
	case TASK_TRIGGER_IDLE:
		idleTrigger := IdleTrigger{
			TaskTrigger: taskTriggerObj,
		}

		return idleTrigger, nil
	case TASK_TRIGGER_LOGON:
		delay, err := StringToPeriod(oleutil.MustGetProperty(trigger, "Delay").ToString())
		if err != nil {
			return nil, fmt.Errorf("error parsing ILogonTrigger object: error parsing Delay field: %v", err)
		}
		userID := oleutil.MustGetProperty(trigger, "UserId").ToString()

		logonTrigger := LogonTrigger{
			TaskTrigger: taskTriggerObj,
			Delay:       delay,
			UserID:      userID,
		}

		return logonTrigger, nil
	case TASK_TRIGGER_MONTHLYDOW:
		daysOfWeek := DayOfWeek(oleutil.MustGetProperty(trigger, "DaysOfWeek").Val)
		monthsOfYear := Month(oleutil.MustGetProperty(trigger, "MonthsOfYear").Val)
		randomDelay, err := StringToPeriod(oleutil.MustGetProperty(trigger, "RandomDelay").ToString())
		if err != nil {
			return nil, fmt.Errorf("error parsing IMonthlyDOWTrigger object: error parsing RandomDelay field: %v", err)
		}
		runOnLastWeekOfMonth := oleutil.MustGetProperty(trigger, "RunOnLastWeekOfMonth").Value().(bool)
		weeksOfMonth := Week(oleutil.MustGetProperty(trigger, "WeeksOfMonth").Val)

		monthlyDOWTrigger := MonthlyDOWTrigger{
			TaskTrigger:          taskTriggerObj,
			DaysOfWeek:           daysOfWeek,
			MonthsOfYear:         monthsOfYear,
			RandomDelay:          randomDelay,
			RunOnLastWeekOfMonth: runOnLastWeekOfMonth,
			WeeksOfMonth:         weeksOfMonth,
		}

		return monthlyDOWTrigger, nil
	case TASK_TRIGGER_MONTHLY:
		daysOfMonth := DayOfMonth(oleutil.MustGetProperty(trigger, "DaysOfMonth").Val)
		monthsOfYear := Month(oleutil.MustGetProperty(trigger, "MonthsOfYear").Val)
		randomDelay, err := StringToPeriod(oleutil.MustGetProperty(trigger, "RandomDelay").ToString())
		if err != nil {
			return nil, fmt.Errorf("error parsing IMonthlyTrigger object: error parsing RandomDelay field: %v", err)
		}
		runOnLastDayOfMonth := oleutil.MustGetProperty(trigger, "RunOnLastDayOfMonth").Value().(bool)

		monthlyTrigger := MonthlyTrigger{
			TaskTrigger:         taskTriggerObj,
			DaysOfMonth:         daysOfMonth,
			MonthsOfYear:        monthsOfYear,
			RandomDelay:         randomDelay,
			RunOnLastDayOfMonth: runOnLastDayOfMonth,
		}

		return monthlyTrigger, nil
	case TASK_TRIGGER_REGISTRATION:
		delay, err := StringToPeriod(oleutil.MustGetProperty(trigger, "Delay").ToString())
		if err != nil {
			return nil, fmt.Errorf("error parsing IRegistrationTrigger object: error parsing Delay field: %v", err)
		}
		registrationTrigger := RegistrationTrigger{
			TaskTrigger: taskTriggerObj,
			Delay:       delay,
		}

		return registrationTrigger, nil
	case TASK_TRIGGER_TIME:
		randomDelay, err := StringToPeriod(oleutil.MustGetProperty(trigger, "RandomDelay").ToString())
		if err != nil {
			return nil, fmt.Errorf("error parsing ITimeTrigger object: error parsing RandomDelay field: %v", err)
		}
		timetrigger := TimeTrigger{
			TaskTrigger: taskTriggerObj,
			RandomDelay: randomDelay,
		}

		return timetrigger, nil
	case TASK_TRIGGER_WEEKLY:
		daysOfWeek := DayOfWeek(oleutil.MustGetProperty(trigger, "DaysOfWeek").Val)
		randomDelay, err := StringToPeriod(oleutil.MustGetProperty(trigger, "RandomDelay").ToString())
		if err != nil {
			return nil, fmt.Errorf("error parsing IWeeklyTrigger object: error parsing RandomDelay field: %v", err)
		}
		weeksInterval := WeekInterval(oleutil.MustGetProperty(trigger, "WeeksInterval").Val)

		weeklyTrigger := WeeklyTrigger{
			TaskTrigger:  taskTriggerObj,
			DaysOfWeek:   daysOfWeek,
			RandomDelay:  randomDelay,
			WeekInterval: weeksInterval,
		}

		return weeklyTrigger, nil
	case TASK_TRIGGER_SESSION_STATE_CHANGE:
		delay, err := StringToPeriod(oleutil.MustGetProperty(trigger, "Delay").ToString())
		if err != nil {
			return nil, fmt.Errorf("error parsing ISessionStateChangeTrigger object: error parsing RandomDelay field: %v", err)
		}
		stateChange := TaskSessionStateChangeType(oleutil.MustGetProperty(trigger, "StateChange").Val)
		userID := oleutil.MustGetProperty(trigger, "UserId").ToString()

		sessionStateChangeTrigger := SessionStateChangeTrigger{
			TaskTrigger: taskTriggerObj,
			Delay:       delay,
			StateChange: stateChange,
			UserId:      userID,
		}

		return sessionStateChangeTrigger, nil
	case TASK_TRIGGER_CUSTOM_TRIGGER_01:
		customTrigger := CustomTrigger{
			TaskTrigger: taskTriggerObj,
		}

		return customTrigger, nil

	default:
		return nil, errors.New("unsupported ITrigger type")
	}
}

var oleAutomationEpoch = time.Date(1899, time.December, 30, 0, 0, 0, 0, time.UTC)

func variantTimeOrZero(v *ole.VARIANT) time.Time {
	if v == nil || v.VT != ole.VT_DATE {
		return time.Time{}
	}

	return oleDateToTime(math.Float64frombits(uint64(v.Val)))
}

func oleDateToTime(value float64) time.Time {
	if value == 0 || math.IsNaN(value) || math.IsInf(value, 0) {
		return time.Time{}
	}

	const day = 24 * time.Hour
	days, frac := math.Modf(value)
	dayDuration := time.Duration(int64(days)) * day
	fracDuration := time.Duration(frac * float64(day))

	return oleAutomationEpoch.Add(dayDuration + fracDuration)
}
