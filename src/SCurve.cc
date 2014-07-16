#include "SLHCUpgradeTracker/CBCAnalysis/interface/SCurve.h"

#include <cmath>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <TH1F.h>
#include <TF1.h>
#include <TMath.h>
#include <TEfficiency.h>
#include <TDirectory.h>

//----------------------------------------------------------------------------------------------
//---------------------------- Unnamed namespace declarations ----------------------------------
//---------------------- (definitions are at the bottom of the file) ---------------------------
//----------------------------------------------------------------------------------------------
namespace
{
	/** @brief Sentry class to restore a redirected output stream if an exception is thrown.
	 *
	 * Used for when standard output is redirected to stop all the crap the minuit prints out.
	 * When this goes out of scope standard output will be reset to go to wherever it went
	 * before hand.
	 *
	 * This isn't actually used because after testing I realised it doesn't affect oldschool
	 * printf which is what I wanted. It might come in useful at some point though so I'll
	 * leave it here for now.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 04/Nov/2013
	 */
	class RedirectedStreamSentry
	{
	public:
		RedirectedStreamSentry( std::ostream& outputStreamToRedirect, std::streambuf* placeToRedirectTo )
			: redirectedStream_(outputStreamToRedirect), pOriginalOutput_( outputStreamToRedirect.rdbuf() )
		{
			redirectedStream_.rdbuf( placeToRedirectTo );
		}
		~RedirectedStreamSentry()
		{
			redirectedStream_.rdbuf( pOriginalOutput_ );
		}
	protected:
		std::ostream& redirectedStream_;
		std::streambuf* pOriginalOutput_;
	};

	/** @brief Sentry class to work with old school printf as RedirectedStreamSentry does for std::cout
	 *
	 * I wrote the RedirectedStreamSentry class and then realised it didn't work because Minuit
	 * uses stdio instead of iostream. Apparantly this code is non-portable and a little dodgy
	 * though. I've put an "#ifndef" in so that it can be disabled if DONT_USE_DODGY_PRINTF_REDIRECTION
	 * is defined.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 */
	class RedirectedPrintfSentry
	{
	public:
		RedirectedPrintfSentry()
		{
#ifndef DONT_USE_DODGY_PRINTF_REDIRECTION
			oldStdout_=dup(1);
			freopen ("/dev/null","w",stdout);
#endif
		}
		~RedirectedPrintfSentry()
		{
#ifndef DONT_USE_DODGY_PRINTF_REDIRECTION
			FILE *fp2 = fdopen(oldStdout_, "w");
			fclose(stdout);
			*stdout = *fp2;
#endif
		}
	protected:
		int oldStdout_;
	};
}
//----------------------------------------------------------------------------------------------
//------------------------- cbcanalyser::SCurveEntry definitions -------------------------------
//----------------------------------------------------------------------------------------------

cbcanalyser::SCurveEntry::SCurveEntry()
	: eventsOn_(0), eventsOff_(0)
{
	// No operation besides the initialiser list.
}

size_t& cbcanalyser::SCurveEntry::eventsOn()
{
	return eventsOn_;
}

const size_t& cbcanalyser::SCurveEntry::eventsOn() const
{
	return eventsOn_;
}

size_t& cbcanalyser::SCurveEntry::eventsOff()
{
	return eventsOff_;
}

const size_t& cbcanalyser::SCurveEntry::eventsOff() const
{
	return eventsOff_;
}

float cbcanalyser::SCurveEntry::fraction() const
{
	if( (eventsOn_+eventsOff_)==0 ) return 0;
	return static_cast<float>(eventsOn_)/static_cast<float>(eventsOn_+eventsOff_);
}

float cbcanalyser::SCurveEntry::fractionError() const
{
	if( (eventsOn_+eventsOff_)==0 ) return 0;
	return std::sqrt(eventsOn_)/static_cast<float>(eventsOn_+eventsOff_);
}

