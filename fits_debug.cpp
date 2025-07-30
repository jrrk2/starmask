#include <iostream>
#include <FITS/FITS.h>
#include <pcl/api/APIInterface.h>
#include <API.cpp>

void *dummy_action(...)
{
  return nullptr;
}

void *dummy_handle(...)
{
  return nullptr;
}

void *dummy_get_console(...)
{
  return (void*)dummy_handle;
}

int dummy_write_console(void *handle, const char *msg, int flag)
{
  std::cout << msg;
  return api_true;
}

void *dummy_get_proc_status(...)
{
  return (void*)0;
}

// Proper thread creation that returns a valid thread handle
void *dummy_create_thread(void* (*start_routine)(void*), void* arg, int priority)
{
  pthread_t* thread = new pthread_t;
  if (pthread_create(thread, nullptr, start_routine, arg) == 0) {
    return (void*)thread;
  }
  delete thread;
  return (void*)1;  // Return non-null even if thread creation fails
}

void *dummy_current_thread(...)
{
  static pthread_t current = pthread_self();
  return (void*)&current;
}

/*
 	   return (*API->Thread->GetThreadStatusEx)( thread, &status, 0x00000001 ) != api_false &&
     	          (status & 0x80000000) != 0;
   
 */

int dummy_thread_statusex(void *thread, uint32 *status, int flags)
{
  pthread_t current = (pthread_t)thread;
  *status = 0;
  return api_true;
}

void *dummy_pixel_traits(...)
{
  return nullptr;
}

void dummy_get_flag( const char *nam, api_bool *rslt )
{
  std::cout << nam;
  if (!strcmp(nam, "Process/EnableThreadCPUAffinity"))
    *rslt = api_true;
  else
    *rslt = api_false;
}

int dummy_get_integer( const char *nam, int32 *rslt, int flag )
{
  if (!strcmp(nam, "System/NumberOfProcessors"))
    *rslt = 4;
  else
    *rslt = 0;
  return api_true;
}

int dummy_max_proc(...)
{
  return 4;
}

int dummy_thread_perf(...)
{
  return 1;
}

void dummy_thread_exec(void *handle, void *dispatcher)
{
  std::cout << "thread exec";
}

void *function_resolver_fn ( const char* nam)
{
  if (!strcmp(nam, "Global/GetConsole"))
    return (void *)dummy_get_console;
   if (!strcmp(nam, "Global/GetGlobalFlag"))
    return (void *)dummy_get_flag;
   if (!strcmp(nam, "Global/GetGlobalInteger"))
    return (void *)dummy_get_integer;
   if (!strcmp(nam, "Global/MaxProcessorsAllowedForModule"))
    return (void *)dummy_max_proc;
  if (!strcmp(nam, "Global/WriteConsole"))
    return (void *)dummy_write_console;
  if (!strcmp(nam, "Global/GetProcessStatus"))
    return (void *)dummy_get_proc_status;
  if (!strcmp(nam, "Thread/CreateThread"))
    return (void *)dummy_create_thread;
  if (!strcmp(nam, "Thread/GetCurrentThread"))
    return (void *)dummy_current_thread;
  if (!strcmp(nam, "Thread/SetThreadExecRoutine"))
    return (void *)dummy_thread_exec;
  if (!strcmp(nam, "Thread/GetThreadStatusEx"))
    return (void *)dummy_thread_statusex;
  if (!strcmp(nam, "Thread/PerformanceAnalysisValue"))
    return (void *)dummy_thread_perf;

  if (!strcmp(nam, "Global/GetPixelTraitsLUT"))
    return (void *)dummy_pixel_traits;
  return (void *)dummy_action;
}

/*
PCL_MODULE_EXPORT uint32
InitializePixInsightModule( api_handle        hModule,
                            function_resolver R,
                            uint32            apiVersion,
                            void*             reserved )
{
   s_moduleHandle = hModule;
   return APIInitializer::InitAPI( R, apiVersion );
}
 */

void *hModule(...)
{
  std::cout << "hModule called";
  return nullptr;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <fits_file>" << std::endl;
        return 1;
    }

    s_moduleHandle = (void *)hModule;
    
    std::string filePath = argv[1];
    std::cout << "Testing FITS file: " << filePath << std::endl;
    std::cout << "=================================" << std::endl;
    
    try {
      API = new pcl::APIInterface(function_resolver_fn);
        pcl::FITSReader reader;
        pcl::String pclPath(filePath.c_str());
        
        std::cout << "1. Opening FITS file..." << std::endl;
        reader.Open(pclPath);
        std::cout << "   ✓ File opened successfully" << std::endl;
        
        std::cout << "2. Reading image..." << std::endl;
        pcl::Image *pclImage = new pcl::Image;
	reader.SetIndex(0);
        reader.ReadImage(*pclImage);
        std::cout << "   ✓ Image read successfully" << std::endl;
        
        std::cout << "3. Image info:" << std::endl;
        std::cout << "   Dimensions: " << pclImage->Width() << " x " << pclImage->Height() << std::endl;
        std::cout << "   Channels: " << pclImage->NumberOfChannels() << std::endl;
        std::cout << "   Sample type: " << (pclImage->IsFloatSample() ? "Float" : "Integer") << std::endl;
        std::cout << "   Bits per sample: " << pclImage->BitsPerSample() << std::endl;
        
        std::cout << "4. Reading FITS keywords..." << std::endl;
        try {
            pcl::FITSKeywordArray keywords = reader.ReadFITSKeywords();
            std::cout << "   ✓ Found " << keywords.Length() << " keywords" << std::endl;
            
            // Show first few keywords
            for (size_t i = 0; i < std::min(size_t(5), keywords.Length()); ++i) {
                std::cout << "   " << keywords[i].name.c_str() << " = " << keywords[i].value.c_str() << std::endl;
            }
        } catch (const pcl::Error& e) {
            std::cout << "   ✗ Error reading keywords: " << e.Message().c_str() << std::endl;
        }
        
        reader.Close();
        std::cout << "✓ FITS file processed successfully!" << std::endl;
        return 0;
        
    } catch (const pcl::Error& e) {
        std::cout << "✗ PCL Error: " << e.Message().c_str() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cout << "✗ Standard Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "✗ Unknown error" << std::endl;
        return 1;
    }
}
