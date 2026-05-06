//go:build windows
// +build windows

package taskmaster

import (
	"errors"
	"time"
)

var defaultTime = time.Time{}

func validateDefinition(def Definition) error {
	var err error

	if def.Actions == nil {
		return ErrNoActions
	}
	if err = validateActions(def.Actions); err != nil {
		return err
	}
	if err = validateTriggers(def.Triggers); err != nil {
		return err
	}

	if def.Principal.UserID != "" && def.Principal.GroupID != "" {
		return ErrInvalidPrincipal
	}

	return nil
}

func validateActions(actions []Action) error {
	for _, action := range actions {
		switch action.GetType() {
		case TASK_ACTION_EXEC:
			return nil
		case TASK_ACTION_COM_HANDLER:
			return nil
		default:
			return errors.New("invalid task action type")
		}
	}

	return nil
}

func validateTriggers(triggers []Trigger) error {
	for _, trigger := range triggers {
		switch t := trigger.(type) {
		case BootTrigger:
			return nil
		case DailyTrigger:
			if t.GetStartBoundary() == defaultTime {
				return errors.New("invalid DailyTrigger: StartBoundary is required")
			} else if t.DayInterval > EveryOtherDay {
				return errors.New("invalid DailyTrigger: invalid DayInterval")
			}

			return nil
		case EventTrigger:
			if t.Subscription == "" {
				return errors.New("invalid EventTrigger: Subscription is required")
			}

			return nil
		case IdleTrigger:
			return nil
		case LogonTrigger:
			return nil
		case MonthlyDOWTrigger:
			if t.GetStartBoundary() == defaultTime {
				return errors.New("invalid MonthlyDOWTrigger: StartBoundary is required")
			} else if t.DaysOfWeek == 0 {
				return errors.New("invalid MonthlyDOWTrigger: DaysOfWeek is required")
			} else if t.DaysOfWeek > AllDays {
				return errors.New("invalid MonthlyDOWTrigger: invalid DaysOfWeek")
			} else if t.MonthsOfYear == 0 {
				return errors.New("invalid MonthlyDOWTrigger: MonthsOfYear is required")
			} else if t.MonthsOfYear > AllMonths {
				return errors.New("invalid MonthlyDOWTrigger: invalid MonthsOfYear")
			} else if t.WeeksOfMonth == 0 {
				return errors.New("invalid MonthlyDOWTrigger: WeeksOfMonth is required")
			} else if t.WeeksOfMonth > AllWeeks {
				return errors.New("invalid MonthlyDOWTrigger: invalid WeeksOfMonth")
			}

			return nil
		case MonthlyTrigger:
			if t.GetStartBoundary() == defaultTime {
				return errors.New("invalid MonthlyTrigger: StartBoundary is required")
			} else if t.DaysOfMonth == 0 {
				return errors.New("invalid MonthlyTrigger: DaysOfMonth is required")
			} else if t.DaysOfMonth > AllDaysOfMonth {
				return errors.New("invalid MonthlyTrigger: invalid DaysOfMonth")
			} else if t.MonthsOfYear == 0 {
				return errors.New("invalid MonthlyTrigger: MonthsOfYear is required")
			} else if t.MonthsOfYear > AllMonths {
				return errors.New("invalid MonthlyTrigger: invalid MonthsOfYear")
			}

			return nil
		case RegistrationTrigger:
			return nil
		case SessionStateChangeTrigger:
			return nil
		case TimeTrigger:
			return nil
		case WeeklyTrigger:
			if t.GetStartBoundary() == defaultTime {
				return errors.New("invalid WeeklyTrigger: StartBoundary is required")
			} else if t.DaysOfWeek == 0 {
				return errors.New("invalid WeeklyTrigger: DaysOfWeek is required")
			} else if t.DaysOfWeek > AllDays {
				return errors.New("invalid WeeklyTrigger: invalid DaysOfWeek")
			} else if t.WeekInterval == 0 {
				return errors.New("invalid WeeklyTrigger: WeekInterval is required")
			} else if t.WeekInterval > EveryOtherWeek {
				return errors.New("invalid WeeklyTrigger: invalid WeekInterval")
			}

			return nil
		default:
			return errors.New("invalid task trigger type")
		}
	}
	return nil
}