bool cbcanalyser::SCurveEntry::operator==( const SCurveEntry& otherSCurveEntry ) const
{
	return ( eventsOn_==otherSCurveEntry.eventsOn_ ) && ( eventsOff_==otherSCurveEntry.eventsOff_ );
}

bool cbcanalyser::SCurveEntry::operator!=( const SCurveEntry& otherSCurveEntry ) const
{
	return !( (*this)==otherSCurveEntry );
}

void cbcanalyser::SCurveEntry::dumpToStream( std::ostream& outputStream ) const
{
	// There will be lots of these entries so I'll just abbreviate the class identifier to "SCE"
	// instead of "SCurveEntry".
	outputStream << "SCE " << eventsOn_ << " " << eventsOff_ << " ";
}

void cbcanalyser::SCurveEntry::restoreFromStream( std::istream& inputStream )
{
	std::string identifier;
	size_t eventsOn;
	size_t eventsOff;

	inputStream >> identifier >> eventsOn >> eventsOff;

	if( identifier!="SCE" ) throw std::runtime_error( "SCurveEntry::restoreFromStream - stream does not describe a SCurveEntry object" );

	eventsOn_=eventsOn;
	eventsOff_=eventsOff;
}

//----------------------------------------------------------------------------------------------
//---------------------------- cbcanalyser::SCurve definitions ---------------------------------
//----------------------------------------------------------------------------------------------

cbcanalyser::SCurve::SCurve()
	: fit_maxEfficiency_(-1.), fit_standardDeviation_(-1), fit_mean_(-1)
{
}

bool cbcanalyser::SCurve::operator==( const SCurve& otherSCurve ) const
{
	if( entries_.size()!=otherSCurve.entries_.size() ) return false;

//	for( size_t index=0; index<entries_.size(); ++index )
//	{
//		if( entries_[index]!=otherSCurve.entries_[index] ) return false;
//	}

	return entries_==otherSCurve.entries_;
}

bool cbcanalyser::SCurve::operator!=( const SCurve& otherSCurve ) const
{
	return !( (*this)==otherSCurve );
}

cbcanalyser::SCurveEntry& cbcanalyser::SCurve::getEntry( float threshold )
{
	return entries_[threshold];
}

const cbcanalyser::SCurveEntry& cbcanalyser::SCurve::getEntry( float threshold ) const
{
	const auto& findResult=entries_.find( threshold );
	if( findResult==entries_.end() ) throw std::runtime_error( "No entry for '"+std::to_string(threshold)+"' in SCurve" );
	return findResult->second;
}

const std::vector<float> cbcanalyser::SCurve::getValidThresholds() const
{
	std::vector<float> returnValue;
	for( const auto& thresholdEntryPair : entries_ )
	{
		returnValue.push_back( thresholdEntryPair.first );
	}

	return returnValue;
}

size_t cbcanalyser::SCurve::size() const
{
	return entries_.size();
}

std::unique_ptr<TEfficiency> cbcanalyser::SCurve::createHistogram( const std::string& name ) const
{
	if( entries_.empty() ) return std::unique_ptr<TEfficiency>(); // return nullptr if histogram is undefined


	// First figure out what binning I need for the entries that I have
	std::vector<double> binLowerEdges;
	cbcanalyser::calculateBinning( binLowerEdges, entries_, [](std::map<float,SCurveEntry>::const_iterator iValue)->float{return iValue->first;} );

	std::unique_ptr<TEfficiency> pNewHistogram( new TEfficiency( name.c_str(), name.c_str(), binLowerEdges.size()-1, &binLowerEdges[0] ) );
	pNewHistogram->SetDirectory(nullptr);

	for( const auto& thresholdEntryPair : entries_ )
	{
		// Need to do a const cast because for some reason FindBin is not a const method
		// and GetPassedHistogram returns a const TH1*. TEfficiency has no direct FindBin
		// method.
		int binNumber=const_cast<TH1*>(pNewHistogram->GetPassedHistogram())->FindBin( thresholdEntryPair.first );
		pNewHistogram->SetTotalEvents( binNumber, thresholdEntryPair.second.eventsOn()+thresholdEntryPair.second.eventsOff() );
		pNewHistogram->SetPassedEvents( binNumber, thresholdEntryPair.second.eventsOn() );
	}

	return pNewHistogram;
}

