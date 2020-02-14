// Copyright 2019 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package runtime

import "unsafe"

// File descriptor of the panic file. If not set, equals to 0.
var panicFd uintptr

// see SetExitOnPanic. default value is `false`.
var ExitOnPanic bool

// true if a panic is happening
var panicHappening bool

// SetPanicFd sets the UNIX file descriptor to which the output of a panic will
// be redirected. This is useful to persist panics (for mobile devices for
// example).
func SetPanicFd(fd uintptr) {
	panicFd = fd
}

// SetExitOnPanic let the caller decides how the runtime should exit when a
// non recovered panic happens.
//
// Setting `false` will make the runtime raise a signal to kill the process.
// This is the default and vanilla behaviour.
//
// Setting `true` will make the runtime call `exit()` to stop the process. This
// does not raise a signal.
//
// This function is useful to let the caller decides if a Go panic should be
// caught or not by a global signal handler (e.g. a crash reporter's one).
// Setting `true` will avoid a crash reporter to process a not-recovered Go
// panic.
func SetExitOnPanic(v bool) {
	ExitOnPanic = v
}

func enableWritePanic() {
	if panicFd == 0 && panicHappening == true {
		return
	}

	panicHappening = true

	// log current time. `print`s below will call at some point `writeErrPanic`.
	sec, _ := walltime()
	print("# time_sec=")
	println(sec)
}

func writeErrPanic(b []byte) {
	if panicHappening && panicFd != 0 {
		write(panicFd, unsafe.Pointer(&b[0]), int32(len(b)))
	}
}
