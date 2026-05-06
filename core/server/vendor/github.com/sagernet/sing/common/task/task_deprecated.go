package task

import "context"

// Deprecated: Use Group instead
func Run(ctx context.Context, tasks ...func() error) error {
	var group Group
	for _, task := range tasks {
		currentTask := task
		group.Append0(func(ctx context.Context) error {
			return currentTask()
		})
	}
	return group.Run(ctx)
}

// Deprecated: Use Group instead
func Any(ctx context.Context, tasks ...func(ctx context.Context) error) error {
	var group Group
	for _, task := range tasks {
		currentTask := task
		group.Append0(func(ctx context.Context) error {
			return currentTask(ctx)
		})
	}
	group.FastFail()
	return group.Run(ctx)
}
