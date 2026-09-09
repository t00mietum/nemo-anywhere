/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-crash.c - write a report when the program dies unexpectedly.

   Copyright © 2026 t00mietum.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>

#include "nemo-crash.h"
#include "nemo-file-utilities.h"

#include <nemo-build-number.h>

#include <string.h>

#ifdef G_OS_WIN32
#include <windows.h>
#include <dbghelp.h>
#include <signal.h>
#include <stdio.h>
#include <wchar.h>

#ifndef STATUS_FATAL_APP_EXIT
#define STATUS_FATAL_APP_EXIT 0x40000015L
#endif
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#endif

#define CRASH_PATH_MAX 1024
#define CRASH_MAX_FRAMES 64

static char report_parent[CRASH_PATH_MAX];
static char report_dir[CRASH_PATH_MAX];
static char report_path[CRASH_PATH_MAX];
static gboolean installed = FALSE;

#ifdef G_OS_WIN32
static wchar_t report_parent_w[CRASH_PATH_MAX];
static wchar_t report_dir_w[CRASH_PATH_MAX];
static wchar_t report_path_w[CRASH_PATH_MAX];
static wchar_t dump_path_w[CRASH_PATH_MAX];
#endif

/* Everything below the divider runs after the program is already broken, so it
   allocates nothing and calls nothing that takes a lock. */

static gboolean
build_paths (void)
{
	g_autofree char *parent = NULL;
	g_autofree char *dir = NULL;
	g_autofree char *stamp = NULL;
	g_autofree char *name = NULL;
	g_autofree char *path = NULL;
	GDateTime *now = NULL;
	guint pid;

#ifdef G_OS_WIN32
	pid = (guint) GetCurrentProcessId ();
#else
	pid = (guint) getpid ();
#endif

	/* Deliberately not nemo_get_user_directory: that creates the config dir
	   and migrates an older one, and a run that only prints its version has
	   no business doing either. The directory is made when there is
	   something to put in it. */
	parent = g_build_filename (nemo_get_user_config_root (), NEMO_APP_SLUG, NULL);
	dir = g_build_filename (parent, "crash", NULL);

	now = g_date_time_new_now_local ();
	stamp = g_date_time_format (now, "%Y%m%d-%H%M%S");
	g_date_time_unref (now);

	name = g_strdup_printf ("crash-%s-%u.txt", stamp, pid);
	path = g_build_filename (dir, name, NULL);

	if (strlen (path) + 5 >= CRASH_PATH_MAX)
		return FALSE;

	g_strlcpy (report_parent, parent, sizeof report_parent);
	g_strlcpy (report_dir, dir, sizeof report_dir);
	g_strlcpy (report_path, path, sizeof report_path);

#ifdef G_OS_WIN32
	{
		g_autofree gunichar2 *wparent = g_utf8_to_utf16 (parent, -1, NULL, NULL, NULL);
		g_autofree gunichar2 *wdir = g_utf8_to_utf16 (dir, -1, NULL, NULL, NULL);
		g_autofree gunichar2 *wpath = g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
		g_autofree char *dump = NULL;
		g_autofree gunichar2 *wdump = NULL;

		dump = g_strconcat (path, ".dmp", NULL);
		wdump = g_utf8_to_utf16 (dump, -1, NULL, NULL, NULL);

		if (wparent == NULL || wdir == NULL || wpath == NULL || wdump == NULL)
			return FALSE;

		wcsncpy (report_parent_w, (const wchar_t *) wparent, CRASH_PATH_MAX - 1);
		wcsncpy (report_dir_w, (const wchar_t *) wdir, CRASH_PATH_MAX - 1);
		wcsncpy (report_path_w, (const wchar_t *) wpath, CRASH_PATH_MAX - 1);
		wcsncpy (dump_path_w, (const wchar_t *) wdump, CRASH_PATH_MAX - 1);
	}
#endif

	return TRUE;
}

/* ------------------------------------------------------------------ */

#ifndef G_OS_WIN32

static void
write_all (int fd, const char *s, size_t len)
{
	if (fd < 0)
		return;

	while (len > 0) {
		ssize_t n = write (fd, s, len);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return;
		}
		if (n == 0)
			return;

		s += n;
		len -= (size_t) n;
	}
}

static void
write_str (int fd, const char *s)
{
	write_all (fd, s, strlen (s));
}

