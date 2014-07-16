#include "SLHCUpgradeTracker/CBCAnalysis/plugins/AnalyseCBCOutput.h"

#include <iostream>
#include <stdexcept>
#include <FWCore/Framework/interface/MakerMacros.h>
#include <FWCore/Framework/interface/Event.h>
#include <DataFormats/Common/interface/TriggerResults.h>
#include <DataFormats/FEDRawData/interface/FEDRawDataCollection.h>
#include <EventFilter/SiStripRawToDigi/interface/SiStripFEDBuffer.h>
#include <FWCore/ServiceRegistry/interface/Service.h>
#include <CommonTools/UtilAlgos/interface/TFileService.h>
#include <FWCore/MessageService/interface/MessageLogger.h>
#include "SLHCUpgradeTracker/CBCAnalysis/interface/stringManipulationTools.h"
#include "SLHCUpgradeTracker/CBCAnalysis/interface/CBC2ChannelUnpacker.h"
#include "TMath.h"


namespace cbcanalyser
{
	DEFINE_FWK_MODULE(AnalyseCBCOutput);
}

namespace // Use the unnamed namespace for tools only used in this file
{
	/** @brief Closes the fstream as soon as it goes out of scope.
	 *
	 * Used as an exception safe way to close files.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 04/Sep/2013
	 */
	class FileStreamSentry
	{
	public:
		FileStreamSentry( std::fstream& fileStream ) : fileStream_(fileStream) {}
		~FileStreamSentry() { fileStream_.close(); }
	private:
		std::fstream& fileStream_;
	};

}

cbcanalyser::AnalyseCBCOutput::AnalyseCBCOutput( const edm::ParameterSet& config )
	: stripThresholdOffsets_(128), pSCurveEntryToMonitorForDQM_(nullptr), server_(*this)
{
	debug_=config.getUntrackedParameter<bool>("debug",false);

	if( debug_ ) std::cout << "cbcanalyser::AnalyseCBCOutput::AnalyseCBCOutput()" << std::endl;

	I2CValuesFilename_=config.getParameter<std::string>("trimFilename");
	savedStateFilename_=config.getUntrackedParameter<std::string>("savedStateFilename","");

	std::string hostname=config.getUntrackedParameter<std::string>("commsServerHostname");
	std::string port=config.getUntrackedParameter<std::string>("commsServerPort");

	if( debug_ ) std::cout << "cbcanalyser::AnalyseCBCOutput - Starting server on host " << hostname << " and port " << port << std::endl;
	server_.start( hostname, port );

	runsProcessed_=0;

	if( !savedStateFilename_.empty() )
	{
		try{ restoreState( savedStateFilename_ ); }
		catch( std::exception& error ){ std::cerr << "Couldn't restore state because: " << error.what() << std::endl; }
	}

}

cbcanalyser::AnalyseCBCOutput::~AnalyseCBCOutput()
{
	if( debug_ ) std::cout << "cbcanalyser::AnalyseCBCOutput::~AnalyseCBCOutput(). Analysed " << eventsProcessed_ << " events in " << runsProcessed_ << " runs." << std::endl;

	// For some reason I can't fathom, the last run is never included. I'll try and load the state
	// back from disk.
	if( eventsProcessed_==0 && !savedStateFilename_.empty() )
	{
		try{ restoreState( savedStateFilename_ ); }
		catch( std::exception& error ){ std::cerr << "Couldn't restore state because: " << error.what() << std::endl; }
	}


	//
	// Now that the job has finished, create root histograms from all of the
	// data that has been collected.
	//
	edm::Service<TFileService> pFileService;
	detectorSCurves_.createHistograms( &pFileService->file() );
	pFileService->file().Write();

	//
	// If the constructor is called then job has reached it's natural conclusion.
	// Truncate the state file so that the next job starts fresh.
	//
	if( !savedStateFilename_.empty() )
	{
		std::fstream blankFile( savedStateFilename_, std::ios_base::out | std::ios_base::trunc );
		blankFile.close();
	}
}

void cbcanalyser::AnalyseCBCOutput::fillDescriptions( edm::ConfigurationDescriptions& descriptions )
{
	//std::cout << "cbcanalyser::AnalyseCBCOutput::fillDescriptions()" << std::endl;
}