void cbcanalyser::SCurve::restoreFromEfficiency( const TEfficiency* pEfficiency )
{
	// Clear whatever was there before hand
	entries_.clear();
	const TH1* pPassedHistogram=pEfficiency->GetPassedHistogram();
	const TH1* pTotalHistogram=pEfficiency->GetTotalHistogram();

	// Loop over all of the bins in the histogram
	for( int binIndex=1; binIndex<pPassedHistogram->GetXaxis()->GetNbins()+1; ++binIndex )
	{
		SCurveEntry& thisEntry=entries_[pPassedHistogram->GetBinCenter(binIndex)];
		thisEntry.eventsOn()=pPassedHistogram->GetBinContent(binIndex);
		thisEntry.eventsOff()=pTotalHistogram->GetBinContent(binIndex)-pPassedHistogram->GetBinContent(binIndex);
	}
}

std::unique_ptr<TF1> cbcanalyser::SCurve::fit() const
{
	std::unique_ptr<TEfficiency> pHistogram=createHistogram( "efficiencyForFit" );

	return fitHistogram( pHistogram );
}

std::unique_ptr<TF1> cbcanalyser::SCurve::fitHistogram( const std::unique_ptr<TEfficiency>& pHistogram )
{
	// The fit fails quite often unless I have a very good starting
	// estimate of the fit parameters. I'll first look through the
	// efficiency and find an estimate for the midpoint.
	int numberOfBins=pHistogram->GetPassedHistogram()->GetXaxis()->GetNbins();
	int highBin=numberOfBins;
	int lowBin=1;
	std::cout << "Checking bins" << std::endl;
	while( highBin-lowBin>1 )
	{
		int midBin=(lowBin+highBin)/2;
		std::cout << "   " << lowBin << ", " << midBin << ", " << highBin << " mid eff=" << pHistogram->GetEfficiency(midBin) << std::endl;
		if( pHistogram->GetEfficiency(midBin)>0.5 ) highBin=midBin;
		else lowBin=midBin;
	}
	double midpoint=pHistogram->GetPassedHistogram()->GetBinCenter(lowBin);
	std::cout << "*** Midpoint Estimate is " << midpoint << std::endl;

	//
	// Need to figure out the minimum and maximum for the fit by
	// examining the low and high edges of the TEfficiency.
	float lowEdge=pHistogram->GetPassedHistogram()->GetBinLowEdge(1);
	// To get the high edge need to check low edge of the overflow bin
	float highEdge=pHistogram->GetPassedHistogram()->GetBinLowEdge( pHistogram->GetPassedHistogram()->GetNbinsX()+1 );

	std::string name=pHistogram->GetName();
	name+="-efficiencyFitFunction";
	std::unique_ptr<TF1> pFitFunction( new TF1( name.c_str(), "([0]*0.5)*( 1 + TMath::Erf( [1]*(x-[2])/TMath::Sqrt2() ) )", lowEdge, highEdge ) );

	//pFitFunction->SetParameters( 1, 6, (lowEdge+highEdge)/2 ); // Set initial parameters
	pFitFunction->SetParameters( 1, 6, midpoint );
	//pFitFunction->SetParLimits(0,0,1); // Limit range of p0 to be between 0 and 1
	pFitFunction->FixParameter(0,1); // Fix p0 to be 1

	pHistogram->Fit( pFitFunction.get() );

	return pFitFunction;
}