static void
write_num (int fd, gsize value, int base)
{
	static const char digits[] = "0123456789abcdef";
	char buf[24];
	int i = (int) sizeof buf;

	if (value == 0) {
		write_str (fd, "0");
		return;
	}

	while (value > 0 && i > 0) {
		buf[--i] = digits[value % (gsize) base];
		value /= (gsize) base;
	}

	write_all (fd, buf + i, sizeof buf - (size_t) i);
}

static const char *
signal_name (int sig)
{
	switch (sig) {
	case SIGSEGV: return "SIGSEGV";
	case SIGBUS:  return "SIGBUS";
	case SIGILL:  return "SIGILL";
	case SIGFPE:  return "SIGFPE";
	case SIGABRT: return "SIGABRT";
	default:      return "signal";
	}
}

static void
write_report (int fd, int sig, const void *fault_addr, void *const *frames, int n_frames)
{
#if !HAVE_EXECINFO_H
	(void) frames;
	(void) n_frames;
#endif

	write_str (fd, "nemo-anywhere " NEMO_VERSION_STRING "\n");
	write_str (fd, "died on ");
	write_str (fd, signal_name (sig));
	write_str (fd, " (");
	write_num (fd, (gsize) sig, 10);
	write_str (fd, ") at 0x");
	write_num (fd, (gsize) fault_addr, 16);
	write_str (fd, "\npid ");
	write_num (fd, (gsize) getpid (), 10);
	write_str (fd, "\n\n");

#if HAVE_EXECINFO_H
	if (n_frames > 0)
		backtrace_symbols_fd (frames, n_frames, fd);
	else
#endif
		write_str (fd, "no backtrace available\n");
}

static volatile sig_atomic_t handling = 0;

static void
crash_signal_handler (int sig, siginfo_t *info, void *context)
{
	void *frames[CRASH_MAX_FRAMES];
	int n_frames = 0;
	int fd;

	(void) context;

	/* A fault inside the handler must not loop back in here. */
	if (handling)
		_exit (128 + sig);
	handling = 1;

#if HAVE_EXECINFO_H
	n_frames = backtrace (frames, CRASH_MAX_FRAMES);
#endif

	/* mkdir is a bare syscall wrapper, so it is safe here. It and the open are
	   both allowed to fail: stderr still gets the report. */
	mkdir (report_parent, DEFAULT_NEMO_DIRECTORY_MODE);
	mkdir (report_dir, 0700);
	fd = open (report_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);

	write_report (fd, sig, info != NULL ? info->si_addr : NULL, frames, n_frames);

	if (fd >= 0) {
		close (fd);
		write_str (STDERR_FILENO, "\nnemo-anywhere crashed. Report written to ");
		write_str (STDERR_FILENO, report_path);
		write_str (STDERR_FILENO, "\n");
	}

	write_report (STDERR_FILENO, sig, info != NULL ? info->si_addr : NULL,
		      frames, n_frames);

	/* Hand the signal back so a core file, or a debugger, still gets one. */
	signal (sig, SIG_DFL);
	raise (sig);
	_exit (128 + sig);
}

static void
install_posix (void)
{
	static const int signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
	static char alt_stack[64 * 1024];
	struct sigaction sa;
	stack_t ss;
	gsize i;

#if HAVE_EXECINFO_H
	{
		/* The first backtrace loads the unwinder, which is not something to
		   do from inside a handler. */
		void *warmup[4];

		backtrace (warmup, G_N_ELEMENTS (warmup));
	}
#endif

	ss.ss_sp = alt_stack;
	ss.ss_size = sizeof alt_stack;
	ss.ss_flags = 0;
	sigaltstack (&ss, NULL);

	memset (&sa, 0, sizeof sa);
	sa.sa_sigaction = crash_signal_handler;
	sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
	sigemptyset (&sa.sa_mask);

	for (i = 0; i < G_N_ELEMENTS (signals); i++)
		sigaction (signals[i], &sa, NULL);
}

#else /* G_OS_WIN32 */

