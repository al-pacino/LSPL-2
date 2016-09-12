#include <common.h>
#include <Tools.h>

namespace LsplTools {

///////////////////////////////////////////////////////////////////////////////

void ReplaceTabsWithSpacesInSignleLine( string& line )
{
	string result;
	result.reserve( line.length() );
	size_t offset = 0;
	for( char c : line ) {
		check_logic( c != '\n' && c != '\r' );
		if( c == '\t' ) {
			const size_t spaceCount = TabSize - ( offset % TabSize );
			result += string( spaceCount, ' ' );
			offset += spaceCount;
		} else {
			result += c;
			if( IsByteFirstInUtf8Symbol( c ) ) {
				offset++;
			}
		}
	}
	line = move( result );
}

string::size_type IsValidUtf8( const string& text )
{
	return string::npos;
}

string::size_type IsValidText( const string& text )
{
	return string::npos;
}

///////////////////////////////////////////////////////////////////////////////

} // end of LsplTools namespace
