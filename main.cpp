#include <string>
#include <map>
#include <fstream>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/support_istream_iterator.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/operators.hpp>

using namespace std;
using namespace boost::spirit;
namespace ph = boost::phoenix;
namespace sn = boost::spirit::standard_wide;

// ==================================================================================================================================================

typedef string					String;
typedef pair<String, String>	Field;
typedef map<String, String>		FieldList;

struct Entry
{
	String		type;			// article, book, inbook, etc.
	String		citation_key;	// for example: @article{citation_key, ...
	FieldList	fields;			// tag = content

	void	dump()
	{
		cout << "-------------------------------------" << endl
			 << "Type: " << type << endl
			 << "Key : " << citation_key << endl;
		for( auto v : fields )
			cout << v.first << " = " << v.second << endl;
	}
};

BOOST_FUSION_ADAPT_STRUCT(
	Entry,
	(String,	type)
	(String,	citation_key)
	(FieldList,	fields)
)

// ==================================================================================================================================================
template <class ForwardIterator, class Skipper>
class Parser : public qi::grammar<ForwardIterator, Entry(), Skipper>
{
public:
	Parser() : Parser::base_type( m_start, "Parser" )
	{
		// Документ может состоять из мусора и записей
		m_start = m_junk >> m_entry;

		// Мусор это любые символы кроме @
		m_junk = *~qi::lit( '@' );
		// Запись начинается с @ и содержит тип записи и тело, которое находится между {}
		m_entry = '@' >>
					m_type[ ph::at_c<0>( _val ) = _1 ] >>
					'{' >> m_body[ ph::at_c<1>( _val ) = ph::at_c<0>( _1 ), ph::at_c<2>( _val ) = ph::at_c<1>( _1 ) ] >> '}';

		// Тип это несколько букв и/или цифр
		m_type = +sn::alnum;
		// Тело содержит ключ и набор полей
		m_body = m_citation_key >> ',' >> m_fields >> -qi::lit( ',' );

		// Ключ это любые символы до запятой
		m_citation_key = qi::lexeme[ +(~qi::char_( ',' ) - sn::space) ];
		// Набор полей состояит из нескольких полей
		m_fields = -( m_field % ',' );

		// Поле содержит ключ и значение
		m_field = m_key >> '=' >> m_value;

		m_key = qi::lexeme[ +(~qi::char_( "=,})" ) - sn::space) ];
		m_value = m_quoted | +~qi::char_( ",})#" );

		m_quoted = qi::lexeme[ ( '"' >> *m_innerQuoteText >> '"' ) | ( '{' >> *m_innerBraceText >> '}' ) ]
							 [ _val = ph::accumulate(_1, ph::construct<std::string>()) ];

		m_innerQuoteText %= qi::as_string[ qi::char_('{') >> *(m_innerQuoteText | m_escapedText) >> qi::char_('}') ] | m_quoteText;
		m_innerBraceText %= qi::as_string[ qi::char_('{') >> *(m_innerBraceText | m_escapedText) >> qi::char_('}') ] | m_escapedText;

		m_quoteText = +( m_escapedQuote | ~qi::char_("\"{}") );
		m_escapedText = !qi::lit('{') >> +( m_escapedBrace | ~qi::char_("{}") );

		m_escapedBrace.add
			("\\{", '{')
			("\\}", '}')
			;
		m_escapedQuote.add
			("\\\"", '"')
			;
	}

private:
	typedef boost::fusion::vector<String,FieldList>		BodyParam;

	qi::symbols<char, char>								m_escapedBrace;
	qi::symbols<char, char>								m_escapedQuote;

	qi::rule<ForwardIterator, Entry(),		Skipper>	m_start;
	qi::rule<ForwardIterator,				Skipper>	m_junk;
	qi::rule<ForwardIterator, Entry(),		Skipper>	m_entry;
	qi::rule<ForwardIterator, String(),		Skipper>	m_type;
	qi::rule<ForwardIterator, BodyParam(),	Skipper>	m_body;
	qi::rule<ForwardIterator, String(),		Skipper>	m_citation_key;
	qi::rule<ForwardIterator, FieldList(),	Skipper>	m_fields;
	qi::rule<ForwardIterator, Field(),		Skipper>	m_field;
	qi::rule<ForwardIterator, String(),		Skipper>	m_key;
	qi::rule<ForwardIterator, String(),		Skipper>	m_value;
	qi::rule<ForwardIterator, String(),		Skipper>	m_quoted;
	qi::rule<ForwardIterator, String()>					m_innerQuoteText;
	qi::rule<ForwardIterator, String()>					m_innerBraceText;
	qi::rule<ForwardIterator, String()>					m_quoteText;
	qi::rule<ForwardIterator, String()>					m_escapedText;
};

// ==================================================================================================================================================

template<class ForwardIterator, class Skipper, class Container>
inline bool read( ForwardIterator first, ForwardIterator last, Skipper& s, Container& e,
				  boost::enable_if<boost::is_same<typename Container::value_type, Entry> >* /*dummy*/ = NULL )
{
	Parser<ForwardIterator, Skipper> p;
	return qi::phrase_parse(first, last, *p, s, e );
}

// ==================================================================================================================================================

int main( int argc, char *argv[] )
{
	if( argc < 2 )
	{
		cerr << "Using: bibparser file.bib" << endl;
		return 1;
	}

	string filename = argv[ 1 ];
	cout << "Parsing file: " << filename << endl;

	ifstream is( filename );
	is.unsetf( ios_base::skipws );
	if( !is )
	{
		cerr << "Error open source file" << endl;
		return 1;
	}

	boost::spirit::istream_iterator beg( is );
	boost::spirit::istream_iterator end;

	deque<Entry> e;

	auto space = sn::space | '%' >> *(qi::char_ - qi::eol) >> qi::eol;

	cout << "result = " << read( beg, end, space, e ) << endl;

	for( auto v : e )
		v.dump();

	return 0;
}