void cbcanalyser::AnalyseCBCOutput::beginJob()
{
	if( debug_ ) std::cout << "cbcanalyser::AnalyseCBCOutput::beginJob()" << std::endl;
}

void cbcanalyser::AnalyseCBCOutput::dumpSCurveToStream( std::ostream& output )
{
	// Copy to a non-atomic version before the loop
	float globalThreshold=globalComparatorThreshold_;
	// The global threshold should be between 0 and 1, so make sure this is the case
	if( globalThreshold<0 ) globalThreshold=0;
	else if( globalThreshold>1 ) globalThreshold=1;

	// Loop over all the FEDs
	for( const auto& fedIndex : detectorSCurves_.getValidFedIndices() )
	{
		// Loop over all the FED channels
		auto& fedSCurves=detectorSCurves_.getFedSCurves(fedIndex);
		for( const auto& channelIndex : fedSCurves.getValidChannelIndices() )
		{
			// Loop over all the strips of the CBC chip connected on this channel
			auto& channelSCurves=fedSCurves.getFedChannelSCurves(channelIndex);
			output << "FED " << fedIndex << ", FED channel " << channelIndex << ", threshold=" << globalThreshold << " -" << "\n";
			for( const auto& stripIndex : channelSCurves.getValidStripIndices() )
			{
				auto& sCurve=channelSCurves.getStripSCurve(stripIndex);
				// I don't want to dump every single point along the x-axis, just enough
				// to see what's going on. I'll dump information for the point on the x-axis
				// (i.e. threshold or whatever) that is currently being modified.
				//
				// Convert the [0,1] of the global threshold to the bin number in the s-curve.
				// Add the 0.5 so that round happens properly, although I also need to take 1
				// off because bin numbers start from 0.
				size_t thresholdBin=static_cast<size_t>( globalThreshold*sCurve.maxiumumEntries()-0.5 );
				const auto& sCurveEntry=sCurve.getEntry( thresholdBin );

				output << std::setw(3) << std::right << sCurveEntry.eventsOn() << ":" << std::setw(3) << std::left << sCurveEntry.eventsOff() << " ";
				if( stripIndex%16 == 15 ) output << "\n";
			} // end of loop over strips
		} // end of loop over FED channels
	} // end of loop over FEDs
}

