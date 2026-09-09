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

#include <glib/gstdio.h>
#include <string.h>

#ifdef G_OS_WIN32
#include <windows.h>
#include <signal.h>
#include <stdio.h>
#include <wchar.h>

#ifndef STATUS_FATAL_APP_EXIT
#define STATUS_FATAL_APP_EXIT 0x40000015L
#endif
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if HAVE_BACKTRACE
#include <execinfo.h>
#endif
#endif

#define CRASH_PATH_MAX 1024
#define CRASH_MAX_FRAMES 64

/* Old reports are the user's to read, not ours to hoard. */
#define CRASH_KEEP_REPORTS 20

static char report_parent[CRASH_PATH_MAX];
static char report_dir[CRASH_PATH_MAX];
static char report_path[CRASH_PATH_MAX];
static char started_stamp[32];
static gboolean installed = FALSE;

#ifdef G_OS_WIN32
static wchar_t report_parent_w[CRASH_PATH_MAX];
static wchar_t report_dir_w[CRASH_PATH_MAX];
static wchar_t report_path_w[CRASH_PATH_MAX];
#endif

/* An empty value reads as unset, the way the rest of the tree treats one. */
static gboolean
is_empty_env (const char *name)
{
	const char *value = g_getenv (name);

	return value == NULL || *value == '\0';
}

static gboolean
build_paths (void)
{
	g_autofree char *parent = NULL;
	g_autofree char *dir = NULL;
	g_autofree char *name = NULL;
	g_autofree char *path = NULL;
	GDateTime *now = NULL;
	g_autofree char *stamp = NULL;
	guint pid;

#ifdef G_OS_WIN32
	pid = (guint) GetCurrentProcessId ();
#else
	pid = (guint) getpid ();
#endif

	/* Deliberately not nemo_get_user_directory: that creates the config dir
	   and migrates an older one, and a run that only prints its version has
	   no business doing either. The directory is made when there is finally
	   something to put in it. */
	parent = g_build_filename (nemo_get_user_config_root (), NEMO_APP_SLUG, NULL);
	dir = g_build_filename (parent, "crash", NULL);

	now = g_date_time_new_now_local ();
	stamp = g_date_time_format (now, "%Y%m%d-%H%M%S");
	g_date_time_unref (now);

	/* The name carries when the run STARTED, because a signal handler cannot
	   safely work out what time it is. The file's own timestamp is the crash. */
	name = g_strdup_printf ("crash-%s-%u.txt", stamp, pid);
	path = g_build_filename (dir, name, NULL);

	if (strlen (path) >= CRASH_PATH_MAX)
		return FALSE;

	g_strlcpy (started_stamp, stamp, sizeof started_stamp);
	g_strlcpy (report_parent, parent, sizeof report_parent);
	g_strlcpy (report_dir, dir, sizeof report_dir);
	g_strlcpy (report_path, path, sizeof report_path);

#ifdef G_OS_WIN32
	{
		g_autofree gunichar2 *wparent = g_utf8_to_utf16 (parent, -1, NULL, NULL, NULL);
		g_autofree gunichar2 *wdir = g_utf8_to_utf16 (dir, -1, NULL, NULL, NULL);
		g_autofree gunichar2 *wpath = g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);

		if (wparent == NULL || wdir == NULL || wpath == NULL)
			return FALSE;

		if (wcslen ((const wchar_t *) wpath) >= CRASH_PATH_MAX)
			return FALSE;

		wcscpy (report_parent_w, (const wchar_t *) wparent);
		wcscpy (report_dir_w, (const wchar_t *) wdir);
		wcscpy (report_path_w, (const wchar_t *) wpath);
	}
#endif

	return TRUE;
}

typedef struct {
	char *name;
	gint64 written;
} Report;

static void
report_free (gpointer data)
{
	Report *report = data;

	g_free (report->name);
	g_free (report);
}

/* By when it was written, not by the name: the name carries when the run
   started, and a window open for a week can crash after one opened an hour ago. */
static int
compare_written (gconstpointer a, gconstpointer b)
{
	const Report *ra = *(const Report * const *) a;
	const Report *rb = *(const Report * const *) b;

	if (ra->written != rb->written)
		return ra->written < rb->written ? -1 : 1;

	/* Second resolution, so ties are ordinary. Without a tie-break the newest
	   moves between launches and the announcement repeats. */
	return g_strcmp0 (ra->name, rb->name);
}

