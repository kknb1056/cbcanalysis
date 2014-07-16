#include <cppunit/extensions/HelperMacros.h>


/** @brief A cppunit TestFixture to test the classes in SCurves.h
 *
 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
 * @date 03/Aug/2013
 */
class SCurveUnitTestSuite : public CPPUNIT_NS::TestFixture
{
	CPPUNIT_TEST_SUITE(SCurveUnitTestSuite);
//	CPPUNIT_TEST(testSaveAndRestore);
//	CPPUNIT_TEST(testCalculateBinning);
//	CPPUNIT_TEST(testTEfficiencyCreation);
//	CPPUNIT_TEST(testFitting);
	CPPUNIT_TEST(testRestoreFromTEfficiency);
	CPPUNIT_TEST_SUITE_END();

protected:

public:
	void setUp();

protected:
	void testSaveAndRestore();
	void testCalculateBinning();
	void testTEfficiencyCreation();
	void testRestoreFromTEfficiency();
	void testFitting();
};





#include <cppunit/config/SourcePrefix.h>
#include <iostream>
#include <stdexcept>
#include "SLHCUpgradeTracker/CBCAnalysis/interface/SCurve.h"
#include <TEfficiency.h>
#include <TH1.h>
#include <TMath.h>
#include <TFile.h>

CPPUNIT_TEST_SUITE_REGISTRATION(SCurveUnitTestSuite);

void SCurveUnitTestSuite::setUp()
{

}

void SCurveUnitTestSuite::testSaveAndRestore()
{
	const std::string testOutputFilename="testOutputForSCurveUnitTests-youCanDeleteThisFile.txt";

	cbcanalyser::DetectorSCurves detectorSCurves;
	cbcanalyser::DetectorSCurves restoredDetectorSCurves;

	// First fill with random data
	detectorSCurves.getStripSCurve( 0, 0, 0 ).getEntry(0).eventsOn()+=30;
	detectorSCurves.getStripSCurve( 0, 0, 0 ).getEntry(0).eventsOff()+=2342;
	detectorSCurves.getStripSCurve( 0, 0, 1 ).getEntry(0).eventsOn()+=23;
	detectorSCurves.getStripSCurve( 0, 0, 1 ).getEntry(0).eventsOff()+=34567;
	detectorSCurves.getStripSCurve( 0, 1, 0 ).getEntry(0).eventsOn()+=43152;
	detectorSCurves.getStripSCurve( 1, 0, 0 ).getEntry(0).eventsOn()+=3223;
	detectorSCurves.getStripSCurve( 1, 0, 0 ).getEntry(0).eventsOn()+=9;

	// Write out the data to a file
	{ // block to keep temporary variables' scope limited
		std::ofstream outputFile( testOutputFilename, std::ios::trunc );
		CPPUNIT_ASSERT( outputFile.is_open() );

		CPPUNIT_ASSERT_NO_THROW( detectorSCurves.dumpToStream(outputFile) );
		outputFile.close();
	}

	// Read back the saved data into a different object
	{ // block to keep temporary variables' scope limited
		std::ifstream inputFile( testOutputFilename );
		CPPUNIT_ASSERT( inputFile.is_open() );

		CPPUNIT_ASSERT_NO_THROW( restoredDetectorSCurves.restoreFromStream(inputFile) );
		inputFile.close();
	}


	//
	// Compare what was restored to the original object
	// and make sure they're the same.
	//
	const auto fedIndices=detectorSCurves.getValidFedIndices();
	const auto restoredFedIndices=restoredDetectorSCurves.getValidFedIndices();
	CPPUNIT_ASSERT_EQUAL( fedIndices.size(), restoredFedIndices.size() );

	for( size_t fedIndex=0; fedIndex<fedIndices.size() && fedIndex<restoredFedIndices.size(); ++fedIndex )
	{
		CPPUNIT_ASSERT_EQUAL( fedIndices[fedIndex], restoredFedIndices[fedIndex] );

		auto& fedSCurve=detectorSCurves.getFedSCurves(fedIndex);
		auto& restoredFedSCurve=restoredDetectorSCurves.getFedSCurves(fedIndex);

		const auto channelIndices=fedSCurve.getValidChannelIndices();
		const auto restoredChannelIndices=restoredFedSCurve.getValidChannelIndices();
		CPPUNIT_ASSERT_EQUAL( channelIndices.size(), restoredChannelIndices.size() );

		for( size_t channelIndex=0; channelIndex<channelIndices.size() && channelIndex<restoredChannelIndices.size(); ++channelIndex )
		{
			CPPUNIT_ASSERT_EQUAL( channelIndices[channelIndex], restoredChannelIndices[channelIndex] );

			auto& channelSCurve=fedSCurve.getFedChannelSCurves(channelIndex);
			auto& restoredChannelSCurve=restoredFedSCurve.getFedChannelSCurves(channelIndex);

			const auto stripIndices=channelSCurve.getValidStripIndices();
			const auto restoredStripIndices=restoredChannelSCurve.getValidStripIndices();
			CPPUNIT_ASSERT_EQUAL( stripIndices.size(), restoredStripIndices.size() );

			for( size_t stripIndex=0; stripIndex<stripIndices.size() && stripIndex<restoredStripIndices.size(); ++stripIndex )
			{
				CPPUNIT_ASSERT_EQUAL( stripIndices[stripIndex], restoredStripIndices[stripIndex] );

				const auto& sCurve=channelSCurve.getStripSCurve(stripIndex);
				const auto& restoredSCurve=restoredChannelSCurve.getStripSCurve(stripIndex);

				// I actually bothered to write an equality operator for this class
				CPPUNIT_ASSERT( sCurve==restoredSCurve );
			}
		}
	}
}

