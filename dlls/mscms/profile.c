/*
 * MSCMS - Color Management System for Wine
 *
 * Copyright 2004 Hans Leidekker
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "wine/debug.h"

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "wingdi.h"
#include "winuser.h"
#include "icm.h"

#define LCMS_API_FUNCTION(f) extern typeof(f) * p##f;
#include "lcms_api.h"
#undef LCMS_API_FUNCTION

#define IS_SEPARATOR(ch)  ((ch) == '\\' || (ch) == '/')

static void MSCMS_basename( LPCWSTR path, LPWSTR name )
{
    INT i = lstrlenW( path );

    while (i > 0 && !IS_SEPARATOR(path[i - 1])) i--;
    lstrcpyW( name, &path[i] );
}

static const WCHAR rgbprofile[] =
{ 'c',':','\\','w','i','n','d','o','w','s','\\', 's','y','s','t','e','m','3','2',
  '\\','s','p','o','o','l','\\','d','r','i','v','e','r','s',
  '\\','c','o','l','o','r','\\','s','r','g','b',' ','c','o','l','o','r',' ',
  's','p','a','c','e',' ','p','r','o','f','i','l','e','.','i','c','m',0 };

WINE_DEFAULT_DEBUG_CHANNEL(mscms);

/******************************************************************************
 * GetColorDirectoryA               [MSCMS.@]
 *
 * See GetColorDirectoryW.
 */
BOOL WINAPI GetColorDirectoryA( PCSTR machine, PSTR buffer, PDWORD size )
{
    INT len;
    LPWSTR bufferW;
    BOOL ret = FALSE;
    DWORD sizeW;

    TRACE( "( %p, %p )\n", buffer, size );

    if (machine || !size) return FALSE;

    if (!buffer)
    {
        ret = GetColorDirectoryW( NULL, NULL, &sizeW );
        *size = sizeW / sizeof(WCHAR);
        return FALSE;
    }

    sizeW = *size * sizeof(WCHAR);

    bufferW = HeapAlloc( GetProcessHeap(), 0, sizeW );

    if (bufferW)
    {
        ret = GetColorDirectoryW( NULL, bufferW, &sizeW );
        *size = WideCharToMultiByte( CP_ACP, 0, bufferW, -1, NULL, 0, NULL, NULL );

        if (ret)
        {
            len = WideCharToMultiByte( CP_ACP, 0, bufferW, *size, buffer, *size, NULL, NULL );
            if (!len) ret = FALSE;
        }

        HeapFree( GetProcessHeap(), 0, bufferW );
    }
    return ret;
}

/******************************************************************************
 * GetColorDirectoryW               [MSCMS.@]
 *
 * Get the directory where color profiles are stored.
 *
 * PARAMS
 *  machine  [I]   Name of the machine for which to get the color directory.
 *                 Must be NULL, which indicates the local machine.
 *  buffer   [I]   Buffer to receive the path name.
 *  size     [I/O] Size of the buffer in bytes. On return the variable holds
 *                 the number of bytes actually needed.
 */
BOOL WINAPI GetColorDirectoryW( PCWSTR machine, PWSTR buffer, PDWORD size )
{
    /* FIXME: Get this directory from the registry? */
    static const WCHAR colordir[] =
    { 'c',':','\\','w','i','n','d','o','w','s','\\', 's','y','s','t','e','m','3','2',
      '\\','s','p','o','o','l','\\','d','r','i','v','e','r','s','\\','c','o','l','o','r',0 };

    DWORD len;

    TRACE( "( %p, %p )\n", buffer, size );

    if (machine || !size) return FALSE;

    len = lstrlenW( colordir ) * sizeof(WCHAR);

    if (len <= *size && buffer)
    {
        lstrcpyW( buffer, colordir );
        *size = len;
        return TRUE;
    }

    *size = len;
    return FALSE;
}

