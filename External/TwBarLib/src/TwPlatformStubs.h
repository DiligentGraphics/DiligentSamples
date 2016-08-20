//  ---------------------------------------------------------------------------
//
//  @file       TwPlatformStubs.h
//  @brief      Platform stubs for unsupported functions
//  @author     Egor Yusov
//
//  note:       Private header
//
//  ---------------------------------------------------------------------------

#if !defined ANT_TW_PLATFORM_STUBS
#define ANT_TW_PLATFORM_STUBS

#ifdef ANT_UNIVERSAL_WINDOWS

inline int MessageBox( HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType )
{
    OutputDebugStringA( "Message Box is not supported on Windows Store platform\n" );
    OutputDebugStringA( "Message Box caption: " );
    OutputDebugString( lpCaption );
    OutputDebugStringA( "\nMessage Box text: " );
    OutputDebugString( lpText );
    OutputDebugStringA( "\n" );
    return 0;
}

#endif

#endif  // !defined ANT_TW_PLATFORM_STUBS