/* Two jobs at startup: say that an earlier run left a report, since that run
   had no chance to, and drop the oldest so the folder cannot grow forever. */
static void
sweep_old_reports (void)
{
	g_autoptr (GPtrArray) reports = g_ptr_array_new_with_free_func (report_free);
	g_autoptr (GDir) dir = NULL;
	g_autofree char *marker = NULL;
	g_autofree char *seen = NULL;
	const char *entry;
	const char *newest;
	guint i;

	dir = g_dir_open (report_dir, 0, NULL);
	if (dir == NULL)
		return;

	while ((entry = g_dir_read_name (dir)) != NULL) {
		g_autofree char *path = NULL;
		GStatBuf info;
		Report *report;

		if (!g_str_has_prefix (entry, "crash-") || !g_str_has_suffix (entry, ".txt"))
			continue;

		path = g_build_filename (report_dir, entry, NULL);
		if (g_stat (path, &info) != 0)
			continue;

		report = g_new0 (Report, 1);
		report->name = g_strdup (entry);
		report->written = (gint64) info.st_mtime;
		g_ptr_array_add (reports, report);
	}

	if (reports->len == 0)
		return;

	g_ptr_array_sort (reports, compare_written);

	/* Said once per report rather than once per launch, which matters when
	   every window is its own process. */
	newest = ((const Report *) g_ptr_array_index (reports, reports->len - 1))->name;
	marker = g_build_filename (report_dir, "last-seen", NULL);

	if (!g_file_get_contents (marker, &seen, NULL, NULL))
		seen = NULL;

	if (g_strcmp0 (seen, newest) != 0) {
		g_message ("An earlier run stopped unexpectedly. Its report is in %s",
			   report_dir);
		g_file_set_contents (marker, newest, -1, NULL);
	}

	for (i = 0; reports->len - i > CRASH_KEEP_REPORTS; i++) {
		const Report *report = g_ptr_array_index (reports, i);
		g_autofree char *old = g_build_filename (report_dir, report->name, NULL);

		g_unlink (old);
	}
}

/* Past here the program is already broken. Nothing below allocates, and on
   POSIX nothing below is outside what a signal handler may call. */

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

/* si_addr only means an address for the faults. For an abort it carries
   whoever sent the signal, which reads as a plausible code address and is not
   one. */
static gboolean
has_fault_address (int sig)
{
	return sig == SIGSEGV || sig == SIGBUS || sig == SIGILL || sig == SIGFPE;
}

static void
write_header (int fd, int sig, const siginfo_t *info)
{
	write_str (fd, "nemo-anywhere " NEMO_VERSION_STRING "\n");
	write_str (fd, "started ");
	write_str (fd, started_stamp);
	write_str (fd, "\ndied on ");
	write_str (fd, signal_name (sig));
	write_str (fd, " (");
	write_num (fd, (gsize) sig, 10);
	write_str (fd, ")");

	if (info != NULL && has_fault_address (sig)) {
		write_str (fd, " at 0x");
		write_num (fd, (gsize) info->si_addr, 16);
	}

	write_str (fd, "\npid ");
	write_num (fd, (gsize) getpid (), 10);
	write_str (fd, "\n\nstack (addr2line -e <module> <the bare +0x in parentheses>):\n");
}

static volatile gint handling = 0;