/******************************************************************************
 * GetColorProfileElement               [MSCMS.@]
 *
 * Retrieve data for a specified tag type.
 *
 * PARAMS
 *  profile  [I]   Handle to a color profile.
 *  type     [I]   ICC tag type. 
 *  offset   [I]   Offset in bytes to start copying from. 
 *  size     [I/O] Size of the buffer in bytes. On return the variable holds
 *                 the number of bytes actually needed.
 *  buffer   [O]   Buffer to receive the tag data.
 *  ref      [O]   Pointer to a BOOL that specifies whether more than one tag
 *                 references the data.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI GetColorProfileElement( HPROFILE profile, TAGTYPE type, DWORD offset, PDWORD size,
                                    PVOID buffer, PBOOL ref )
{
    BOOL ret = FALSE;
#ifdef HAVE_LCMS_H
    icProfile *iccprofile = MSCMS_hprofile2iccprofile( profile );
    DWORD i, count;
    icTag tag;

    TRACE( "( %p, 0x%08lx, %ld, %p, %p, %p )\n", profile, type, offset, size, buffer, ref );

    if (!iccprofile || !size || !ref) return FALSE;
    count = MSCMS_get_tag_count( iccprofile );

    for (i = 0; i < count; i++)
    {
        MSCMS_get_tag_by_index( iccprofile, i, &tag );

        if (tag.sig == type)
        {
            if ((tag.size - offset) > *size || !buffer)
            {
                *size = (tag.size - offset);
                return FALSE;
            }

            MSCMS_get_tag_data( iccprofile, &tag, offset, buffer );

            *ref = FALSE; /* FIXME: calculate properly */
            return TRUE;
        }
    }

#endif /* HAVE_LCMS_H */
    return ret;
}

/******************************************************************************
 * GetColorProfileElementTag               [MSCMS.@]
 *
 * Get the tag type from a color profile by index. 
 *
 * PARAMS
 *  profile  [I]   Handle to a color profile.
 *  index    [I]   Index into the tag table of the color profile.
 *  type     [O]   Pointer to a variable that holds the ICC tag type on return.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * NOTES
 *  The tag table index starts at 1.
 *  Use GetCountColorProfileElements to retrieve a count of tagged elements.
 */
BOOL WINAPI GetColorProfileElementTag( HPROFILE profile, DWORD index, PTAGTYPE type )
{
    BOOL ret = FALSE;
#ifdef HAVE_LCMS_H
    icProfile *iccprofile = MSCMS_hprofile2iccprofile( profile );
    DWORD count;
    icTag tag;

    TRACE( "( %p, %ld, %p )\n", profile, index, type );

    if (!iccprofile || !type) return FALSE;

    count = MSCMS_get_tag_count( iccprofile );
    if (index > count || index < 1) return FALSE;

    MSCMS_get_tag_by_index( iccprofile, index - 1, &tag );
    *type = tag.sig;

    ret = TRUE;

#endif /* HAVE_LCMS_H */
    return ret;
}

/******************************************************************************
 * GetColorProfileFromHandle               [MSCMS.@]
 *
 * Retrieve an ICC color profile by handle.
 *
 * PARAMS
 *  profile  [I]   Handle to a color profile.
 *  buffer   [O]   Buffer to receive the ICC profile.
 *  size     [I/O] Size of the buffer in bytes. On return the variable holds the
 *                 number of bytes actually needed.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * NOTES
 *  The profile returned will be in big-endian format.
 */
BOOL WINAPI GetColorProfileFromHandle( HPROFILE profile, PBYTE buffer, PDWORD size )
{
    BOOL ret = FALSE;
#ifdef HAVE_LCMS_H
    icProfile *iccprofile = MSCMS_hprofile2iccprofile( profile );
    PROFILEHEADER header;

    TRACE( "( %p, %p, %p )\n", profile, buffer, size );

    if (!iccprofile || !size) return FALSE;
    MSCMS_get_profile_header( iccprofile, &header );

    if (!buffer || header.phSize > *size)
    {
        *size = header.phSize;
        return FALSE;
    }

    /* No endian conversion needed */
    memcpy( buffer, iccprofile, header.phSize );

    *size = header.phSize;
    ret = TRUE;

#endif /* HAVE_LCMS_H */
    return ret;
}

