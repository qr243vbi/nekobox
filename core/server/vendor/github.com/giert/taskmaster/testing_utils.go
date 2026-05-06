//go:build windows
// +build windows

package taskmaster

import (
	"strings"
	"testing"
)

const (
	testTaskFolderName = "TaskmasterTests"
	testTaskRoot       = `\` + testTaskFolderName
)

func setupTaskService(t *testing.T) *TaskService {
	t.Helper()

	taskService, err := Connect()
	if err != nil {
		t.Fatalf("failed to connect to Task Scheduler: %v", err)
	}

	resetTestFolder(t, &taskService)

	t.Cleanup(func() {
		resetTestFolder(t, &taskService)
		taskService.Disconnect()
	})

	return &taskService
}

func resetTestFolder(t *testing.T, taskService *TaskService) {
	t.Helper()

	if taskService.taskFolderExist(testTaskRoot) {
		if _, err := taskService.DeleteFolder(testTaskRoot, true); err != nil {
			t.Fatalf("failed to delete %s: %v", testTaskRoot, err)
		}
	}
}

func testTaskPath(parts ...string) string {
	if len(parts) == 0 {
		return testTaskRoot
	}

	cleaned := make([]string, 0, len(parts))
	for _, part := range parts {
		cleaned = append(cleaned, strings.Trim(part, "\\"))
	}

	return testTaskRoot + `\` + strings.Join(cleaned, `\`)
}

func createTestTask(taskSvc *TaskService) RegisteredTask {
	newTaskDef := taskSvc.NewTaskDefinition()
	newTaskDef.AddAction(ExecAction{
		Path: "cmd.exe",
		Args: "/c timeout $(Arg0)",
	})
	newTaskDef.Settings.MultipleInstances = TASK_INSTANCES_PARALLEL

	task, _, err := taskSvc.CreateTask(testTaskPath("TestTask"), newTaskDef, true)
	if err != nil {
		panic(err)
	}

	return task
}

func withRegisteredTask(t *testing.T, taskSvc *TaskService, path string, fn func(RegisteredTask)) {
	t.Helper()

	task, err := taskSvc.GetRegisteredTask(path)
	if err != nil {
		t.Fatalf("failed to get registered task %s: %v", path, err)
	}
	defer task.Release()

	fn(task)
}

func requireActionCount(t *testing.T, task RegisteredTask, expected int) {
	t.Helper()

	if len(task.Definition.Actions) != expected {
		t.Fatalf("expected %d actions, got %d", expected, len(task.Definition.Actions))
	}
}

func requireTriggerCount(t *testing.T, task RegisteredTask, expected int) {
	t.Helper()

	if len(task.Definition.Triggers) != expected {
		t.Fatalf("expected %d triggers, got %d", expected, len(task.Definition.Triggers))
	}
}

func requireActionAt[T Action](t *testing.T, task RegisteredTask, idx int) T {
	t.Helper()

	if idx >= len(task.Definition.Actions) {
		t.Fatalf("expected action at index %d, only %d actions available", idx, len(task.Definition.Actions))
	}

	action, ok := task.Definition.Actions[idx].(T)
	if !ok {
		var zero T
		t.Fatalf("expected action %T at index %d, got %T", zero, idx, task.Definition.Actions[idx])
	}

	return action
}

func requireTriggerAt[T Trigger](t *testing.T, task RegisteredTask, idx int) T {
	t.Helper()

	if idx >= len(task.Definition.Triggers) {
		t.Fatalf("expected trigger at index %d, only %d triggers available", idx, len(task.Definition.Triggers))
	}

	trigger, ok := task.Definition.Triggers[idx].(T)
	if !ok {
		var zero T
		t.Fatalf("expected trigger %T at index %d, got %T", zero, idx, task.Definition.Triggers[idx])
	}

	return trigger
}