void SCurveUnitTestSuite::testCalculateBinning()
{
	std::vector<float> binCentres;
	binCentres.push_back(3);
	binCentres.push_back(5);
	binCentres.push_back(8);
	binCentres.push_back(9);

	// The bin centres above should produce the following binning:
	// 2-4      : bin with centre 3
	// 4-6      : bin with centre 5
	// 6-7.5    : dummy bin as a spacer
	// 7.5-8.5  : bin with centre 8
	// 8.5-9.5  : bin with centre 9
	//
	// So the output vector should contain [2,4,6,7.5,8.5,9.5] (lower edges plus global upper edge)


	std::vector<float> binLowerEdges;
	cbcanalyser::calculateBinning( binLowerEdges, binCentres );

	CPPUNIT_ASSERT( binLowerEdges.size()==6 );
	CPPUNIT_ASSERT( binLowerEdges[0]==2 );
	CPPUNIT_ASSERT( binLowerEdges[1]==4 );
	CPPUNIT_ASSERT( binLowerEdges[2]==6 );
	CPPUNIT_ASSERT( binLowerEdges[3]==7.5 );
	CPPUNIT_ASSERT( binLowerEdges[4]==8.5 );
	CPPUNIT_ASSERT( binLowerEdges[5]==9.5 );

	//
	// Now try the same thing with a map and custom value retriever
	//
	std::map<float,std::string> centresAndValues;
	centresAndValues[3]="Some random";
	centresAndValues[5]="data that";
	centresAndValues[8]="I don't";
	centresAndValues[9]="care about";
	// Adding an extra bin at the end changes the widths of both the bin centred on 9
	// and the bin centred on 8. The bin centred on 9 has to be smaller, which allows
	// the bin centred on 8 to be bigger.
	centresAndValues[9.5]="blah blah blah";
	// The bin centres above should now produce the following binning:
	// 2-4        : bin with centre 3
	// 4-6        : bin with centre 5
	// 6-7.25     : dummy bin as a spacer
	// 7.25-8.75  : bin with centre 8
	// 8.75-9.25  : bin with centre 9
	// 9.25-9.75  : bin with centre 9.5
	//
	// So the output vector should contain [2,4,6,7.25,8.75,9.25,9.75] (lower edges plus global upper edge)


	binLowerEdges.clear();
	cbcanalyser::calculateBinning( binLowerEdges, centresAndValues, [](std::map<float,std::string>::const_iterator iValue)->float{return iValue->first;} );


	CPPUNIT_ASSERT( binLowerEdges.size()==7 );
	CPPUNIT_ASSERT( binLowerEdges[0]==2 );
	CPPUNIT_ASSERT( binLowerEdges[1]==4 );
	CPPUNIT_ASSERT( binLowerEdges[2]==6 );
	CPPUNIT_ASSERT( binLowerEdges[3]==7.25 );
	CPPUNIT_ASSERT( binLowerEdges[4]==8.75 );
	CPPUNIT_ASSERT( binLowerEdges[5]==9.25 );
	CPPUNIT_ASSERT( binLowerEdges[6]==9.75 );

}

