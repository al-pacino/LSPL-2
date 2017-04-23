#pragma once

#include <common.h>
#include <PatternMatch.h>

using namespace Lspl::Text;

namespace Lspl {
namespace Pattern {

///////////////////////////////////////////////////////////////////////////////

CPatternWordCondition::CPatternWordCondition( const TValue offset, const TSign param ) :
	Size( 1 ),
	Strong( true ),
	Param( param ),
	Offsets( new TValue[Size] )
{
	Offsets[0] = offset;
}

CPatternWordCondition::CPatternWordCondition( const TValue offset,
		const vector<TValue>& words, const TSign param ) :
	Size( Cast<TValue>( words.size() ) ),
	Strong( false ),
	Param( param ),
	Offsets( new TValue[Size] )
{
	debug_check_logic( !words.empty() );
	debug_check_logic( words.size() < Max );
	for( TValue i = 0; i < Size; i++ ) {
		if( words[i] < Max ) {
			debug_check_logic( words[i] <= offset );
			Offsets[i] = offset - words[i];
		} else {
			Offsets[i] = Max;
		}
	}
}

CPatternWordCondition::CPatternWordCondition(
	const CPatternWordCondition& another )
{
	*this = another;
}

CPatternWordCondition& CPatternWordCondition::operator=(
	const CPatternWordCondition& another )
{
	Size = another.Size;
	Strong = another.Strong;
	Param = another.Param;
	Offsets = new TValue[Size];
	memcpy( Offsets, another.Offsets, Size * sizeof( TValue ) );
	return *this;
}

CPatternWordCondition::CPatternWordCondition( CPatternWordCondition&& another )
{
	*this = move( another );
}

CPatternWordCondition& CPatternWordCondition::operator=(
	CPatternWordCondition&& another )
{
	Size = another.Size;
	Strong = another.Strong;
	Param = another.Param;
	Offsets = another.Offsets;
	another.Offsets = nullptr;
	return *this;
}

CPatternWordCondition::~CPatternWordCondition()
{
	delete[] Offsets;
}

void CPatternWordCondition::Print( ostream& out ) const
{
	out << Param
		<< ( Strong ? "==" : "=" )
		<< static_cast<uint32_t>( Offsets[0] );
	for( TValue i = 1; i < Size; i++ ) {
		out << "," << static_cast<uint32_t>( Offsets[i] );
	}
}

///////////////////////////////////////////////////////////////////////////////

CDataEditor::CDataEditor( CData& _data ) :
	data( _data )
{
}

CDataEditor::~CDataEditor()
{
	Restore();
}

const CData::value_type& CDataEditor::Value( const CData::size_type index ) const
{
	debug_check_logic( index < data.size() );
	return data[index];
}

void CDataEditor::Set( const CData::size_type index,
	CData::value_type&& value ) const
{
	debug_check_logic( index < data.size() );
	dump.insert( make_pair( index, data[index] ) );
	data[index] = value;
}

void CDataEditor::Restore()
{
	for( pair<const CData::size_type, CData::value_type>& key : dump ) {
		debug_check_logic( key.first < data.size() );
		data[key.first] = move( key.second );
	}
}

///////////////////////////////////////////////////////////////////////////////

bool CTransition::Match( const CWord& word, CAnnotationIndices& indices ) const
{
	if( Word ) {
		if( !word.MatchWord( WordOrAttributesRegex ) ) {
			return false;
		}
		indices = move( word.AnnotationIndices() );
		return true;
	} else {
		return word.MatchAttributes( WordOrAttributesRegex, indices );
	}
}

///////////////////////////////////////////////////////////////////////////////

void CActions::Add( shared_ptr<IAction> action )
{
	actions.emplace_back( action );
}

bool CActions::Run( const CMatchContext& context ) const
{
	for( const shared_ptr<IAction>& action : actions ) {
		if( !action->Run( context ) ) {
			return false;
		}
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////

CMatchContext::CMatchContext( const CText& text, const CStates& states ) :
	text( text ),
	states( states ),
	wordIndex( 0 ),
	initialWordIndex( 0 ),
	dataEditor( data )
{
	data.reserve( 32 );
}

const TWordIndex CMatchContext::Shift() const
{
	return ( Word() - InitialWord() );
}

void CMatchContext::Match( const TWordIndex _initialWordIndex )
{
	initialWordIndex = _initialWordIndex;
	wordIndex = initialWordIndex;
	match( 0 );
}

void CMatchContext::match( const TStateIndex stateIndex )
{
	const CState& state = states[stateIndex];
	const CTransitions& transitions = state.Transitions;

	wordIndex--; // dirty hack
	if( !state.Actions.Run( *this ) // conditions are not met
		|| transitions.empty() // leaf
		|| !( ++wordIndex < Text().End() ) ) // end of text
	{
		return;
	}

	for( const CTransition& transition : transitions ) {
		data.emplace_back();
		if( transition.Match( Text().Word( wordIndex ), data.back() ) ) {
			wordIndex++;
			match( transition.NextState );
			wordIndex--;
		}
		data.pop_back();
	}
}

///////////////////////////////////////////////////////////////////////////////

CAgreementAction::CAgreementAction( const CPatternWordCondition& _condition ) :
	condition( _condition )
{
	// TODO: checks
}

bool CAgreementAction::Run( const CMatchContext& context ) const
{
	const CDataEditor& editor = context.DataEditor();
	const CArgreements& agreements = context.Text().Argreements();
	const TWordIndex index2 = context.Shift();
	const CAnnotationIndices& indices2 = editor.Value( index2 );
	
	CArgreements::CKey key{ { 0, context.Word() }, condition.Param };
	for( CPatternWordCondition::TValue i = 0; i < condition.Size; i++ ) {
		const CPatternWordCondition::TValue offset = condition.Offsets[i];
		debug_check_logic( offset < index2 );
		const TWordIndex index1 = index2 - offset;
		const CAnnotationIndices& indices1 = editor.Value( index1 );
		key.first.first = context.Word() - offset;
		CAgreement agr = agreements.Agreement( key, condition.Strong );
		agr.first = CAnnotationIndices::Intersection( agr.first, indices1 );
		agr.second = CAnnotationIndices::Intersection( agr.second, indices2 );

		if( agr.first.IsEmpty() || agr.second.IsEmpty() ) {
			return false;
		}

		editor.Set( index1, move( agr.first ) );
		editor.Set( index2, move( agr.second ) );
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////

CDictionaryAction::CDictionaryAction( const CPatternWordCondition& _condition ) :
	condition( _condition )
{
	// TODO: checks
}

bool CDictionaryAction::Run( const CMatchContext& context ) const
{
	vector<string> words;
	words.emplace_back();
	for( CPatternWordCondition::TValue i = 0; i < condition.Size; i++ ) {
		const CPatternWordCondition::TValue offset = condition.Offsets[i];
		if( offset == CPatternWordCondition::Max ) {
			debug_check_logic( !words.back().empty() );
			words.back().pop_back();
			words.emplace_back();
		} else {
			debug_check_logic( offset <= context.Shift() );
			const TWordIndex word = context.Word() - offset;
			words.back() += context.Text().Word( word ).text + " ";
		}
	}
	debug_check_logic( !words.back().empty() );
	words.back().pop_back();

#ifdef _DEBUG
	cout << "dictionary{" << condition.Param << "}(";
	bool first = true;
	for( const string& word : words ) {
		if( first ) {
			first = false;
		} else {
			cout << ",";
		}
		cout << word;
	}
	cout << ");" << endl;
#endif

	return true;
}

///////////////////////////////////////////////////////////////////////////////

CPrintAction::CPrintAction( ostream& _out ) :
	out( _out )
{
}

bool CPrintAction::Run( const CMatchContext& context ) const
{
	const TWordIndex begin = context.InitialWord();
	const TWordIndex end = context.Word();

	out << "{";
	for( TWordIndex wi = begin; wi < end; wi++ ) {
		out << context.Text().Word( wi ).text << " ";
	}
	out << context.Text().Word( end ).text << "}" << endl;

	return true;
}

///////////////////////////////////////////////////////////////////////////////

} // end of Pattern namespace
} // end of Lspl namespace