std::tuple<float,float,float,float,float> cbcanalyser::SCurve::fitParameters() const
{
	std::unique_ptr<TF1> pFitFunction=fit();
	return std::tuple<float,float,float,float,float>( pFitFunction->GetChisquare(), pFitFunction->GetNDF(), pFitFunction->GetParameter(0), pFitFunction->GetParameter(1), pFitFunction->GetParameter(2) );
}

void cbcanalyser::SCurve::storeFitParameters( const TF1& fittedFunction )
{
	if ( fittedFunction.GetNpar() != 3 ) return;

	fit_chi2_=fittedFunction.GetChisquare();
	fit_ndf_=fittedFunction.GetNDF();
	fit_maxEfficiency_=fittedFunction.GetParameter(0);
	fit_standardDeviation_=fittedFunction.GetParameter(1);
	fit_mean_=fittedFunction.GetParameter(2);

	return;
}

void cbcanalyser::SCurve::dumpToStream( std::ostream& outputStream ) const
{
	outputStream << "SCurve " << entries_.size() << " ";
	for( const auto& entry : entries_ )
	{
		outputStream << entry.first << " ";
		entry.second.dumpToStream(outputStream); // Then delegate to the SCurveEntry class
	}
}

void cbcanalyser::SCurve::restoreFromStream( std::istream& inputStream )
{
	// Use a temporary object, so that if restoring fails I'm not left with a
	// half modified instance.
	cbcanalyser::SCurve temporaryInstance;

	std::string identifier;
	inputStream >> identifier;
	if( identifier!="SCurve" ) throw std::runtime_error( "SCurve::restoreFromStream - stream does not describe a SCurve object" );


	size_t numberOfEntries;
	inputStream >> numberOfEntries;

	for( size_t entry=0; entry<numberOfEntries; ++entry )
	{
		float threshold;
		inputStream >> threshold;
		// Delegate to each entry to restore its state from disk
		temporaryInstance.entries_[threshold].restoreFromStream(inputStream);
	}

	// If everything went smoothly and I get to this point, I can overwrite the contents
	// with what was read from disk.
	(*this)=temporaryInstance;
}

size_t cbcanalyser::SCurve::maxiumumEntries()
{
	return entries_.size();
}


//----------------------------------------------------------------------------------------------
//----------------------- cbcanalyser::FedChannelSCurves definitions ---------------------------
//----------------------------------------------------------------------------------------------

cbcanalyser::SCurve& cbcanalyser::FedChannelSCurves::getStripSCurve( size_t stripNumber )
{
	return stripSCurves_[stripNumber];
}

const cbcanalyser::SCurve& cbcanalyser::FedChannelSCurves::getStripSCurve( size_t stripNumber ) const
{
	const auto& findResult=stripSCurves_.find( stripNumber );
	if( findResult==stripSCurves_.end() ) throw std::runtime_error( "No entry for '"+std::to_string(stripNumber)+"' in FED channel" );
	return findResult->second;
}

std::vector<size_t> cbcanalyser::FedChannelSCurves::getValidStripIndices() const
{
	std::vector<size_t> returnValue;
	for( const auto& stripNumberSCurvesPair : stripSCurves_ ) returnValue.push_back( stripNumberSCurvesPair.first );
	return returnValue;
}

void cbcanalyser::FedChannelSCurves::createHistograms( TDirectory* pParentDirectory ) const
{
	std::stringstream stringConverter;

	for( const auto& stripNumberSCurvesPair : stripSCurves_ )
	{
		stringConverter.str("");
		stringConverter << "Strip " << std::setfill('0') << std::setw(3) << stripNumberSCurvesPair.first;

		std::unique_ptr<TEfficiency> pNewHistogram=stripNumberSCurvesPair.second.createHistogram( stringConverter.str() );
		pNewHistogram->SetDirectory( pParentDirectory );

		// Fit this S-Curve
		//cbcanalyser::FitSCurve fit( *pNewHistogram, stringConverter.str() );
		//std::unique_ptr<TF1> pNewFittedFunction=fit.performFit();
		std::unique_ptr<TF1> pNewFittedFunction=cbcanalyser::SCurve::fitHistogram( pNewHistogram );

		// Set current directory and write fitted function
		pParentDirectory->cd();
		//pNewFittedFunction->Write();

		pNewHistogram.release(); // When the directory gets set, the directory takes ownership
		pNewFittedFunction.release();
	}

}