void SCurveUnitTestSuite::testTEfficiencyCreation()
{
	// Create an SCurve and add some random data
	cbcanalyser::SCurve scurve;
	scurve.getEntry(3).eventsOn()=0;
	scurve.getEntry(3).eventsOff()=10;

	scurve.getEntry(5).eventsOn()=5;
	scurve.getEntry(5).eventsOff()=5;

	scurve.getEntry(7).eventsOn()=10;
	scurve.getEntry(7).eventsOff()=0;

	std::unique_ptr<TEfficiency> pEfficiency=scurve.createHistogram( "myHistogram" );

	// Make sure the binning was created properly. The only way I know of is to inspect
	// either the passed or total histograms, since TEfficiency doesn't have a method
	// to get this directly. GetPaintedGraph returns nullptr if the TEfficiency
	// hasn't been painted yet.
	{ // Block to limit scope of local variables
		const TH1* pPassedHistogram=pEfficiency->GetPassedHistogram();

		CPPUNIT_ASSERT( pPassedHistogram->GetNbinsX()==3 );

		CPPUNIT_ASSERT( pPassedHistogram->GetBinLowEdge(1)==2 );
		CPPUNIT_ASSERT( pPassedHistogram->GetBinLowEdge(2)==4 );
		CPPUNIT_ASSERT( pPassedHistogram->GetBinLowEdge(3)==6 );
		CPPUNIT_ASSERT( pPassedHistogram->GetBinLowEdge(4)==8 );

		CPPUNIT_ASSERT( pPassedHistogram->GetBinContent(1)==0 );
		CPPUNIT_ASSERT( pPassedHistogram->GetBinContent(2)==5 );
		CPPUNIT_ASSERT( pPassedHistogram->GetBinContent(3)==10 );

		const TH1* pTotalHistogram=pEfficiency->GetTotalHistogram();

		CPPUNIT_ASSERT( pTotalHistogram->GetNbinsX()==3 );

		CPPUNIT_ASSERT( pTotalHistogram->GetBinLowEdge(1)==2 );
		CPPUNIT_ASSERT( pTotalHistogram->GetBinLowEdge(2)==4 );
		CPPUNIT_ASSERT( pTotalHistogram->GetBinLowEdge(3)==6 );
		CPPUNIT_ASSERT( pTotalHistogram->GetBinLowEdge(4)==8 );

		CPPUNIT_ASSERT( pTotalHistogram->GetBinContent(1)==10 );
		CPPUNIT_ASSERT( pTotalHistogram->GetBinContent(2)==10 );
		CPPUNIT_ASSERT( pTotalHistogram->GetBinContent(3)==10 );
	} // end of block to limit scope of local variables

	// Check that the efficiencies are okay.
	CPPUNIT_ASSERT( pEfficiency->GetEfficiency(1)==0 );
	CPPUNIT_ASSERT( pEfficiency->GetEfficiency(2)==0.5 );
	CPPUNIT_ASSERT( pEfficiency->GetEfficiency(3)==1 );

	// Check the errors. I got these from printing out the values given by the TEfficiency which
	// defeats the purpose of the unit test (because it will pass by definition). This could be
	// useful for regression testing though.
	CPPUNIT_ASSERT_DOUBLES_EQUAL( 0, pEfficiency->GetEfficiencyErrorLow(1), 0.00001 );
	CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.168149, pEfficiency->GetEfficiencyErrorUp(1), 0.00001 );

	CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.195182, pEfficiency->GetEfficiencyErrorLow(2), 0.00001 );
	CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.195182, pEfficiency->GetEfficiencyErrorUp(2), 0.00001 );

	CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.168149, pEfficiency->GetEfficiencyErrorLow(3), 0.00001 );
	CPPUNIT_ASSERT_DOUBLES_EQUAL( 0, pEfficiency->GetEfficiencyErrorUp(3), 0.00001 );
}