static HANDLE
open_report_file (const wchar_t *path)
{
	CreateDirectoryW (report_parent_w, NULL);
	CreateDirectoryW (report_dir_w, NULL);

	return CreateFileW (path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
			    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

static void
write_str (HANDLE h, const char *s)
{
	DWORD written = 0;

	if (h == NULL || h == INVALID_HANDLE_VALUE)
		return;

	WriteFile (h, s, (DWORD) strlen (s), &written, NULL);
}

static void
write_wide (HANDLE h, const wchar_t *s)
{
	char narrow[CRASH_PATH_MAX * 2];

	if (h == NULL || h == INVALID_HANDLE_VALUE)
		return;

	if (WideCharToMultiByte (CP_UTF8, 0, s, -1, narrow, (int) sizeof narrow,
				 NULL, NULL) > 0)
		write_str (h, narrow);
}

static void
write_num (HANDLE h, guint64 value, int base, int pad)
{
	static const char digits[] = "0123456789abcdef";
	char buf[24];
	int i = (int) sizeof buf;

	buf[--i] = '\0';

	do {
		buf[--i] = digits[value % (guint64) base];
		value /= (guint64) base;
		pad--;
	} while ((value > 0 || pad > 0) && i > 0);

	write_str (h, buf + i);
}

static const char *
exception_name (DWORD code)
{
	switch (code) {
	case EXCEPTION_ACCESS_VIOLATION:      return "access violation";
	case EXCEPTION_STACK_OVERFLOW:        return "stack overflow";
	case EXCEPTION_ILLEGAL_INSTRUCTION:   return "illegal instruction";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "integer divide by zero";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "float divide by zero";
	case EXCEPTION_IN_PAGE_ERROR:         return "page error";
	case EXCEPTION_PRIV_INSTRUCTION:      return "privileged instruction";
	case STATUS_FATAL_APP_EXIT:           return "aborted";
	default:                              return "exception";
	}
}

/* Where the module wanted to be loaded. Frames are reported at that address
   rather than the one they ran at, so the number in the report is the one
   addr2line takes. A mingw build carries no PDB, so this is the only way a
   frame means anything on another machine. */
static DWORD64
preferred_base (DWORD64 loaded_base)
{
	const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *) (UINT_PTR) loaded_base;
	const IMAGE_NT_HEADERS *nt;

	if (loaded_base == 0 || IsBadReadPtr (dos, sizeof *dos) ||
	    dos->e_magic != IMAGE_DOS_SIGNATURE)
		return loaded_base;

	nt = (const IMAGE_NT_HEADERS *) ((const char *) dos + dos->e_lfanew);

	if (IsBadReadPtr (nt, sizeof *nt) || nt->Signature != IMAGE_NT_SIGNATURE)
		return loaded_base;

	return nt->OptionalHeader.ImageBase;
}

static void
write_stack (HANDLE h, const CONTEXT *context)
{
	HANDLE process = GetCurrentProcess ();
	CONTEXT walk = *context;
	STACKFRAME64 frame;
	DWORD machine;
	int depth;

	memset (&frame, 0, sizeof frame);

#if defined (__x86_64__) || defined (_M_X64)
	machine = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset = walk.Rip;
	frame.AddrFrame.Offset = walk.Rbp;
	frame.AddrStack.Offset = walk.Rsp;
#elif defined (__aarch64__) && defined (IMAGE_FILE_MACHINE_ARM64)
	machine = IMAGE_FILE_MACHINE_ARM64;
	frame.AddrPC.Offset = walk.Pc;
	frame.AddrFrame.Offset = walk.Fp;
	frame.AddrStack.Offset = walk.Sp;
#else
	machine = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = walk.Eip;
	frame.AddrFrame.Offset = walk.Ebp;
	frame.AddrStack.Offset = walk.Esp;
#endif
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Mode = AddrModeFlat;

	SymSetOptions (SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
	SymInitialize (process, NULL, TRUE);

	write_str (h, "stack (addr2line -e <module> <address>):\n");

	for (depth = 0; depth < CRASH_MAX_FRAMES; depth++) {
		DWORD64 base;
		DWORD64 offset;
		char module[MAX_PATH];

		if (!StackWalk64 (machine, process, GetCurrentThread (), &frame, &walk,
				  NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
			break;

		if (frame.AddrPC.Offset == 0)
			break;

		base = SymGetModuleBase64 (process, frame.AddrPC.Offset);
		offset = base != 0 ? frame.AddrPC.Offset - base : 0;

		write_str (h, "  0x");
		write_num (h, preferred_base (base) + offset, 16, 16);

		if (base != 0 &&
		    GetModuleFileNameA ((HMODULE) (UINT_PTR) base, module, sizeof module) > 0) {
			const char *leaf = strrchr (module, '\\');

			write_str (h, "  ");
			write_str (h, leaf != NULL ? leaf + 1 : module);
			write_str (h, "+0x");
			write_num (h, offset, 16, 0);
		}

		write_str (h, "\n");
	}

	SymCleanup (process);
}

static void
write_dump (EXCEPTION_POINTERS *info)
{
	MINIDUMP_EXCEPTION_INFORMATION mei;
	HANDLE h;

	h = CreateFileW (dump_path_w, GENERIC_WRITE, 0, NULL,
			 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (h == INVALID_HANDLE_VALUE)
		return;

	mei.ThreadId = GetCurrentThreadId ();
	mei.ExceptionPointers = info;
	mei.ClientPointers = FALSE;

	MiniDumpWriteDump (GetCurrentProcess (), GetCurrentProcessId (), h,
			   MiniDumpNormal | MiniDumpWithThreadInfo, &mei, NULL, NULL);

	CloseHandle (h);
}

/* Reports go to the file and to stderr, so this runs twice. */
static void
write_body (HANDLE h, const EXCEPTION_RECORD *record, const CONTEXT *context)
{
	write_str (h, "nemo-anywhere " NEMO_VERSION_STRING "\n");
	write_str (h, "died on ");
	write_str (h, exception_name (record->ExceptionCode));
	write_str (h, " (0x");
	write_num (h, record->ExceptionCode, 16, 8);
	write_str (h, ") at 0x");
	write_num (h, (guint64) (UINT_PTR) record->ExceptionAddress, 16, 0);
	write_str (h, "\npid ");
	write_num (h, GetCurrentProcessId (), 10, 0);
	write_str (h, "\n\n");

	write_stack (h, context);
}

static volatile LONG handling = 0;
static gboolean quiet = FALSE;

static void
report_and_die (EXCEPTION_POINTERS *info)
{
	const EXCEPTION_RECORD *record = info->ExceptionRecord;
	HANDLE h;

	if (InterlockedExchange (&handling, 1) != 0)
		TerminateProcess (GetCurrentProcess (), 3);

	h = open_report_file (report_path_w);

	write_body (h, record, info->ContextRecord);

	if (h != INVALID_HANDLE_VALUE) {
		CloseHandle (h);

		/* Nothing reads stderr in a windowed build, but a console one and
		   every test do. */
		write_str (GetStdHandle (STD_ERROR_HANDLE),
			   "\nnemo-anywhere crashed. Report written to ");
		write_wide (GetStdHandle (STD_ERROR_HANDLE), report_path_w);
		write_str (GetStdHandle (STD_ERROR_HANDLE), "\n");
	}

	write_body (GetStdHandle (STD_ERROR_HANDLE), record, info->ContextRecord);

	write_dump (info);

	/* Nothing is watching stderr in a windowed build, so say it in the one
	   place the user will see. */
	if (!quiet) {
		wchar_t message[CRASH_PATH_MAX + 128];

		_snwprintf (message, G_N_ELEMENTS (message) - 1,
			    L"Nemo Anywhere stopped unexpectedly.\n\n"
			    L"A report was written to:\n%ls",
			    report_path_w);
		message[G_N_ELEMENTS (message) - 1] = L'\0';

		MessageBoxW (NULL, message, L"Nemo Anywhere",
			     MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
	}

	TerminateProcess (GetCurrentProcess (), 3);
}

static LONG WINAPI
crash_exception_filter (EXCEPTION_POINTERS *info)
{
	report_and_die (info);

	return EXCEPTION_EXECUTE_HANDLER;
}

/* abort() never reaches the unhandled-exception filter, and g_error and every
   failed assertion go out that way. */
static void
crash_abort_handler (int sig)
{
	EXCEPTION_RECORD record;
	EXCEPTION_POINTERS info;
	CONTEXT context;

	(void) sig;

	RtlCaptureContext (&context);

	memset (&record, 0, sizeof record);
	record.ExceptionCode = STATUS_FATAL_APP_EXIT;
	record.ExceptionAddress = (PVOID) (UINT_PTR) crash_abort_handler;

	info.ExceptionRecord = &record;
	info.ContextRecord = &context;

	report_and_die (&info);
}

static void
install_win32 (void)
{
	quiet = g_getenv ("NEMO_NO_CRASH_DIALOG") != NULL;

	SetUnhandledExceptionFilter (crash_exception_filter);
	signal (SIGABRT, crash_abort_handler);
}

#endif /* G_OS_WIN32 */

void
nemo_crash_handler_install (void)
{
	if (installed)
		return;

	if (g_getenv ("NEMO_NO_CRASH_HANDLER") != NULL)
		return;

	if (!build_paths ())
		return;

	installed = TRUE;

#ifdef G_OS_WIN32
	install_win32 ();
#else
	install_posix ();
#endif
}

const char *
nemo_crash_report_path (void)
{
	return installed ? report_path : NULL;
}