void cbcanalyser::AnalyseCBCOutput::analyze( const edm::Event& event, const edm::EventSetup& setup )
{
	++eventsProcessed_;
	if( debug_ ) std::cout << "cbcanalyser::AnalyseCBCOutput::analyze() event " << eventsProcessed_ << std::endl;
	//if( pSCurveEntryToMonitorForDQM_==nullptr ) std::cout << std::endl;
	//else std::cout << " Strip ratio=" << pSCurveEntryToMonitorForDQM_->fraction() << "(" << pSCurveEntryToMonitorForDQM_->eventsOn() << ":" << pSCurveEntryToMonitorForDQM_->eventsOff() << ")" << std::endl;

	edm::Handle<FEDRawDataCollection> hRawData;
	event.getByLabel( "rawDataCollector", hRawData );

	// Since this could change if someone makes a http request, copy it into another
	// variable so that all data has the same value for this event.
	// N.B. globalComparatorThreshold_ should be std::atomic<float> but at the moment
	// that will compile but not link. I think the compiler version is too old.
	float globalThreshold=globalComparatorThreshold_;
	// The global threshold should be between 0 and 1, so make sure this is the case
	if( globalThreshold<0 ) globalThreshold=0;
	else if( globalThreshold>1 ) globalThreshold=1;

	size_t fedIndex;
	for( fedIndex=0; fedIndex<sistrip::CMS_FED_ID_MAX; ++fedIndex )
	{
		const FEDRawData& fedData=hRawData->FEDData(fedIndex);

		if( fedData.size()!=0 )
		{
			// Check to see if this FED is one of the ones allocated to the strip tracker
			if( fedIndex < sistrip::FED_ID_MIN || fedIndex > sistrip::FED_ID_MAX )
			{
				//std::cout << "Skipping FEDRawData at fedIndex " << std::dec << fedIndex << " has size " << fedData.size() << std::endl;
				continue;
			}

			if( debug_ ) std::cout << "FEDRawData at fedIndex " << std::dec << fedIndex << " has size " << fedData.size() << std::endl;
			try
			{
				{
					cbcanalyser::CBC2ChannelUnpacker unpacker(fedData);
				}
				sistrip::FEDBuffer myBuffer(fedData.data(),fedData.size());
				//myBuffer.print( std::cout );

				for ( uint16_t feIndex = 0; feIndex<sistrip::FEUNITS_PER_FED; ++feIndex )
				{
					if( !myBuffer.fePresent(feIndex) ) continue;

					if( debug_ ) std::cout << "FE " << feIndex << " is present" << std::endl;

					for ( uint16_t channelInFe = 0; channelInFe < sistrip::FEDCH_PER_FEUNIT; ++channelInFe )
					{
						const uint16_t channelIndex=feIndex*sistrip::FEDCH_PER_FEUNIT+channelInFe;
						const sistrip::FEDChannel& channel=myBuffer.channel(channelIndex);

						cbcanalyser::CBC2ChannelUnpacker unpacker(channel);
						if( !unpacker.hasData() ) continue;

						cbcanalyser::FedChannelSCurves& fedChannelSCurves=detectorSCurves_.getFedChannelSCurves( fedIndex, channelIndex );

						const std::vector<bool>& hits=unpacker.hits();

						if( debug_ ) std::cout << "Channel " << channelIndex << " has " << hits.size() << " hits: ";

						for( size_t stripNumber=0; stripNumber<hits.size(); ++stripNumber )
						{
							cbcanalyser::SCurve& sCurve=fedChannelSCurves.getStripSCurve(stripNumber);
							// Convert the [0,1] of the global threshold to the bin number in the s-curve.
							// Add the 0.5 so that round happens properly, although I also need to take 1
							// off because bin numbers start from 0.
							size_t thresholdBin=static_cast<size_t>( globalThreshold*sCurve.maxiumumEntries()-0.5 );
							cbcanalyser::SCurveEntry& sCurveEntry=sCurve.getEntry( thresholdBin );

							if( hits[stripNumber]==true ) ++sCurveEntry.eventsOn();
							else ++sCurveEntry.eventsOff();

							if( debug_ )
							{
								if( hits[stripNumber]==true ) std::cout << "1";
								else std::cout << ".";
							}

							if( pSCurveEntryToMonitorForDQM_==nullptr ) pSCurveEntryToMonitorForDQM_=&sCurveEntry;
						}

						if( debug_ ) std::cout << '\n';

					} // end of loop over FED channels
				}
			}
			catch( std::exception& error )
			{
				std::cout << "Exception: "<< error.what() << std::endl;
			}

		} // end of "if FED has data"
	} // end of loop over FEDs

	//if( debug_ ) dumpSCurveToStream( std::cout );
}

void cbcanalyser::AnalyseCBCOutput::endJob()
{
	if( debug_ ) std::cout << "cbcanalyser::AnalyseCBCOutput::endJob(). Analysed " << eventsProcessed_ << " events in " << runsProcessed_ << " runs." << std::endl;
}

void cbcanalyser::AnalyseCBCOutput::beginRun( const edm::Run& run, const edm::EventSetup& setup )
{
	if( debug_ ) std::cout << "cbcanalyser::AnalyseCBCOutput::beginRun()" << std::endl;
	try { readI2CValues(); }
	catch( std::exception& error )
	{
		std::cerr << "readI2CValues() failed because: " << error.what() << std::endl;
	}
	eventsProcessed_=0;
	++runsProcessed_;
}

void cbcanalyser::AnalyseCBCOutput::endRun( const edm::Run& run, const edm::EventSetup& setup )
{
	if( debug_ ) std::cout << "cbcanalyser::AnalyseCBCOutput::endRun(). Analysed " << eventsProcessed_ << " events in " << runsProcessed_ << " runs." << std::endl;

	if( !savedStateFilename_.empty() && eventsProcessed_>0 ) saveState( savedStateFilename_ );
}

void cbcanalyser::AnalyseCBCOutput::beginLuminosityBlock( const edm::LuminosityBlock& lumiBlock, const edm::EventSetup& setup )
{
	if( debug_ ) std::cout << "cbcanalyser::AnalyseCBCOutput::beginLuminosityBlock(). Analysed " << eventsProcessed_ << " events in " << runsProcessed_ << " runs." << std::endl;
}

