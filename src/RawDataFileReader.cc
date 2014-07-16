#include "SLHCUpgradeTracker/CBCAnalysis/interface/RawDataFileReader.h"

#include <stdexcept>
#include <string>
#include <istream>
#include <iostream>
#include <iomanip>

// Use the unnamed namespace for things only used in this file
namespace
{
	class EndianOrderer
	{
	public:
		EndianOrderer( size_t wordSizeInBytes, bool switchEndianess=true ) : wordLength_(wordSizeInBytes), switchEndianess_(switchEndianess) {}
		void reorder( std::vector<unsigned char>& data )
		{
			if( !switchEndianess_ ) return;

			if( data.size() % wordLength_ != 0 ) throw std::runtime_error( "Data provided is not a multiple of the word size" );

			for( size_t byte=0; byte<data.size(); byte+=wordLength_ )
			{
				for( size_t byteInWord=0; byteInWord<wordLength_/2; ++byteInWord )
				{
					std::swap( data[byte+byteInWord], data[byte+wordLength_-byteInWord-1] );
				}
			}
		}
	protected:
		size_t wordLength_;
		bool switchEndianess_;
	};

	template<class T> T getBytes( const unsigned char originalData[], size_t firstByte, size_t lastByte )
	{
		if( firstByte>lastByte ) std::swap( firstByte, lastByte );

		T result=0;
		for( size_t byte=0; byte<=lastByte-firstByte; ++byte )
		{
			result+=( static_cast<unsigned long int>(originalData[lastByte-byte]) << 8*byte );
		}
		return result;
	}

	template<class T> T getBytes( const std::vector<unsigned char>& originalData, size_t firstByte, size_t lastByte )
	{
		return getBytes<T>( &originalData[0], firstByte, lastByte );
	}

	std::vector<bool> getBits( const unsigned char& byte, size_t firstBit, size_t lastBit )
	{
		if( firstBit>lastBit ) std::swap( firstBit, lastBit );

		std::vector<bool> returnValue(lastBit-firstBit+1);
//std::cout << "Start test" << std::endl;
		for( size_t byteNumber=firstBit/8; byteNumber<=(lastBit)/8; ++byteNumber )
		{
			const unsigned char* pByte=(&byte+byteNumber);
			size_t startBitInThisByte=0;
			if( 8*byteNumber<firstBit ) startBitInThisByte=firstBit-8*byteNumber;
			for( size_t index=startBitInThisByte; index<8 && 8*byteNumber+index<=lastBit; ++index )
			{
				unsigned char mask=( 128 >> index );
//				std::cout << "byteNumber=" << byteNumber << ", index=" << index << " mask=" << (int)mask << ", 8*byteNumber+index-firstBit=" << 8*byteNumber+index-firstBit
//						<< " (*pByte)=" << (int)(*pByte) << " gives=" << ((*pByte) & mask) << std::endl;
				returnValue[8*byteNumber+index-firstBit]=((*pByte) & mask);
			}
		}
		return returnValue;
	}


	class RawCBCEventImplementation : public cbcanalyser::RawCBCEvent
	{
	public:
		RawCBCEventImplementation() : channelData_(254) {}
		virtual ~RawCBCEventImplementation() {}

		void fill( const unsigned char* data )
		{
			errorBits_=getBits( data[0], 0, 1 );
			size_t unformattedStatus=(getBytes<size_t>( data, 0, 1 ) & 0x3fff); // Mask off the two bits of the error bits
			status_= (unformattedStatus >> 6);
			channelData_=getBits( data[1], 2, 255 );
			stubData_=getBytes<unsigned char>( data, 34, 34 );
		}

		// Methods implementations required by the interface
		virtual const std::vector<bool>& errorBits() { return errorBits_; }
		virtual unsigned char status() { return status_; }
		virtual const std::vector<bool>& channelData() { return channelData_; }
		virtual unsigned char stubData() { return stubData_; }
	private:
		std::vector<bool> errorBits_;
		unsigned char status_;
		std::vector<bool> channelData_;
		unsigned char stubData_;
	};

	class RawDataEventImplementation : public cbcanalyser::RawDataEvent
	{
	public:
		RawDataEventImplementation( std::istream& inputFile ) : endianOrderer_(8,false) // Use 8 byte size words (i.e. 64 bit)
		{
			std::vector<unsigned char> data(168);
			inputFile.read( reinterpret_cast<char*>(&data[0]), data.size() );
//			std::cout << "Raw data:" << " sizeof(unsigned char)=" << sizeof(unsigned char) << " sizeof(char)=" << sizeof(char) << " sizeof(char)=" << sizeof(char) << "\n";
//			for( size_t byte=0; byte<168; ++byte )
//			{
//				std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[byte]);
//
//				if( byte%16==15 ) std::cout << "\n";
//				else if( byte%4==3 ) std::cout << "  ";
//				else std::cout << " ";
//			}
//			std::cout << std::dec << std::setfill(' ');

			endianOrderer_.reorder( data );

			bunchCounter_=getBytes<size_t>( data, 1, 3 );
			orbitCounter_=getBytes<size_t>( data, 5, 7 );
			lumisection_=getBytes<size_t>( data, 9, 11 );
			l1aCounter_=getBytes<size_t>( data, 13, 15 );
			cbcCounter_=getBytes<size_t>( data, 17, 19 );

			cbc0_.fill( &data[20] );
			cbc1_.fill( &data[56] );
			cbc2_.fill( &data[92] );
			cbc3_.fill( &data[128] );
		}

		// Methods implementations required by the interface
		virtual ~RawDataEventImplementation() {}
		virtual size_t bunchCounter() { return bunchCounter_; }
		virtual size_t orbitCounter() { return orbitCounter_; }
		virtual size_t lumisection() { return lumisection_; }
		virtual size_t l1aCounter() { return l1aCounter_; }
		virtual size_t cbcCounter() { return cbcCounter_; }
		virtual cbcanalyser::RawCBCEvent& cbc( size_t cbcNumber )
		{
			if( cbcNumber==0 ) return cbc0_;
			else if( cbcNumber==1 ) return cbc1_;
			else if( cbcNumber==2 ) return cbc2_;
			else if( cbcNumber==3 ) return cbc3_;
			else throw std::runtime_error( "Invalid CBC number "+std::to_string(cbcNumber) );
		}
	protected:
		RawCBCEventImplementation cbc0_;
		RawCBCEventImplementation cbc1_;
		RawCBCEventImplementation cbc2_;
		RawCBCEventImplementation cbc3_;
		EndianOrderer endianOrderer_;
		size_t bunchCounter_;
		size_t orbitCounter_;
		size_t lumisection_;
		size_t l1aCounter_;
		size_t cbcCounter_;
	};
} // end of the unnamed namespace


cbcanalyser::RawDataFileReader::RawDataFileReader( std::istream& inputFile )
	: inputFile_(inputFile)
{
	// No operation besides the initialiser list
}

std::unique_ptr<cbcanalyser::RawDataEvent> cbcanalyser::RawDataFileReader::nextEvent()
{
	if( !inputFile_.good() ) return nullptr;

	std::unique_ptr<RawDataEvent> returnValue( dynamic_cast<RawDataEvent*>( new RawDataEventImplementation(inputFile_) ) );

	if( !inputFile_.good() ) return nullptr;
	else return returnValue;
}
