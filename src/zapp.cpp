#include "zapp.h"
#include <filesystem>

#ifdef __APPLE__
#include <unistd.h>
#endif

#ifdef HAS_CRASHPAD

#include <client/crash_report_database.h>
#include <client/settings.h>
#include <client/crashpad_client.h>

typedef std::wstring StringType;

using namespace base;
using namespace crashpad;
using namespace std;


StringType getExecutableDir() {
  HMODULE hModule = GetModuleHandleW(NULL);
  WCHAR path[MAX_PATH];
  DWORD retVal = GetModuleFileNameW(hModule, path, MAX_PATH);
  if (retVal == 0) return NULL;

  wchar_t *lastBackslash = wcsrchr(path, '\\');
  if (lastBackslash == NULL) return NULL;
  *lastBackslash = 0;

  return path;
//   return std::filesystem::path(path).string();
}

bool initializeCrashpad() {
  // Get directory where the exe lives so we can pass a full path to handler, reportsDir, metricsDir and attachments
  StringType exeDir = getExecutableDir();

  // Ensure that handler is shipped with your application
  base::FilePath handler(exeDir + L"/path/to/crashpad_handler");

  // Directory where reports will be saved. Important! Must be writable or crashpad_handler will crash.
  base::FilePath reportsDir(exeDir + L"/path/to/crashpad");

  // Directory where metrics will be saved. Important! Must be writable or crashpad_handler will crash.
  base::FilePath metricsDir(exeDir + L"/path/to/crashpad");

  // Configure url with BugSplat’s public fred database. Replace 'fred' with the name of your BugSplat database.
  std::string url = "https://fred.bugsplat.com/post/bp/crash/crashpad.php";

  // Metadata that will be posted to the server with the crash report map
  map<std::string, std::string> annotations;
  annotations["format"] = "minidump";           // Required: Crashpad setting to save crash as a minidump
  annotations["database"] = "fred";             // Required: BugSplat appName
  annotations["product"] = "myCrashpadCrasher"; // Required: BugSplat appName
  annotations["version"] = "1.0.0";             // Required: BugSplat appVersion
  annotations["key"] = "Sample key";            // Optional: BugSplat key field
  annotations["user"] = "fred@bugsplat.com";    // Optional: BugSplat user email
  annotations["list_annotations"] = "Sample comment"; // Optional: BugSplat crash description

  // Disable crashpad rate limiting so that all crashes have dmp files
  vector<std::string> arguments; 
  arguments.push_back("--no-rate-limit");

  // Initialize Crashpad database
  unique_ptr<CrashReportDatabase> database = CrashReportDatabase::Initialize(reportsDir);
  if (database == NULL) return false;

  // File paths of attachments to uploaded with minidump file at crash time - default upload limit is 2MB
  vector<FilePath> attachments;
  FilePath attachment(exeDir + L"/path/to/attachment.txt");
  attachments.push_back(attachment);

  // Enable automated crash uploads
  Settings *settings = database->GetSettings();
  if (settings == NULL) return false;
  settings->SetUploadsEnabled(true);

  // Start crash handler
//   CrashpadClient *client = new CrashpadClient();
//   bool status = client->StartHandler(handler, reportsDir, metricsDir, url, annotations, arguments, true, true, attachments);
//   return status;
    return 0;
}

#endif



// #ifdef HAS_BREAKPAD
// #include "client/windows/sender/crash_report_sender.h"
// #include "client/windows/handler/exception_handler.h"

// bool minidump_callback(const wchar_t* dump_path, const wchar_t* minidump_id, void* context, EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion, bool succeeded)
// {
//   google_breakpad::CrashReportSender sender(L"crash.checkpoint");

//   std::string filename = L"_dump_path_";
//   filename += L"\\";
//   filename += minidump_id;
//   filename += L".dmp";

//   std::map<std::string, std::string> files;
//   files.insert(std::make_pair(filename, filename));

//   // At this point you may include custom data to be part of the crash report.
//   std::map<std::string, std::string> userCustomData;
//   userCustomData.insert(std::make_pair(L"desc", L"Hello World"));

//   sender.SendCrashReport(L"https://api.raygun.com/entries/breakpad?apikey=o841YPlfAEh7AuZyVLw", userCustomData, files, 0);

//   return true;
// }
// #endif

bool is_in_osx_application_bundle()
{
#ifdef __APPLE__
    return std::filesystem::current_path().string().find("/ZeldaClassic.app/") != std::string::npos;
#else
    return false;
#endif
}

void crash(int b)
{
//   volatile int* a = (int*)(NULL);
//   *a = 1;
    int a = 1/ b;
}

void common_main_setup(int argc, char **argv)
{
#ifdef HAS_CRASHPAD
    initializeCrashpad();
#endif

    // This allows for opening a binary from Finder and having ZC be in its expected
    // working directory (the same as the binary). Only used when not inside an application bundle,
    // and only for testing purposes really. See comment about `SKIP_APP_BUNDLE` in buildpack_osx.sh
#ifdef __APPLE__
    if (!is_in_osx_application_bundle() && argc > 0) {
        chdir(std::filesystem::path(argv[0]).parent_path().c_str());
    }
#endif

    // https://stackoverflow.com/questions/45733174/breakpad-exception-handler-not-used-in-a-dll-on-windows
    // __try 
    // {
    //     crash();
    // }
    // __except( true )
    // { 
    //     delete pHandler;
    // }

    crash(0);
    // try 
    // {
    //     crash(0);
    // }
    // catch(std::exception &ex)
    // { 
    //     delete pHandler;
    // }


}