void cbcanalyser::AnalyseCBCOutput::endLuminosityBlock( const edm::LuminosityBlock& lumiBlock, const edm::EventSetup& setup )
{
	if( debug_ ) std::cout << "cbcanalyser::AnalyseCBCOutput::endLuminosityBlock(). Analysed " << eventsProcessed_ << " events in " << runsProcessed_ << " runs." << std::endl;
}

void cbcanalyser::AnalyseCBCOutput::handleRequest( const httpserver::HttpServer::Request& request, httpserver::HttpServer::Reply& reply )
{
	//
	// Check the URI in the request and see what the request resource is. If it's recognised, farm off to
	// specific methods. Also see if there are any parameters included as part of the URI.
	//

	// Split off any parameters in the uri
	std::string resource;
	std::vector< std::pair<std::string,std::string> > parameters;
	size_t characterPosition=request.uri.find_first_of("?");
	resource=request.uri.substr(0,characterPosition);
	if( characterPosition!=std::string::npos )
	{
		std::string parameterString=request.uri.substr(characterPosition+1);
		// For now I'll just decode as one parameter, and work out logic for others later
		parameters.resize(1);
		characterPosition=parameterString.find_first_of("=");
		parameters[0].first=parameterString.substr(0,characterPosition);
		parameters[0].second=parameterString.substr(characterPosition+1);
	}


	try
	{
		if( resource=="/changeVar" ) return request_changeVar( reply, parameters );
		else if( resource=="/scurveFits" ) return request_scurveFits( reply );
		else if( resource=="/createFakeData" ) return request_createFakeData( reply );
		else
		{
			//
			// If the request wasn't recognised print some stuff for debugging
			//
			std::stringstream outputStream;

			outputStream << "Request was:" << "\n"
					<< 	"method=" << request.method << "\n"
					<< 	"uri=" << request.uri << "\n"
					<< 	"http_version_major=" << request.http_version_major << "\n"
					<< 	"http_version_minor=" << request.http_version_minor << "\n"
					<< 	"headers.size()=" << request.headers.size() << "\n";
			for( const auto& header : request.headers ) outputStream << "\t" << header.name << "=" << header.value << "\n";

			outputStream << "\n" << "globalComparatorThreshold_=" << globalComparatorThreshold_ << "\n";

			outputStream << "Decoded uri as:" << "\n"
					<< "resource=" << resource << "\n";
			for( const auto& parameter : parameters ) outputStream << parameter.first << "=" << parameter.second << "\n";

			reply.status=httpserver::HttpServer::Reply::StatusType::ok;
			reply.content=outputStream.str();
			reply.headers.resize( 2 );
			reply.headers[0].name="Content-Length";
			reply.headers[0].value=std::to_string( reply.content.size() );
			reply.headers[1].name="Content-Type";
			reply.headers[1].value="text/plain";
		}
	} // end of try block
	catch( std::exception& error )
	{
		reply.status=httpserver::HttpServer::Reply::StatusType::bad_request;
		reply.content=std::string("Exception encountered: ")+error.what();
		reply.headers.resize( 2 );
		reply.headers[0].name="Content-Length";
		reply.headers[0].value=std::to_string( reply.content.size() );
		reply.headers[1].name="Content-Type";
		reply.headers[1].value="text/plain";
	}
}

