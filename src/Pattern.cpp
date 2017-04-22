#include <common.h>
#include <Pattern.h>
#include <TranspositionSupport.h>
#include <Parser.h>

using namespace Lspl::Configuration;
using Lspl::Parser::CIndexedName;

namespace Lspl {
namespace Pattern {

///////////////////////////////////////////////////////////////////////////////

CPatternArgument::CPatternArgument() :
	Type( PAT_None ),
	Element( 0 ),
	Reference( 0 ),
	Sign( 0 )
{
}

CPatternArgument::CPatternArgument( const TElement element,
		const TPatternArgumentType type, const TSign sign,
		const TReference reference ) :
	Type( type ),
	Element( element ),
	Reference( reference ),
	Sign( sign )
{
}

bool CPatternArgument::Defined() const
{
	return ( Type != PAT_None );
}

bool CPatternArgument::HasSign() const
{
	return ( Type == PAT_ElementSign || Type == PAT_ReferenceElementSign );
}

bool CPatternArgument::HasReference() const
{
	return ( Type == PAT_ReferenceElement || Type == PAT_ReferenceElementSign );
}

bool CPatternArgument::Inconsistent( const CPatternArgument& arg ) const
{
	if( !Defined() || !arg.Defined() ) {
		return false;
	}
	if( HasSign() != arg.HasSign() ) {
		return true;
	}
	return ( Sign != arg.Sign );
}

void CPatternArgument::Print( const CPatterns& context, ostream& out ) const
{
	debug_check_logic( Type != PAT_None );
	if( HasReference() ) {
		out << context.Reference( Reference ) << ".";
	}
	out << context.Element( Element );
	if( HasSign() ) {
		out << "." << context.SignName( Sign );
	}
}

size_t CPatternArgument::Hasher::operator()(
	const CPatternArgument& arg ) const
{
	return ( arg.Reference ^ ( arg.Element << 16 ) ^ ( arg.Sign << 24 )
		^ ( static_cast<size_t>( arg.Type ) << 30 ) );
}

bool CPatternArgument::Comparator::operator()(
	const CPatternArgument& arg1, const CPatternArgument& arg2 ) const
{
	return ( arg1.Type == arg2.Type
		&& arg1.Element == arg2.Element
		&& arg1.Reference == arg2.Reference
		&& arg1.Sign == arg2.Sign );
}

///////////////////////////////////////////////////////////////////////////////

CPatternSequence::CPatternSequence( CPatternBasePtrs&& _elements,
		const bool _transposition ) :
	elements( move( _elements ) ),
	transposition( _transposition )
{
	debug_check_logic( !elements.empty() );
}


void CPatternSequence::Print( const CPatterns& context, ostream& out ) const
{
	bool first = true;
	for( const CPatternBasePtr& childNode : elements ) {
		if( first ) {
			first = false;
		} else {
			out << ( transposition ? " ~ " : " " );
		}
		childNode->Print( context, out );
	}
}

size_t CPatternSequence::MinSizePrediction() const
{
	size_t minSizePrediction = 0;
	for( const CPatternBasePtr& childNode : elements ) {
		minSizePrediction += childNode->MinSizePrediction();
	}
	return minSizePrediction;
}

void CPatternSequence::Build( CPatternBuildContext& context,
	CPatternVariants& variants, const size_t maxSize ) const
{
	vector<CPatternVariants> allSubVariants;
	collectAllSubVariants( context, allSubVariants, maxSize );

	if( allSubVariants.empty() ) {
		return;
	}

	debug_check_logic( allSubVariants.size() == elements.size() );
	context.AddVariants( allSubVariants, variants, maxSize );

	if( !transposition ) {
		return;
	}

	const CTranspositionSupport::CSwaps& swaps =
		CTranspositionSupport::Instance().Swaps( allSubVariants.size() );
	for( const CTranspositionSupport::CSwap& swap : swaps ) {
		swap.Apply( allSubVariants );
		context.AddVariants( allSubVariants, variants, maxSize );
	}
}

void CPatternSequence::collectAllSubVariants( CPatternBuildContext& context,
	vector<CPatternVariants>& allSubVariants, const size_t maxSize ) const
{
	allSubVariants.clear();
	if( maxSize == 0 ) {
		return;
	}

	const size_t minSize = MinSizePrediction();
	if( minSize > maxSize ) {
		return;
	}

	allSubVariants.reserve( elements.size() );
	for( const CPatternBasePtr& childNode : elements ) {
		const size_t emsp = childNode->MinSizePrediction();
		const size_t mes = maxSize - minSize + emsp;

		CPatternVariants subVariants;
		childNode->Build( context, subVariants, mes );
		if( subVariants.empty() ) {
			allSubVariants.clear();
			return;
		}

		allSubVariants.push_back( subVariants );
	}
}

///////////////////////////////////////////////////////////////////////////////

CCondition::CCondition( const bool _strong,
		const CPatternArguments& _arguments ) :
	strong( _strong ),
	arguments( move( _arguments ) )
{
	debug_check_logic( arguments.size() == 2 );
	debug_check_logic( arguments[0].HasSign() == arguments[1].HasSign() );
}

CCondition::CCondition( const string& _dictionary,
		const CPatternArguments& _arguments ) :
	strong( false ),
	dictionary( _dictionary ),
	arguments( move( _arguments ) )
{
	debug_check_logic( !dictionary.empty() );
	debug_check_logic( !arguments.empty() );
}

void CCondition::Print( const CPatterns& context, ostream& out ) const
{
	if( dictionary.empty() ) {
		bool first = true;
		for( const CPatternArgument& arg : arguments ) {
			if( first ) {
				first = false;
			} else {
				out << ( strong ? "==" : "=" );
			}
			arg.Print( context, out );
		}
	} else {
		out << dictionary << "(";
		bool first = true;
		for( const CPatternArgument& arg : arguments ) {
			if( arg.Type == PAT_None ) {
				out << ", ";
				first = true;
			} else {
				if( first ) {
					first = false;
				} else {
					out << " ";
				}
				arg.Print( context, out );
			}
		}
		out << ")";
	}
}

///////////////////////////////////////////////////////////////////////////////

CConditions::CConditions( vector<CCondition>&& conditions ) :
	data( move( conditions ) )
{
	for( TIndex i = 0; i < data.size(); i++ ) {
		for( const CPatternArgument& word : data[i].Arguments() ) {
			if( !word.HasSign() ) {
				indices.insert( make_pair( word, i ) );
			} else {
				CPatternArgument tmp = word;
				tmp.Type = word.Type == PAT_ElementSign ?
					PAT_Element : PAT_ReferenceElement;
				indices.insert( make_pair( tmp, i ) );
			}
		}
	}
}

void CConditions::Print( const CPatterns& context, ostream& out ) const
{
	if( data.empty() ) {
		return;
	}

	out << "<<";
	bool first = true;
	for( const CCondition& condition : data ) {
		if( first ) {
			first = false;
		} else {
			out << ",";
		}
		condition.Print( context, out );
	}
	out << ">>";
}

///////////////////////////////////////////////////////////////////////////////

CPatternAlternative::CPatternAlternative( CPatternBasePtr&& _element,
		CConditions&& _conditions ) :
	element( move( _element ) ),
	conditions( _conditions )
{
	debug_check_logic( static_cast<bool>( element ) );
}

void CPatternAlternative::Print( const CPatterns& context, ostream& out ) const
{
	element->Print( context, out );
	conditions.Print( context, out );
}

size_t CPatternAlternative::MinSizePrediction() const
{
	return element->MinSizePrediction();
}

void CPatternAlternative::Build( CPatternBuildContext& context,
	CPatternVariants& variants, const size_t maxSize ) const
{
	element->Build( context, variants, maxSize );
#pragma message( "not implemented!" )
	// TODO: not implemented
	/*for( CPatternVariant& variant : variants ) {
		debug_check_logic( !variant.empty() );
		variant.back().Conditions = conditions;
	}*/
	variants.SortAndRemoveDuplicates( context.Patterns() );
}

///////////////////////////////////////////////////////////////////////////////

CPatternAlternatives::CPatternAlternatives( CPatternBasePtrs&& _alternatives ) :
	alternatives( move( _alternatives ) )
{
	debug_check_logic( !alternatives.empty() );
}

void CPatternAlternatives::Print( const CPatterns& context, ostream& out ) const
{
	out << "( ";
	bool first = true;
	for( const CPatternBasePtr& alternative : alternatives ) {
		if( first ) {
			first = false;
		} else {
			out << " | ";
		}
		alternative->Print( context, out );
	}
	out << " )";
}

size_t CPatternAlternatives::MinSizePrediction() const
{
	check_logic( !alternatives.empty() );
	size_t minSizePrediction = numeric_limits<size_t>::max();
	for( const CPatternBasePtr& alternative : alternatives ) {
		minSizePrediction = min( minSizePrediction, alternative->MinSizePrediction() );
	}
	return minSizePrediction;
}

void CPatternAlternatives::Build( CPatternBuildContext& context,
	CPatternVariants& variants, const size_t maxSize ) const
{
	for( const CPatternBasePtr& alternative : alternatives ) {
		CPatternVariants subVariants;
		alternative->Build( context, subVariants, maxSize );
		variants.insert( variants.end(),
			subVariants.cbegin(), subVariants.cend() );
	}
	variants.SortAndRemoveDuplicates( context.Patterns() );
}

///////////////////////////////////////////////////////////////////////////////

CPatternRepeating::CPatternRepeating( CPatternBasePtr&& _element,
		const size_t _minCount, const size_t _maxCount ) :
	element( move( _element ) ),
	minCount( _minCount ),
	maxCount( _maxCount )
{
	debug_check_logic( static_cast<bool>( element ) );
	debug_check_logic( minCount <= maxCount );
	debug_check_logic( maxCount > 0 );
}

void CPatternRepeating::Print( const CPatterns& context, ostream& out ) const
{
	out << "{ ";
	element->Print( context, out );
	out << " }<" << minCount << "," << maxCount << ">";
}

size_t CPatternRepeating::MinSizePrediction() const
{
	return minCount;
}

void CPatternRepeating::Build( CPatternBuildContext& context,
	CPatternVariants& variants, const size_t maxSize ) const
{
	variants.clear();
	debug_check_logic( minCount <= maxCount );

	if( minCount == 0 ) {
		variants.emplace_back();
	}
	if( maxSize == 0 ) {
		return;
	}

	const size_t start = minCount > 0 ? minCount : 1;
	const size_t nmsp = element->MinSizePrediction();
	const size_t nsmsp = nmsp * start;
	if( nsmsp > maxSize ) {
		return;
	}

	size_t finish = min( maxCount, maxSize / nmsp );
	const size_t elementMaxSize = finish - nsmsp + nmsp;

	CPatternVariants subVariants;
	element->Build( context, subVariants, elementMaxSize );

	vector<CPatternVariants> allSubVariants( start, subVariants );
	context.AddVariants( allSubVariants, variants, maxSize );

	for( size_t count = start + 1; count <= finish; count++ ) {
		const size_t variantsSize = variants.size();
		for( size_t vi = 0; vi < variantsSize; vi++ ) {
			const CPatternVariant& variant = variants[vi];
			for( const CPatternVariant& subVariant : subVariants ) {
				if( variant.size() + subVariant.size() <= maxSize ) {
					variants.push_back( variant );
					variants.back() += subVariant;
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

CPatternRegexp::CPatternRegexp( const string& _regexp ) :
	regexp( _regexp )
{
	debug_check_logic( !regexp.empty() );
}

void CPatternRegexp::Print( const CPatterns& /*context*/, ostream& out ) const
{
	out << '"' << regexp << '"';
}

size_t CPatternRegexp::MinSizePrediction() const
{
	return 1;
}

void CPatternRegexp::Build( CPatternBuildContext& context,
	CPatternVariants& variants, const size_t maxSize ) const
{
	variants.clear();
	if( maxSize > 0 ) {
		CPatternVariant variant;
		variant.emplace_back( &regexp );
		variants.push_back( variant );
	}
}

///////////////////////////////////////////////////////////////////////////////

CSignRestriction::CSignRestriction( const TElement _element,
		const TSign _sign, CSignValues&& _values, const bool _exclude ) :
	element( _element ),
	sign( _sign ),
	exclude( _exclude ),
	values( move( _values ) )
{
	debug_check_logic( !values.IsEmpty() );
}

void CSignRestriction::Intersection( const CSignRestriction& restriction )
{
	debug_check_logic( sign == restriction.Sign() );
	if( exclude && restriction.exclude ) {
		values = CSignValues::Union( values, restriction.values );
	} else if( exclude && !restriction.exclude ) {
		exclude = false;
		values = CSignValues::Difference( restriction.values, values );
	} else if( !exclude && restriction.exclude ) {
		values = CSignValues::Difference( values, restriction.values );
	} else if( !exclude && !restriction.exclude ) {
		values = CSignValues::Intersection( values, restriction.values );
	}
}

bool CSignRestriction::IsEmpty( const CPatterns& context ) const
{
	const CWordSign& wordSign = context.Configuration().WordSigns()[sign];
	debug_check_logic( wordSign.Type != WST_None );
	if( exclude ) {
		if( wordSign.Type == WST_String ) {
			// todo: check
			return false;
		} else {
			return ( values.Size() == wordSign.Values.Size() );
		}
	} else {
		return values.IsEmpty();
	}
}

void CSignRestriction::Print( const CPatterns& context, ostream& out ) const
{
	out << context.SignName( sign );
	out << ( exclude ? "!=" : "=" );
	for( CSignValues::SizeType i = 0; i < values.Size(); i++ ) {
		if( i > 0 ) {
			out << "|";
		}
		out << context.SignValue( sign, values.Value( i ) );
	}
}

///////////////////////////////////////////////////////////////////////////////

bool CSignRestrictions::Add( CSignRestriction&& restriction )
{
	struct {
		bool operator()( const CSignRestriction& restriction1,
			const CSignRestriction& restriction2 ) const
		{
			return ( restriction1.Element() < restriction2.Element()
				|| ( restriction1.Element() == restriction2.Element()
					&& restriction1.Sign() < restriction2.Sign() ) );
		}
	} comparator;

	auto i = lower_bound( data.begin(), data.end(), restriction, comparator );
	if( i != data.cend() && !comparator( restriction, *i ) ) { // *i == restriction
		return false;
	}
	data.insert( i, move( restriction ) );
	return true;
}

void CSignRestrictions::Intersection( const CSignRestrictions& restrictions,
	const TElement element )
{
	if( restrictions.data.empty() ) {
		return;
	}

	struct {
		bool operator()( const CSignRestriction& restriction,
			const TElement element ) const
		{
			return restriction.Element() < element;
		}
		bool operator()( const TElement element,
			const CSignRestriction& restriction ) const
		{
			return element < restriction.Element();
		}
	} filter;
	auto range = equal_range( restrictions.data.begin(),
		restrictions.data.end(), element, filter );

	struct {
		bool operator()( const CSignRestriction& restriction1,
			const CSignRestriction& restriction2 ) const
		{
			return restriction1.Sign() < restriction2.Sign();
		}
	} comparator;

	for( auto i = range.first; i != range.second; ++i ) {
		auto j = lower_bound( data.begin(), data.end(), *i, comparator );
		if( j != data.cend() && !comparator( *j, *i ) ) { // *i == *j
			j->Intersection( *i );
		} else {
			data.insert( j, *i );
		}
	}
}

bool CSignRestrictions::IsEmpty( const CPatterns& context ) const
{
	for( const CSignRestriction& signRestriction : data ) {
		if( signRestriction.IsEmpty( context ) ) {
			return true;
		}
	}
	return false;
}

void CSignRestrictions::Print( const CPatterns& context, ostream& out ) const
{
	if( data.empty() ) {
		return;
	}

	out << "<";
	bool first = true;
	for( const CSignRestriction& signRestriction : data ) {
		if( first ) {
			first = false;
		} else {
			out << ",";
		}
		signRestriction.Print( context, out );
	}
	out << ">";
}

///////////////////////////////////////////////////////////////////////////////

CPatternElement::CPatternElement( const TElement _element ) :
	element( _element )
{
}

CPatternElement::CPatternElement( const TElement _element,
		CSignRestrictions&& _signs ) :
	element( _element ),
	signs( move( _signs ) )
{
}

void CPatternElement::Print( const CPatterns& context, ostream& out ) const
{
	out << context.Element( element );
	signs.Print( context, out );
}

size_t CPatternElement::MinSizePrediction() const
{
	return 1;
}

void CPatternElement::Build( CPatternBuildContext& context,
	CPatternVariants& variants, const size_t maxSize ) const
{
	variants.clear();
	if( maxSize > 0 ) {
		CPatternVariant variant;
		variant.emplace_back( CPatternArgument( element ), signs );
		variants.push_back( variant );
	}
}

///////////////////////////////////////////////////////////////////////////////

CPatternReference::CPatternReference( const TReference _reference ) :
	reference( _reference )
{
}

CPatternReference::CPatternReference( const TReference _reference,
		CSignRestrictions&& _signs ) :
	reference( _reference ),
	signs( move( _signs ) )
{
}

void CPatternReference::Print( const CPatterns& context, ostream& out ) const
{
	out << context.Reference( reference );
	signs.Print( context, out );
}

size_t CPatternReference::MinSizePrediction() const
{
	return 1;
}

void CPatternReference::Build( CPatternBuildContext& context,
	CPatternVariants& variants, const size_t maxSize ) const
{
	const CPattern& pattern = context.Patterns().ResolveReference( reference );
	pattern.Build( context, variants, maxSize );

	auto last = variants.begin();
	for( auto variant = last; variant != variants.end(); ++variant ) {
		bool isEmpty = false;
		for( CPatternWord& word : *variant ) {
			if( word.Id.Type == PAT_ReferenceElement ) {
				word.Id.Reference = reference;
				// apply SignRestrictions
				word.SignRestrictions.Intersection( signs, word.Id.Element );
				if( word.SignRestrictions.IsEmpty( context.Patterns() ) ) {
					isEmpty = true;
					break;
				}
			} else {
				debug_check_logic( word.Id.Type == PAT_None );
			}
		}
		if( !isEmpty ) {
			*last = move( *variant );
			++last;
		}
	}
	variants.erase( last, variants.end() );
}

///////////////////////////////////////////////////////////////////////////////

CPattern::CPattern( const string& _name, CPatternBasePtr&& _root,
		const CPatternArguments& _arguments ) :
	name( _name ),
	root( move( _root ) ),
	arguments( _arguments )
{
	debug_check_logic( !name.empty() );
	debug_check_logic( static_cast<bool>( root ) );
}

void CPattern::Print( const CPatterns& context, ostream& out ) const
{
	out << name;
	if( !arguments.empty() ) {
		out << "( ";
		bool first = true;
		for( const CPatternArgument& arg : arguments ) {
			if( first ) {
				first = false;
			} else {
				out << ", ";
			}
			arg.Print( context, out );
		}
		out << " )";
	}
	out << " = ";
	root->Print( context, out );
	out << endl;
}

size_t CPattern::MinSizePrediction() const
{
	return root->MinSizePrediction();
}

void CPattern::Build( CPatternBuildContext& context,
	CPatternVariants& variants, const size_t maxSize ) const
{
	const size_t correctMaxSize = context.PushMaxSize( name, maxSize );
	root->Build( context, variants, correctMaxSize );
	const size_t topMaxSize = context.PopMaxSize( name );
	debug_check_logic( topMaxSize == correctMaxSize );

	// correct ids
	const COrderedStrings::SizeType mainSize = context.Patterns()
		.Configuration().WordSigns().MainWordSign().Values.Size();
	const TReference patternReference = context.Patterns().PatternReference( name );
	for( CPatternVariant& variant : variants ) {
		for( CPatternWord& word : variant ) {
			if( word.Id.Type != PAT_Element ) {
				word.Id = CPatternArgument();
				continue;
			}
			for( CPatternArguments::size_type i = 0; i < arguments.size(); i++ ) {
				if( word.Id.Element == arguments[i].Element ) {
					word.Id.Type = PAT_ReferenceElement;
					word.Id.Element = word.Id.Element % mainSize + i * mainSize;
					word.Id.Reference = patternReference;
					break;
				}
			}
			if( word.Id.Type == PAT_Element ) {
				word.Id = CPatternArgument();
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

CPatterns::CPatterns( Configuration::CConfigurationPtr _configuration ) :
	configuration( _configuration )
{
	check_logic( static_cast<bool>( configuration ) );
}

void CPatterns::Print( ostream& out ) const
{
	for( const CPattern& pattern : Patterns ) {
		pattern.Print( *this, out );
		CPatternBuildContext buildContext( *this );
		CPatternVariants variants;
		pattern.Build( buildContext, variants, 5 );
		variants.Print( *this, out );
		out << endl;
	}
}

string CPatterns::Element( const TElement element ) const
{
	const COrderedStrings& values
		= Configuration().WordSigns().MainWordSign().Values;
	CIndexedName name;
	name.Index = element / values.Size();
	name.Name = values.Value( element % values.Size() );
	return name.Normalize();
}

string CPatterns::Reference( const TReference reference ) const
{
	CIndexedName refName;
	refName.Index = reference / Patterns.size();
	refName.Name = Patterns[reference % Patterns.size()].Name();
	return refName.Normalize();
}

string CPatterns::SignName( const CSignRestriction::TSign sign ) const
{
	const CWordSigns& signs = Configuration().WordSigns();
	debug_check_logic( sign < signs.Size() );
	return signs[sign].Names.Value( 0 );
}

string CPatterns::SignValue( const CSignRestriction::TSign signIndex,
	const CSignValues::ValueType value ) const
{
	const CWordSigns& signs = Configuration().WordSigns();
	debug_check_logic( signIndex < signs.Size() );
	const CWordSign& sign = signs[signIndex];
	if( sign.Type == WST_String ) {
		return String( value );
	} else {
		debug_check_logic( sign.Type == WST_Main || sign.Type == WST_Enum );
		debug_check_logic( value < sign.Values.Size() );
		return sign.Values.Value( value );
	}
}

string CPatterns::String( const CSignValues::ValueType index ) const
{
	debug_check_logic( index < Strings.size() );
	return Strings[index];
}

TReference CPatterns::PatternReference( const string& name,
	const TReference nameIndex ) const
{
	auto pi = Names.find( name );
	if( pi == Names.cend() ) {
		return numeric_limits<TSign>::max();
	}
	return ( pi->second + nameIndex * Names.size() );
}

const CPattern& CPatterns::ResolveReference( const TReference reference ) const
{
	return Patterns[reference % Patterns.size()];
}

///////////////////////////////////////////////////////////////////////////////

void CPatternWord::Print( const CPatterns& context, ostream& out ) const
{
	if( Regexp != nullptr ) {
		out << '"' << *Regexp << '"';
	} else {
		if( Id.Type != PAT_None ) {
			//debug_check_logic( Id.Type == PAT_ReferenceElement );
			Id.Print( context, out );
		}
		SignRestrictions.Print( context, out );
	}
}

void CPatternVariant::Print( const CPatterns& context, ostream& out ) const
{
	for( const CPatternWord& word : *this ) {
		out << " ";
		word.Print( context, out );
	}
}

///////////////////////////////////////////////////////////////////////////////

void CPatternVariants::Print( const CPatterns& context, ostream& out ) const
{
	for( const CPatternVariant& variant : *this ) {
		variant.Print( context, out );
		out << endl;
	}
}

void CPatternVariants::SortAndRemoveDuplicates( const CPatterns& context )
{
#pragma message( "very inefficient" )
	typedef pair<string, CPatternVariant> CPair;
	vector<CPair> pairs;
	pairs.reserve( this->size() );
	for( CPatternVariant& variant : *this ) {
		ostringstream oss1;
		variant.Print( context, oss1 );
		pairs.push_back( make_pair( oss1.str(), move( variant ) ) );
	}

	struct {
		bool operator()( const CPair& v1, const CPair& v2 )
		{
			return ( v1.first < v2.first );
		}
	} comparatorL;
	sort( pairs.begin(), pairs.end(), comparatorL );

	struct {
		bool operator()( const CPair& v1, const CPair& v2 )
		{
			return ( v1.first == v2.first );
		}
	} comparatorEq;
	auto last = unique( pairs.begin(), pairs.end(), comparatorEq );
	pairs.erase( last, pairs.end() );

	this->clear();
	for( CPair& pair : pairs ) {
		this->emplace_back( move( pair.second ) );
	}
}

///////////////////////////////////////////////////////////////////////////////

CPatternBuildContext::CPatternBuildContext( const CPatterns& _patterns ) :
	patterns( _patterns )
{
}

size_t CPatternBuildContext::PushMaxSize( const string& name,
	const size_t maxSize )
{
	auto pair = namedMaxSizes.insert( make_pair( name, stack<size_t>() ) );
	stack<size_t>& maxSizes = pair.first->second;

	if( maxSizes.empty() || maxSize < maxSizes.top() ) {
		maxSizes.push( maxSize );
	} else {
		debug_check_logic( maxSize == maxSizes.top() );
		maxSizes.push( maxSize - 1 );
	}

	return maxSizes.top();
}

size_t CPatternBuildContext::PopMaxSize( const string& name )
{
	auto si = namedMaxSizes.find( name );
	debug_check_logic( si != namedMaxSizes.end() );
	const size_t topMaxSize = si->second.top();
	si->second.pop();
	return topMaxSize;
}

void CPatternBuildContext::AddVariants(
	const vector<CPatternVariants>& allSubVariants,
	vector<CPatternVariant>& variants, const size_t maxSize )
{
	vector<size_t> indices( allSubVariants.size(), 0 );
	do {
		CPatternVariant variant;
		for( size_t i = 0; i < indices.size(); i++ ) {
			variant += allSubVariants[i][indices[i]];
		}
		if( variant.size() <= maxSize ) {
			variants.push_back( variant );
		}
	} while( nextIndices( allSubVariants, indices ) );
}

bool CPatternBuildContext::nextIndices(
	const vector<CPatternVariants>& allSubVariants, vector<size_t>& indices )
{
	for( size_t pos = indices.size(); pos > 0; pos-- ) {
		const size_t realPos = pos - 1;
		if( indices[realPos] < allSubVariants[realPos].size() - 1 ) {
			indices[realPos]++;
			return true;
		} else {
			indices[realPos] = 0;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

} // end of Pattern namespace
} // end of Lspl namespace
