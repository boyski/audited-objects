// Profiler.h : Declaration of the CProfiler

#pragma once
#include "resource.h"       // main symbols

#include "cpci_i.h"

// CProfiler

class ATL_NO_VTABLE CProfiler :
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CProfiler, &CLSID_Profiler>,
	public ICorProfilerCallback2
{
public:
	CProfiler()
	{
	}

DECLARE_REGISTRY_RESOURCEID(IDR_PROFILER)


BEGIN_COM_MAP(CProfiler)
	COM_INTERFACE_ENTRY(ICorProfilerCallback2)
	COM_INTERFACE_ENTRY(ICorProfilerCallback)
END_COM_MAP()



	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

private:
	CComQIPtr<ICorProfilerInfo2> m_spCorInfo;

	// ICorProfilerCallback2
	virtual HRESULT STDMETHODCALLTYPE ThreadNameChanged(ThreadID threadId, ULONG cchName, WCHAR name[  ]) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE GarbageCollectionStarted(int cGenerations, BOOL generationCollected[  ], COR_PRF_GC_REASON reason) { return E_NOTIMPL; };
	virtual HRESULT STDMETHODCALLTYPE SurvivingReferences(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[  ], ULONG cObjectIDRangeLength[  ]) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE GarbageCollectionFinished( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE FinalizeableObjectQueued(DWORD finalizerFlags, ObjectID objectID) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RootReferences2(ULONG cRootRefs, ObjectID rootRefIds[  ], COR_PRF_GC_ROOT_KIND rootKinds[  ], COR_PRF_GC_ROOT_FLAGS rootFlags[  ], UINT_PTR rootIds[  ]) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE HandleCreated(GCHandleID handleId, ObjectID initialObjectId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE HandleDestroyed(GCHandleID handleId) { return E_NOTIMPL; }

	// ICorProfilerCallback
	virtual HRESULT STDMETHODCALLTYPE Initialize(IUnknown *pICorProfilerInfoUnk);
	virtual HRESULT STDMETHODCALLTYPE Shutdown( void);
	virtual HRESULT STDMETHODCALLTYPE AppDomainCreationStarted(AppDomainID appDomainId) { return E_NOTIMPL; };
	virtual HRESULT STDMETHODCALLTYPE AppDomainCreationFinished(AppDomainID appDomainId, HRESULT hrStatus) { return E_NOTIMPL; };
	virtual HRESULT STDMETHODCALLTYPE AppDomainShutdownStarted(AppDomainID appDomainId) { return E_NOTIMPL; };
	virtual HRESULT STDMETHODCALLTYPE AppDomainShutdownFinished(AppDomainID appDomainId, HRESULT hrStatus) { return E_NOTIMPL; };
	virtual HRESULT STDMETHODCALLTYPE AssemblyLoadStarted(AssemblyID assemblyId) { return E_NOTIMPL; };
	virtual HRESULT STDMETHODCALLTYPE AssemblyLoadFinished(AssemblyID assemblyId, HRESULT hrStatus) { return E_NOTIMPL; };
	virtual HRESULT STDMETHODCALLTYPE AssemblyUnloadStarted(AssemblyID assemblyId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE AssemblyUnloadFinished(AssemblyID assemblyId, HRESULT hrStatus) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ModuleLoadStarted(ModuleID moduleId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ModuleLoadFinished(ModuleID moduleId, HRESULT hrStatus) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ModuleUnloadStarted(ModuleID moduleId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ModuleUnloadFinished(ModuleID moduleId, HRESULT hrStatus) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ModuleAttachedToAssembly(ModuleID moduleId, AssemblyID AssemblyId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ClassLoadStarted(ClassID classId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ClassLoadFinished(ClassID classId, HRESULT hrStatus) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ClassUnloadStarted(ClassID classId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ClassUnloadFinished(ClassID classId, HRESULT hrStatus) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE FunctionUnloadStarted(FunctionID functionId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE JITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE JITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE JITCachedFunctionSearchStarted(FunctionID functionId, BOOL *pbUseCachedFunction) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE JITCachedFunctionSearchFinished(FunctionID functionId, COR_PRF_JIT_CACHE result) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE JITFunctionPitched(FunctionID functionId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE JITInlining(FunctionID callerId, FunctionID calleeId, BOOL *pfShouldInline) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ThreadCreated(ThreadID threadId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ThreadDestroyed(ThreadID threadId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ThreadAssignedToOSThread(ThreadID managedThreadId, DWORD osThreadId){ return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RemotingClientInvocationStarted( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RemotingClientSendingMessage(GUID *pCookie, BOOL fIsAsync) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RemotingClientReceivingReply(GUID *pCookie, BOOL fIsAsync) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RemotingClientInvocationFinished( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RemotingServerReceivingMessage(GUID *pCookie, BOOL fIsAsync) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RemotingServerInvocationStarted( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RemotingServerInvocationReturned( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RemotingServerSendingReply(GUID *pCookie, BOOL fIsAsync) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE UnmanagedToManagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ManagedToUnmanagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RuntimeSuspendStarted(COR_PRF_SUSPEND_REASON suspendReason) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RuntimeSuspendFinished( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RuntimeSuspendAborted( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RuntimeResumeStarted( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RuntimeResumeFinished( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RuntimeThreadSuspended(ThreadID threadId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RuntimeThreadResumed(ThreadID threadId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE MovedReferences(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[  ], ObjectID newObjectIDRangeStart[  ], ULONG cObjectIDRangeLength[  ]) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ObjectAllocated(ObjectID objectId, ClassID classId) { return E_NOTIMPL; };
	virtual HRESULT STDMETHODCALLTYPE ObjectsAllocatedByClass(ULONG cClassCount, ClassID classIds[  ], ULONG cObjects[  ]) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ObjectReferences(ObjectID objectId, ClassID classId, ULONG cObjectRefs, ObjectID objectRefIds[  ]) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE RootReferences(ULONG cRootRefs, ObjectID rootRefIds[  ]) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionThrown(ObjectID thrownObjectId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionSearchFunctionEnter(FunctionID functionId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionSearchFunctionLeave( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionSearchFilterEnter(FunctionID functionId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionSearchFilterLeave( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionSearchCatcherFound(FunctionID functionId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionOSHandlerEnter(UINT_PTR __unused) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionOSHandlerLeave(UINT_PTR __unused) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionUnwindFunctionEnter(FunctionID functionId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionUnwindFunctionLeave( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionUnwindFinallyEnter(FunctionID functionId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionUnwindFinallyLeave( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionCatcherEnter(FunctionID functionId, ObjectID objectId) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionCatcherLeave( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE COMClassicVTableCreated(ClassID wrappedClassId, REFGUID implementedIID, void *pVTable, ULONG cSlots) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE COMClassicVTableDestroyed(ClassID wrappedClassId, REFGUID implementedIID, void *pVTable) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionCLRCatcherFound( void) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE ExceptionCLRCatcherExecute( void) { return E_NOTIMPL; }

public:

};

OBJECT_ENTRY_AUTO(__uuidof(Profiler), CProfiler)