void cbcanalyser::AnalyseCBCOutput::request_scurveFits( httpserver::HttpServer::Reply& reply )
{
	std::stringstream outputStream;

	outputStream << "{" << "\n"
			<< "\t" << "\"scurves\" : [" << "\n";

	// Loop over all the FEDs
	for( const auto& fedIndex : detectorSCurves_.getValidFedIndices() )
	{
		// Loop over all the FED channels
		auto& fedSCurves=detectorSCurves_.getFedSCurves(fedIndex);
		for( const auto& channelIndex : fedSCurves.getValidChannelIndices() )
		{
			// Loop over all the strips of the CBC chip connected on this channel
			auto& channelSCurves=fedSCurves.getFedChannelSCurves(channelIndex);
			//output << "FED " << fedIndex << ", FED channel " << channelIndex << ", threshold=" << globalThreshold << " -" << "\n";
			for( const auto& stripIndex : channelSCurves.getValidStripIndices() )
			{
				auto& sCurve=channelSCurves.getStripSCurve(stripIndex);
				std::tuple<float,float,float,float,float> fitParameters;
				fitParameters=sCurve.fitParameters();

				// Output the fit parameters as JSON
				outputStream << "\t" << "{" << "\n"
						<< "\t\t" << "\"fed\" : " << fedIndex << "," << "\n"
						<< "\t\t" << "\"fedChannel\" : " << channelIndex << "," << "\n"
						<< "\t\t" << "\"cbcChannel\" : " << stripIndex << "," << "\n"
						<< "\t\t" << "\"fitParameters\" : {" << "\n"
						<< "\t\t\t" << "\"chi2\" : " << std::get<0>(fitParameters) << "," << "\n"
						<< "\t\t\t" << "\"NDF\" : " << std::get<1>(fitParameters) << "," << "\n"
						<< "\t\t\t" << "\"maxEfficiency\" : " << std::get<2>(fitParameters) << "," << "\n"
						<< "\t\t\t" << "\"standardDeviation\" : " << std::get<3>(fitParameters) << "," << "\n"
						<< "\t\t\t" << "\"mean\" : " << std::get<4>(fitParameters) << "," << "\n"
						<< "\t\t" << "}" << "\n"
						<< "\t" << "}," << "\n";
			} // end of loop over strips
		} // end of loop over FED channels
	} // end of loop over FEDs

	outputStream << "\t" << "]" << "\n"
			<< "}" << "\n";

	reply.status=httpserver::HttpServer::Reply::StatusType::ok;
	reply.content=outputStream.str();
	reply.headers.resize( 2 );
	reply.headers[0].name="Content-Length";
	reply.headers[0].value=std::to_string( reply.content.size() );
	reply.headers[1].name="Content-Type";
	reply.headers[1].value="application/json";
}

void cbcanalyser::AnalyseCBCOutput::request_createFakeData( httpserver::HttpServer::Reply& reply )
{
	const size_t numberOfThresholds=100;
	const size_t numberOfEventsPerThreshold=100;
	const float firstThreshold=0;
	const float lastThreshold=5;
	const float meanTurnOn=(firstThreshold+lastThreshold)*0.45;
	const float standardDeviation=30.0/(lastThreshold-firstThreshold);

	cbcanalyser::FedChannelSCurves& fedChannelSCurves=detectorSCurves_.getFedChannelSCurves( 42, 6 );

	for( size_t stripNumber=0; stripNumber<128; ++stripNumber )
	{
		cbcanalyser::SCurve& sCurve=fedChannelSCurves.getStripSCurve(stripNumber);

		for( size_t index=0; index<numberOfThresholds; ++index )
		{
			float threshold=firstThreshold+(lastThreshold-firstThreshold)/static_cast<float>(numberOfThresholds-1)*static_cast<float>(index);
			// Randomise the data very slightly so that not all channels are the same. Make some
			// random numbers between 0.95 and 1.05 so that the values will fluctuate +/- 5%
			float meanFluctuated=meanTurnOn*(static_cast<float>(std::rand())/static_cast<float>(RAND_MAX)*0.1+0.95);
			float stddevFluctuated=standardDeviation*(static_cast<float>(std::rand())/static_cast<float>(RAND_MAX)*0.1+0.95);
			float efficiency=0.5*( 1 + TMath::Erf( stddevFluctuated*(threshold-meanFluctuated)/TMath::Sqrt2() ) );
			cbcanalyser::SCurveEntry& entry=sCurve.getEntry(threshold);
			entry.eventsOn()=efficiency*static_cast<float>( numberOfEventsPerThreshold )+0.5; // Add 0.5 so that it rounds properly
			entry.eventsOff()=numberOfEventsPerThreshold-entry.eventsOn();
		}
	}

	reply.status=httpserver::HttpServer::Reply::StatusType::ok;
	reply.content="Fake data created for "+std::to_string(numberOfThresholds)+" thresholds between "+std::to_string(firstThreshold)+" and "+std::to_string(lastThreshold)+"\n";
	reply.headers.resize( 2 );
	reply.headers[0].name="Content-Length";
	reply.headers[0].value=std::to_string( reply.content.size() );
	reply.headers[1].name="Content-Type";
	reply.headers[1].value="text/plain";
}

