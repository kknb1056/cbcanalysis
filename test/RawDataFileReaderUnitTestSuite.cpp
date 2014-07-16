#include <cppunit/extensions/HelperMacros.h>


/** @brief A cppunit TestFixture to test the classes in RawDataFileReader.h
 *
 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
 * @date 06/Jan/2014
 */
class RawDataFileReaderUnitTestSuite : public CPPUNIT_NS::TestFixture
{
	CPPUNIT_TEST_SUITE(RawDataFileReaderUnitTestSuite);
	//CPPUNIT_TEST(testSimpleFile);
	CPPUNIT_TEST_SUITE_END();

protected:

public:
	void setUp();

protected:
	void testSimpleFile();
};





#include <cppunit/config/SourcePrefix.h>
#include <iostream>
#include <stdexcept>
#include "SLHCUpgradeTracker/CBCAnalysis/interface/RawDataFileReader.h"

CPPUNIT_TEST_SUITE_REGISTRATION(RawDataFileReaderUnitTestSuite);

void RawDataFileReaderUnitTestSuite::setUp()
{

}

void RawDataFileReaderUnitTestSuite::testSimpleFile()
{
	std::ifstream inputFile( "/tmp/comparisonDump-100evt.dat" );
	cbcanalyser::RawDataFileReader reader( inputFile );

	for( size_t eventNumber=0; eventNumber<5; ++eventNumber )
	{
		std::unique_ptr<cbcanalyser::RawDataEvent> pEvent=reader.nextEvent();
		std::cout << "\n"
				<< "bunchCounter=" << pEvent->bunchCounter() << "\n"
				<< "orbitCounter=" << pEvent->orbitCounter() << "\n"
				<< "lumisection=" << pEvent->lumisection() << "\n"
				<< "l1aCounter=" << pEvent->l1aCounter() << "\n"
				<< "cbcCounter=" << pEvent->cbcCounter() << "\n";

		for( size_t cbcIndex=0; cbcIndex<4; ++cbcIndex )
		{
			std::cout << "CBC " << cbcIndex << std::endl;
			cbcanalyser::RawCBCEvent& cbcEvent=pEvent->cbc(cbcIndex);

			std::cout << "\t" << "Error bits: ";
			for( const auto& errorBit : cbcEvent.errorBits() )
			{
				if( errorBit ) std::cout << "1";
				else std::cout << ".";
			}
			std::cout << std::endl;

			std::cout << "\t" << "Status: " << std::hex << (int)cbcEvent.status() << std::dec << std::endl;

			std::cout << "\t" << "Channel data: ";
			for( const auto& channel : cbcEvent.channelData() )
			{
				if( channel ) std::cout << "1";
				else std::cout << ".";
			}
			std::cout << std::endl;
		}
	}
}