void cbcanalyser::FedChannelSCurves::restoreFromDirectory( TDirectory* pParentDirectory )
{
	// Get rid of anything that was present before hand
	stripSCurves_.clear();

	// I'll just try and retrieve all possible values for now. I haven't got time
	// to implement something that works out the channel number from the names
	// of the subdirectories.
	std::stringstream stringConverter;
	for( size_t stripChannelNumber=0; stripChannelNumber<256; ++stripChannelNumber )
	{
		stringConverter.str("");
		stringConverter << "Strip " << std::setfill('0') << std::setw(3) << stripChannelNumber;

		TEfficiency* pEfficiency=dynamic_cast<TEfficiency*>( pParentDirectory->Get(stringConverter.str().c_str()) );
		if( pEfficiency!=nullptr )
		{
			stripSCurves_[stripChannelNumber].restoreFromEfficiency(pEfficiency);
		}
	}
}

void cbcanalyser::FedChannelSCurves::dumpToStream( std::ostream& outputStream ) const
{
	outputStream << "FedChannelSCurves " << stripSCurves_.size() << " ";
	for( const auto& stripNumberSCurvesPair : stripSCurves_ )
	{
		outputStream << stripNumberSCurvesPair.first << " "; // Dump the strip number
		stripNumberSCurvesPair.second.dumpToStream(outputStream); // Then delegate to the SCurve class
	}
}

void cbcanalyser::FedChannelSCurves::restoreFromStream( std::istream& inputStream )
{
	// Use a temporary object, so that if restoring fails I'm not left with a
	// half modified instance.
	cbcanalyser::FedChannelSCurves temporaryInstance;

	std::string identifier;
	inputStream >> identifier;
	if( identifier!="FedChannelSCurves" ) throw std::runtime_error( "FedChannelSCurves::restoreFromStream - stream does not describe a FedChannelSCurves object" );


	size_t numberOfEntries;
	inputStream >> numberOfEntries;

	size_t stripNumber;
	for( size_t entry=0; entry<numberOfEntries; ++entry )
	{
		inputStream >> stripNumber;
		// Create a new SCurves object
		cbcanalyser::SCurve& newSCurve=temporaryInstance.getStripSCurve(stripNumber);
		// And then delegate to that to restore its state from disk
		newSCurve.restoreFromStream(inputStream);
	}

	// If everything went smoothly and I get to this point, I can overwrite the contents
	// with what was read from disk.
	(*this)=temporaryInstance;
}

//----------------------------------------------------------------------------------------------
//-------------------------- cbcanalyser::FitSCurve definitions --------------------------------
//----------------------------------------------------------------------------------------------

//cbcanalyser::FitSCurve::FitSCurve( TEfficiency & sCurve, const std::string& name )
//        : sCurveToFit_(sCurve)
//{
//  fitFunction_ = new TF1(TString(name+"_fittedFunction"), "([0]*0.5)*( 1 + TMath::Erf( [1]*(x-[2])/TMath::Sqrt2() ) )", 0, 1 );
//}
//
//std::unique_ptr<TF1> cbcanalyser::FitSCurve::performFit() const
//{
////   Define fit function and set initial parameters
//  std::unique_ptr<TF1> pFitFunction(fitFunction_);
//  pFitFunction->SetParameters(1., 1.1, 0.5); // Set initial parameters
//  pFitFunction->SetParLimits(0,0,1); // Limit range of p0 to be between 0 and 1
//  // Do the fit
//  sCurveToFit_.Fit(pFitFunction.get());
//  return pFitFunction;
//}

