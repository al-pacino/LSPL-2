#pragma once

#include <Configuration.h>
#include <PatternMatch.h>

namespace Lspl {
namespace Pattern {

///////////////////////////////////////////////////////////////////////////////

class CPatterns;
class CPatternVariant;
class CPatternVariants;
class CPatternBuildContext;

///////////////////////////////////////////////////////////////////////////////

typedef size_t TElement;
typedef size_t TReference;
typedef Text::TAttribute TSign;

// Sample( A7, N7.c, Sub.Pa, SubSub.c ) = A7 N7 Sub SubSub
// Sub( Pa5 ) = Pa5
// SubSub( Pn7 ) = Pn7
enum TPatternArgumentType {
	PAT_None,
	PAT_Element,				// A7
	PAT_ElementSign,			// N7.c
	PAT_ReferenceElement,		// Sub.Pa
	PAT_ReferenceElementSign	// SubSub.c
};

struct CPatternArgument {
	TPatternArgumentType Type;
	TElement Element;
	TReference Reference;
	TSign Sign;

	CPatternArgument();
	explicit CPatternArgument( const TElement element,
		const TPatternArgumentType type = PAT_Element,
		const TSign sign = 0, const TReference reference = 0 );

	bool Defined() const;
	bool HasSign() const;
	void RemoveSign();
	bool HasReference() const;
	bool Inconsistent( const CPatternArgument& arg ) const;
	void Print( const CPatterns& context, ostream& out ) const;

	struct Hasher {
		size_t operator()( const CPatternArgument& arg ) const;
	};
	struct Comparator {
		bool operator()( const CPatternArgument& arg1,
			const CPatternArgument& arg2 ) const;
	};
};

typedef vector<CPatternArgument> CPatternArguments;

///////////////////////////////////////////////////////////////////////////////

class IPatternBase {
	IPatternBase( const IPatternBase& ) = delete;
	IPatternBase& operator=( const IPatternBase& ) = delete;

public:
	IPatternBase();
	virtual ~IPatternBase();
	IPatternBase( IPatternBase&& ) = default;
	IPatternBase& operator=( IPatternBase&& ) = default;

	virtual void Print( const CPatterns& context, ostream& out ) const = 0;
	virtual TVariantSize MinSizePrediction() const = 0;
	virtual void Build( CPatternBuildContext& context,
		CPatternVariants& variants, const TVariantSize maxSize ) const = 0;
};

typedef unique_ptr<IPatternBase> CPatternBasePtr;
typedef vector<CPatternBasePtr> CPatternBasePtrs;

///////////////////////////////////////////////////////////////////////////////

class CPatternSequence : public IPatternBase {
public:
	explicit CPatternSequence( CPatternBasePtrs&& elements,
		const bool transposition = false );
	~CPatternSequence() override {}

	// IPatternBase
	void Print( const CPatterns& context, ostream& out ) const override;
	TVariantSize MinSizePrediction() const override;
	void Build( CPatternBuildContext& context, CPatternVariants& variants,
		const TVariantSize maxSize ) const override;

private:
	const bool transposition;
	const CPatternBasePtrs elements;

	void collectAllSubVariants( CPatternBuildContext& context,
		vector<CPatternVariants>& allSubVariants,
		const TVariantSize maxSize ) const;
};

///////////////////////////////////////////////////////////////////////////////

class CCondition {
public:
	CCondition( const bool strong, const CPatternArguments& arguments );
	CCondition( const string& dictionary, const CPatternArguments& arguments );
	bool Agreement() const { return dictionary.empty(); }
	bool SelfAgreement() const { return Agreement() && arguments.size() == 1; }
	bool Strong() const { return strong; }
	const string& Dictionary() const { return dictionary; }
	const CPatternArguments& Arguments() const { return arguments; }
	void Print( const CPatterns& context, ostream& out ) const;

private:
	bool strong;
	string dictionary;
	CPatternArguments arguments;
};

class CConditions {
public:
	explicit CConditions( vector<CCondition>&& conditions );
	void Apply( CPatternVariant& variant ) const;
	void Print( const CPatterns& context, ostream& out ) const;

private:
	vector<CCondition> data;
	typedef unordered_multimap<
		CPatternArgument, pair<TVariantSize, TVariantSize>,
		CPatternArgument::Hasher,
		CPatternArgument::Comparator> CIndices;
	CIndices indices;

	// Agreement Link : condition_index, word_index, argument_index
	// Dictionary Link : condition_index, argument_index, word_index
	typedef set<array<TVariantSize, 3>> CLinks;

