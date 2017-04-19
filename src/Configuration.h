#pragma once

namespace Lspl {
namespace Configuration {

///////////////////////////////////////////////////////////////////////////////

template<typename VALUE_TYPE>
class COrderedList {
public:
	typedef VALUE_TYPE ValueType;
	typedef typename vector<ValueType>::size_type SizeType;

	COrderedList() = default;

	bool IsEmpty() const { return values.empty(); }
	SizeType Size() const { return values.size(); }
	const ValueType& Value( const SizeType index ) const
	{
		debug_check_logic( index < Size() );
		return values[index];
	}
	bool Add( const ValueType& value );
	bool Has( const ValueType& value ) const;
	bool Find( const ValueType& value, SizeType& index ) const;
	void Print( ostream& out, const char* const delimiter ) const;

	static COrderedList Union( const COrderedList&, const COrderedList& );
	static COrderedList Difference( const COrderedList&, const COrderedList& );
	static COrderedList Intersection( const COrderedList&, const COrderedList& );

private:
	vector<ValueType> values;
};

template<typename VALUE_TYPE>
bool COrderedList<VALUE_TYPE>::Add( const ValueType& value )
{
	auto i = lower_bound( values.begin(), values.end(), value );
	if( i != values.cend() && *i == value ) {
		return false;
	}
	values.insert( i, value );
	return true;
}

template<typename VALUE_TYPE>
bool COrderedList<VALUE_TYPE>::Has( const ValueType& value ) const
{
	return binary_search( values.cbegin(), values.cend(), value );
}

template<typename VALUE_TYPE>
bool COrderedList<VALUE_TYPE>::Find( const ValueType& value,
	SizeType& index ) const
{
	auto i = lower_bound( values.cbegin(), values.cend(), value );
	if( i != values.cend() && *i == value ) {
		index = i - values.cbegin();
		return true;
	}
	return false;
}

template<typename VALUE_TYPE>
void COrderedList<VALUE_TYPE>::Print( ostream& out,
	const char* const delimiter ) const
{
	auto i = values.cbegin();
	if( i != values.cend() ) {
		out << *i;
		for( ++i; i != values.cend(); ++i ) {
			out << delimiter << *i;
		}
	}
}

template<typename VALUE_TYPE>
COrderedList<VALUE_TYPE> COrderedList<VALUE_TYPE>::Union(
	const COrderedList& a, const COrderedList& b )
{
	COrderedList result;
	set_union( a.values.cbegin(), a.values.cend(),
		b.values.cbegin(), b.values.cend(),
		back_inserter( result.values ) );
	return result;
}

template<typename VALUE_TYPE>
COrderedList<VALUE_TYPE> COrderedList<VALUE_TYPE>::Difference(
	const COrderedList& a, const COrderedList& b )
{
	COrderedList result;
	set_difference( a.values.cbegin(), a.values.cend(),
		b.values.cbegin(), b.values.cend(),
		back_inserter( result.values ) );
	return result;
}

template<typename VALUE_TYPE>
COrderedList<VALUE_TYPE> COrderedList<VALUE_TYPE>::Intersection(
	const COrderedList& a, const COrderedList& b )
{
	COrderedList result;
	set_intersection( a.values.cbegin(), a.values.cend(),
		b.values.cbegin(), b.values.cend(),
		back_inserter( result.values ) );
	return result;
}

///////////////////////////////////////////////////////////////////////////////

enum TWordSignType {
	WST_None,
	WST_Main,
	WST_Enum,
	WST_String
};

typedef COrderedList<string> COrderedStrings;

struct CWordSign {
	bool Consistent;
	TWordSignType Type;
	COrderedStrings Names;
	COrderedStrings Values;

	CWordSign() :
		Consistent( false ),
		Type( WST_None )
	{
	}

	void Print( ostream& out ) const;
};

class CWordSigns {
	friend class CWordSignsBuilder;
	CWordSigns( const CWordSigns& ) = delete;
	CWordSigns& operator=( const CWordSigns& ) = delete;

public:
	typedef vector<CWordSign>::size_type SizeType;

	CWordSigns();
	bool IsEmpty() const;
	SizeType Size() const;
	SizeType MainWordSignIndex() const;
	const CWordSign& MainWordSign() const;
	const CWordSign& operator[]( SizeType index ) const;
	bool Find( const string& name, SizeType& index ) const;
	void Print( ostream& out ) const;

private:
	vector<CWordSign> wordSigns;
	typedef unordered_map<string, SizeType> CNameIndices;
	CNameIndices nameIndices;
};

class CWordSignsBuilder {
public:
	explicit CWordSignsBuilder( const CWordSigns::SizeType count );
	void Add( CWordSign&& wordSign );
	bool Build( ostream& errorStream, CWordSigns& wordSigns );

private:
	vector<CWordSign> mainSigns;
	vector<CWordSign> consistentSigns;
	vector<CWordSign> notConsistentSigns;
};

class CConfiguration {
	CConfiguration( const CConfiguration& ) = delete;
	CConfiguration& operator=( const CConfiguration& ) = delete;

public:
	const CWordSigns& WordSigns() const
	{
		return wordSigns;
	}

protected:
	CWordSigns wordSigns;

	CConfiguration()
	{
	}
};

typedef shared_ptr<CConfiguration> CConfigurationPtr;

const char* JsonConfigurationSchemeText();

CConfigurationPtr LoadConfigurationFromFile( const char* filename,
	ostream& errorStream, ostream& logStream );

///////////////////////////////////////////////////////////////////////////////

} // end of Configuration namespace
} // end of Lspl namespace
