/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup creator
 */

#ifndef WITH_PYTHON_MODULE

#  include <cerrno>
#  include <cstdlib>

#  if defined(__linux__) && defined(__GNUC__)
#    ifndef _GNU_SOURCE
#      define _GNU_SOURCE
#    endif
#    include <cfenv>
#  endif

#  if (defined(__APPLE__) && (defined(__i386__) || defined(__x86_64__)))
#    define OSX_SSE_FPE
#    include <xmmintrin.h>
#  endif

#  ifdef WIN32
#    include <float.h>
#    include <windows.h>

#    include "BLI_winstuff.h"

#    include "GPU_platform.hh"
#  endif

#  include "BLI_fileops.h"
#  include "BLI_path_utils.hh"
#  include "BLI_string.h"
#  include "BLI_system.h"
#  include BLI_SYSTEM_PID_H

#  include "BKE_appdir.hh" /* #BKE_tempdir_session_purge. */
#  include "BKE_blender.hh"
#  include "BKE_blender_version.h"
#  include "BKE_global.hh"
#  include "BKE_main.hh"
#  include "BKE_report.hh"
#  include "BKE_wm_runtime.hh"

/* for passing information between creator and gameengine */
#  ifdef WITH_GAMEENGINE
#    include "LA_SystemCommandLine.h"
#  else /* dummy */
#    define SYS_SystemHandle int
#  endif

#  include <csignal>

#  ifdef WITH_PYTHON
#    include "BPY_extern_python.hh" /* #BPY_python_backtrace. */
#  endif

#  include "creator_intern.h" /* Own include. */

#  if defined(__linux__) || defined(_WIN32) || defined(OSX_SSE_FPE)
/**
 * Set breakpoints here when running in debug mode, useful to catch floating point errors.
 */
static void sig_handle_fpe(int /*sig*/)
{
  fprintf(stderr, "debug: SIGFPE trapped\n");
}
#  endif

/* Handling `Ctrl-C` event in the console. */
static void sig_handle_blender_esc(int sig)
{
  /* Forces render loop to read queue, not sure if its needed. */
  G.is_break = true;

  if (sig == 2) {
    static int count = 0;
    if (count) {
      printf("\nBlender killed\n");
      exit(2);
    }
    printf("\nSent an internal break event. Press ^C again to kill Blender\n");
    count++;
  }
}

static void crashlog_file_generate(const char *filepath, const void *os_info)
{
  /* Might be called after WM/Main exit, so needs to be careful about nullptr-checking before
   * de-referencing. */

  wmWindowManager *wm = G_MAIN ? static_cast<wmWindowManager *>(G_MAIN->wm.first) : nullptr;

  FILE *fp;
  char header[512];

  printf("Writing: %s\n", filepath);
  fflush(stdout);

#  ifndef BUILD_DATE
  SNPRINTF(header, "# " BLEND_VERSION_FMT ", Unknown revision\n", BLEND_VERSION_ARG);
#  else
  SNPRINTF(header,
           "# " BLEND_VERSION_FMT ", Commit date: %s %s, Hash %s\n",
           BLEND_VERSION_ARG,
           build_commit_date,
           build_commit_time,
           build_hash);
#  endif

  /* Open the crash log. */
  errno = 0;
  fp = BLI_fopen(filepath, "wb");
  if (fp == nullptr) {
    fprintf(stderr,
            "Unable to save '%s': %s\n",
            filepath,
            errno ? strerror(errno) : "Unknown error opening file");
  }
  else {
    if (wm) {
      BKE_report_write_file_fp(fp, &wm->runtime->reports, header);
    }

    fputs("\n# backtrace\n", fp);
    BLI_system_backtrace_with_os_info(fp, os_info);

#  ifdef WITH_PYTHON
    /* Generate python back-trace if Python is currently active. */
    BPY_python_backtrace(fp);
#  endif

    fclose(fp);
  }
}

static void sig_cleanup_and_terminate(int signum)
{
  /* Delete content of temp directory. */
  BKE_tempdir_session_purge();

  /* Really crash. */
  signal(signum, SIG_DFL);
#  ifndef WIN32
  kill(getpid(), signum);
#  else
  TerminateProcess(GetCurrentProcess(), signum);
#  endif
}

static void sig_handle_crash_fn(int signum)
{
  char filepath_crashlog[FILE_MAX];
  BKE_blender_globals_crash_path_get(filepath_crashlog);
  crashlog_file_generate(filepath_crashlog, nullptr);
  sig_cleanup_and_terminate(signum);
}

