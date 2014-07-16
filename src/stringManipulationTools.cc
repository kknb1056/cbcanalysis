#include "SLHCUpgradeTracker/CBCAnalysis/interface/stringManipulationTools.h"

#include <sstream>
#include <stdexcept>
#include <cmath>

namespace // Use the unnamed namespace for things only used in this file
{
	/// ASCII codes of characters that are considered whitespace (space, tab, carriage return, line feed).
    const char* whitespace="\x20\x09\x0D\x0A";
} // end of the unnamed namespace


float cbcanalyser::tools::convertStringToFloat( const std::string& string )
{
	float returnValue;
	std::stringstream stringConverter( string );
	stringConverter >> returnValue;
	if( stringConverter.fail() || !stringConverter.eof() ) throw std::runtime_error( "Unable to convert \""+string+"\" to a float" );
	return returnValue;
}

int cbcanalyser::tools::convertHexToInt( const std::string& string )
{
	std::string hexAsString=string;

	if( hexAsString.size()>2 )
	{
		if( hexAsString[0]=='0' && hexAsString[1]=='x' ) hexAsString=hexAsString.substr(2);
	}

	int value=0;

	for( size_t position=0; position<hexAsString.size(); ++position )
	{
		float power=std::pow(2,(hexAsString.size()-position-1)*4);
		if( hexAsString[position]=='0' ) value+=0;
		else if( hexAsString[position]=='1' ) value+=1*power;
		else if( hexAsString[position]=='2' ) value+=2*power;
		else if( hexAsString[position]=='3' ) value+=3*power;
		else if( hexAsString[position]=='4' ) value+=4*power;
		else if( hexAsString[position]=='5' ) value+=5*power;
		else if( hexAsString[position]=='6' ) value+=6*power;
		else if( hexAsString[position]=='7' ) value+=7*power;
		else if( hexAsString[position]=='8' ) value+=8*power;
		else if( hexAsString[position]=='9' ) value+=9*power;
		else if( hexAsString[position]=='a' || hexAsString[position]=='A' ) value+=10*power;
		else if( hexAsString[position]=='b' || hexAsString[position]=='B' ) value+=11*power;
		else if( hexAsString[position]=='c' || hexAsString[position]=='C' ) value+=12*power;
		else if( hexAsString[position]=='d' || hexAsString[position]=='D' ) value+=13*power;
		else if( hexAsString[position]=='e' || hexAsString[position]=='E' ) value+=14*power;
		else if( hexAsString[position]=='f' || hexAsString[position]=='F' ) value+=15*power;
		else throw std::runtime_error( "Unexpected char "+hexAsString[position] );
	}

	return value;
}

std::vector<std::string> cbcanalyser::tools::splitByWhitespace( const std::string& stringToSplit )
{
	std::vector<std::string> returnValue;

	size_t currentPosition=0;
	size_t nextDelimeter=0;
	do
	{
		// Skip over any leading whitespace
		size_t nextElementStart=stringToSplit.find_first_not_of( ::whitespace, currentPosition );
		if( nextElementStart!=std::string::npos ) currentPosition=nextElementStart;

		// Find the next whitespace and subtract everything up to that point
		nextDelimeter=stringToSplit.find_first_of( ::whitespace, currentPosition );
		std::string element=stringToSplit.substr( currentPosition, nextDelimeter-currentPosition );
		returnValue.push_back(element);

		// skip over any trailing whitespace
		nextElementStart=stringToSplit.find_first_not_of( ::whitespace, nextDelimeter );
		if( nextElementStart!=std::string::npos ) currentPosition=nextElementStart;
		else nextDelimeter=std::string::npos;

	} while( nextDelimeter!=std::string::npos );

	return returnValue;
}

std::vector<std::string> cbcanalyser::tools::splitByDelimeters( const std::string& stringToSplit, const std::string& delimeters )
{
	std::vector<std::string> returnValue;

	size_t currentPosition=0;
	size_t nextDelimeter=0;
	do
	{
		// Find the next occurence of one of the delimeters and subtract everything up to that point
		nextDelimeter=stringToSplit.find_first_of( delimeters, currentPosition );
		std::string element=stringToSplit.substr( currentPosition, nextDelimeter-currentPosition );
		returnValue.push_back(element);

		// Move on to the next part of the string
		if( nextDelimeter+1<stringToSplit.size() ) currentPosition=nextDelimeter+1;
		else nextDelimeter=std::string::npos;

	} while( nextDelimeter!=std::string::npos );

	return returnValue;
}
