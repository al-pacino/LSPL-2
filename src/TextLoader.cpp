#include <common.h>
#include <TextLoader.h>

#define RAPIDJSON_ASSERT debug_check_logic
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/istreamwrapper.h>

using namespace Lspl::Pattern;
using namespace Lspl::Configuration;

///////////////////////////////////////////////////////////////////////////////

namespace Lspl {
namespace Text {

///////////////////////////////////////////////////////////////////////////////

bool LoadText( const CPatterns& context, const char* filename, CWords& _words )
{
	_words.clear();
	using namespace rapidjson;

	Document textDocument;
	{
		ifstream ifs1( filename );
		IStreamWrapper isw1( ifs1 );
		textDocument.ParseStream( isw1 );
	}
	if( textDocument.HasParseError() ) {
		cerr << "Parse text '" << filename << "' error at char "
			<< textDocument.GetErrorOffset() << ": "
			<< GetParseError_En( textDocument.GetParseError() ) << endl;
		return false;
	}

	if( !textDocument.IsObject()
		|| !textDocument.HasMember( "text" )
		|| !textDocument["text"].IsArray() )
	{
		cerr << "bad 'text' element" << endl;
		return false;
	}

	const CConfiguration& configuration = context.Configuration();
	const CWordSigns& wordSigns = configuration.WordSigns();

	CWords words;
	Value wordsArray = textDocument["text"].GetArray();
	for( rapidjson::SizeType wi = 0; wi < wordsArray.Size(); wi++ ) {
		if( !wordsArray[wi].IsObject()
			|| !wordsArray[wi].HasMember( "word" )
			|| !wordsArray[wi]["word"].IsString()
			|| !wordsArray[wi].HasMember( "annotations" )
			|| !wordsArray[wi]["annotations"].IsArray()
			|| wordsArray[wi]["annotations"].GetArray().Size() == 0 )
		{
			cerr << "bad 'word' #" << wi << " element" << endl;
			return false;
		}

		Value wordObject = wordsArray[wi].GetObject();

		words.emplace_back();
		words.back().text = wordObject["word"].GetString();
		words.back().word = ToStringEx( words.back().text );

		Value annotations = wordObject["annotations"].GetArray();
		for( rapidjson::SizeType ai = 0; ai < annotations.Size(); ai++ ) {
			if( !annotations[ai].IsObject() ) {
				cerr << "bad 'word' #" << wi
					<< " 'annotation' #" << ai
					<< " element" << endl;
				return false;
			}

			CAttributes attributes( Cast<TAttribute>( wordSigns.Size() ) );

			Value attrObject = annotations[ai].GetObject();
			for( Value::ConstMemberIterator attr = attrObject.MemberBegin();
				attr != attrObject.MemberEnd(); ++attr )
			{
				if( !attr->value.IsString() ) {
					cerr << "bad 'word' #" << wi
						<< " 'annotation' #" << ai
						<< " attribute value" << endl;
					return false;
				}

				CWordSigns::SizeType index;
				if( wordSigns.Find( attr->name.GetString(), index ) ) {
					const CWordSign& wordSign = wordSigns[index];
					const string value = attr->value.GetString();
					COrderedStrings::SizeType valueIndex;
					if( wordSign.Type == WST_String ) {
						valueIndex = context.StringIndex( value );
					} else {
						if( !wordSign.Values.Find( value, valueIndex ) ) {
							valueIndex = wordSigns[index].Values.Size();
						}
					}
					// TODO: check!
					attributes.Set( Cast<TAttribute>( index ),
						Cast<TAttributeValue>( valueIndex ) );
				}
			}

			if( attributes.Get( MainAttribute ) == NullAttributeValue ) {
				cerr << "bad 'word' #" << wi
					<< " 'annotation' #" << ai
					<< " has no main attribute" << endl;
				return false;
			}

			words.back().annotations.emplace_back( move( attributes ) );
		}
	}

	_words = move( words );
	return true;
}

///////////////////////////////////////////////////////////////////////////////

} // end of Text namespace
} // end of Lspl namespace