/******************************************************************************
 * GetColorProfileHeader               [MSCMS.@]
 *
 * Retrieve a color profile header by handle.
 *
 * PARAMS
 *  profile  [I]   Handle to a color profile.
 *  header   [O]   Buffer to receive the ICC profile header.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * NOTES
 *  The profile header returned will be adjusted for endianess.
 */
BOOL WINAPI GetColorProfileHeader( HPROFILE profile, PPROFILEHEADER header )
{
    BOOL ret = FALSE;
#ifdef HAVE_LCMS_H
    icProfile *iccprofile = MSCMS_hprofile2iccprofile( profile );

    TRACE( "( %p, %p )\n", profile, header );

    if (!iccprofile || !header) return FALSE;

    MSCMS_get_profile_header( iccprofile, header );
    return TRUE;

#endif /* HAVE_LCMS_H */
    return ret;
}

/******************************************************************************
 * GetCountColorProfileElements               [MSCMS.@]
 *
 * Retrieve the number of elements in a color profile.
 *
 * PARAMS
 *  profile  [I] Handle to a color profile.
 *  count    [O] Pointer to a variable which is set to the number of elements
 *               in the color profile.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI GetCountColorProfileElements( HPROFILE profile, PDWORD count )
{
    BOOL ret = FALSE;
#ifdef HAVE_LCMS_H
    icProfile *iccprofile = MSCMS_hprofile2iccprofile( profile );

    TRACE( "( %p, %p )\n", profile, count );

    if (!iccprofile || !count) return FALSE;
    *count = MSCMS_get_tag_count( iccprofile );
    ret = TRUE;

#endif /* HAVE_LCMS_H */
    return ret;
}

BOOL WINAPI GetStandardColorSpaceProfileA( PCSTR machine, DWORD id, PSTR profile, PDWORD size )
{
    INT len;
    LPWSTR profileW;
    BOOL ret = FALSE;
    DWORD sizeW;

    TRACE( "( 0x%08lx, %p, %p )\n", id, profile, size );

    if (machine || !size) return FALSE;

    sizeW = *size * sizeof(WCHAR);

    if (!profile)
    {
        ret = GetStandardColorSpaceProfileW( NULL, id, NULL, &sizeW );
        *size = sizeW / sizeof(WCHAR);
        return FALSE;
    }

    profileW = HeapAlloc( GetProcessHeap(), 0, sizeW );

    if (profileW)
    {
        ret = GetStandardColorSpaceProfileW( NULL, id, profileW, &sizeW );
        *size = WideCharToMultiByte( CP_ACP, 0, profileW, -1, NULL, 0, NULL, NULL );

        if (ret)
        {
            len = WideCharToMultiByte( CP_ACP, 0, profileW, *size, profile, *size, NULL, NULL );
            if (!len) ret = FALSE;
        }

        HeapFree( GetProcessHeap(), 0, profileW );
    }
    return ret;
}

BOOL WINAPI GetStandardColorSpaceProfileW( PCWSTR machine, DWORD id, PWSTR profile, PDWORD size )
{
    DWORD len;

    TRACE( "( 0x%08lx, %p, %p )\n", id, profile, size );

    if (machine || !size) return FALSE;

    switch (id)
    {
        case 0x52474220: /* 'RGB ' */
            len = sizeof( rgbprofile );

            if (*size < len || !profile)
            {
                *size = len;
                return TRUE;
            }

            lstrcpyW( profile, rgbprofile );
            break;

        default:
            return FALSE;
    }

    return TRUE;
}

/******************************************************************************
 * InstallColorProfileA               [MSCMS.@]
 *
 * See InstallColorProfileW.
 */
BOOL WINAPI InstallColorProfileA( PCSTR machine, PCSTR profile )
{
    UINT len;
    LPWSTR profileW;
    BOOL ret = FALSE;

    TRACE( "( %s )\n", debugstr_a(profile) );

    if (machine || !profile) return FALSE;

    len = MultiByteToWideChar( CP_ACP, 0, profile, -1, NULL, 0 );
    profileW = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );

    if (profileW)
    {
        MultiByteToWideChar( CP_ACP, 0, profile, -1, profileW, len );

        ret = InstallColorProfileW( NULL, profileW );
        HeapFree( GetProcessHeap(), 0, profileW );
    }
    return ret;
}

