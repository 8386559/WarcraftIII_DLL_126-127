#include "Main.h"


pGetHeroInt GetHeroInt;
// �������� ��������� �����
int __stdcall GetUnitOwnerSlot( int unitaddr )
{
	return *( int* ) ( unitaddr + 88 );
}



// �������� �� ���� ������
BOOL __stdcall IsHero( int unitaddr )
{
	if ( unitaddr )
	{
		UINT ishero = *( UINT* ) ( unitaddr + 48 );
		ishero = ishero >> 24;
		ishero = ishero - 64;
		return ishero < 0x19;
	}
	return FALSE;
}


// �������� �� ���� �������
BOOL __stdcall IsTower( int unitaddr )
{
	if ( unitaddr )
	{
		UINT istower = *( UINT* ) ( unitaddr + 0x5C );
		return ( istower & 0x10000 ) > 0;
	}
	return FALSE;
}


// ��������� ���� ��� �� ����
BOOL __stdcall IsNotBadUnit( int unitaddr )
{
	if ( unitaddr > 0 )
	{
		int xaddraddr = ( int ) &UnitVtable;

		if ( *( BYTE* ) xaddraddr != *( BYTE* ) unitaddr )
			return FALSE;
		else if ( *( BYTE* ) ( xaddraddr + 1 ) != *( BYTE* ) ( unitaddr + 1 ) )
			return FALSE;
		else if ( *( BYTE* ) ( xaddraddr + 2 ) != *( BYTE* ) ( unitaddr + 2 ) )
			return FALSE;
		else if ( *( BYTE* ) ( xaddraddr + 3 ) != *( BYTE* ) ( unitaddr + 3 ) )
			return FALSE;

		unsigned int unitflag = *( unsigned int* ) ( unitaddr + 0x20 );
		unsigned int unitflag2 = *( unsigned int* ) ( unitaddr + 0x5C );

		if ( unitflag & 1u )
			return FALSE;

		if ( !( unitflag & 2u ) )
			return FALSE;

		if ( unitflag2 & 0x100u )
			return FALSE;

		return TRUE;
	}

	return FALSE;
}




// ��������� ������� ��� �� �������
BOOL __stdcall IsNotBadItem( int itemaddr )
{
	if ( itemaddr > 0 )
	{
		int xaddraddr = ( int ) &ItemVtable;

		if ( *( BYTE* ) xaddraddr != *( BYTE* ) itemaddr )
			return FALSE;
		else if ( *( BYTE* ) ( xaddraddr + 1 ) != *( BYTE* ) ( itemaddr + 1 ) )
			return FALSE;
		else if ( *( BYTE* ) ( xaddraddr + 2 ) != *( BYTE* ) ( itemaddr + 2 ) )
			return FALSE;
		else if ( *( BYTE* ) ( xaddraddr + 3 ) != *( BYTE* ) ( itemaddr + 3 ) )
			return FALSE;

		return TRUE;
	}

	return FALSE;
}

