
#include <experimental/filesystem>
#include "Main.h"
extern "C" { FILE __iob_func[ 3 ] = { *stdin,*stdout,*stderr }; }
#pragma comment(lib,"legacy_stdio_definitions.lib")
#include "blpaletter.h"


#define MASK_56 (((u_int64_t)1<<56)-1) /* i.e., (u_int64_t)0xffffffffffffff */

#include "fnv.h"

u_int64_t GetBufHash( const char * data, size_t data_len )
{
	u_int64_t hash;
	hash = fnv_64_buf( ( void * )data, ( size_t )data_len, FNV1_64_INIT );
	hash = ( hash >> 56 ) ^ ( hash & MASK_56 );
	return hash;
}

struct ICONMDLCACHE
{
	u_int64_t hash;
	size_t hashlen;
	char * buf;
	size_t size;
};

vector<ICONMDLCACHE> ICONMDLCACHELIST;
vector<FileRedirectStruct> FileRedirectList;


BOOL GetFromIconMdlCache( string filename, ICONMDLCACHE * iconhelperout )
{
	size_t filelen = filename.length( );
	u_int64_t hash = GetBufHash( filename.c_str( ), filelen );
	for ( ICONMDLCACHE ih : ICONMDLCACHELIST )
	{
		if ( ih.hashlen == filelen && ih.hash == hash )
		{
			*iconhelperout = ih;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL IsFileRedirected( string filename )
{
	for ( FileRedirectStruct DotaRedirectHelp : FileRedirectList )
	{
		if (filename == DotaRedirectHelp.NewFilePath )
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL IsMemInCache( int addr )
{
	for ( ICONMDLCACHE ih : ICONMDLCACHELIST )
	{
		if ( ( int )ih.buf == addr )
			return TRUE;
	}
	return FALSE;
}

void FreeAllIHelpers( )
{
	if ( !ICONMDLCACHELIST.empty( ) )
	{
		for ( ICONMDLCACHE ih : ICONMDLCACHELIST )
		{
			delete[ ] ih.buf;
		}

		ICONMDLCACHELIST.clear( );
	}
	if ( !FileRedirectList.empty( ) )
		FileRedirectList.clear( );

	if ( !FakeFileList.empty( ) )
		FakeFileList.clear( );
}



void WINAPI SMemZero( LPVOID lpDestination, DWORD dwLength )
{
	DWORD dwPrevLen = dwLength;
	LPDWORD lpdwDestination = ( LPDWORD )lpDestination;
	LPBYTE lpbyDestination;

	dwLength >>= 2;

	while ( dwLength-- )
		*lpdwDestination++ = 0;

	lpbyDestination = ( LPBYTE )lpdwDestination;

	dwLength = dwPrevLen;
	dwLength &= 3;

	while ( dwLength-- )
		*lpbyDestination++ = 0;
}

LPVOID WINAPI SMemAlloc( LPVOID lpvMemory, DWORD dwSize )
{
	LPVOID lpMemory = VirtualAlloc( lpvMemory, dwSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );// malloc( dwSize );
	if ( lpMemory ) SMemZero( lpMemory, dwSize );
	return lpMemory;
}

void WINAPI SMemFree( LPVOID lpvMemory )
{
	if ( lpvMemory ) VirtualFree( lpvMemory, 0, MEM_RELEASE );// Storm_403_org( lpvMemory, "delete", -1, 0 );
}

DWORD WINAPI SMemCopy( LPVOID lpDestination, LPCVOID lpSource, DWORD dwLength )
{
	DWORD dwPrevLen = dwLength;
	LPDWORD lpdwDestination = ( LPDWORD )lpDestination, lpdwSource = ( LPDWORD )lpSource;
	LPBYTE lpbyDestination, lpbySource;

	dwLength >>= 2;

	while ( dwLength-- )
		*lpdwDestination++ = *lpdwSource++;

	lpbyDestination = ( LPBYTE )lpdwDestination;
	lpbySource = ( LPBYTE )lpdwSource;

	dwLength = dwPrevLen;
	dwLength &= 3;

	while ( dwLength-- )
		*lpbyDestination++ = *lpbySource++;

	return dwPrevLen;
}


char MPQFilePath[ 4000 ];

const char * DisabledIconSignature = "Disabled\\DIS";
const char * DisabledIconSignature2 = "Disabled\\DISDIS";
const char * CommandButtonsDisabledIconSignature = "CommandButtonsDisabled\\DIS";


BOOL replaceAll( std::string& str, const std::string& from, const std::string& to )
{
	BOOL Replaced = FALSE;
	if ( from.empty( ) )
		return Replaced;
	size_t start_pos = 0;
	while ( ( start_pos = str.find( from, start_pos ) ) != std::string::npos )
	{
		str.replace( start_pos, from.length( ), to );
		start_pos += to.length( );
		Replaced = TRUE;
	}
	return Replaced;
}


GameGetFile GameGetFile_org = NULL;
GameGetFile GameGetFile_ptr;

int idddd = 0;

void ApplyTerrainFilter( string filename, int * OutDataPointer, size_t * OutSize, BOOL IsTga )
{
#ifdef DOTA_HELPER_LOG
	AddNewLineToDotaHelperLog( "ApplyTerrainFilter" );
#endif
	ICONMDLCACHE tmpih;
	BOOL FoundOldHelper = GetFromIconMdlCache( filename.c_str( ), &tmpih );
	if ( FoundOldHelper )
	{
		*OutDataPointer = ( int )tmpih.buf;
		*OutSize = tmpih.size;
		return;
	}

	char * originfiledata = ( char * )( int )*OutDataPointer;
	size_t sz = *OutSize;


	int w = 0, h = 0, bpp = 0, mipmaps = 0, alphaflag = 8, compress = 1, alphaenconding = 5;
	unsigned long rawImageSize = 0;
	Buffer InBuffer;
	InBuffer.buf = ( char* )originfiledata;
	InBuffer.length = sz;
	Buffer OutBuffer;
	if ( IsTga )
		rawImageSize = ( unsigned long )TGA2Raw( InBuffer, OutBuffer, w, h, bpp, filename.c_str( ) );
	else
		rawImageSize = Blp2Raw( InBuffer, OutBuffer, w, h, bpp, mipmaps, alphaflag, compress, alphaenconding, filename.c_str( ) );
	if ( rawImageSize > 0 )
	{
		BGRAPix * OutImage = ( BGRAPix* )OutBuffer.buf;
		for ( unsigned long i = 0; i < OutBuffer.length / 4; i++ )
		{
			if ( /*OutImage[ i ].A == 0xFF && */( OutImage[ i ].G > 40 || OutImage[ i ].B > 40 || OutImage[ i ].R > 40 ) )
			{
				if ( OutImage[ i ].R < 235 && OutImage[ i ].G < 235 && OutImage[ i ].B < 235 )
				{
					OutImage[ i ].R += 25;
					OutImage[ i ].G += 25;
					OutImage[ i ].B += 25;
				}
				OutImage[ i ].R = 0;
				OutImage[ i ].G = 0;
				OutImage[ i ].B = 0;
			}
			else if ( true /*OutImage[ i ].A == 0xFF*/ )
			{
				if ( OutImage[ i ].R > 0 && OutImage[ i ].R < 250 )
					OutImage[ i ].R += 5;
				if ( OutImage[ i ].G > 0 && OutImage[ i ].G < 250 )
					OutImage[ i ].G += 5;
				if ( OutImage[ i ].B > 0 && OutImage[ i ].B < 250 )
					OutImage[ i ].B += 5;

				OutImage[ i ].R = 170;
				OutImage[ i ].G = 170;
				OutImage[ i ].B = 170;
				OutImage[ i ].R = 0;
				OutImage[ i ].G = 0;
				OutImage[ i ].B = 0;
			}
		}


		Buffer ResultBuffer;

		CreatePalettedBLP( OutBuffer, ResultBuffer, 256, filename.c_str( ), w, h, bpp, alphaflag, mipmaps );

		if ( OutBuffer.buf != NULL )
		{
			OutBuffer.length = 0;
			delete[ ] OutBuffer.buf;
			OutBuffer.buf = 0;
		}

		if ( ResultBuffer.buf != NULL )
		{
			//	MessageBoxA( 0, "OK5", "OK5", 0 );
			tmpih.buf = ResultBuffer.buf;
			tmpih.size = ResultBuffer.length;
			tmpih.hashlen = filename.length( );
			tmpih.hash = GetBufHash( filename.c_str( ), tmpih.hashlen );
			ICONMDLCACHELIST.push_back( tmpih );
			if ( !IsMemInCache( *OutDataPointer ) )
				Storm_403_org( ( void* )*OutDataPointer, "delete", -1, 0 );
			*OutDataPointer = ( int )tmpih.buf;
			*OutSize = tmpih.size;
		}
	}

}



int __stdcall ApplyTerrainFilterDirectly( char * filename, int * OutDataPointer, size_t * OutSize, BOOL IsTga )
{
	ApplyTerrainFilter( filename, OutDataPointer, OutSize, IsTga );
	return 0;
}


void ApplyIconFilter( string filename, int * OutDataPointer, size_t * OutSize )
{
#ifdef DOTA_HELPER_LOG
	AddNewLineToDotaHelperLog( "ApplyIconFilter" );
#endif
	ICONMDLCACHE tmpih;
	BOOL FoundOldHelper = GetFromIconMdlCache( filename.c_str( ), &tmpih );
	if ( FoundOldHelper )
	{
		*OutDataPointer = ( int )tmpih.buf;
		*OutSize = tmpih.size;
		return;
	}

	char * originfiledata = ( char * )( int )*OutDataPointer;
	size_t sz = *OutSize;


	int w = 0, h = 0, bpp = 0, mipmaps = 0, alphaflag = 0, compress = 0, alphaenconding = 0;
	unsigned long rawImageSize = 0;
	Buffer InBuffer;
	InBuffer.buf = ( char* )originfiledata;
	InBuffer.length = sz;
	Buffer OutBuffer;

	rawImageSize = Blp2Raw( InBuffer, OutBuffer, w, h, bpp, mipmaps, alphaflag, compress, alphaenconding, filename.c_str( ) );
	if ( rawImageSize > 0 )
	{
		BGRAPix * OutImage = ( BGRAPix* )OutBuffer.buf;
		BGRAPix BlackPix;

		BlackPix.A = 0xFF;
		BlackPix.R = 0;
		BlackPix.G = 0;
		BlackPix.B = 0;

		for ( int x = 0; x < 4; x++ )
		{
			for ( int y = 0; y < 64; y++ )
			{
				OutImage[ x * 64 + y ] = BlackPix;//����
				OutImage[ y * 64 + x ] = BlackPix;//����
				OutImage[ ( 63 - x ) * 64 + y ] = BlackPix;//���
				OutImage[ y * 64 + 63 - x ] = BlackPix;//�����
			}
		}

		for ( int x = 4; x < 60; x++ )
		{
			for ( int y = 4; y < 60; y++ )
			{
				int id = x * 64 + y;
				int ave = ( min( min( OutImage[ id ].R, OutImage[ id ].G ), OutImage[ id ].B ) + max( max( OutImage[ id ].R, OutImage[ id ].G ), OutImage[ id ].B ) ) / 2;
				OutImage[ id ].R = FixBounds( ( ave + OutImage[ id ].R ) / 4 );
				OutImage[ id ].G = FixBounds( ( ave + OutImage[ id ].G ) / 4 );
				OutImage[ id ].B = FixBounds( ( ave + OutImage[ id ].B ) / 4 );
			}
		}

		//����������� �����
		//8 ����� ���������
		for ( int x = 4; x < 12; x++ )
		{
			for ( int y = x; y < 64 - x; y++ )
			{
				double colorfix = ( x - 3.0 ) / 9.0;

				OutImage[ x * 64 + y ].R = FixBounds( colorfix * OutImage[ x * 64 + y ].R );//����
				OutImage[ x * 64 + y ].G = FixBounds( colorfix * OutImage[ x * 64 + y ].G );//����
				OutImage[ x * 64 + y ].B = FixBounds( colorfix * OutImage[ x * 64 + y ].B );//����

				OutImage[ y * 64 + x ].R = FixBounds( colorfix * OutImage[ y * 64 + x ].R );//����
				OutImage[ y * 64 + x ].G = FixBounds( colorfix * OutImage[ y * 64 + x ].G );//����
				OutImage[ y * 64 + x ].B = FixBounds( colorfix * OutImage[ y * 64 + x ].B );//����

				OutImage[ ( 63 - x ) * 64 + y ].R = FixBounds( colorfix * OutImage[ ( 63 - x ) * 64 + y ].R );//���
				OutImage[ ( 63 - x ) * 64 + y ].G = FixBounds( colorfix * OutImage[ ( 63 - x ) * 64 + y ].G );//���
				OutImage[ ( 63 - x ) * 64 + y ].B = FixBounds( colorfix * OutImage[ ( 63 - x ) * 64 + y ].B );//���

				OutImage[ y * 64 + 63 - x ].R = FixBounds( colorfix * OutImage[ y * 64 + 63 - x ].R );//�����
				OutImage[ y * 64 + 63 - x ].G = FixBounds( colorfix * OutImage[ y * 64 + 63 - x ].G );//�����
				OutImage[ y * 64 + 63 - x ].B = FixBounds( colorfix * OutImage[ y * 64 + 63 - x ].B );//�����
			}
		}

		Buffer ResultBuffer;

		CreatePalettedBLP( OutBuffer, ResultBuffer, 256, filename.c_str( ), w, h, bpp, alphaflag, mipmaps );

		if ( OutBuffer.buf != NULL )
		{
			OutBuffer.length = 0;
			delete[ ] OutBuffer.buf;
			OutBuffer.buf = 0;
		}

		if ( ResultBuffer.buf != NULL )
		{
			tmpih.buf = ResultBuffer.buf;
			tmpih.size = ResultBuffer.length;
			tmpih.hashlen = filename.length( );
			tmpih.hash = GetBufHash( filename.c_str( ), tmpih.hashlen );
			ICONMDLCACHELIST.push_back( tmpih );
			if ( !IsMemInCache( *OutDataPointer ) )
				Storm_403_org( ( void* )*OutDataPointer, "delete", -1, 0 );
			*OutDataPointer = ( int )tmpih.buf;
			*OutSize = tmpih.size;
		}
	}

}

BOOL FixDisabledIconPath( string _filename, int * OutDataPointer, size_t * OutSize, BOOL unknown )
{
	string filename = _filename;
#ifdef DOTA_HELPER_LOG
	AddNewLineToDotaHelperLog( "FixDisabledIconPath" );
#endif

	BOOL CreateDarkIcon = FALSE;
	BOOL result = FALSE;
	if ( filename.find( DisabledIconSignature2 ) != string::npos )
	{
		if ( replaceAll( filename, DisabledIconSignature2, "\\" ) )
		{
			result = GameGetFile_ptr( filename.c_str( ), OutDataPointer, OutSize, unknown );
			if ( result )
				CreateDarkIcon = TRUE;
		}
	}


	if ( !result )
	{
		filename = _filename;
		if ( filename.find( DisabledIconSignature ) != string::npos )
		{
			if ( replaceAll( filename, DisabledIconSignature, "\\" ) )
			{
				result = GameGetFile_ptr( filename.c_str( ), OutDataPointer, OutSize, unknown );
				if ( result )
					CreateDarkIcon = TRUE;
			}
		}
	}

	if ( !result )
	{
		filename = _filename;
		if ( filename.find( DisabledIconSignature ) != string::npos )
		{
			if ( replaceAll( filename, CommandButtonsDisabledIconSignature, "PassiveButtons\\" ) )
			{
				result = GameGetFile_ptr( filename.c_str( ), OutDataPointer, OutSize, unknown );
				if ( result )
					CreateDarkIcon = TRUE;
			}
		}
	}


	if ( !result )
	{
		filename = _filename;
		if ( filename.find( DisabledIconSignature ) != string::npos )
		{
			if ( replaceAll( filename, CommandButtonsDisabledIconSignature, "AutoCastButtons\\" ) )
			{
				result = GameGetFile_ptr( filename.c_str( ), OutDataPointer, OutSize, unknown );
				if ( result )
					CreateDarkIcon = TRUE;
			}
		}
	}


	if ( CreateDarkIcon )
	{
		ApplyIconFilter( _filename, OutDataPointer, OutSize );
	}
	//else MessageBoxA( 0, filename, "Bad file path:", 0 );

	return result;
}




vector<ModelCollisionFixStruct> ModelCollisionFixList;
vector<ModelTextureFixStruct> ModelTextureFixList;
vector<ModelPatchStruct> ModelPatchList;
vector<ModelRemoveTagStruct> ModelRemoveTagList;
vector<ModelSequenceReSpeedStruct> ModelSequenceReSpeedList;
vector<ModelSequenceValueStruct> ModelSequenceValueList;
vector<ModelScaleStruct> ModelScaleList;

int __stdcall FixModelCollisionSphere( const char * mdlpath, float X, float Y, float Z, float Radius )
{
	ModelCollisionFixStruct tmpModelFix;
	tmpModelFix.FilePath = mdlpath;
	tmpModelFix.X = X;
	tmpModelFix.Y = Y;
	tmpModelFix.Z = Z;
	tmpModelFix.Radius = Radius;
	ModelCollisionFixList.push_back( tmpModelFix );
	return 0;
}


int __stdcall FixModelTexturePath( const char * mdlpath, int textureid, const char * texturenew )
{
	ModelTextureFixStruct tmpModelFix;
	tmpModelFix.FilePath = mdlpath;
	tmpModelFix.TextureID = textureid;
	tmpModelFix.NewTexturePath = texturenew;
	ModelTextureFixList.push_back( tmpModelFix );
	return 0;
}



int __stdcall PatchModel( const char * mdlpath, const char * pathPatch )
{
	ModelPatchStruct tmpModelFix;
	tmpModelFix.FilePath = mdlpath;
	tmpModelFix.patchPath = pathPatch;
	ModelPatchList.push_back( tmpModelFix );
	return 0;
}

int __stdcall RemoveTagFromModel( const char * mdlpath, const char * tagname )
{
	ModelRemoveTagStruct tmpModelFix;
	tmpModelFix.FilePath = mdlpath;
	 tmpModelFix.TagName = tagname;
	ModelRemoveTagList.push_back( tmpModelFix );
	return 0;
}

int __stdcall ChangeAnimationSpeed( const char * mdlpath, const char * SeqenceName, float Speed )
{
	ModelSequenceReSpeedStruct tmpModelFix;
	tmpModelFix.FilePath = mdlpath ;
	tmpModelFix.AnimationName =SeqenceName ;
	tmpModelFix.SpeedUp = Speed;
	ModelSequenceReSpeedList.push_back( tmpModelFix );
	return 0;
}



int __stdcall SetSequenceValue( const char * mdlpath, const char * SeqenceName, int Indx, float Value )
{
	if ( Indx < 0 || Indx > 6 )
		return -1;

	ModelSequenceValueStruct tmpModelFix;
	 tmpModelFix.FilePath= mdlpath ;
	 tmpModelFix.AnimationName = SeqenceName;
	tmpModelFix.Indx = Indx;
	tmpModelFix.Value = Value;
	ModelSequenceValueList.push_back( tmpModelFix );
	return 0;
}


int __stdcall SetModelScale( const char * mdlpath, float Scale )
{
	ModelScaleStruct tmpModelFix;
	tmpModelFix.FilePath = mdlpath;
	tmpModelFix.Scale = Scale;
	ModelScaleList.push_back( tmpModelFix );
	return 0;
}



struct Mdx_Texture        //NrOfTextures = ChunkSize / 268
{
	int ReplaceableId;
	CHAR FileName[ 260 ];
	unsigned int Flags;                       //#1 - WrapWidth
									   //#2 - WrapHeight
};

struct Mdx_FLOAT3
{
	float x;
	float y;
	float z;
};

struct Mdx_Sequence      //NrOfSequences = ChunkSize / 132
{
	CHAR Name[ 80 ];

	int IntervalStart;
	int IntervalEnd;
	FLOAT MoveSpeed;
	unsigned int Flags;                       //0 - Looping
									   //1 - NonLooping
	FLOAT Rarity;
	unsigned int SyncPoint;

	FLOAT BoundsRadius;
	Mdx_FLOAT3 MinimumExtent;
	Mdx_FLOAT3 MaximumExtent;
};

vector<BYTE> FullPatchData;

struct Mdx_SequenceTime
{
	int * IntervalStart;
	int * IntervalEnd;
};



struct Mdx_Track
{
	int NrOfTracks;
	int InterpolationType;             //0 - None
										 //1 - Linear
										 //2 - Hermite
										 //3 - Bezier
	unsigned int GlobalSequenceId;
};

struct Mdx_Tracks
{
	int NrOfTracks;
	unsigned int GlobalSequenceId;

};
struct Mdx_Node
{
	unsigned int InclusiveSize;

	CHAR Name[ 80 ];

	unsigned int ObjectId;
	unsigned int ParentId;
	unsigned int Flags;                         //0        - Helper
										 //#1       - DontInheritTranslation
										 //#2       - DontInheritRotation
										 //#4       - DontInheritScaling
										 //#8       - Billboarded
										 //#16      - BillboardedLockX
										 //#32      - BillboardedLockY
										 //#64      - BillboardedLockZ
										 //#128     - CameraAnchored
										 //#256     - Bone
										 //#512     - Light
										 //#1024    - EventObject
										 //#2048    - Attachment
										 //#4096    - ParticleEmitter
										 //#8192    - CollisionShape
										 //#16384   - RibbonEmitter
										 //#32768   - Unshaded / EmitterUsesMdl
										 //#65536   - SortPrimitivesFarZ / EmitterUsesTga
										 //#131072  - LineEmitter
										 //#262144  - Unfogged
										 //#524288  - ModelSpace
										 //#1048576 - XYQuad
};

struct Mdx_GeosetAnimation
{
	unsigned int InclusiveSize;

	FLOAT Alpha;
	unsigned int Flags;                       //#1 - DropShadow
									   //#2 - Color
	Mdx_FLOAT3 Color;

	unsigned int GeosetId;

};

void ProcessNodeAnims( BYTE * ModelBytes, size_t _offset, vector<int *> & TimesForReplace )
{
	Mdx_Track tmpTrack;
	size_t offset = _offset;
	if ( memcmp( &ModelBytes[ offset ], "KGTR", 4 ) == 0 )
	{
		offset += 4;
		memcpy( &tmpTrack, &ModelBytes[ offset ], sizeof( Mdx_Track ) );
		offset += sizeof( Mdx_Track );
		for ( int i = 0; i < tmpTrack.NrOfTracks; i++ )
		{
			TimesForReplace.push_back( ( int * )&ModelBytes[ offset ] );
			offset += ( tmpTrack.InterpolationType > 1 ? 40 : 16 );
		}
	}

	if ( memcmp( &ModelBytes[ offset ], "KGRT", 4 ) == 0 )
	{
		offset += 4;
		memcpy( &tmpTrack, &ModelBytes[ offset ], sizeof( Mdx_Track ) );
		offset += sizeof( Mdx_Track );
		for ( int i = 0; i < tmpTrack.NrOfTracks; i++ )
		{
			TimesForReplace.push_back( ( int * )&ModelBytes[ offset ] );
			offset += ( tmpTrack.InterpolationType > 1 ? 52 : 20 );
		}
	}

	if ( memcmp( &ModelBytes[ offset ], "KGSC", 4 ) == 0 )
	{
		offset += 4;
		memcpy( &tmpTrack, &ModelBytes[ offset ], sizeof( Mdx_Track ) );
		offset += sizeof( Mdx_Track );
		for ( int i = 0; i < tmpTrack.NrOfTracks; i++ )
		{
			TimesForReplace.push_back( ( int * )&ModelBytes[ offset ] );
			offset += ( tmpTrack.InterpolationType > 1 ? 40 : 16 );
		}
	}


	if ( memcmp( &ModelBytes[ offset ], "KGAO", 4 ) == 0 )
	{
		offset += 4;
		memcpy( &tmpTrack, &ModelBytes[ offset ], sizeof( Mdx_Track ) );
		offset += sizeof( Mdx_Track );
		for ( int i = 0; i < tmpTrack.NrOfTracks; i++ )
		{
			TimesForReplace.push_back( ( int * )&ModelBytes[ offset ] );
			offset += ( tmpTrack.InterpolationType > 1 ? 16 : 8 );
		}
	}

	if ( memcmp( &ModelBytes[ offset ], "KGAC", 4 ) == 0 )
	{
		offset += 4;
		memcpy( &tmpTrack, &ModelBytes[ offset ], sizeof( Mdx_Track ) );
		offset += sizeof( Mdx_Track );
		for ( int i = 0; i < tmpTrack.NrOfTracks; i++ )
		{
			TimesForReplace.push_back( ( int * )&ModelBytes[ offset ] );
			offset += ( tmpTrack.InterpolationType > 1 ? 16 : 8 );
		}
	}

}


BYTE HelperBytesPart1[ ] = {
							0x42,0x4F,0x4E,0x45,0x88,0x00,0x00,0x00,0x80,0x00,
							0x00,0x00,0x42,0x6F,0x6E,0x65,0x5F,0x52,0x6F,0x6F,
							0x74,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							0x00,0x00 };
BYTE HelperBytesPart2[ ] = { 0xFF,0xFF,0xFF,0xFF,0x00,0x01,0x00,0x00,0x4B,
							0x47,0x53,0x43,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
							0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };

BYTE HelperBytesPart3[ ] = { 0xFF,0xFF,0xFF,0xFF,
							 0xFF,0xFF,0xFF,0xFF };


void ProcessMdx( string filename, int * OutDataPointer, size_t * OutSize, BOOL unknown )
{
#ifdef DOTA_HELPER_LOG
	AddNewLineToDotaHelperLog( "ProcessModel" );
#endif
	BYTE * ModelBytes = ( BYTE* )*OutDataPointer;
	size_t sz = *OutSize;



	for ( unsigned int i = 0; i < ModelSequenceValueList.size( ); i++ )
	{
		ModelSequenceValueStruct mdlfix = ModelSequenceValueList[ i ];
		if ( filename ==  mdlfix.FilePath )
		{
			size_t offset = 0;
			if ( memcmp( &ModelBytes[ offset ], "MDLX", 4 ) == 0 )
			{
				offset += 4;
				while ( offset < sz )
				{
					if ( memcmp( &ModelBytes[ offset ], "SEQS", 4 ) == 0 )
					{
						Mdx_Sequence tmpSequence;

						offset += 4;

						size_t currenttagsize = *( size_t* )&ModelBytes[ offset ];
						size_t SequencesCount = currenttagsize / sizeof( Mdx_Sequence );

						size_t newoffset = offset + currenttagsize;
						offset += 4;
						while ( SequencesCount > 0 )
						{
							SequencesCount--;
							memcpy( &tmpSequence, &ModelBytes[ offset ], sizeof( Mdx_Sequence ) );

							if ( mdlfix.AnimationName.length() == 0 || mdlfix.AnimationName == tmpSequence.Name )
							{
								size_t NeedPatchOffset = offset + 104 + ( mdlfix.Indx * 4 );
								*( float* )&ModelBytes[ NeedPatchOffset ] = mdlfix.Value;
							}

							offset += sizeof( Mdx_Sequence );
						}
						offset = newoffset;
					}
					else
					{
						offset += 4;
						offset += *( int* )&ModelBytes[ offset ];
					}

					offset += 4;
				}

			}


			if ( IsKeyPressed( '0' ) && FileExist( ".\\Test1234.mdx" ) )
			{
				FILE *f;
				fopen_s( &f, ".\\Test1234.mdx", "wb" );
				fwrite( ModelBytes, sz, 1, f );
				fclose( f );
				MessageBoxA( 0, "Ok dump", "DUMP", 0 );
			}


			ModelSequenceValueList.erase( ModelSequenceValueList.begin( ) + ( int )i );
			i--;
		}

	}

	for ( unsigned int i = 0; i < ModelSequenceReSpeedList.size( ); i++ )
	{
		ModelSequenceReSpeedStruct mdlfix = ModelSequenceReSpeedList[ i ];
		if ( filename == mdlfix.FilePath )
		{

			int SequenceID = 0;
			int ReplaceSequenceID = -1;

			// First find Animation and shift others
			vector<Mdx_SequenceTime> Sequences;

			// Next find all objects with Node struct and shift 
			vector<int *> TimesForReplace;

			// Shift any others animations
			// Next need search and shift needed animation
			size_t offset = 0;
			if ( memcmp( &ModelBytes[ offset ], "MDLX", 4 ) == 0 )
			{
				offset += 4;
				while ( offset < sz )
				{
					if ( memcmp( &ModelBytes[ offset ], "SEQS", 4 ) == 0 )
					{

						Mdx_Sequence tmpSequence;

						offset += 4;

						size_t currenttagsize = *( size_t* )&ModelBytes[ offset ];
						size_t SequencesCount = currenttagsize / sizeof( Mdx_Sequence );

						size_t newoffset = offset + currenttagsize;
						offset += 4;
						while ( SequencesCount > 0 )
						{
							SequencesCount--;
							memcpy( &tmpSequence, &ModelBytes[ offset ], sizeof( Mdx_Sequence ) );


							if (  mdlfix.AnimationName == tmpSequence.Name )
							{
								ReplaceSequenceID = SequenceID;
							}


							Mdx_SequenceTime CurrentSequenceTime;
							CurrentSequenceTime.IntervalStart = ( int* )&ModelBytes[ offset + 80 ];
							CurrentSequenceTime.IntervalEnd = ( int* )&ModelBytes[ offset + 84 ];
							Sequences.push_back( CurrentSequenceTime );

							offset += sizeof( Mdx_Sequence );
							SequenceID++;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "BONE", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							ProcessNodeAnims( ModelBytes, offset + sizeof( Mdx_Node ), TimesForReplace );
							offset += tmpNode.InclusiveSize + 8;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "HELP", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							ProcessNodeAnims( ModelBytes, offset + sizeof( Mdx_Node ), TimesForReplace );
							offset += tmpNode.InclusiveSize;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "LITE", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							size_t size_of_this_struct = *( size_t* )&ModelBytes[ offset ];
							offset += 4;
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							ProcessNodeAnims( ModelBytes, offset + sizeof( Mdx_Node ), TimesForReplace );
							offset += size_of_this_struct - 4;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "ATCH", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							size_t size_of_this_struct = *( size_t* )&ModelBytes[ offset ];
							offset += 4;
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							ProcessNodeAnims( ModelBytes, offset + sizeof( Mdx_Node ), TimesForReplace );
							offset += size_of_this_struct - 4;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "PREM", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							size_t size_of_this_struct = *( size_t* )&ModelBytes[ offset ];
							offset += 4;
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							ProcessNodeAnims( ModelBytes, offset + sizeof( Mdx_Node ), TimesForReplace );
							offset += size_of_this_struct - 4;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "PRE2", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							size_t size_of_this_struct = *( size_t* )&ModelBytes[ offset ];
							offset += 4;
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							ProcessNodeAnims( ModelBytes, offset + sizeof( Mdx_Node ), TimesForReplace );
							offset += size_of_this_struct - 4;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "GEOA", 4 ) == 0 )
					{
						Mdx_GeosetAnimation tmpGeosetAnimation;
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						while ( newoffset > offset )
						{
							memcpy( &tmpGeosetAnimation, &ModelBytes[ offset ], sizeof( Mdx_GeosetAnimation ) );
							ProcessNodeAnims( ModelBytes, offset + sizeof( Mdx_GeosetAnimation ), TimesForReplace );

							offset += tmpGeosetAnimation.InclusiveSize;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "RIBB", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							size_t size_of_this_struct = *( size_t* )&ModelBytes[ offset ];
							offset += 4;
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							ProcessNodeAnims( ModelBytes, offset + sizeof( Mdx_Node ), TimesForReplace );
							offset += size_of_this_struct - 4;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "EVTS", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						Mdx_Tracks tmpTracks;
						while ( newoffset > offset )
						{
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							ProcessNodeAnims( ModelBytes, offset + sizeof( Mdx_Node ), TimesForReplace );
							offset += tmpNode.InclusiveSize;
							if ( memcmp( &ModelBytes[ offset ], "KEVT", 4 ) == 0 )
							{
								offset += 4;
								memcpy( &tmpTracks, &ModelBytes[ offset ], sizeof( Mdx_Tracks ) );
								offset += sizeof( Mdx_Tracks );
								for ( int n = 0; n < tmpTracks.NrOfTracks; n++ )
								{
									TimesForReplace.push_back( ( int* )&ModelBytes[ offset ] );
									offset += 4;
								}
							}
							else offset += 4;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "CLID", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							ProcessNodeAnims( ModelBytes, offset + sizeof( Mdx_Node ), TimesForReplace );
							offset += tmpNode.InclusiveSize;
							unsigned int size_of_this_struct = *( unsigned int* )&ModelBytes[ offset ];
							offset += 4;
							size_of_this_struct = size_of_this_struct == 0 ? 24u : 16u;
							offset += size_of_this_struct;
						}
						offset = newoffset;
					}
					else
					{
						offset += 4;
						offset += *( int* )&ModelBytes[ offset ];
					}

					offset += 4;
				}
			}

			if ( ReplaceSequenceID != -1 )
			{

				int SeqEndTime = *Sequences[ ( unsigned int )ReplaceSequenceID ].IntervalEnd;
				int SeqStartTime = *Sequences[ ( unsigned int )ReplaceSequenceID ].IntervalStart;
				int NewEndTime = SeqStartTime + ( int )( ( SeqEndTime - SeqStartTime ) / mdlfix.SpeedUp );
				int AddTime = NewEndTime - SeqEndTime;

				for ( unsigned int n = 0; n < Sequences.size( ); n++ )
				{
					if ( *Sequences[ n ].IntervalStart >= SeqEndTime )
					{
						*Sequences[ n ].IntervalStart += AddTime;
						*Sequences[ n ].IntervalEnd += AddTime;
					}
				}

				*Sequences[ ( unsigned int )ReplaceSequenceID ].IntervalEnd = NewEndTime;

				for ( int * dwTime : TimesForReplace )
				{
					if ( *dwTime >= SeqEndTime )
					{
						*dwTime += AddTime;
					}
					else if ( *dwTime <= SeqEndTime && *dwTime >= SeqStartTime )
					{
						*dwTime = ( int )SeqStartTime + ( int )( ( float )( *dwTime - SeqStartTime ) / mdlfix.SpeedUp );
					}
				}


				if ( IsKeyPressed( '0' ) && FileExist( ".\\Test1234.mdx" ) )
				{
					FILE *f;
					fopen_s( &f, ".\\Test1234.mdx", "wb" );
					fwrite( ModelBytes, sz, 1, f );
					fclose( f );
					MessageBoxA( 0, "Ok dump", "DUMP", 0 );
				}

			}


			TimesForReplace.clear( );
			Sequences.clear( );

			ModelSequenceReSpeedList.erase( ModelSequenceReSpeedList.begin( ) + ( int )i );
			i--;
		}

	}


	for ( unsigned int i = 0; i < ModelRemoveTagList.size( ); i++ )
	{
		ModelRemoveTagStruct mdlfix = ModelRemoveTagList[ i ];
		if (  filename == mdlfix.FilePath )
		{
			BOOL TagFound = FALSE;
			size_t TagStartOffset = 0;
			size_t TagSize = 0;
			size_t offset = 0;
			if ( memcmp( &ModelBytes[ offset ], "MDLX", 4 ) == 0 )
			{
				offset += 4;
				while ( offset < sz )
				{
					if ( memcmp( &ModelBytes[ offset ], mdlfix.TagName.c_str( ), 4 ) == 0 )
					{

						TagFound = TRUE;
						TagStartOffset = offset;
						offset += 4;
						offset += *( int* )&ModelBytes[ offset ];
						TagSize = offset - TagStartOffset;

					}
					else
					{
						offset += 4;
						offset += *( int* )&ModelBytes[ offset ];
					}

					offset += 4;
				}

			}

			if ( TagFound )
			{
				memcpy( &ModelBytes[ TagStartOffset ], &ModelBytes[ TagStartOffset + TagSize + 4 ], sz - ( TagStartOffset + TagSize ) );
				memset( &ModelBytes[ sz - TagSize - 4 ], 0xFF, TagSize );

				sz = sz - TagSize - 4;
				*OutSize = sz;
			}


			ModelRemoveTagList.erase( ModelRemoveTagList.begin( ) + ( int )i );
			i--;
		}
	}


	for ( unsigned int i = 0; i < ModelScaleList.size( ); i++ )
	{
		ModelScaleStruct mdlfix = ModelScaleList[ i ];
		if ( filename == mdlfix.FilePath )
		{
			if ( !FullPatchData.empty( ) )
				FullPatchData.clear( );

			char TagName[ 5 ];
			memset( TagName, 0, 5 );
			size_t offset = 0;

			DWORD MaxObjectId = 0;

			std::vector<DWORD *> parents;

			DWORD OffsetToInsertPivotPoint = 0;

			BOOL FoundGLBS = FALSE;
			char * strGLBS = "GLBS";

			if ( memcmp( &ModelBytes[ offset ], "MDLX", 4 ) == 0 )
			{
				offset += 4;
				while ( offset < sz )
				{
					memcpy( TagName, &ModelBytes[ offset ], 4 );
					if ( memcmp( &ModelBytes[ offset ], strGLBS, 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						FoundGLBS = TRUE;

						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "PIVT", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];

						*( size_t* )&ModelBytes[ offset ] = 12 + *( size_t* )&ModelBytes[ offset ];

						OffsetToInsertPivotPoint = newoffset + 4;

						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "BONE", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;

						while ( newoffset > offset )
						{
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );

							if ( tmpNode.ObjectId != 0xFFFFFFFF && tmpNode.ObjectId > MaxObjectId )
							{
								MaxObjectId = tmpNode.ObjectId;
							}
							parents.push_back( ( DWORD* )&ModelBytes[ offset + 88 ] );

							offset += tmpNode.InclusiveSize + 8;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "HELP", 4 ) == 0 )
					{

						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							if ( tmpNode.ObjectId != 0xFFFFFFFF && tmpNode.ObjectId > MaxObjectId )
							{
								MaxObjectId = tmpNode.ObjectId;
							}
							parents.push_back( ( DWORD* )&ModelBytes[ offset + 88 ] );


							offset += tmpNode.InclusiveSize;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "LITE", 4 ) == 0 )
					{

						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							size_t size_of_this_struct = *( size_t* )&ModelBytes[ offset ];
							offset += 4;
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							if ( tmpNode.ObjectId != 0xFFFFFFFF && tmpNode.ObjectId > MaxObjectId )
							{
								MaxObjectId = tmpNode.ObjectId;
							}
							parents.push_back( ( DWORD* )&ModelBytes[ offset + 88 ] );

							offset += size_of_this_struct - 4;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "ATCH", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						//Mdx_Tracks tmpTracks;
						while ( newoffset > offset )
						{
							size_t size_of_this_struct = *( size_t* )&ModelBytes[ offset ];
							offset += 4;

							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );

							if ( tmpNode.ObjectId != 0xFFFFFFFF && tmpNode.ObjectId > MaxObjectId )
							{
								MaxObjectId = tmpNode.ObjectId;
							}
							parents.push_back( ( DWORD* )&ModelBytes[ offset + 88 ] );
							//*( DWORD* )&ModelBytes[ offset + 88 ] = 0xFFFFFFFF;
							/*offset += tmpNode.InclusiveSize;


							char * attchname = ( char * )&ModelBytes[ offset ];
							offset += 260;


							DWORD attchid = *( DWORD * )&ModelBytes[ offset ];

							offset += 4;

							if ( memcmp( &ModelBytes[ offset ], "KATV", 4 ) == 0 )
							{
							offset += 4;
							Mdx_Track tmpTrack;
							memcpy( &tmpTrack, &ModelBytes[ offset ], sizeof( Mdx_Track ) );
							offset += sizeof( Mdx_Track );
							for ( DWORD i = 0; i < tmpTrack.NrOfTracks; i++ )
							{
							offset += ( tmpTrack.InterpolationType > 1 ? 16 : 8 );
							}
							}*/

							offset += size_of_this_struct - 4;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "PREM", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							size_t size_of_this_struct = *( size_t* )&ModelBytes[ offset ];
							offset += 4;
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							if ( tmpNode.ObjectId != 0xFFFFFFFF && tmpNode.ObjectId > MaxObjectId )
							{
								MaxObjectId = tmpNode.ObjectId;
							}
							parents.push_back( ( DWORD* )&ModelBytes[ offset + 88 ] );

							offset += size_of_this_struct - 4;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "PRE2", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							size_t size_of_this_struct = *( size_t* )&ModelBytes[ offset ];
							offset += 4;
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );

							if ( tmpNode.ObjectId != 0xFFFFFFFF && tmpNode.ObjectId > MaxObjectId )
							{
								MaxObjectId = tmpNode.ObjectId;
							}
							parents.push_back( ( DWORD* )&ModelBytes[ offset + 88 ] );

							offset += size_of_this_struct - 4;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "RIBB", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							size_t size_of_this_struct = *( size_t* )&ModelBytes[ offset ];
							offset += 4;
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							if ( tmpNode.ObjectId != 0xFFFFFFFF && tmpNode.ObjectId > MaxObjectId )
							{
								MaxObjectId = tmpNode.ObjectId;
							}
							parents.push_back( ( DWORD* )&ModelBytes[ offset + 88 ] );

							offset += size_of_this_struct - 4;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "EVTS", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						Mdx_Tracks tmpTracks;
						while ( newoffset > offset )
						{
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );
							if ( tmpNode.ObjectId != 0xFFFFFFFF && tmpNode.ObjectId > MaxObjectId )
							{
								MaxObjectId = tmpNode.ObjectId;
							}
							parents.push_back( ( DWORD* )&ModelBytes[ offset + 88 ] );

							offset += tmpNode.InclusiveSize;
							if ( memcmp( &ModelBytes[ offset ], "KEVT", 4 ) == 0 )
							{
								offset += 4;
								memcpy( &tmpTracks, &ModelBytes[ offset ], sizeof( Mdx_Tracks ) );
								offset += sizeof( Mdx_Tracks );
								for ( int i = 0; i < tmpTracks.NrOfTracks; i++ )
								{
									offset += 4;
								}
							}
							else offset += 4;
						}
						offset = newoffset;
					}
					else if ( memcmp( &ModelBytes[ offset ], "CLID", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( size_t* )&ModelBytes[ offset ];
						offset += 4;
						Mdx_Node tmpNode;
						while ( newoffset > offset )
						{
							memcpy( &tmpNode, &ModelBytes[ offset ], sizeof( Mdx_Node ) );

							if ( tmpNode.ObjectId != 0xFFFFFFFF && tmpNode.ObjectId > MaxObjectId )
							{
								MaxObjectId = tmpNode.ObjectId;
							}
							parents.push_back( ( DWORD* )&ModelBytes[ offset + 88 ] );

							offset += tmpNode.InclusiveSize;
							unsigned int size_of_this_struct = *( unsigned int* )&ModelBytes[ offset ];
							offset += 4;
							size_of_this_struct = size_of_this_struct == 0 ? 24 : 16;
							offset += size_of_this_struct;
						}
						offset = newoffset;
					}
					else
					{
						offset += 4;
						offset += *( int* )&ModelBytes[ offset ];
					}

					offset += 4;
				}
			}


			MaxObjectId++;

			for ( DWORD * parOffsets : parents )
			{
				DWORD curparent = *parOffsets;
				if ( curparent == 0xFFFFFFFF )
				{
					*parOffsets = MaxObjectId;
				}
			}

			FullPatchData.insert( FullPatchData.end( ), ( BYTE* )( ModelBytes ), ( BYTE* )( ModelBytes + sz ) );

			if ( OffsetToInsertPivotPoint != 0 )
			{
				char ZeroPos[ 12 ];
				memset( ZeroPos, 0, sizeof( ZeroPos ) );
				FullPatchData.insert( FullPatchData.begin( ) + OffsetToInsertPivotPoint, ZeroPos, ZeroPos + 12 );
			}

			FullPatchData.insert( FullPatchData.end( ), ( BYTE* )( HelperBytesPart1 ), ( BYTE* )( HelperBytesPart1 + sizeof( HelperBytesPart1 ) ) );
			BYTE * patchbytes = ( BYTE * )&MaxObjectId;

			FullPatchData.push_back( patchbytes[ 0 ] );
			FullPatchData.push_back( patchbytes[ 1 ] );
			FullPatchData.push_back( patchbytes[ 2 ] );
			FullPatchData.push_back( patchbytes[ 3 ] );

			FullPatchData.insert( FullPatchData.end( ), ( BYTE* )( HelperBytesPart2 ), ( BYTE* )( HelperBytesPart2 + sizeof( HelperBytesPart2 ) ) );


			float scaleall = mdlfix.Scale;

			patchbytes = ( BYTE * )&scaleall;


			FullPatchData.push_back( patchbytes[ 0 ] );
			FullPatchData.push_back( patchbytes[ 1 ] );
			FullPatchData.push_back( patchbytes[ 2 ] );
			FullPatchData.push_back( patchbytes[ 3 ] );

			FullPatchData.push_back( patchbytes[ 0 ] );
			FullPatchData.push_back( patchbytes[ 1 ] );
			FullPatchData.push_back( patchbytes[ 2 ] );
			FullPatchData.push_back( patchbytes[ 3 ] );

			FullPatchData.push_back( patchbytes[ 0 ] );
			FullPatchData.push_back( patchbytes[ 1 ] );
			FullPatchData.push_back( patchbytes[ 2 ] );
			FullPatchData.push_back( patchbytes[ 3 ] );

			FullPatchData.insert( FullPatchData.end( ), ( BYTE* )( HelperBytesPart3 ), ( BYTE* )( HelperBytesPart3 + sizeof( HelperBytesPart3 ) ) );

			if ( !FoundGLBS )
			{
				FullPatchData.push_back( strGLBS[ 0 ] );
				FullPatchData.push_back( strGLBS[ 1 ] );
				FullPatchData.push_back( strGLBS[ 2 ] );
				FullPatchData.push_back( strGLBS[ 3 ] );
				DWORD szGLBS = 4;
				patchbytes = ( BYTE * )&szGLBS;
				FullPatchData.push_back( patchbytes[ 0 ] );
				FullPatchData.push_back( patchbytes[ 1 ] );
				FullPatchData.push_back( patchbytes[ 2 ] );
				FullPatchData.push_back( patchbytes[ 3 ] );
				szGLBS = 0;
				patchbytes = ( BYTE * )&szGLBS;
				FullPatchData.push_back( patchbytes[ 0 ] );
				FullPatchData.push_back( patchbytes[ 1 ] );
				FullPatchData.push_back( patchbytes[ 2 ] );
				FullPatchData.push_back( patchbytes[ 3 ] );
			}


			if ( IsKeyPressed( '0' ) && FileExist( ".\\Test1234.mdx" ) )
			{
				FILE *f;
				fopen_s( &f, ".\\Test1234.mdx", "wb" );
				fwrite( &FullPatchData[ 0 ], FullPatchData.size( ), 1, f );
				fclose( f );
				MessageBoxA( 0, "Ok dump", "DUMP", 0 );
			}


			ICONMDLCACHE * tmpih = new ICONMDLCACHE( );
			BOOL FoundOldHelper = GetFromIconMdlCache( filename, tmpih );


			if ( FoundOldHelper )
			{
				Buffer ResultBuffer;
				ResultBuffer.buf = new char[ FullPatchData.size( ) ];
				ResultBuffer.length = FullPatchData.size( );

				memcpy( &ResultBuffer.buf[ 0 ], &FullPatchData[ 0 ], FullPatchData.size( ) );

				delete[ ] tmpih->buf;
				tmpih->buf = ResultBuffer.buf;
				tmpih->size = ResultBuffer.length;
				*OutDataPointer = ( int )tmpih->buf;
				*OutSize = tmpih->size;
			}
			else
			{
				Buffer ResultBuffer;
				ResultBuffer.buf = new char[ FullPatchData.size( ) ];
				ResultBuffer.length = FullPatchData.size( );

				memcpy( &ResultBuffer.buf[ 0 ], &FullPatchData[ 0 ], FullPatchData.size( ) );

				tmpih->buf = ResultBuffer.buf;
				tmpih->size = ResultBuffer.length;
				tmpih->hashlen = filename.length( );
				tmpih->hash = GetBufHash( filename.c_str( ), tmpih->hashlen );

				ICONMDLCACHELIST.push_back( *tmpih );


				Storm_403_org( ( void* )*OutDataPointer, "delete", -1, 0 );

				*OutDataPointer = ( int )tmpih->buf;
				*OutSize = tmpih->size;

				ModelBytes = ( BYTE * )tmpih->buf;
				sz = tmpih->size;
			}


			ModelScaleList.erase( ModelScaleList.begin( ) + ( int )i );
			i--;
		}
	}


	if ( !FullPatchData.empty( ) )
		FullPatchData.clear( );
	for ( unsigned int i = 0; i < ModelPatchList.size( ); i++ )
	{
		ModelPatchStruct mdlfix = ModelPatchList[ i ];
		if ( filename == mdlfix.FilePath )
		{
			int PatchFileData;
			size_t PatchFileSize;

			if ( GameGetFile_ptr( mdlfix.patchPath.c_str( ), &PatchFileData, &PatchFileSize, unknown ) )
			{
				FullPatchData.insert( FullPatchData.end( ), ( char* )( PatchFileData ), ( char* )( PatchFileData + PatchFileSize ) );
			}

			ModelPatchList.erase( ModelPatchList.begin( ) + ( int )i );
			i--;
		}
	}

	if ( !FullPatchData.empty( ) )
	{

		ICONMDLCACHE * tmpih = new ICONMDLCACHE( );
		BOOL FoundOldHelper = GetFromIconMdlCache( filename, tmpih );


		if ( FoundOldHelper )
		{
			Buffer ResultBuffer;
			ResultBuffer.buf = new char[ tmpih->size + FullPatchData.size( ) ];

			ResultBuffer.length = tmpih->size + FullPatchData.size( );

			memcpy( &ResultBuffer.buf[ 0 ], tmpih->buf, sz );

			memcpy( &ResultBuffer.buf[ sz ], &FullPatchData[ 0 ], FullPatchData.size( ) );


			delete[ ] tmpih->buf;
			tmpih->buf = ResultBuffer.buf;
			tmpih->size = ResultBuffer.length;
			*OutDataPointer = ( int )tmpih->buf;
			*OutSize = tmpih->size;
		}
		else
		{
			Buffer ResultBuffer;
			ResultBuffer.buf = new char[ sz + FullPatchData.size( ) ];
			ResultBuffer.length = sz + FullPatchData.size( );

			memcpy( &ResultBuffer.buf[ 0 ], ModelBytes, sz );
			memcpy( &ResultBuffer.buf[ sz ], &FullPatchData[ 0 ], FullPatchData.size( ) );

			tmpih->buf = ResultBuffer.buf;
			tmpih->size = ResultBuffer.length;

			tmpih->hashlen = filename.length( );
			tmpih->hash = GetBufHash( filename.c_str( ), tmpih->hashlen );

			ICONMDLCACHELIST.push_back( *tmpih );


			Storm_403_org( ( void* )*OutDataPointer, "delete", -1, 0 );


			*OutDataPointer = ( int )tmpih->buf;
			*OutSize = tmpih->size;

			ModelBytes = ( BYTE * )tmpih->buf;
			sz = tmpih->size;

		}

		if ( IsKeyPressed( '0' ) && FileExist( ".\\Test1234.mdx" ) )
		{
			FILE *f;
			fopen_s( &f, ".\\Test1234.mdx", "wb" );
			fwrite( ModelBytes, sz, 1, f );
			fclose( f );
		}

		delete tmpih;
		FullPatchData.clear( );
	}


	for ( unsigned int i = 0; i < ModelCollisionFixList.size( ); i++ )
	{
		ModelCollisionFixStruct mdlfix = ModelCollisionFixList[ i ];
		if (  filename == mdlfix.FilePath )
		{
			size_t offset = 0;
			if ( memcmp( &ModelBytes[ offset ], "MDLX", 4 ) == 0 )
			{
				offset += 4;
				while ( offset < sz )
				{
					if ( memcmp( &ModelBytes[ offset ], "CLID", 4 ) == 0 )
					{
						offset += 4;
						size_t newoffset = offset + *( int* )&ModelBytes[ offset ];
						offset += 4;
						offset += *( int* )&ModelBytes[ offset ];
						int shapetype = *( int* )&ModelBytes[ offset ];

						if ( shapetype == 2 )
						{
							offset += 4;
							*( float* )&ModelBytes[ offset ] = mdlfix.X;
							offset += 4;
							*( float* )&ModelBytes[ offset ] = mdlfix.Y;
							offset += 4;
							*( float* )&ModelBytes[ offset ] = mdlfix.Z;
							offset += 4;
							*( float* )&ModelBytes[ offset ] = mdlfix.Radius;
						}
						offset = newoffset + 4;
					}
					else
					{
						offset += 4;
						offset += *( int* )&ModelBytes[ offset ];
					}
					offset += 4;
				}
			}
			ModelCollisionFixList.erase( ModelCollisionFixList.begin( ) + ( int )i );
			i--;
		}

	}


	for ( unsigned int i = 0; i < ModelTextureFixList.size( ); i++ )
	{
		ModelTextureFixStruct mdlfix = ModelTextureFixList[ i ];
		int TextureID = 0;
		if (filename == mdlfix.FilePath )
		{
			size_t offset = 0;
			if ( memcmp( &ModelBytes[ offset ], "MDLX", 4 ) == 0 )
			{
				offset += 4;
				while ( offset < sz )
				{
					if ( memcmp( &ModelBytes[ offset ], "TEXS", 4 ) == 0 )
					{
						Mdx_Texture tmpTexture;

						//char TextNameBuf[ 0x100 ];
						offset += 4;
						int TagSize = *( int* )&ModelBytes[ offset ];
						size_t newoffset = offset + TagSize;
						int TexturesCount = TagSize / ( int ) sizeof( Mdx_Texture );
						offset += 4;


						while ( TexturesCount > 0 )
						{
							TexturesCount--;
							memcpy( &tmpTexture, &ModelBytes[ offset ], sizeof( Mdx_Texture ) );
							TextureID++;

							if ( mdlfix.TextureID == TextureID )
							{
								if ( mdlfix.NewTexturePath.length( ) > 3 )
								{
									tmpTexture.ReplaceableId = 0;
									sprintf_s( tmpTexture.FileName, 260, "%s", mdlfix.NewTexturePath.c_str( ) );
								}
								else
								{
									tmpTexture.ReplaceableId = atoi( mdlfix.NewTexturePath.c_str( ) );
									memset( tmpTexture.FileName, 0, 260 );
								}
								memcpy( &ModelBytes[ offset ], &tmpTexture, sizeof( Mdx_Texture ) );
							}
							offset += sizeof( Mdx_Texture );
						}
						offset = newoffset + 4;
					}
					else
					{
						offset += 4;
						offset += *( int* )&ModelBytes[ offset ];
					}
					offset += 4;
				}
			}

			ModelTextureFixList.erase( ModelTextureFixList.begin( ) + ( int )i );
			i--;
		}

	}


}


int __stdcall RedirectFile( const char * RealFilePath, const char * NewFilePath )
{
	FileRedirectStruct tmpModelFix;
	tmpModelFix.NewFilePath = NewFilePath;
	tmpModelFix.RealFilePath = RealFilePath;
	FileRedirectList.push_back( tmpModelFix );
	return 0;
}

void PrintLog( const char * str )
{
	FILE * f;

	fopen_s( &f, ".\\text.txt", "a+" );
	if ( f != NULL )
	{
		fprintf_s( f, "%s\n", str );
		fclose( f );
	}
}


BOOL ProcessFile( string filename, int * OutDataPointer, size_t * OutSize, BOOL unknown, BOOL IsFileExistOld )
{


	BOOL IsFileExist = IsFileExistOld;

#ifdef DOTA_HELPER_LOG
	AddNewLineToDotaHelperLog( "ProcessFile" );
#endif
	int filenamelen = filename.length( );
	if ( filenamelen > 4 )
	{
		string FileExtension = ToLower( fs::path( filename ).extension( ).string( ) );

#ifdef DOTA_HELPER_LOG
		AddNewLineToDotaHelperLog( "ProcessFileStart..." );
#endif

		if ( FileExtension == string(".tga") )
		{

		}
		else if ( FileExtension == string( ".blp") )
		{
			if ( !IsFileExist )
			{
				IsFileExist = FixDisabledIconPath( filename, OutDataPointer, OutSize, unknown );
			}
			else
			{
				/*	if ( strstr( FilePathLower.c_str( ), "terrainart" ) == FilePathLower.c_str( ) ||
						 strstr( FilePathLower.c_str( ), "replaceabletextures\\cliff" ) == FilePathLower.c_str( ) )
						ApplyTerrainFilter( filename, OutDataPointer, OutSize, FALSE );*/
			}
		}
		else if ( FileExtension == string( ".mdx") )
		{
			if ( IsFileExist )
			{
				ProcessMdx( filename, OutDataPointer, OutSize, unknown );
			}
			else
			{
				//return GameGetFile_ptr( "Objects\\InvalidObject\\InvalidObject.mdx", OutDataPointer, OutSize, unknown );
			}
		}

	}


	return IsFileExist;
}

vector<FakeFileStruct> FakeFileList;

void AddNewFakeFile( char * filename, BYTE * buffer, size_t FileSize )
{
	FakeFileStruct tmpstr;
	tmpstr.buffer = buffer;
	tmpstr.filename = filename;
	tmpstr.size = FileSize;
	FakeFileList.push_back( tmpstr );
}

BOOL __fastcall GameGetFile_my( const char * filename, int * OutDataPointer, unsigned int * OutSize, BOOL unknown )
{

#ifdef DOTA_HELPER_LOG
	AddNewLineToDotaHelperLog( string( "Start File Helper..[" ) + to_string( unknown ) + "]" );
#endif

	if ( TerminateStarted )
	{

#ifdef DOTA_HELPER_LOG
		AddNewLineToDotaHelperLog( "TerminateStarted" );
#endif

		return GameGetFile_ptr( filename, OutDataPointer, OutSize, unknown );
	}

	if ( !OutDataPointer || !OutSize )
	{
#ifdef DOTA_HELPER_LOG
		AddNewLineToDotaHelperLog( "Bad Pointers" );
#endif
		return GameGetFile_ptr( filename, OutDataPointer, OutSize, unknown );
	}

	for ( FakeFileStruct fs : FakeFileList )
	{
		if ( _stricmp( filename, fs.filename ) == 0 )
		{
			*OutDataPointer = ( int )fs.buffer;
			*OutSize = fs.size;
			return TRUE;
		}
	}

#ifdef DOTA_HELPER_LOG
	if ( filename && *filename != '\0' )
		AddNewLineToDotaHelperLog( string( "FileHelper:" ) + string( filename ) );
	else
		AddNewLineToDotaHelperLog( "FileHelper(BADFILENAME)" );
#endif

	BOOL IsFileExist = GameGetFile_ptr( filename, OutDataPointer, OutSize, unknown );



	if ( !*InGame && !MainFuncWork )
	{
#ifdef DOTA_HELPER_LOG
		AddNewLineToDotaHelperLog( "Game not found or main not set" );
#endif

		return IsFileExist;
	}

	if ( filename == NULL || *filename == '\0' )
	{

#ifdef DOTA_HELPER_LOG
		AddNewLineToDotaHelperLog( "Bad file name" );
#endif

		return IsFileExist;
	}

	if ( !IsFileExist )
	{
#ifdef DOTA_HELPER_LOG
		AddNewLineToDotaHelperLog( "NoFileFound" );
#endif
	}
	else
	{
#ifdef DOTA_HELPER_LOG
		AddNewLineToDotaHelperLog( "FileFound" );
#endif
	}

	IsFileExist = ProcessFile( filename, OutDataPointer, OutSize, unknown, IsFileExist );


#ifdef DOTA_HELPER_LOG
	AddNewLineToDotaHelperLog( "ProcessFileENDING" );
#endif

	if ( !IsFileExist )
	{
#ifdef DOTA_HELPER_LOG
		AddNewLineToDotaHelperLog( "NoFileFound" );
#endif
	}
	else
	{
#ifdef DOTA_HELPER_LOG
		AddNewLineToDotaHelperLog( "FileFound" );
#endif
	}

	if ( !IsFileExist )
	{
		for ( FileRedirectStruct DotaRedirectHelp : FileRedirectList )
		{
			if ( filename == DotaRedirectHelp.NewFilePath )
			{
				ICONMDLCACHE * tmpih = new ICONMDLCACHE( );
				BOOL FoundOldHelper = GetFromIconMdlCache( DotaRedirectHelp.NewFilePath, tmpih );

				if ( !FoundOldHelper )
				{
					IsFileExist = GameGetFile_ptr( DotaRedirectHelp.RealFilePath.c_str(), OutDataPointer, OutSize, unknown );
					if ( IsFileExist )
					{
						char * DataPointer = ( char * )*OutDataPointer;
						size_t DataSize = *OutSize;

						Buffer ResultBuffer;
						ResultBuffer.buf = new char[ DataSize ];
						ResultBuffer.length = DataSize;

						memcpy( &ResultBuffer.buf[ 0 ], DataPointer, DataSize );

						tmpih->buf = ResultBuffer.buf;
						tmpih->size = ResultBuffer.length;

						tmpih->hashlen = DotaRedirectHelp.NewFilePath.length( );
						tmpih->hash = GetBufHash( DotaRedirectHelp.NewFilePath.c_str( ), tmpih->hashlen );

						ICONMDLCACHELIST.push_back( *tmpih );

						*OutDataPointer = ( int )tmpih->buf;
						*OutSize = tmpih->size;

						IsFileExist = ProcessFile( DotaRedirectHelp.NewFilePath, OutDataPointer, OutSize, unknown, IsFileExist );

						return IsFileExist;
					}
				}
				else
				{

					*OutDataPointer = ( int )tmpih->buf;
					*OutSize = tmpih->size;

					IsFileExist = ProcessFile( DotaRedirectHelp.NewFilePath, OutDataPointer, OutSize, unknown, IsFileExist );

					return TRUE;
				}

				delete[ ] tmpih;
			}
		}


	}

	if ( !IsFileExist )
	{
#ifdef DOTA_HELPER_LOG
		AddNewLineToDotaHelperLog( "Not found" );
#endif
		/*if ( filename && *filename != '\0' )
		{
			MessageBoxA( 0, filename, "File not found", 0 );
		}*/
	}

#ifdef DOTA_HELPER_LOG
	AddNewLineToDotaHelperLog( "ProcessFileEND" );
#endif

	return IsFileExist;
}