/******************************************************************************
 * InstallColorProfileW               [MSCMS.@]
 *
 * Install a color profile.
 *
 * PARAMS
 *  machine  [I] Name of the machine to install the profile on. Must be NULL,
 *               which indicates the local machine.
 *  profile  [I] Full path name of the profile to install.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI InstallColorProfileW( PCWSTR machine, PCWSTR profile )
{
    WCHAR dest[MAX_PATH], base[MAX_PATH];
    DWORD size = sizeof(dest);
    static const WCHAR slash[] = { '\\', 0 };

    TRACE( "( %s )\n", debugstr_w(profile) );

    if (machine || !profile) return FALSE;

    if (!GetColorDirectoryW( machine, dest, &size )) return FALSE;

    MSCMS_basename( profile, base );

    lstrcatW( dest, slash );
    lstrcatW( dest, base );

    /* Is source equal to destination? */
    if (!lstrcmpW( profile, dest )) return TRUE;

    return CopyFileW( profile, dest, TRUE );
}

/******************************************************************************
 * IsColorProfileTagPresent               [MSCMS.@]
 *
 * Determine if a given ICC tag type is present in a color profile.
 *
 * PARAMS
 *  profile  [I] Color profile handle.
 *  tag      [I] ICC tag type.
 *  present  [O] Pointer to a BOOL variable. Set to TRUE if tag type is present,
 *               FALSE otherwise.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI IsColorProfileTagPresent( HPROFILE profile, TAGTYPE type, PBOOL present )
{
    BOOL ret = FALSE;
#ifdef HAVE_LCMS_H
    cmsHPROFILE cmsprofile = MSCMS_hprofile2cmsprofile( profile );

    TRACE( "( %p, 0x%08lx, %p )\n", profile, type, present );

    if (!cmsprofile || !present) return FALSE;
    ret = cmsIsTag( cmsprofile, type );

#endif /* HAVE_LCMS_H */
    return *present = ret;
}

/******************************************************************************
 * IsColorProfileValid               [MSCMS.@]
 *
 * Determine if a given color profile is valid.
 *
 * PARAMS
 *  profile  [I] Color profile handle.
 *  valid    [O] Pointer to a BOOL variable. Set to TRUE if profile is valid,
 *               FALSE otherwise.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE 
 */
BOOL WINAPI IsColorProfileValid( HPROFILE profile, PBOOL valid )
{
    BOOL ret = FALSE;
#ifdef HAVE_LCMS_H
    icProfile *iccprofile = MSCMS_hprofile2iccprofile( profile );

    TRACE( "( %p, %p )\n", profile, valid );

    if (!valid) return FALSE;
    if (iccprofile) return *valid = TRUE;

#endif /* HAVE_LCMS_H */
    return ret;
}

/******************************************************************************
 * SetColorProfileElement               [MSCMS.@]
 *
 * Set data for a specified tag type.
 *
 * PARAMS
 *  profile  [I]   Handle to a color profile.
 *  type     [I]   ICC tag type.
 *  offset   [I]   Offset in bytes to start copying to.
 *  size     [I/O] Size of the buffer in bytes. On return the variable holds the
 *                 number of bytes actually needed.
 *  buffer   [O]   Buffer holding the tag data.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI SetColorProfileElement( HPROFILE profile, TAGTYPE type, DWORD offset, PDWORD size,
                                    PVOID buffer )
{
    BOOL ret = FALSE;
#ifdef HAVE_LCMS_H
    icProfile *iccprofile = MSCMS_hprofile2iccprofile( profile );
    DWORD i, count;
    icTag tag;

    TRACE( "( %p, 0x%08lx, %ld, %p, %p )\n", profile, type, offset, size, buffer );

    if (!iccprofile || !size || !buffer) return FALSE;
    count = MSCMS_get_tag_count( iccprofile );

    for (i = 0; i < count; i++)
    {
        MSCMS_get_tag_by_index( iccprofile, i, &tag );

        if (tag.sig == type)
        {
            if (offset > tag.size) return FALSE;

            MSCMS_set_tag_data( iccprofile, &tag, offset, buffer );
            return TRUE;
        }
    }

#endif /* HAVE_LCMS_H */
    return ret;
}