#  ifdef WIN32
extern LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS *ExceptionInfo)
{
  /* If this is a stack overflow then we can't walk the stack, so just try to show
   * where the error happened. */
  if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
    HMODULE mod;
    CHAR modulename[MAX_PATH];
    LPVOID address = ExceptionInfo->ExceptionRecord->ExceptionAddress;
    fprintf(stderr, "Error   : EXCEPTION_STACK_OVERFLOW\n");
    fprintf(stderr, "Address : 0x%p\n", address);
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, LPCSTR(address), &mod)) {
      if (GetModuleFileName(mod, modulename, MAX_PATH)) {
        fprintf(stderr, "Module  : %s\n", modulename);
      }
    }
  }
  else {
    char filepath_crashlog[FILE_MAX];
    BLI_windows_exception_print_message(ExceptionInfo);
    BKE_blender_globals_crash_path_get(filepath_crashlog);
    crashlog_file_generate(filepath_crashlog, ExceptionInfo);

    /* Disable popup in background mode to avoid blocking automation.
     * (e.g., when used by a render farm; see #142314). */
    if (!G.background) {
      std::string version;
#    ifndef BUILD_DATE
      const char *build_hash = G_MAIN ? G_MAIN->build_hash : "unknown";
      version = std::string(BKE_blender_version_string()) + ", hash: `" + build_hash + "`";
#    else
      version = std::string(BKE_blender_version_string()) + ", Commit date: " + build_commit_date +
                " " + build_commit_time + ", hash: `" + build_hash + "`";
#    endif

      BLI_windows_exception_show_dialog(
          filepath_crashlog, G.filepath_last_blend, GPU_platform_gpu_name(), version.c_str());
    }
    sig_cleanup_and_terminate(SIGSEGV);
  }

  return EXCEPTION_EXECUTE_HANDLER;
}
#  endif

static void sig_handle_abort(int /*signum*/)
{
  /* Delete content of temp directory. */
  BKE_tempdir_session_purge();
}

void main_signal_setup()
{
  if (app_state.signal.use_crash_handler) {
#  ifdef WIN32
    SetUnhandledExceptionFilter(windows_exception_handler);
#  else
    /* After parsing arguments. */
    signal(SIGSEGV, sig_handle_crash_fn);
#  endif
  }

#  ifdef WIN32
  /* Prevent any error mode dialogs from hanging the application. */
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOGPFAULTERRORBOX |
               SEM_NOOPENFILEERRORBOX);
#  endif

  if (app_state.signal.use_abort_handler) {
    signal(SIGABRT, sig_handle_abort);
  }
}

void main_signal_setup_background()
{
  /* for all platforms, even windows has it! */
  BLI_assert(G.background);

  /* Support pressing `Ctrl-C` to close Blender in background-mode.
   * Useful to be able to cancel a render operation. */
  signal(SIGINT, sig_handle_blender_esc);
}

void main_signal_setup_fpe()
{
#  if defined(__linux__) || defined(_WIN32) || defined(OSX_SSE_FPE)
  /* Zealous but makes float issues a heck of a lot easier to find!
   * Set breakpoints on #sig_handle_fpe. */
  signal(SIGFPE, sig_handle_fpe);

#    if defined(__linux__) && defined(__GNUC__) && defined(HAVE_FEENABLEEXCEPT)
  feenableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);
#    endif /* defined(__linux__) && defined(__GNUC__) */
#    if defined(OSX_SSE_FPE)
  /* OSX uses SSE for floating point by default, so here
   * use SSE instructions to throw floating point exceptions. */
  _MM_SET_EXCEPTION_MASK(_MM_MASK_MASK &
                         ~(_MM_MASK_OVERFLOW | _MM_MASK_INVALID | _MM_MASK_DIV_ZERO));
#    endif /* OSX_SSE_FPE */
#    if defined(_WIN32) && defined(_MSC_VER)
  /* Enables all floating-point exceptions. */
  _controlfp_s(nullptr, 0, _MCW_EM);
  /* Hide the ones we don't care about. */
  _controlfp_s(nullptr, _EM_DENORMAL | _EM_UNDERFLOW | _EM_INEXACT, _MCW_EM);
#    endif /* _WIN32 && _MSC_VER */
#  endif
}

#endif /* WITH_PYTHON_MODULE */
