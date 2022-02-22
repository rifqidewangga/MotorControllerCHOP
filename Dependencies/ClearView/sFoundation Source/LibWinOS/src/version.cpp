// version.cpp file
//---------------------------------------------------------------------------
#include "version.h"
#include <assert.h>
#include <tchar.h>
#include <string.h>

//---------------------------------------------------------------------------

#pragma comment(lib, "version.lib")

CVersionInfo::CVersionInfo(void)
{
    // Set the file path variable equal to the current application path
    _TCHAR *path = new _TCHAR[MAX_PATH];
    unsigned int nLength = GetModuleFileName(NULL, path, MAX_PATH);
    m_bValidPath = (nLength > 0);

    // A valid filepath must exist
	if(!m_bValidPath)  {
		delete [] path;
        return;
	}
	m_pszFilePath = path;
    m_bVersionInfoExists = GetLanguageCodes();
	delete [] path;
}

bool CVersionInfo::SetFilePath(string szPath)
{
    bool bReturn = false;

    // Make sure the path parameter points to a valid file
    GET_FILEEX_INFO_LEVELS fxInfoLevels;
	fxInfoLevels = GetFileExInfoStandard;
    WIN32_FILE_ATTRIBUTE_DATA fileAttribs;

    // Copy path parameter to internal variable (handling Unicode reqs)
    m_pszFilePath = szPath;

    // Verify existance of file
	m_bValidPath = (GetFileAttributesEx(m_pszFilePath.c_str(), fxInfoLevels,
        (void*)&fileAttribs) ? true : false);

    // A valid filepath must be passed in
    if(m_bValidPath)
    {
        m_bVersionInfoExists = GetLanguageCodes();
        bReturn = m_bVersionInfoExists;
    }

    return bReturn;
}

CVersionInfo::~CVersionInfo(void)
{
    // Free allocated memory
    while(m_lstLangCode.size() > 0)
	{
		//delete *(m_lstLangCode.begin());
		m_lstLangCode.pop_front();
	}
}

bool CVersionInfo::GetLanguageCodes(void)
{
    bool bReturn = false;

    // Determine if version info exists for selected application
    DWORD dwVerHnd = 0;
    LPVOID  lpvMem = NULL;

	DWORD dwVerInfoSize = GetFileVersionInfoSize(m_pszFilePath.c_str(), &dwVerHnd);

    // If version info exists...
    if (dwVerInfoSize)
    {  
        bReturn = true;
        HANDLE  hMem;
        LPVOID  lpvMem;

        hMem = GlobalAlloc(GMEM_MOVEABLE, dwVerInfoSize);
        lpvMem = GlobalLock(hMem);
        GetFileVersionInfo(m_pszFilePath.c_str(), dwVerHnd, dwVerInfoSize, lpvMem);

        // Structure used to store enumerated languages and code pages.
        struct LANGANDCODEPAGE {
            WORD wLanguage;
            WORD wCodePage;
        } *lpTranslate;
        UINT cbTranslate = 0;

        // Read the list of languages and code pages.
        VerQueryValue(lpvMem, _T("\\VarFileInfo\\Translation"),
        (void**)&lpTranslate, &cbTranslate);

        // Store the code for each language in version info.
        if(m_lstLangCode.size() > 0)
            m_lstLangCode.clear();
        int nLang = 0;
        for( ;nLang<(cbTranslate/sizeof(struct LANGANDCODEPAGE));nLang++ )
        {
            LPTSTR szLangCode = new _TCHAR[26];
            wsprintf(szLangCode, _T("\\StringFileInfo\\%04x%04x\\"),
                lpTranslate[nLang].wLanguage, lpTranslate[nLang].wCodePage);
			string lCode = szLangCode;
            //m_lstLangCode.push_back(szLangCode);
			m_lstLangCode.push_back(lCode);
        }
        m_nLangCount = nLang;

        GlobalUnlock(hMem);
        GlobalFree(hMem);
    }
    else
    {            
        m_nLangCount = 0;
    }

    // Set default language
    m_nLanguage = 0;

    return bReturn;
}