//----------------------------------------------------------------------------------------------
//-------------------------- cbcanalyser::FedSCurves definitions -------------------------------
//----------------------------------------------------------------------------------------------

cbcanalyser::FedChannelSCurves& cbcanalyser::FedSCurves::getFedChannelSCurves( size_t fedChannelNumber )
{
	return fedChannelSCurves_[fedChannelNumber];
}

const cbcanalyser::FedChannelSCurves& cbcanalyser::FedSCurves::getFedChannelSCurves( size_t fedChannelNumber ) const
{
	const auto& findResult=fedChannelSCurves_.find( fedChannelNumber );
	if( findResult==fedChannelSCurves_.end() ) throw std::runtime_error( "No entry for '"+std::to_string(fedChannelNumber)+"' in FED" );
	return findResult->second;
}

cbcanalyser::SCurve& cbcanalyser::FedSCurves::getStripSCurve( size_t fedChannelNumber, size_t stripNumber )
{
	return fedChannelSCurves_[fedChannelNumber].getStripSCurve(stripNumber);
}

std::vector<size_t> cbcanalyser::FedSCurves::getValidChannelIndices() const
{
	std::vector<size_t> returnValue;
	for( const auto& fedChannelNumberSCurvesPair : fedChannelSCurves_ ) returnValue.push_back( fedChannelNumberSCurvesPair.first );
	return returnValue;
}

void cbcanalyser::FedSCurves::createHistograms( TDirectory* pParentDirectory ) const
{
	std::stringstream stringConverter;

	for( const auto& fedChannelNumberSCurvesPair : fedChannelSCurves_ )
	{
		stringConverter.str("");
		stringConverter << "CBC " << std::setfill('0') << std::setw(2) << fedChannelNumberSCurvesPair.first;

		TDirectory* pSubDirectory=pParentDirectory->mkdir( stringConverter.str().c_str() );
		fedChannelNumberSCurvesPair.second.createHistograms( pSubDirectory );
	}

}

void cbcanalyser::FedSCurves::restoreFromDirectory( TDirectory* pParentDirectory )
{
	// Get rid of anything that was present before hand
	fedChannelSCurves_.clear();

	// I'll just try and retrieve all possible values for now. I haven't got time
	// to implement something that works out the FED channel number from the names
	// of the subdirectories.
	std::stringstream stringConverter;
	for( size_t fedChannelNumber=0; fedChannelNumber<100; ++fedChannelNumber )
	{
		stringConverter.str("");
		stringConverter << "CBC " << std::setfill('0') << std::setw(2) << fedChannelNumber;

		TDirectory* pSubDirectory=pParentDirectory->GetDirectory(stringConverter.str().c_str());
		if( pSubDirectory!=nullptr )
		{
			fedChannelSCurves_[fedChannelNumber].restoreFromDirectory(pSubDirectory);
		}
	}
}

void cbcanalyser::FedSCurves::dumpToStream( std::ostream& outputStream ) const
{
	outputStream << "FedSCurves " << fedChannelSCurves_.size() << " ";
	for( const auto& fedChannelNumberSCurvesPair : fedChannelSCurves_ )
	{
		outputStream << fedChannelNumberSCurvesPair.first << " "; // Dump the FED channel number
		fedChannelNumberSCurvesPair.second.dumpToStream(outputStream); // Then delegate to the FedSCurves class
	}
}

