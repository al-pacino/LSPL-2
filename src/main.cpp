#include <common.h>
#include <Parser.h>
#include <Tokenizer.h>
#include <PatternMatch.h>
#include <Configuration.h>
#include <ErrorProcessor.h>
#include <PatternsFileProcessor.h>

using namespace Lspl;
using namespace Lspl::Text;
using namespace Lspl::Parser;
using namespace Lspl::Pattern;
using namespace Lspl::Configuration;

///////////////////////////////////////////////////////////////////////////////

namespace {

class CRecognitionCallback : public IRecognitionCallback {
public:
	explicit CRecognitionCallback( const CPatterns& patterns );

	void OnRecognized(
		const Text::TWordIndex begin, const Text::TWordIndex end,
		const Text::CText& text, const CData& data,
		const CVariantParts& parts ) override;

private:
	const CPatterns& patterns;
};

CRecognitionCallback::CRecognitionCallback( const CPatterns& _patterns ) :
	patterns( _patterns )
{
}

void CRecognitionCallback::OnRecognized(
	const TWordIndex begin, const TWordIndex end,
	const CText& text, const CData& /*data*/,
	const CVariantParts& parts )
{
	TWordIndex wi = begin;
	for( const CBaseVariantPart* const vp : parts ) {
		if( vp == nullptr ) {
			cout << "} ";
		} else {
			switch( vp->Type() ) {
				case VPR_Word:
					cout << patterns.Element( vp->Word() ) << ":"
						<< text.Word( wi ).text << " ";
					wi++;
					break;
				case VPR_Regexp:
					cout << vp->Regexp() << ":"
						<< text.Word( wi ).text << " ";
					wi++;
					break;
				case VPR_Instance:
					cout << patterns.Reference( vp->Instance() ) << "{ ";
					break;
			}
		}
	}
	check_logic( ( end + 1 ) == wi );
	cout << endl;
}

}

///////////////////////////////////////////////////////////////////////////////

int main( int argc, const char* argv[] )
{
	try {
		if( argc != 5 ) {
			cerr << "Usage: lspl2 CONFIGURATION PATTERNS TEXT RESULT" << endl;
			return 1;
		}

		CConfigurationPtr conf( new CConfiguration );
		if( !conf->LoadFromFile( argv[1], cout, cerr ) ) {
			return 1;
		}

		CAnnotation::SetArgreementBegin( conf->Attributes().Size() );
		for( TAttribute a = 0; a < conf->Attributes().Size(); a++ ) {
			if( conf->Attributes()[a].Agreement() ) {
				CAnnotation::SetArgreementBegin( a );
				break;
			}
		}

		CErrorProcessor errorProcessor;
		CPatternsBuilder patternsBuilder( conf, errorProcessor );
		patternsBuilder.ReadFromFile( argv[2] );
		patternsBuilder.CheckAndBuildIfPossible();

		if( errorProcessor.HasAnyErrors() ) {
			errorProcessor.PrintErrors( cerr, argv[2] );
			return 1;
		}

		const CPatterns patterns = patternsBuilder.GetResult();
		patterns.Print( cout );

		CText text( conf );
		if( !text.LoadFromFile( argv[3], cerr ) ) {
			return 1;
		}

		CRecognitionCallback recognitionCallback( patterns );

		for( TReference ref = 0; ref < patterns.Size(); ref++ ) {
			const CPattern& pattern = patterns.Pattern( ref );
			cout << pattern.Name() << endl;

			CPatternBuildContext buildContext( patterns );
			CPatternVariants variants;
			pattern.Build( buildContext, variants, 12 );
			variants.Print( patterns, cout );
			variants.Build( buildContext );

			CMatchContext matchContext( text, buildContext.States );
			matchContext.SetRecognitionCallback( &recognitionCallback );
			for( TWordIndex wi = 0; wi < text.Length(); wi++ ) {
				matchContext.Match( wi );
			}

			cout << endl;
		}
	} catch( exception& e ) {
		cerr << e.what() << endl;
		return 1;
	} catch( ... ) {
		cerr << "unknown error!";
		return 1;
	}
	return 0;
}