void SCurveUnitTestSuite::testRestoreFromTEfficiency()
{
	std::cout << "\n" << "Testing restore from TEfficiency" << std::endl;
	cbcanalyser::FedSCurves myFeds;

	TFile* pInputFile=TFile::Open("/tmp/histograms.root");
	myFeds.restoreFromDirectory(pInputFile);

	TFile* pOutputFile=TFile::Open("/tmp/newHistograms.root","RECREATE");
	myFeds.createHistograms(pOutputFile);
	pOutputFile->Write();
}

void SCurveUnitTestSuite::testFitting()
{
	//
	// Create an SCurve and add data that follows an ideal error function
	//

	// Fake some measurements that follow an ideal error function
	const size_t numberOfThresholds=100;
	const size_t numberOfEventsPerThreshold=100;
	const float firstThreshold=3;
	const float lastThreshold=8;
	const float meanTurnOn=(firstThreshold+lastThreshold)*0.45;
	const float standardDeviation=30.0/(lastThreshold-firstThreshold);

	cbcanalyser::SCurve scurve;
	for( size_t index=0; index<numberOfThresholds; ++index )
	{
		float threshold=firstThreshold+(lastThreshold-firstThreshold)/static_cast<float>(numberOfThresholds-1)*static_cast<float>(index);
		float efficiency=0.5*( 1 + TMath::Erf( standardDeviation*(threshold-meanTurnOn)/TMath::Sqrt2() ) );
		cbcanalyser::SCurveEntry& entry=scurve.getEntry(threshold);
		entry.eventsOn()=efficiency*static_cast<float>( numberOfEventsPerThreshold )+0.5; // Add 0.5 so that it rounds properly
		entry.eventsOff()=numberOfEventsPerThreshold-entry.eventsOn();
	}

//	std::unique_ptr<TFile> pOutputFile( new TFile("testOutput.root","RECREATE") );
//	std::unique_ptr<TEfficiency> pEfficiency=scurve.createHistogram( "fakeDataTest" );
//	pEfficiency->SetDirectory( pOutputFile.get() );
//	pEfficiency.release(); // Once in the TFile the file takes ownership
//	pOutputFile->Write();
//	pOutputFile->Close();

	//
	// Now that I have filled it with fake data, I'll try and fit it and see
	// if the fit parameters compare to the simulation parameters
	//
	std::tuple<float,float,float,float,float> fitParameters=scurve.fitParameters();
	CPPUNIT_ASSERT_DOUBLES_EQUAL( 1, std::get<2>(fitParameters), 0.00001 );
	CPPUNIT_ASSERT_DOUBLES_EQUAL( standardDeviation, std::get<3>(fitParameters), standardDeviation*0.05 ); // Make sure they match to within 5%
	CPPUNIT_ASSERT_DOUBLES_EQUAL( meanTurnOn, std::get<4>(fitParameters), meanTurnOn*0.05 ); // Make sure they match to within 5%

}