static void
crash_signal_handler (int sig, siginfo_t *info, void *context)
{
	void *frames[CRASH_MAX_FRAMES];
	int n_frames = 0;
	sigset_t unblock;
	int fd;

	(void) context;
	(void) frames;

	/* One report per process. A second thread faulting waits rather than
	   truncating the first one's report, but not forever: the thread writing
	   it can be stuck behind a lock the crash left held. */
	if (!g_atomic_int_compare_and_exchange (&handling, 0, 1)) {
		struct timespec wait = { 5, 0 };

		nanosleep (&wait, NULL);
		_exit (128 + sig);
	}

	/* mkdir is a bare syscall wrapper, so it is safe here. It and the open
	   are both allowed to fail: stderr still gets the report. */
	mkdir (report_parent, DEFAULT_NEMO_DIRECTORY_MODE);
	mkdir (report_dir, 0700);
	fd = open (report_path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);

	/* Everything known for certain goes out before the stack is collected.
	   Walking it is the part that can fault again, and a report that says
	   only what killed the program still beats no report. */
	write_header (fd, sig, info);

	if (fd >= 0) {
		write_str (STDERR_FILENO, "\nnemo-anywhere crashed. Report written to ");
		write_str (STDERR_FILENO, report_path);
		write_str (STDERR_FILENO, "\n");
	}

	write_header (STDERR_FILENO, sig, info);

#if HAVE_BACKTRACE
	n_frames = backtrace (frames, CRASH_MAX_FRAMES);
#endif

#if HAVE_BACKTRACE
	if (n_frames > 0) {
		backtrace_symbols_fd (frames, n_frames, fd);
		backtrace_symbols_fd (frames, n_frames, STDERR_FILENO);
	} else
#endif
	{
		write_str (fd, "  not available in this build\n");
		write_str (STDERR_FILENO, "  not available in this build\n");
	}

	if (fd >= 0)
		close (fd);

	/* Hand the signal back, so a core file and an attached debugger still get
	   one. The signal is blocked on the way in here, so it has to be unblocked
	   or the raise below only marks it pending and the _exit wins. */
	signal (sig, SIG_DFL);
	sigemptyset (&unblock);
	sigaddset (&unblock, sig);
	pthread_sigmask (SIG_UNBLOCK, &unblock, NULL);
	raise (sig);

	_exit (128 + sig);
}

static void
install_posix (void)
{
	static const int signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
	/* Only the thread that registers it gets an alternate stack, so a stack
	   overflow is caught on the main thread and not on a worker. */
	static char alt_stack[64 * 1024];
	stack_t ss;
	gsize i;

#if HAVE_BACKTRACE
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

	for (i = 0; i < G_N_ELEMENTS (signals); i++) {
		struct sigaction sa;

		memset (&sa, 0, sizeof sa);
		sa.sa_sigaction = crash_signal_handler;
		sa.sa_flags = SA_SIGINFO | SA_ONSTACK;

		/* Everything else fatal stays blocked while the report is written,
		   or a second one arriving cuts it in half. */
		sigfillset (&sa.sa_mask);

		sigaction (signals[i], &sa, NULL);
	}
}

#else /* G_OS_WIN32 */

/* The report is built once and written twice, so the unwind is not paid for
   again on the way to stderr. */
static char report_text[16 * 1024];
static gsize report_len = 0;

static void
emit_str (const char *s)
{
	static const char cut[] = "  (report truncated)\n";
	static gboolean full = FALSE;
	gsize len = strlen (s);

	if (full)
		return;

	if (report_len + len + sizeof cut >= sizeof report_text) {
		full = TRUE;
		memcpy (report_text + report_len, cut, sizeof cut - 1);
		report_len += sizeof cut - 1;
		return;
	}

	memcpy (report_text + report_len, s, len);
	report_len += len;
}

static void
emit_num (guint64 value, int base, int pad)
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

	emit_str (buf + i);
}

static void
write_handle (HANDLE h, const char *s, gsize len)
{
	DWORD written = 0;

	if (h == NULL || h == INVALID_HANDLE_VALUE)
		return;

	WriteFile (h, s, (DWORD) len, &written, NULL);
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

/* A crashed process can hand over anything, and a read that faults in here
   loses the whole report, so every address off the stack is checked first. */
static gboolean
readable (const void *p, gsize len)
{
	MEMORY_BASIC_INFORMATION mbi;
	const char *start = p;

	if (p == NULL)
		return FALSE;

	if (VirtualQuery (p, &mbi, sizeof mbi) != sizeof mbi)
		return FALSE;

	if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
		return FALSE;

	return (const char *) mbi.BaseAddress + mbi.RegionSize >= start + len;
}

/* Where the module wanted to be loaded. Frames are reported at that address
   rather than the one they ran at, so the number in the report is the one
   addr2line takes. A mingw build carries no PDB, so this is all a frame can
   be made to mean on another machine. */
static DWORD64
preferred_base (DWORD64 loaded_base)
{
	const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *) (UINT_PTR) loaded_base;
	const IMAGE_NT_HEADERS *nt;

	if (!readable (dos, sizeof *dos) || dos->e_magic != IMAGE_DOS_SIGNATURE)
		return loaded_base;

	if (dos->e_lfanew <= 0 || dos->e_lfanew > 0x10000)
		return loaded_base;

	nt = (const IMAGE_NT_HEADERS *) ((const char *) dos + dos->e_lfanew);

	if (!readable (nt, sizeof *nt) || nt->Signature != IMAGE_NT_SIGNATURE)
		return loaded_base;

	return nt->OptionalHeader.ImageBase;
}