bool CVersionInfo::SetLanguage(int nLanguage)
{
    bool bReturn = false;

    if(nLanguage >= 0 && nLanguage < m_nLangCount && m_nLangCount > 0)
    {          
        bReturn = true;
        m_nLanguage = nLanguage;
    }

    return bReturn;
}

int CVersionInfo::GetLanguageCount(void)
{
    return m_nLangCount;
}

string CVersionInfo::GetData(string szData)
{
//    string szReturn = NULL;

    // Create query string
	//LPTSTR szRequest = new _TCHAR[szData.length() + 26];
	string szRequest, szReturn;
    list<string>::iterator itr = m_lstLangCode.begin();

    for(int nPos=0;nPos<m_nLanguage;nPos++)
        itr++;
    //_tcscpy(szRequest, *itr);
	//_tcscat(szRequest, szData);
	szRequest = *itr;
	szRequest.append(szData);

    // Get version information from the application
    DWORD dwVerHnd = 0;
    DWORD dwVerInfoSize = GetFileVersionInfoSize(m_pszFilePath.c_str(), &dwVerHnd);
    if (dwVerInfoSize)
    {
        // If we were able to get the information, process it:
        HANDLE  hMem;
        LPVOID  lpvMem;

        hMem = GlobalAlloc(GMEM_MOVEABLE, dwVerInfoSize);
        lpvMem = GlobalLock(hMem);
        GetFileVersionInfo(m_pszFilePath.c_str(), dwVerHnd, dwVerInfoSize, lpvMem);

        LPSTR pszReturn = NULL;
        UINT nVerSize = 0;

        if(VerQueryValue(lpvMem, szRequest.c_str(),
            (void**)&pszReturn, &nVerSize))
        {
            //szReturn = new _TCHAR[_tcsclen(pszReturn) + 1];
            //_tcscpy(szReturn, pszReturn);
			//delete [] szRequest;
			szReturn = pszReturn;
        }

        GlobalUnlock(hMem);
        GlobalFree(hMem);
    }
    return szReturn;
}

string CVersionInfo::GetComments(void)
{
    return GetData(_T("Comments"));
}

string CVersionInfo::GetCompanyName(void)
{
    return GetData(_T("CompanyName"));
}

string CVersionInfo::GetFileDescription(void)
{
    return GetData(_T("FileDescription"));
}

string CVersionInfo::GetFileVersion(void)
{ 
    return GetData(_T("FileVersion"));
}

string CVersionInfo::GetInternalName(void)
{
    return GetData(_T("InternalName"));
}

string CVersionInfo::GetLegalCopyright(void)
{
    return GetData(_T("LegalCopyright"));
}

string CVersionInfo::GetLegalTrademarks(void)
{ 
    return GetData(_T("LegalTrademarks"));
}

string CVersionInfo::GetOriginalFilename(void)
{ 
    return GetData(_T("OriginalFilename"));
}

string CVersionInfo::GetProductName(void)
{ 
    return GetData(_T("ProductName"));
}

string CVersionInfo::GetProductVersion(void)
{ 
    return GetData(_T("ProductVersion"));
}

string CVersionInfo::GetPrivateBuild(void)
{ 
    return GetData(_T("PrivateBuild"));
}

string CVersionInfo::GetSpecialBuild(void)
{ 
    return GetData(_T("SpecialBuild"));
}

string CVersionInfo::GetFileDir()
{
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];
	errno_t err;
	err = _splitpath_s(m_pszFilePath.c_str(), drive, _MAX_DRIVE, dir, _MAX_DIR, fname,
		_MAX_FNAME, ext, _MAX_EXT);
	if (err != 0)
	{
		return "";
	}
	else {
		return dir;
	}
}