void cbcanalyser::FedSCurves::restoreFromStream( std::istream& inputStream )
{
	// Use a temporary object, so that if restoring fails I'm not left with a
	// half modified instance.
	cbcanalyser::FedSCurves temporaryInstance;

	std::string identifier;
	inputStream >> identifier;
	if( identifier!="FedSCurves" ) throw std::runtime_error( "FedSCurves::restoreFromStream - stream does not describe a FedSCurves object" );


	size_t numberOfEntries;
	inputStream >> numberOfEntries;

	size_t fedChannelNumber;
	for( size_t entry=0; entry<numberOfEntries; ++entry )
	{
		inputStream >> fedChannelNumber;
		// Create a new FedChannelSCurves object
		cbcanalyser::FedChannelSCurves& newFedChannelSCurves=temporaryInstance.getFedChannelSCurves(fedChannelNumber);
		// And then delegate to that to restore its state from disk
		newFedChannelSCurves.restoreFromStream(inputStream);
	}

	// If everything went smoothly and I get to this point, I can overwrite the contents
	// with what was read from disk.
	(*this)=temporaryInstance;
}

//----------------------------------------------------------------------------------------------
//------------------------ cbcanalyser::DetectorSCurves definitions ----------------------------
//----------------------------------------------------------------------------------------------

cbcanalyser::FedSCurves& cbcanalyser::DetectorSCurves::getFedSCurves( size_t fedNumber )
{
	return fedSCurves_[fedNumber];
}

cbcanalyser::FedChannelSCurves& cbcanalyser::DetectorSCurves::getFedChannelSCurves( size_t fedNumber, size_t fedChannelNumber )
{
	return fedSCurves_[fedNumber].getFedChannelSCurves(fedChannelNumber);
}

cbcanalyser::SCurve& cbcanalyser::DetectorSCurves::getStripSCurve( size_t fedNumber, size_t fedChannelNumber, size_t stripNumber )
{
	return fedSCurves_[fedNumber].getStripSCurve(fedChannelNumber,stripNumber);
}

std::vector<size_t> cbcanalyser::DetectorSCurves::getValidFedIndices() const
{
	std::vector<size_t> returnValue;
	for( const auto& fedNumberSCurvesPair : fedSCurves_ ) returnValue.push_back( fedNumberSCurvesPair.first );
	return returnValue;
}

void cbcanalyser::DetectorSCurves::createHistograms( TDirectory* pParentDirectory ) const
{
	std::stringstream stringConverter;

	for( const auto& fedNumberSCurvesPair : fedSCurves_ )
	{
		stringConverter.str("");
		stringConverter << "FED " << std::setfill('0') << std::setw(2) << fedNumberSCurvesPair.first;

		TDirectory* pSubDirectory=pParentDirectory->mkdir( stringConverter.str().c_str() );
		fedNumberSCurvesPair.second.createHistograms( pSubDirectory );
	}

}

void cbcanalyser::DetectorSCurves::dumpToStream( std::ostream& outputStream ) const
{
	outputStream << "DetectorSCurves " << fedSCurves_.size() << " ";
	for( const auto& fedNumberSCurvesPair : fedSCurves_ )
	{
		outputStream << fedNumberSCurvesPair.first << " "; // Dump the FED number
		fedNumberSCurvesPair.second.dumpToStream(outputStream); // Then delegate to the FedSCurves class
	}
}

void cbcanalyser::DetectorSCurves::restoreFromStream( std::istream& inputStream )
{
	// Use a temporary object, so that if restoring fails I'm not left with a
	// half modified instance.
	cbcanalyser::DetectorSCurves temporaryInstance;

	std::string identifier;
	inputStream >> identifier;
	if( identifier!="DetectorSCurves" ) throw std::runtime_error( "DetectorSCurves::restoreFromStream - stream does not describe a DetectorSCurves object" );


	size_t numberOfEntries;
	inputStream >> numberOfEntries;

	size_t fedNumber;
	for( size_t entry=0; entry<numberOfEntries; ++entry )
	{
		inputStream >> fedNumber;
		// Create a new FedSCurves object
		cbcanalyser::FedSCurves& newFedSCurves=temporaryInstance.getFedSCurves(fedNumber);
		// And then delegate to that to restore its state from disk
		newFedSCurves.restoreFromStream(inputStream);
	}

	// If everything went smoothly and I get to this point, I can overwrite the contents
	// with what was read from disk.
	(*this)=temporaryInstance;
}