#if defined (__x86_64__) || defined (_M_X64)

/* The OS unwinder, rather than dbghelp: StackWalk64 needs SymInitialize, which
   enumerates every loaded module under the loader lock. A crash under that lock
   is exactly the case this has to survive. */
static void
emit_stack (const CONTEXT *context)
{
	static CONTEXT walk;
	static UNWIND_HISTORY_TABLE history;
	int depth;

	walk = *context;
	memset (&history, 0, sizeof history);

	emit_str ("stack (addr2line -e <module> <address>):\n");

	for (depth = 0; depth < CRASH_MAX_FRAMES; depth++) {
		PRUNTIME_FUNCTION function;
		DWORD64 image_base = 0;
		DWORD64 pc = walk.Rip;
		DWORD64 probe;
		char module[MAX_PATH];

		/* Past the first frame the address is a RETURN address, and for a
		   call that never comes back it belongs to the next function along.
		   One byte back is inside the call itself. */
		probe = depth == 0 ? pc : pc - 1;

		function = RtlLookupFunctionEntry (probe, &image_base, &history);

		emit_str ("  0x");
		emit_num (image_base != 0
			  ? preferred_base (image_base) + (probe - image_base)
			  : probe, 16, 16);

		if (image_base != 0 &&
		    GetModuleFileNameA ((HMODULE) (UINT_PTR) image_base, module,
					sizeof module) > 0) {
			const char *leaf = strrchr (module, '\\');

			emit_str ("  ");
			emit_str (leaf != NULL ? leaf + 1 : module);
			emit_str ("+0x");
			emit_num (probe - image_base, 16, 0);
		}

		emit_str ("\n");

		if (function == NULL) {
			/* A leaf, or a jump to an address with no code behind it at
			   all - the commonest shape of a call through a freed object.
			   Either way the return address is on top of the stack. */
			if (!readable ((const void *) (UINT_PTR) walk.Rsp, sizeof (DWORD64)))
				break;

			walk.Rip = *(DWORD64 *) (UINT_PTR) walk.Rsp;
			walk.Rsp += 8;
		} else {
			PVOID handler_data;
			DWORD64 establisher;

			/* The unwinder reads saved registers off the frame, so a
			   shredded stack pointer has to stop the walk here. */
			if (!readable ((const void *) (UINT_PTR) walk.Rsp, sizeof (DWORD64)))
				break;

			/* The same address the lookup used, or the unwinder decides
			   whether it is in an epilogue by reading the wrong function. */
			RtlVirtualUnwind (UNW_FLAG_NHANDLER, image_base, probe, function,
					  &walk, &handler_data, &establisher, NULL);
		}

		if (walk.Rip == 0)
			break;
	}
}

#else

static void
emit_stack (const CONTEXT *context)
{
	(void) context;

	emit_str ("stack:\n  not available in this build\n");
}

#endif

static volatile gint handling = 0;
static volatile DWORD reporting_thread = 0;
static gboolean quiet = FALSE;

static void
flush_report (HANDLE file)
{
	write_handle (file, report_text, report_len);
	write_handle (GetStdHandle (STD_ERROR_HANDLE), report_text, report_len);
	report_len = 0;
}

