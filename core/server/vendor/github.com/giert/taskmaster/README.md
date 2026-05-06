======================== GIERT'S FORK NOTES ========================

Updates dependencies and implements an interval fix from pull request #22 in the original project.

The updates to go-ole are especially important! Don't wast time on strange errors in production like I did...

The library has a tendency to panic over even parsing errors. My long-term goal is to gradually transition to returning normal errors in more cases.

======================= /GIERT'S FORK NOTES> =======================

[![Build status](https://ci.appveyor.com/api/projects/status/b3gllq093c8ex5ew?svg=true)](https://ci.appveyor.com/project/capnspacehook/taskmaster)
[![Go Report Card](https://goreportcard.com/badge/github.com/capnspacehook/taskmaster)](https://goreportcard.com/report/github.com/capnspacehook/taskmaster)
[![PkgGoDev](https://pkg.go.dev/badge/github.com/capnspacehook/taskmaster)](https://pkg.go.dev/github.com/capnspacehook/taskmaster)

# taskmaster
Windows Task Scheduler Library for Go

**NOTE:** the API is *not* stable, I reserve the right to change it before v1.0. Task Scheduler is complex, and it is difficult to create a sane, useable interface for it. I would highly encourage you to make use of Go modules and pin a specific commit.

![taskmaster villain](img/taskmaster.jpg "Taskmaster")

# What is taskmaster?

Taskmaster is a library for managing Scheduled Tasks in Windows. It allows you to easily create, modify, delete, execute, kill, and view scheduled tasks, on your local machine or on a remote one. It provides much more speed and power than using the native Task Scheduler GUI in Windows, and the Scheduled Task Powershell cmdlets.

Because taskmaster interfaces directly with Task Scheduler COM objects, it allows you to do things you can't do with the Task Scheduler GUI or Powershell cmdlets. COM handler task actions can be viewed, manipulated, and created, more settings can be used when creating or modifying scheduled tasks, etc. Taskmaster exposes the full potential of Windows Scheduled Tasks in a clean, simple interface.

# Documentation

As I was researching the Task Scheduler COM interface more and more, I quickly realized just how complex and confusing all the different parts of Task Scheduler are. So I set out to concisely copy the documentation from MSDN into taskmaster, but also consolidate it and add information that is buried in the depths of MSDN. This should make using both taskmaster and the existing Task Scheduler tools easier, having a ton of information and links to Task Scheduler internals available via GoDocs. If you find info that I missed, feel free to submit an issue or better yet open a PR :)

There are a lot of hidden gotchas and quirks within Task Scheduler, so I would *highly* recommend perusing the official docs before attempting really anything with this library on [MSDN](https://docs.microsoft.com/en-us/windows/win32/taskschd/task-scheduler-start-page).