void cbcanalyser::AnalyseCBCOutput::request_changeVar( httpserver::HttpServer::Reply& reply, const std::vector< std::pair<std::string,std::string> >& parameters )
{
	std::stringstream outputStream;

	//
	// Loop over all of the parameters and see if one of the names is one that I
	// recognise. If so convert the value from string to float and change the
	// variable.
	//
	if( parameters.empty() ) outputStream << "No parameters specified to modify." << "\n";

	for( const auto& parameter : parameters )
	{
		if( parameter.first=="globalComparatorThreshold_" )
		{
			std::stringstream stringConverter;
			stringConverter.str(parameter.second);
			float variable;
			stringConverter >> variable;
			outputStream << "Setting " << parameter.first << " to " << variable << " previous value was " << globalComparatorThreshold_ << "\n";
			globalComparatorThreshold_=variable;
		}
		else
		{
			outputStream << "Can't set unknown variable '" << parameter.first << "'" << "\n";
		}
	}

	reply.status=httpserver::HttpServer::Reply::StatusType::ok;
	reply.content=outputStream.str();
	reply.headers.resize( 2 );
	reply.headers[0].name="Content-Length";
	reply.headers[0].value=std::to_string( reply.content.size() );
	reply.headers[1].name="Content-Type";
	reply.headers[1].value="text/plain";
}

void cbcanalyser::AnalyseCBCOutput::readI2CValues()
{
	std::ifstream trimFile( I2CValuesFilename_ );
	if( !trimFile.is_open() ) throw std::runtime_error( "Unable to open the trim file \""+I2CValuesFilename_+"\"");

	const size_t bufferSize=200;
	char buffer[bufferSize];
	while( trimFile.good() )
	{
		try
		{
			// Get one line at a time
			trimFile.getline( buffer, bufferSize );

			// split the line and lose everything after a comment character
			std::string lineWithoutComments=cbcanalyser::tools::splitByDelimeters( buffer, "#*" ).front();

			// split the line by whitespace into columns
			std::vector<std::string> columns=cbcanalyser::tools::splitByWhitespace( lineWithoutComments );

			if( columns.size()==1 && columns[0].empty() ) continue; // Allow blank lines without giving a warning
			if( columns.size()!=4 ) throw std::runtime_error( "The line does not have the correct number of columns" );

			std::string valueName=columns[0];
			if( valueName.substr(0,7)=="Channel" )
			{
				std::string channelNumberAsString=valueName.substr(7);
				int channelNumber=cbcanalyser::tools::convertStringToFloat(channelNumberAsString);
				if( channelNumber<0 || channelNumber>127 ) throw std::runtime_error( "Unknown channel number "+channelNumberAsString);

				int threshold=cbcanalyser::tools::convertHexToInt(columns[3]);
				stripThresholdOffsets_[channelNumber]=threshold;
			}

		} // end of try block
		catch( std::runtime_error& exception )
		{
			std::cout << "Some error occurred while processing the line \"" << buffer << "\":" << exception.what() << std::endl;
		}
	}


	trimFile.close();
}

void cbcanalyser::AnalyseCBCOutput::saveState( const std::string& filename )
{
	std::fstream outputFile( filename, std::ios_base::out | std::ios_base::trunc );
	if( !outputFile.is_open() ) throw std::runtime_error( "Unable to open the output file \""+filename+"\" to save the analyser state.");
	FileStreamSentry closeFileSentry(outputFile);

	detectorSCurves_.dumpToStream( outputFile );

	outputFile << "stripThresholdOffsets_ " << stripThresholdOffsets_.size() << " ";
	for( const auto& offset : stripThresholdOffsets_ ) outputFile << offset << " ";

	outputFile << eventsProcessed_ << " " << runsProcessed_ << " ";
}

void cbcanalyser::AnalyseCBCOutput::restoreState( const std::string& filename )
{
	std::fstream inputFile( filename, std::ios_base::in );
	if( !inputFile.is_open() ) throw std::runtime_error( "Unable to open the input file \""+filename+"\" to restore the analyser state.");
	FileStreamSentry closeFileSentry(inputFile);

	detectorSCurves_.restoreFromStream( inputFile );

	std::string identifier;
	inputFile >> identifier;
	if( identifier!="stripThresholdOffsets_" ) throw std::runtime_error( "AnalyseCBCOutput::restoreState - didn't read stripThresholdOffsets_ tag." );

	size_t entries;
	inputFile >> entries;
	stripThresholdOffsets_.resize(entries);
	for( size_t index=0; index<entries; ++index ) inputFile >> stripThresholdOffsets_[index];

	inputFile >> eventsProcessed_ >> runsProcessed_;
}
