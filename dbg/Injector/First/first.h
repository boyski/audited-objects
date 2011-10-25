// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the FIRST_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// FIRST_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef FIRST_EXPORTS
#define FIRST_API __declspec(dllexport)
#else
#define FIRST_API __declspec(dllimport)
#endif

typedef BOOL (__cdecl *INJECTOR)(HANDLE, HANDLE, LPCSTR, LPCSTR, void *);
extern FIRST_API BOOL Inject(HANDLE, HANDLE, LPCSTR, LPCSTR, void *);

typedef void (__cdecl *PREMAIN)(void);
extern FIRST_API void PreMain(void);
