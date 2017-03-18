#include <common.h>
#include <Parser.h>
#include <Tokenizer.h>
#include <ErrorProcessor.h>

namespace LsplParser {

///////////////////////////////////////////////////////////////////////////////

CPatternParser::CPatternParser( CErrorProcessor& _errorProcessor ) :
	errorProcessor( _errorProcessor )
{
}

void CPatternParser::Parse( const CTokens& _tokens )
{
	tokens = CTokensList( _tokens );

	if( !readPattern() ) {
		return;
	}

	if( !readTextExtractionPatterns() ) {
		return;
	}

	if( tokens.Has() ) {
		addError( "end of template definition expected" );
	}
}

void CPatternParser::addError( const string& text )
{
	CError error( text );

	if( tokens.Has() ) {
		error.Line = tokens->Line;
		error.LineSegments.push_back( tokens.Token() );
	} else {
		error.Line = tokens.Last().Line;
		error.LineSegments.emplace_back();
	}

	errorProcessor.AddError( move( error ) );
}

// reads Identifier [ . Identifier ]
bool CPatternParser::readExtendedName( CExtendedName& name )
{
	if( !tokens.CheckType( TT_Identifier ) ) {
		addError( "word class or pattern name expected" );
		return false;
	}

	name.first = tokens.TokenPtr();
	tokens.Next(); // skip identifier

	if( tokens.MatchType( TT_Dot ) ) {
		if( !tokens.CheckType( TT_Identifier ) ) {
			addError( "word class attribute name expected" );
			return false;
		}

		name.second = tokens.TokenPtr();
		tokens.Next(); // skip identifier
	} else {
		name.second = nullptr;
	}

	return true;
}

bool CPatternParser::readPatternName()
{
	if( !tokens.CheckType( TT_Identifier ) ) {
		addError( "pattern name expected" );
		return false;
	}

	//..
	tokens.Next();
	return true;
}

bool CPatternParser::readPatternArguments()
{
	if( tokens.MatchType( TT_OpeningParenthesis ) ) {
		CExtendedNames arguments;
		do {
			arguments.emplace_back();
			if( !readExtendedName( arguments.back() ) ) {
				return false;
			}
		} while( tokens.MatchType( TT_Comma ) );

		if( !tokens.MatchType( TT_ClosingParenthesis ) ) {
			addError( "closing parenthesis `)` expected" );
			return false;
		}
	}
	return true;
}

bool CPatternParser::readPattern()
{
	if( !readPatternName() || !readPatternArguments() ) {
		return false;
	}
	if( !tokens.MatchType( TT_EqualSign ) ) {
		addError( "equal sign `=` expected" );
		return false;
	}
	unique_ptr<CAlternativesNode> alternatives;
	if( !readAlternatives( alternatives ) ) {
		return false;
	}

	alternatives->Print( cout ); // TODO: temporary
	cout << endl;
	cout << endl;

	vector<string> variants;
	alternatives->MakeVariants( variants );
	for( const string& variant : variants ) {
		cout << variant << endl;
	}

	return true;
}

bool CPatternParser::readElementCondition( CElementCondition& elementCondition )
{
	elementCondition.Clear();

	if( tokens.CheckType( TT_Identifier ) &&
		( tokens.CheckType( TT_EqualSign, 1 /* offset */ )
			|| tokens.CheckType( TT_ExclamationPointEqualSign, 1 /* offset */ ) ) )
	{
		elementCondition.Name = tokens.TokenPtr();
		elementCondition.Sign = tokens.TokenPtr( 1 /* offset */ );
		tokens.Next( 2 );
	} else if( tokens.CheckType( TT_EqualSign )
		|| tokens.CheckType( TT_ExclamationPointEqualSign ) )
	{
		elementCondition.Sign = tokens.TokenPtr();
		tokens.Next();
	}

	do {
		if( tokens.CheckType( TT_Regexp ) || tokens.CheckType( TT_Identifier ) ) {
			elementCondition.Values.push_back( tokens.TokenPtr() );
			tokens.Next();
		} else {
			addError( "regular expression or word class attribute value expected" );
			return false;
		}
	} while( tokens.MatchType( TT_VerticalBar ) );

	return true;
}

bool CPatternParser::readElementConditions( CElementConditions& elementConditions )
{
	elementConditions.clear();

	if( tokens.MatchType( TT_LessThanSign ) ) {
		do {
			elementConditions.emplace_back();
			if( !readElementCondition( elementConditions.back() ) ) {
				return false;
			}
		} while( tokens.MatchType( TT_Comma ) );

		if( !tokens.MatchType( TT_GreaterThanSign ) ) {
			addError( "greater than sign `>` expected" );
			return false;
		}
	}
	return true;
}

bool CPatternParser::readElement( unique_ptr<CBasePatternNode>& element )
{
	element = nullptr;

	if( tokens.Has() ) {
		switch( tokens->Type ) {
			case TT_Regexp:
				element.reset( new CRegexpNode( tokens.TokenPtr() ) );
				tokens.Next();
				break;
			case TT_Identifier:
			{
				unique_ptr<CElementNode> tmpElement( new CElementNode( tokens.TokenPtr() ) );
				tokens.Next();
				if( !readElementConditions( *tmpElement ) ) {
					return false;
				}
				element = move( tmpElement );
				break;
			}
			case TT_OpeningBrace:
			{
				tokens.Next();
				unique_ptr<CAlternativesNode> alternatives;
				if( !readAlternatives( alternatives ) ) {
					return false;
				}
				check_logic( static_cast<bool>( alternatives ) );
				if( !tokens.MatchType( TT_ClosingBrace ) ) {
					addError( "closing brace `}` expected" );
					return false;
				}

				CTokenPtr min;
				CTokenPtr max;
				if( tokens.MatchType( TT_LessThanSign ) ) { // < NUMBER, NUMBER >
					if( !tokens.MatchType( TT_Number, min ) ) {
						addError( "number (0, 1, 2, etc.) expected" );
						return false;
					}
					if( tokens.MatchType( TT_Comma )
						&& !tokens.MatchType( TT_Number, max ) )
					{
						addError( "number (0, 1, 2, etc.) expected" );
						return false;
					}
					if( !tokens.MatchType( TT_GreaterThanSign ) ) {
						addError( "greater than sign `>` expected" );
						return false;
					}
				}
				element.reset( new CRepeatingNode( move( alternatives ), min, max ) );
				break;
			}
			case TT_OpeningBracket:
			{
				tokens.Next();
				unique_ptr<CAlternativesNode> alternatives;
				if( !readAlternatives( alternatives ) ) {
					return false;
				}
				check_logic( static_cast<bool>( alternatives ) );
				if( !tokens.MatchType( TT_ClosingBracket ) ) {
					addError( "closing bracket `]` expected" );
					return false;
				}
				element.reset( new CRepeatingNode( move( alternatives ) ) );
				break;
			}
			case TT_OpeningParenthesis:
			{
				tokens.Next();
				unique_ptr<CAlternativesNode> alternatives;
				if( !readAlternatives( alternatives ) ) {
					return false;
				}
				check_logic( static_cast<bool>( alternatives ) );
				if( !tokens.MatchType( TT_ClosingParenthesis ) ) {
					addError( "closing parenthesis `)` expected" );
					return false;
				}
				element = move( alternatives );
				break;
			}
			default:
				break;
		}
	}
	return true;
}

bool CPatternParser::readElements( unique_ptr<CBasePatternNode>& out )
{
	out = nullptr;
	unique_ptr<CElementsNode> elements( new CElementsNode );
	while( true ) {
		unique_ptr<CBasePatternNode> element;
		if( !readElement( element ) ) {
			return false;
		}

		if( !element ) {
			break;
		}
		elements->push_back( move( element ) );
	}
	if( elements->empty() ) {
		addError( "at least one template element expected" );
		return false;
	}

	if( elements->size() == 1 ) {
		swap( out, elements->front() );
	} else {
		out.reset( elements.release() );
	}

	return true;
}

bool CPatternParser::readMatchingCondition( CMatchingCondition& condition )
{
	condition.Elements.emplace_back();
	if( !readExtendedName( condition.Elements.back() ) ) {
		return false;
	}

	condition.IsStrong = tokens.MatchType( TT_DoubleEqualSign );
	if( !condition.IsStrong && !tokens.MatchType( TT_EqualSign ) ) {
		addError( "equal sign `=` or double equal `==` sign expected" );
		return false;
	}

	do {
		condition.Elements.emplace_back();
		if( !readExtendedName( condition.Elements.back() ) ) {
			return false;
		}

		if( tokens.CheckType( TT_EqualSign ) ) {
			if( condition.IsStrong ) {
				addError( "inconsistent equal sign `=` and double equal `==` sign" );
			}
		} else if( tokens.CheckType( TT_DoubleEqualSign ) ) {
			if( !condition.IsStrong ) {
				addError( "inconsistent equal sign `=` and double equal `==` sign" );
			}
		}
	} while( tokens.MatchType( TT_EqualSign ) || tokens.MatchType( TT_DoubleEqualSign ) );

	return true;
}

// reads TT_Identifier `(` TT_Identifier { TT_Identifier } { `,` TT_Identifier { TT_Identifier } } `)`
bool CPatternParser::readDictionaryCondition( CDictionaryCondition& condition )
{
	if( !tokens.MatchType( TT_Identifier, condition.DictionaryName ) ) {
		addError( "dictionary name expected" );
		return false;
	}
	if( !tokens.MatchType( TT_OpeningParenthesis ) ) {
		addError( "opening parenthesis `(` expected" );
		return false;
	}
	do {
		condition.Arguments.emplace_back();
		while( tokens.CheckType( TT_Identifier ) ) {
			condition.Arguments.back().push_back( tokens.TokenPtr() );
			tokens.Next();
		}
		if( condition.Arguments.back().empty() ) {
			addError( "at least one pattern element expected" );
			return false;
		}
	} while( tokens.MatchType( TT_Comma ) );

	if( !tokens.MatchType( TT_ClosingParenthesis ) ) {
		addError( "closing parenthesis `)` expected" );
		return false;
	}
	return true;
}

bool CPatternParser::readAlternativeCondition( CAlternativeConditions& conditions )
{
	if( tokens.CheckType( TT_OpeningParenthesis, 1 /* offset */ ) ) {
		conditions.DictionaryConditions.emplace_back();
		return readDictionaryCondition( conditions.DictionaryConditions.back() );
	} else {
		conditions.MatchingConditions.emplace_back();
		return readMatchingCondition( conditions.MatchingConditions.back() );
	}
}

// reads << ... >>
bool CPatternParser::readAlternativeConditions( CAlternativeConditions& conditions )
{
	if( tokens.MatchType( TT_DoubleLessThanSign ) ) {
		do {
			if( !readAlternativeCondition( conditions ) ) {
				return false;
			}
		} while( tokens.MatchType( TT_Comma ) );

		if( !tokens.MatchType( TT_DoubleGreaterThanSign ) ) {
			addError( "double greater than sign `>>` expected" );
			return false;
		}
	}
	return true;
}

bool CPatternParser::readAlternative( unique_ptr<CAlternativeNode>& alternative )
{
	unique_ptr<CTranspositionNode> transposition( new CTranspositionNode );
	do {
		unique_ptr<CBasePatternNode> elements;
		if( !readElements( elements ) ) {
			return false;
		}
		check_logic( static_cast<bool>( elements ) );
		transposition->push_back( move( elements ) );
	} while( tokens.MatchType( TT_Tilde ) );
	check_logic( !transposition->empty() );

	if( transposition->size() == 1 ) {
		alternative.reset( new CAlternativeNode( move( transposition->front() ) ) );
	} else {
		alternative.reset( new CAlternativeNode( move( transposition ) ) );
	}

	if( !readAlternativeConditions( *alternative ) ) {
		return false;
	}

	return true;
}

bool CPatternParser::readAlternatives( unique_ptr<CAlternativesNode>& alternatives )
{
	alternatives.reset( new CAlternativesNode );

	do {
		unique_ptr<CAlternativeNode> alternative;
		if( !readAlternative( alternative ) ) {
			return false;
		}
		check_logic( static_cast<bool>( alternative ) );
		alternatives->push_back( move( alternative ) );
	} while( tokens.MatchType( TT_VerticalBar ) );
	check_logic( !alternatives->empty() );

	return true;
}

bool CPatternParser::readTextExtractionPrefix()
{
	if( tokens.CheckType( TT_EqualSign )
		&& tokens.CheckType( TT_Identifier, 1 /* offset */ )
		&& tokens.Token( 1 /* offset */ ).Text == "text"
		&& tokens.CheckType( TT_GreaterThanSign, 2 /* offset */ ) )
	{
		tokens.Next( 3 /* count */ );
		return true;
	}
	return false;
}

bool CPatternParser::readTextExtractionPatterns()
{
	if( readTextExtractionPrefix() ) {
		do {
			if( !readTextExtractionPattern() ) {
				return false;
			}
		} while( tokens.MatchType( TT_Comma ) );
	}
	return true;
}

bool CPatternParser::readTextExtractionPattern()
{
	if( !readTextExtractionElements() ) {
		return false;
	}

	if( tokens.MatchType( TT_DoubleLessThanSign ) ) {
		do {
			CExtendedName name;
			if( !readExtendedName( name ) ) {
				return false;
			}
			if( !tokens.MatchType( TT_TildeGreaterThanSign ) ) {
				addError( "tilde and greater than sign `~>` expected" );
				return false;
			}
			if( !readExtendedName( name ) ) {
				return false;
			}
		} while( tokens.MatchType( TT_Comma ) );

		if( !tokens.MatchType( TT_DoubleGreaterThanSign ) ) {
			addError( "double greater than sign `>>` expected" );
			return false;
		}
	}
	return true;
}

bool CPatternParser::readTextExtractionElements()
{
	if( !readTextExtractionElement( true /* required */ ) ) {
		return false;
	}

	while( readTextExtractionElement() ) {
	}

	return true;
}

bool CPatternParser::readTextExtractionElement( const bool required )
{
	if( tokens.CheckType( TT_Regexp ) ) {
		tokens.Next();
	} else if( tokens.MatchType( TT_NumberSign ) ) {
		if( !tokens.MatchType( TT_Identifier ) ) {
			addError( "word class or pattern name expected" );
			return false;
		}
	} else if( tokens.MatchType( TT_Identifier ) ) {
		if( tokens.MatchType( TT_LessThanSign ) ) {
			while( tokens.MatchType( TT_Identifier ) ) {
				if( tokens.MatchType( TT_Regexp ) ) {
					// todo:
				} else if( tokens.MatchType( TT_Identifier ) ) {
					// todo:
				} else {
					addError( "regular expression or word class attribute value expected" );
					return false;
				}
			}
			if( !tokens.MatchType( TT_GreaterThanSign ) ) {
				addError( "greater than sign `>` expected" );
				return false;
			}
		}
	} else {
		if( required ) {
			addError( "text extraction element expected" );
		}
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////

} // end of LsplParser namespace
