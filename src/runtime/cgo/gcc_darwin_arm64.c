// Copyright 2014 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <string.h> /* for strerror */
#include <sys/param.h>
#include <unistd.h>
#include <stdlib.h>

#include "libcgo.h"
#include "libcgo_unix.h"

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFString.h>
#endif

static void *threadentry(void*);
static void (*setg_gcc)(void*);

void
_cgo_sys_thread_start(ThreadStart *ts)
{
	pthread_attr_t attr;
	sigset_t ign, oset;
	pthread_t p;
	size_t size;
	int err;

	//fprintf(stderr, "runtime/cgo: _cgo_sys_thread_start: fn=%p, g=%p\n", ts->fn, ts->g); // debug
	sigfillset(&ign);
	pthread_sigmask(SIG_SETMASK, &ign, &oset);

	pthread_attr_init(&attr);
	size = 0;
	pthread_attr_getstacksize(&attr, &size);
	// Leave stacklo=0 and set stackhi=size; mstart will do the rest.
	ts->g->stackhi = size;
	err = _cgo_try_pthread_create(&p, &attr, threadentry, ts);

	pthread_sigmask(SIG_SETMASK, &oset, nil);

	if (err != 0) {
		fprintf(stderr, "runtime/cgo: pthread_create failed: %s\n", strerror(err));
		abort();
	}
}

extern void crosscall1(void (*fn)(void), void (*setg_gcc)(void*), void *g);
static void*
threadentry(void *v)
{
	ThreadStart ts;

	ts = *(ThreadStart*)v;
	free(v);

#if TARGET_OS_IPHONE
	darwin_arm_init_thread_exception_port();
#endif

	crosscall1(ts.fn, setg_gcc, (void*)ts.g);
	return nil;
}

#if TARGET_OS_IPHONE

// init_working_dir sets the current working directory to the app root.
// By default ios/arm64 processes start in "/".
static void
init_working_dir()
{
	CFBundleRef bundle = CFBundleGetMainBundle();
	if (bundle == NULL) {
		fprintf(stderr, "runtime/cgo: no main bundle\n");
		return;
	}
	CFURLRef bundle_url_ref = CFBundleCopyBundleURL(bundle);
	if (bundle_url_ref == NULL) {
		return;
	}
	char bundle_path[MAXPATHLEN];
	Boolean res = CFURLGetFileSystemRepresentation(bundle_url_ref, TRUE, (UInt8 *)bundle_path, MAXPATHLEN);
	CFRelease(bundle_url_ref);
	if (!res) {
		fprintf(stderr, "runtime/cgo: cannot get URL string\n");
		return;
	}

	if (chdir(bundle_path) != 0) {
		fprintf(stderr, "runtime/cgo: chdir(%s) failed\n", bundle_path);
	}

	// The test harness in go_ios_exec passes the relative working directory
	// in the GoExecWrapperWorkingDirectory property of the app bundle.
	CFStringRef wd_ref = CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("GoExecWrapperWorkingDirectory"));
	if (wd_ref != NULL) {
		if (!CFStringGetCString(wd_ref, bundle_path, sizeof(bundle_path), kCFStringEncodingUTF8)) {
			fprintf(stderr, "runtime/cgo: cannot get GoExecWrapperWorkingDirectory string\n");
			return;
		}
		if (chdir(bundle_path) != 0) {
			fprintf(stderr, "runtime/cgo: chdir(%s) failed\n", bundle_path);
		}
	}
}

#endif // TARGET_OS_IPHONE

void
x_cgo_init(G *g, void (*setg)(void*))
{
	pthread_attr_t attr;
	size_t size;

	//fprintf(stderr, "x_cgo_init = %p\n", &x_cgo_init); // aid debugging in presence of ASLR
	setg_gcc = setg;
	pthread_attr_init(&attr);
	pthread_attr_getstacksize(&attr, &size);
	g->stacklo = (uintptr)&attr - size + 4096;
	pthread_attr_destroy(&attr);

#if TARGET_OS_IPHONE
	darwin_arm_init_mach_exception_handler();
	darwin_arm_init_thread_exception_port();
	init_working_dir();
#endif
}
