#include "zapp.h"
#include <filesystem>

#ifdef __APPLE__
#include <unistd.h>
#endif

#ifdef HAS_BREAKPAD
#include "crash_report_sender.h"
#include "exception_handler.h"

bool minidump_callback(const wchar_t* dump_path, const wchar_t* minidump_id, void* context, EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion, bool succeeded)
{
  google_breakpad::CrashReportSender sender(L"crash.checkpoint");

  std::wstring filename = L"_dump_path_";
  filename += L"\\";
  filename += minidump_id;
  filename += L".dmp";

  std::map<std::wstring, std::wstring> files;
  files.insert(std::make_pair(filename, filename));

  // At this point you may include custom data to be part of the crash report.
  std::map<std::wstring, std::wstring> userCustomData;
  userCustomData.insert(std::make_pair(L"desc", L"Hello World"));

  sender.SendCrashReport(L"https://api.raygun.com/entries/breakpad?apikey=o841YPlfAEh7AuZyVLw", userCustomData, files, 0);

  return true;
}
#endif

bool is_in_osx_application_bundle()
{
#ifdef __APPLE__
    return std::filesystem::current_path().string().find("/ZeldaClassic.app/") != std::string::npos;
#else
    return false;
#endif
}

void common_main_setup(int argc, char **argv)
{
    // This allows for opening a binary from Finder and having ZC be in its expected
    // working directory (the same as the binary). Only used when not inside an application bundle,
    // and only for testing purposes really. See comment about `SKIP_APP_BUNDLE` in buildpack_osx.sh
#ifdef __APPLE__
    if (!is_in_osx_application_bundle() && argc > 0) {
        chdir(std::filesystem::path(argv[0]).parent_path().c_str());
    }
#endif

#ifdef HAS_BREAKPAD
    google_breakpad::ExceptionHandler *pHandler = new google_breakpad::ExceptionHandler(
        L"_dump_path_", 0, minidump_callback, 0,
        google_breakpad::ExceptionHandler::HANDLER_ALL, MiniDumpNormal, L"", 0);
#endif
}