static void
report_and_die (EXCEPTION_POINTERS *info, gboolean has_address)
{
	const EXCEPTION_RECORD *record = info->ExceptionRecord;
	HANDLE file;

	if (!g_atomic_int_compare_and_exchange (&handling, 0, 1)) {
		/* Either this thread faulted again inside the filter, in which case
		   there is nothing left to try, or another thread is writing the
		   report and is worth a short wait but not an unbounded one. */
		if (reporting_thread != GetCurrentThreadId ())
			Sleep (5000);

		TerminateProcess (GetCurrentProcess (), record->ExceptionCode);
	}

	reporting_thread = GetCurrentThreadId ();

	CreateDirectoryW (report_parent_w, NULL);
	CreateDirectoryW (report_dir_w, NULL);

	file = CreateFileW (report_path_w, GENERIC_WRITE, FILE_SHARE_READ, NULL,
			    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	/* Nothing reads stderr in a windowed build, but a console one and every
	   test do. */
	if (file != INVALID_HANDLE_VALUE) {
		static const char wrote[] = "\nnemo-anywhere crashed. Report written to ";
		HANDLE err = GetStdHandle (STD_ERROR_HANDLE);

		write_handle (err, wrote, sizeof wrote - 1);
		write_handle (err, report_path, strlen (report_path));
		write_handle (err, "\n", 1);
	}

	emit_str ("nemo-anywhere " NEMO_VERSION_STRING "\n");
	emit_str ("started ");
	emit_str (started_stamp);
	emit_str ("\ndied on ");
	emit_str (exception_name (record->ExceptionCode));
	emit_str (" (0x");
	emit_num (record->ExceptionCode, 16, 8);
	emit_str (")");

	if (has_address) {
		emit_str (" at 0x");
		emit_num ((guint64) (UINT_PTR) record->ExceptionAddress, 16, 0);
	}

	emit_str ("\npid ");
	emit_num (GetCurrentProcessId (), 10, 0);
	emit_str ("\n\n");

	/* Everything known for certain goes out before the stack is walked.
	   Walking it is the part that can fault again, and a report that says
	   only what killed the program still beats no report. */
	flush_report (file);

	emit_stack (info->ContextRecord);
	flush_report (file);

	if (file != INVALID_HANDLE_VALUE)
		CloseHandle (file);

	/* Last, and only once the report is safely on disk: this runs a modal
	   loop, which dispatches messages back into the code that just died, so
	   it is allowed to fail. Not translated, because loading a catalog in a
	   broken process is one more thing that can go wrong. */
	if (!quiet) {
		static wchar_t message[CRASH_PATH_MAX + 128];

		if (file != INVALID_HANDLE_VALUE) {
			_snwprintf (message, G_N_ELEMENTS (message) - 1,
				    L"Nemo Anywhere stopped unexpectedly.\n\n"
				    L"A report was written to:\n%ls",
				    report_path_w);
		} else {
			_snwprintf (message, G_N_ELEMENTS (message) - 1,
				    L"Nemo Anywhere stopped unexpectedly.\n\n"
				    L"No report could be written to:\n%ls",
				    report_path_w);
		}
		message[G_N_ELEMENTS (message) - 1] = L'\0';

		MessageBoxW (NULL, message, L"Nemo Anywhere",
			     MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
	}

	/* Exit with the cause, so a launcher or a smoke script still sees which
	   one it was. The cost is that Windows Error Reporting never buckets it,
	   which is a trade for the report and the dialog above. */
	TerminateProcess (GetCurrentProcess (), record->ExceptionCode);
}

static LONG WINAPI
crash_exception_filter (EXCEPTION_POINTERS *info)
{
	report_and_die (info, TRUE);

	return EXCEPTION_EXECUTE_HANDLER;
}

/* abort() never reaches the unhandled-exception filter, and g_error and every
   failed assertion go out that way. */
static void
crash_abort_handler (int sig)
{
	static EXCEPTION_RECORD record;
	static EXCEPTION_POINTERS info;
	static CONTEXT context;

	(void) sig;

	RtlCaptureContext (&context);

	memset (&record, 0, sizeof record);
	record.ExceptionCode = STATUS_FATAL_APP_EXIT;

	info.ExceptionRecord = &record;
	info.ContextRecord = &context;

	/* An abort has no faulting address to report. */
	report_and_die (&info, FALSE);
}

static void
install_win32 (void)
{
	ULONG guarantee = 64 * 1024;

	quiet = !is_empty_env ("NEMO_NO_CRASH_DIALOG");

	/* Keeps enough stack in reserve for the filter to run after a stack
	   overflow, which is the one crash that otherwise reports nothing. Per
	   thread, and only this one, the same way the alternate stack is on POSIX. */
	SetThreadStackGuarantee (&guarantee);

	/* Whatever was there before is replaced on purpose: a packer's own filter
	   would take the crash and leave no report of ours. */
	SetUnhandledExceptionFilter (crash_exception_filter);
	signal (SIGABRT, crash_abort_handler);
}

#endif /* G_OS_WIN32 */

void
nemo_crash_handler_install (void)
{
	if (installed)
		return;

	if (!is_empty_env ("NEMO_NO_CRASH_HANDLER"))
		return;

	if (!build_paths ())
		return;

	installed = TRUE;

	sweep_old_reports ();

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