BOOL WINAPI SetColorProfileHeader( HPROFILE profile, PPROFILEHEADER header )
{
    BOOL ret = FALSE;
#ifdef HAVE_LCMS_H
    icProfile *iccprofile = MSCMS_hprofile2iccprofile( profile );

    TRACE( "( %p, %p )\n", profile, header );

    if (!iccprofile || !header) return FALSE;

    MSCMS_set_profile_header( iccprofile, header );
    return TRUE;

#endif /* HAVE_LCMS_H */
    return ret;
}

BOOL WINAPI SetStandardColorSpaceProfileA( PCSTR machine, DWORD id, PSTR profile )
{
    FIXME( "( 0x%08lx, %p ) stub\n", id, profile );

    return TRUE;
}

BOOL WINAPI SetStandardColorSpaceProfileW( PCWSTR machine, DWORD id, PWSTR profile )
{
    FIXME( "( 0x%08lx, %p ) stub\n", id, profile );

    return TRUE;
}

/******************************************************************************
 * UninstallColorProfileA               [MSCMS.@]
 *
 * See UninstallColorProfileW.
 */
BOOL WINAPI UninstallColorProfileA( PCSTR machine, PCSTR profile, BOOL delete )
{
    UINT len;
    LPWSTR profileW;
    BOOL ret = FALSE;

    TRACE( "( %s, %x )\n", debugstr_a(profile), delete );

    if (machine || !profile) return FALSE;

    len = MultiByteToWideChar( CP_ACP, 0, profile, -1, NULL, 0 );
    profileW = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );

    if (profileW)
    {
        MultiByteToWideChar( CP_ACP, 0, profile, -1, profileW, len );

        ret = UninstallColorProfileW( NULL, profileW , delete );

        HeapFree( GetProcessHeap(), 0, profileW );
    }
    return ret;
}

/******************************************************************************
 * UninstallColorProfileW               [MSCMS.@]
 *
 * Uninstall a color profile.
 *
 * PARAMS
 *  machine  [I] Name of the machine to uninstall the profile on. Must be NULL,
 *               which indicates the local machine.
 *  profile  [I] Full path name of the profile to uninstall.
 *  delete   [I] Bool that specifies whether the profile file should be deleted.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI UninstallColorProfileW( PCWSTR machine, PCWSTR profile, BOOL delete )
{
    TRACE( "( %s, %x )\n", debugstr_w(profile), delete );

    if (machine || !profile) return FALSE;

    if (delete) return DeleteFileW( profile );

    return TRUE;
}

/******************************************************************************
 * OpenColorProfileA               [MSCMS.@]
 *
 * See OpenColorProfileW.
 */
HPROFILE WINAPI OpenColorProfileA( PPROFILE profile, DWORD access, DWORD sharing, DWORD creation )
{
    HPROFILE handle = NULL;

    TRACE( "( %p, 0x%08lx, 0x%08lx, 0x%08lx )\n", profile, access, sharing, creation );

    if (!profile || !profile->pProfileData) return NULL;

    /* No AW conversion needed for memory based profiles */
    if (profile->dwType & PROFILE_MEMBUFFER)
        return OpenColorProfileW( profile, access, sharing, creation );

    if (profile->dwType & PROFILE_FILENAME)
    {
        UINT len;
        PROFILE profileW;

        profileW.dwType = profile->dwType;
 
        len = MultiByteToWideChar( CP_ACP, 0, profile->pProfileData, -1, NULL, 0 );
        profileW.pProfileData = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );

        if (profileW.pProfileData)
        {
            profileW.cbDataSize = len * sizeof(WCHAR);
            MultiByteToWideChar( CP_ACP, 0, profile->pProfileData, -1, profileW.pProfileData, len );

            handle = OpenColorProfileW( &profileW, access, sharing, creation );
            HeapFree( GetProcessHeap(), 0, profileW.pProfileData );
        }
    }
    return handle;
}