	bool buildLinks( CPatternVariant& variant, CLinks& links ) const;
};

///////////////////////////////////////////////////////////////////////////////

class CPatternAlternative : public IPatternBase {
public:
	CPatternAlternative( CPatternBasePtr&& element, CConditions&& conditions );
	~CPatternAlternative() override {}

	// IPatternBase
	void Print( const CPatterns& context, ostream& out ) const override;
	TVariantSize MinSizePrediction() const override;
	void Build( CPatternBuildContext& context, CPatternVariants& variants,
		const TVariantSize maxSize ) const override;

private:
	CPatternBasePtr element;
	CConditions conditions;
};

typedef unique_ptr<CPatternAlternative> CPatternAlternativePtr;
typedef vector<CPatternAlternativePtr> CPatternAlternativePtrs;

///////////////////////////////////////////////////////////////////////////////

class CPatternAlternatives : public IPatternBase {
public:
	explicit CPatternAlternatives( CPatternBasePtrs&& alternatives );
	~CPatternAlternatives() override {}

	// IPatternBase
	void Print( const CPatterns& context, ostream& out ) const override;
	TVariantSize MinSizePrediction() const override;
	void Build( CPatternBuildContext& context, CPatternVariants& variants,
		const TVariantSize maxSize ) const override;

private:
	const CPatternBasePtrs alternatives;
};

///////////////////////////////////////////////////////////////////////////////

class CPatternRepeating : public IPatternBase {
public:
	CPatternRepeating( CPatternBasePtr&& element,
		const TVariantSize minCount, const TVariantSize maxCount );
	~CPatternRepeating() override {}

	// IPatternBase
	void Print( const CPatterns& context, ostream& out ) const override;
	TVariantSize MinSizePrediction() const override;
	void Build( CPatternBuildContext& context, CPatternVariants& variants,
		const TVariantSize maxSize ) const override;

private:
	const CPatternBasePtr element;
	const TVariantSize minCount;
	const TVariantSize maxCount;
};

///////////////////////////////////////////////////////////////////////////////

class CPatternRegexp : public IPatternBase {
public:
	explicit CPatternRegexp( const string& regexp );
	~CPatternRegexp() override {}

	// IPatternBase
	void Print( const CPatterns& context, ostream& out ) const override;
	TVariantSize MinSizePrediction() const override;
	void Build( CPatternBuildContext& context, CPatternVariants& variants,
		const TVariantSize maxSize ) const override;

private:
	string regexp;
};

///////////////////////////////////////////////////////////////////////////////

typedef COrderedList<Text::TAttributeValue> CSignValues;

class CSignRestriction {
public:
	CSignRestriction( const TElement element, const TSign sign,
		CSignValues&& values, const bool exclude = false );

	TSign Sign() const { return sign; }
	TElement Element() const { return element; }
	// returns true if an intersection is not empty
	void Intersection( const CSignRestriction& signRestriction );
	// returns true if there are no words matching restriction
	bool IsEmpty( const CPatterns& context ) const;
	void Build( Text::CAttributesRestrictionBuilder& builder ) const;
	void Print( const CPatterns& context, ostream& out ) const;

private:
	TElement element;
	TSign sign;
	bool exclude;
	CSignValues values;
};

///////////////////////////////////////////////////////////////////////////////

class CSignRestrictions {
public:
	CSignRestrictions() = default;
	// returns true if the sign restriction was added
	bool Add( CSignRestriction&& signRestriction );
	// returns true if an intersection is not empty
	void Intersection( const CSignRestrictions& signRestrictions,
		const TElement element );
	// returns true if all sign restrictions are empty
	bool IsEmpty( const CPatterns& context ) const;
	Text::CAttributesRestriction Build(
		const Configuration::CConfiguration& configuration ) const;
	void Print( const CPatterns& context, ostream& out ) const;

private:
	vector<CSignRestriction> data;
};

///////////////////////////////////////////////////////////////////////////////

class CPatternElement : public IPatternBase {
public:
	explicit CPatternElement( const TElement element );
	CPatternElement( const TElement element, CSignRestrictions&& signs );
	~CPatternElement() override {}

