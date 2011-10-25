// dllmain.h : Declaration of module class.

class CcpciModule : public CAtlDllModuleT< CcpciModule >
{
public :
	DECLARE_LIBID(LIBID_cpciLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CPCI, "{E1862F3A-A8EA-4152-9067-5FB860A5042C}")
};

extern class CcpciModule _AtlModule;