/******************************************************************************
 * OpenColorProfileW               [MSCMS.@]
 *
 * Open a color profile.
 *
 * PARAMS
 *  profile   [I] Pointer to a color profile structure.
 *  access    [I] Desired access.
 *  sharing   [I] Sharing mode.
 *  creation  [I] Creation mode.
 *
 * RETURNS
 *  Success: Handle to the opened profile.
 *  Failure: NULL
 *
 * NOTES
 *  Values for access:   PROFILE_READ or PROFILE_READWRITE.
 *  Values for sharing:  0 (no sharing), FILE_SHARE_READ and/or FILE_SHARE_WRITE.
 *  Values for creation: one of CREATE_NEW, CREATE_ALWAYS, OPEN_EXISTING,
 *                       OPEN_ALWAYS, TRUNCATE_EXISTING.
 *  Sharing and creation flags are ignored for memory based profiles.
 */
HPROFILE WINAPI OpenColorProfileW( PPROFILE profile, DWORD access, DWORD sharing, DWORD creation )
{
#ifdef HAVE_LCMS_H
    cmsHPROFILE cmsprofile = NULL;
    icProfile *iccprofile = NULL;
    HANDLE handle = NULL;
    DWORD size;

    TRACE( "( %p, 0x%08lx, 0x%08lx, 0x%08lx )\n", profile, access, sharing, creation );

    if (!profile || !profile->pProfileData) return NULL;

    if (profile->dwType & PROFILE_MEMBUFFER)
    {
        FIXME( "access flags not implemented for memory based profiles\n" );

        iccprofile = profile->pProfileData;
        size = profile->cbDataSize;
    
        cmsprofile = cmsOpenProfileFromMem( iccprofile, size );
    }

    if (profile->dwType & PROFILE_FILENAME)
    {
        DWORD read, flags = 0;

        TRACE( "profile file: %s\n", debugstr_w( (WCHAR *)profile->pProfileData ) );

        if (access & PROFILE_READ) flags = GENERIC_READ;
        if (access & PROFILE_READWRITE) flags = GENERIC_READ|GENERIC_WRITE;

        if (!flags) return NULL;

        handle = CreateFileW( profile->pProfileData, flags, sharing, NULL, creation, 0, NULL );
        if (handle == INVALID_HANDLE_VALUE)
        {
            WARN( "Unable to open color profile\n" );
            return NULL;
        }

        if ((size = GetFileSize( handle, NULL )) == INVALID_FILE_SIZE)
        {
            ERR( "Unable to retrieve size of color profile\n" );
            CloseHandle( handle );
            return NULL;
        }

        iccprofile = (icProfile *)HeapAlloc( GetProcessHeap(), 0, size );
        if (!iccprofile)
        {
            ERR( "Unable to allocate memory for color profile\n" );
            CloseHandle( handle );
            return NULL;
        }

        if (!ReadFile( handle, iccprofile, size, &read, NULL ) || read != size)
        {
            ERR( "Unable to read color profile\n" );

            CloseHandle( handle );
            HeapFree( GetProcessHeap, 0, iccprofile );
            return NULL;
        }

        cmsprofile = cmsOpenProfileFromMem( iccprofile, size );
    }

    if (cmsprofile)
        return MSCMS_create_hprofile_handle( handle, iccprofile, cmsprofile );

#endif /* HAVE_LCMS_H */
    return NULL;
}

/******************************************************************************
 * CloseColorProfile               [MSCMS.@]
 *
 * Close a color profile.
 *
 * PARAMS
 *  profile  [I] Handle to the profile.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CloseColorProfile( HPROFILE profile )
{
    BOOL ret = FALSE;

    TRACE( "( %p )\n", profile );

#ifdef HAVE_LCMS_H
    ret = cmsCloseProfile( MSCMS_hprofile2cmsprofile( profile ) );

    HeapFree( GetProcessHeap(), 0, MSCMS_hprofile2iccprofile( profile ) );
    CloseHandle( MSCMS_hprofile2handle( profile ) );

    MSCMS_destroy_hprofile_handle( profile );

#endif /* HAVE_LCMS_H */
    return ret;
}