	// IPatternBase
	void Print( const CPatterns& context, ostream& out ) const override;
	TVariantSize MinSizePrediction() const override;
	void Build( CPatternBuildContext& context, CPatternVariants& variants,
		const TVariantSize maxSize ) const override;

private:
	const TElement element;
	const CSignRestrictions signs;
};

///////////////////////////////////////////////////////////////////////////////

class CPatternReference : public IPatternBase {
public:
	explicit CPatternReference( const TReference reference );
	CPatternReference( const TReference reference, CSignRestrictions&& signs );
	~CPatternReference() override {}

	// IPatternBase
	void Print( const CPatterns& context, ostream& out ) const override;
	TVariantSize MinSizePrediction() const override;
	void Build( CPatternBuildContext& context, CPatternVariants& variants,
		const TVariantSize maxSize ) const override;

private:
	const TReference reference;
	const CSignRestrictions signs;
};

///////////////////////////////////////////////////////////////////////////////

class CPattern : public IPatternBase {
public:
	CPattern( const string& name, CPatternBasePtr&& root,
		const CPatternArguments& arguments );
	CPattern( CPattern&& ) = default;
	CPattern& operator=( CPattern&& ) = default;

	const string& Name() const { return name; }
	const CPatternArguments& Arguments() const { return arguments; }

	// IPatternBase
	void Print( const CPatterns& context, ostream& out ) const override;
	TVariantSize MinSizePrediction() const override;
	void Build( CPatternBuildContext& context, CPatternVariants& variants,
		const TVariantSize maxSize ) const override;

private:
	string name;
	CPatternBasePtr root;
	CPatternArguments arguments;
};

///////////////////////////////////////////////////////////////////////////////

class CPatterns {
	CPatterns( const CPatterns& ) = delete;
	CPatterns& operator=( const CPatterns& ) = delete;

public:
	CPatterns( CPatterns&& ) = default;
	CPatterns& operator=( CPatterns&& ) = default;

	const Configuration::CConfiguration& Configuration() const { return *configuration; }

	TReference Size() const { return Patterns.size(); }
	const CPattern& Pattern( const TReference reference ) const;
	void Print( ostream& out ) const;
	string Element( const TElement element ) const;
	string Reference( const TReference reference ) const;
	TReference PatternReference( const string& name,
		const TReference nameIndex = 0 ) const;

protected:
	vector<CPattern> Patterns;
	unordered_map<string, TReference> Names;

	explicit CPatterns( Configuration::CConfigurationPtr configuration );

private:
	const Configuration::CConfigurationPtr configuration;
};

///////////////////////////////////////////////////////////////////////////////

struct CPatternWord {
	CPatternArgument Id;
	const string* Regexp;
	CSignRestrictions SignRestrictions;
	CActions Actions;

	explicit CPatternWord( const string* const regexp );
	CPatternWord( const CPatternArgument id,
		const CSignRestrictions& signRestrictions );

	void Build( CPatternBuildContext& context ) const;
	void Print( const CPatterns& context, ostream& out ) const;
};

///////////////////////////////////////////////////////////////////////////////

class CPatternVariant : public vector<CPatternWord> {
public:
	CPatternVariant& operator+=( const CPatternVariant& variant )
	{
		this->insert( this->end(), variant.cbegin(), variant.cend() );
		return *this;
	}

	void Build( CPatternBuildContext& context ) const;
	void Print( const CPatterns& context, ostream& out ) const;
};

///////////////////////////////////////////////////////////////////////////////

class CPatternVariants : public vector<CPatternVariant> {
public:
	void SortAndRemoveDuplicates( const CPatterns& context );
	void Build( CPatternBuildContext& context ) const;
	void Print( const CPatterns& context, ostream& out ) const;
};

///////////////////////////////////////////////////////////////////////////////

class CPatternBuildContext {
public:
	CStates States;
	vector<pair<string, TStateIndex>> LastVariant;

	explicit CPatternBuildContext( const CPatterns& patterns );

	const CPatterns& Patterns() const { return patterns; }

	TVariantSize PushMaxSize( const TReference reference,
		const TVariantSize maxSize );
	TVariantSize PopMaxSize( const TReference reference );

	static void AddVariants( const vector<CPatternVariants>& allSubVariants,
		vector<CPatternVariant>& variants, const size_t maxSize );

private:
	const CPatterns& patterns;
	struct CPatternBuildData {
		stack<TVariantSize> MaxSizes;
	};
	vector<CPatternBuildData> data;

	static bool nextIndices( const vector<CPatternVariants>& allSubVariants,
		vector<size_t>& indices );
};

///////////////////////////////////////////////////////////////////////////////

} // end of Pattern namespace
} // end of Lspl namespace
