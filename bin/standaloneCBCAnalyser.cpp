/** @file
 *
 * @brief An executable that opens a port given on the command line and listens for instructions.
 *
 * I can't get the full DAQ chain working at the moment (RU -> BU -> my FUEventProcessor) but I can
 * successfully dump from the GlibStreamer to disk and read it back. Instead of running analysis code
 * in an FUEventProcessor I'll run it in this executable and feed it the DAQ dumps. I'll try and put
 * as much code as possible in utility classes so that it can be transferred to a FUEventProcessor at
 * a later date if required.
 *
 * This executable opens a port and listens for instructions in the form of HTTP GET requests. I'll
 * have a python script performing the runcontrol which will instigate runs which dump a file, and
 * then instruct this program to open the file and analyse it. The python script will then query
 * the results and decide what to do next.
 *
 * I chose not to analyse in the python script because I want any analysis code to be useable from
 * XDAQ and CMSSW.
 *
 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
 * @date 06/Jan/2014
 */

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <iomanip>

#include "SLHCUpgradeTracker/CBCAnalysis/interface/RawDataFileReader.h"
#include "SLHCUpgradeTracker/CBCAnalysis/interface/HttpServer.h"
#include "SLHCUpgradeTracker/CBCAnalysis/interface/SCurve.h"
#include "TFile.h"
#include "TEfficiency.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TGraphAsymmErrors.h"
#include "TPaveStats.h"
#include "TSystem.h"

// Use the unnamed namespace for things only used in this file
namespace
{
	/** @brief Class that handles the HTTP requests.
	 *
	 * Most of the functionality is in here. When a HTTP request comes in, the server passes it on to
	 * handleRequest(...). This looks at the resource requested and passes it on to the appropriate
	 * delegate method. E.g. if the resource "/analyseFile" is requested then the method handle_analyseFile
	 * is called to deal with it.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 06/Jan/2014
	 */
	class HttpRequestHandler : public httpserver::HttpServer::IRequestHandler
	{
	public:
		HttpRequestHandler() : threshold_(0) {}
		virtual ~HttpRequestHandler() {}
		virtual void handleRequest( const httpserver::HttpServer::Request& request, httpserver::HttpServer::Reply& reply );
	protected:
		httpserver::HttpServer::Reply::StatusType handle_analyseFile( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput=nullptr );
		httpserver::HttpServer::Reply::StatusType handle_saveHistograms( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput=nullptr );
		httpserver::HttpServer::Reply::StatusType handle_saveHistogramPicture( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput=nullptr );
		httpserver::HttpServer::Reply::StatusType handle_setThreshold( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput=nullptr );
		httpserver::HttpServer::Reply::StatusType handle_version( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput=nullptr );
		httpserver::HttpServer::Reply::StatusType handle_restoreFromRootFile( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput=nullptr );
		httpserver::HttpServer::Reply::StatusType handle_occupancies( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput=nullptr );
		httpserver::HttpServer::Reply::StatusType handle_fitParameters( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput=nullptr );
		httpserver::HttpServer::Reply::StatusType handle_reset( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput=nullptr );
		/** Copied this from http://stackoverflow.com/questions/154536/encode-decode-urls-in-c */
		std::string urlDecode( std::string& inputString );
		template<class T> std::vector<T> decodeStringToArray( const std::string& inputString );
		template<class T> T convertString( const std::string& inputString );
		template<class T> T convertString_impl( const std::string& inputString, size_t& idx ); ///< @brief convertString delegates to this method, because there is a lot of duplicated code otherwise.
		void openDAQDumpFile( const std::string& filename );
		cbcanalyser::FedSCurves connectedCBCSCurves_;
		float threshold_;
	};

} // end of the unnamed namespace


int main( int argc, char* argv[] )
{
	//
	// Remove all of the root signal handlers. I want this program to die
	// with a simple SIGTERM, the root signal handlers seem to screw this
	// up.
	//
	for( int sig = 0; sig < kMAXSIGNALS; sig++) gSystem->ResetSignal( static_cast<ESignals>(sig) );

	if( argc!=2 )
	{
		std::cout << "Program to analyse DAQ dump files from GlibStreamer." << "\n"
				<< "\n"
				<< "Usage:       " << argv[0] << " <port number>" << "\n"
				<< "   Starts a server listening on the given port. This listens for instructions in the form of" << "\n"
				<< "   HTTP GET requests. To see a list of the commands point a webbrowser at the running server." << "\n"
				<< std::endl;

		return -1;
	}

	try
	{
		HttpRequestHandler handler;
		httpserver::HttpServer server( handler );
		server.start( "127.0.0.1", argv[1] );

		std::cout << "Server started at 127.0.0.1 on port " << argv[1] << std::endl;

		server.blockUntilFinished();
	}
	catch( std::exception& error )
	{
		std::cerr << "An exception occurred: " << error.what() << std::endl;
		return -1;
	}

	return 0;
}

//
// Definitions of things declared in the unnamed namespace
//
namespace
{
	void HttpRequestHandler::handleRequest( const httpserver::HttpServer::Request& request, httpserver::HttpServer::Reply& reply )
	{
		//
		// First split up the URI in the request into the resource and parameter
		// names and values.
		//
		std::string resource;
		std::vector< std::pair<std::string,std::string> > parameters;
		httpserver::HttpServer::splitURI( request.uri, resource, parameters );

		std::stringstream output;

		std::ostream* pDebugOutput=nullptr;
		// Uncomment this line to make the delegate methods print extra info to the logs
		//pDebugOutput=&std::cerr;

		for( const auto& paramValuePair : parameters )
		{
			if( paramValuePair.first=="debug" )
			{
				output << "Raw string=\"" << request.uri << "\"" << "\n";
				output << "Your request was \"" << resource << "\" with " << parameters.size() << " parameters" << "\n";
				for( const auto& paramValuePair : parameters )
				{
					output << "   " << paramValuePair.first << " = " << "\"" << paramValuePair.second << "\""<< "\n";
				}
				output << "\n";
			}
		}


		try
		{
			if( resource=="/" )
			{
				output << "N.B. This is a little out of date at the moment. Have a look in " << __FILE__ << " line " << __LINE__ << " for more up to date details." << "\n"
						<< "Available commands (case sensitive):" << "\n"
						<< "   analyseFile?filename=<filename>          Analyses the file with the supplied filename." << "\n"
						<< "   restoreFromRootFile?filename=<filename>  Restores the data from a file previously saved with saveHistograms." << "\n"
						<< "   setThreshold?value=<value>               Sets the current threshold (the abscissa for all plots) to the specified value." << "\n"
						<< "   saveHistograms?filename=<filename>       Save the histograms in their current state to the specified file. If it already" << "\n"
						<< "                                            exists it will be overwritten." << "\n"
						<< "   version                                  States the version of this code." << "\n"
						<< "   reset                                    Resets any data taken." << "\n"
						<< "\n"
						<< "You can add the parameter \"debug\" to any command to echo your request."
						<< "\n";
			}
			else if( resource=="/analyseFile" || resource=="/analyzeFile" ) reply.status=handle_analyseFile( output, std::cerr, parameters, pDebugOutput );
			else if( resource=="/saveHistograms" ) reply.status=handle_saveHistograms( output, std::cerr, parameters, pDebugOutput );
			else if( resource=="/saveHistogramPicture" ) reply.status=handle_saveHistogramPicture( output, std::cerr, parameters, pDebugOutput );
			else if( resource=="/setThreshold" ) reply.status=handle_setThreshold( output, std::cerr, parameters, pDebugOutput );
			else if( resource=="/version" ) reply.status=handle_version( output, std::cerr, parameters, pDebugOutput );
			else if( resource=="/restoreFromRootFile" ) reply.status=handle_restoreFromRootFile( output, std::cerr, parameters, pDebugOutput );
			else if( resource=="/occupancies" ) reply.status=handle_occupancies( output, std::cerr, parameters, pDebugOutput );
			else if( resource=="/fitParameters" ) reply.status=handle_fitParameters( output, std::cerr, parameters, pDebugOutput );
			else if( resource=="/reset" ) reply.status=handle_reset( output, std::cerr, parameters, pDebugOutput );
			else
			{
				output << "Unknown command. You can see a list of the available commands by requesting the root resource (i.e. \"/\")." << "\n";
				reply.status=httpserver::HttpServer::Reply::StatusType::not_found;
			}
		}
		catch( std::exception& error )
		{
			std::cout << "HttpRequestHandler::handleRequest - some kind of exception was thrown: " << error.what() << std::endl;
			output << "An exception was thrown. Check the server logs for details." << "\n";
			reply.status=httpserver::HttpServer::Reply::StatusType::internal_server_error;
		}

		// Status should already have been set in the "if" statements above
		reply.content=output.str();
		reply.headers.resize( 2 );
		reply.headers[0].name="Content-Length";
		reply.headers[0].value=std::to_string( reply.content.size() );
		reply.headers[1].name="Content-Type";
		reply.headers[1].value="text/plain";

		std::cerr << reply.content << std::endl;
	}

	httpserver::HttpServer::Reply::StatusType HttpRequestHandler::handle_analyseFile( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput )
	{
		if( pDebugOutput ) (*pDebugOutput) << "analyseFile called" << "\n";
		// Look through the parameters and try and find the filename to open
		std::string filename;
		for( const auto& paramValuePair : parameters ) if( paramValuePair.first=="filename" ) filename=paramValuePair.second;

		if( filename.empty() )
		{
			errorLog << "HttpRequestHandler::handle_analyseFile - Error! no filename was supplied, or it's empty." << "\n";
			return httpserver::HttpServer::Reply::StatusType::bad_request;
		}
		else
		{
			try
			{
				openDAQDumpFile( filename );
			}
			catch( std::exception& error )
			{
				errorLog << "HttpRequestHandler::handle_analyseFile - Oh dear. An exception was encountered. Here it is: " << error.what() << "\n";
				return httpserver::HttpServer::Reply::StatusType::internal_server_error;
			}
		}

		return httpserver::HttpServer::Reply::StatusType::ok;
	}

	httpserver::HttpServer::Reply::StatusType HttpRequestHandler::handle_saveHistograms( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput )
	{
		if( pDebugOutput ) (*pDebugOutput) << "saveHistograms called" << "\n";
		// Look through the parameters and try and find the filename to open
		std::string filename;
		for( const auto& paramValuePair : parameters ) if( paramValuePair.first=="filename" ) filename=paramValuePair.second;

		if( filename.empty() )
		{
			errorLog << "Error! no filename was supplied, or it's empty." << "\n";
			return httpserver::HttpServer::Reply::StatusType::bad_request;
		}
		else
		{
			// Open a TFile and save the histograms to it
			TFile outputFile( filename.c_str(), "RECREATE" );
			connectedCBCSCurves_.createHistograms( &outputFile );
			outputFile.Write();
		}
		return httpserver::HttpServer::Reply::StatusType::ok;
	}

	httpserver::HttpServer::Reply::StatusType HttpRequestHandler::handle_saveHistogramPicture( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput )
	{
		// Look through the parameters and try and find the filename to save the plot to
		std::string filename;
		for( const auto& paramValuePair : parameters ) if( paramValuePair.first=="filename" ) filename=paramValuePair.second;
		// Look through the parameters and try and get the array of arrays that shows the
		// channels to include in the plot.
		std::vector< std::vector<int> > channelsForEachCBC;
		std::string channelsString;
		for( const auto& paramValuePair : parameters ) if( paramValuePair.first=="channels" ) channelsString=paramValuePair.second;
		try
		{
			if( pDebugOutput ) (*pDebugOutput) << "Trying to convert '" << channelsString << "' to array." << std::endl;
			if( pDebugOutput ) (*pDebugOutput) << "Decoded, this is '" << urlDecode(channelsString) << "'" << std::endl;
			std::vector<std::string> innerArrays=decodeStringToArray<std::string>( urlDecode(channelsString) );
			for( const auto& arrayAsString : innerArrays )
			{
				channelsForEachCBC.push_back( decodeStringToArray<int>( arrayAsString ) );
			}
		}
		catch( std::exception& error )
		{
			errorLog << "HttpRequestHandler::handle_saveHistogramPicture - Error! Couldn't get the array of arrays for the CBC channels (" << urlDecode(channelsString) << "): " << error.what() << "\n";
			return httpserver::HttpServer::Reply::StatusType::bad_request;
		}

		if( filename.empty() )
		{
			errorLog << "HttpRequestHandler::handle_saveHistogramPicture - Error! no filename was supplied, or it's empty." << "\n";
			return httpserver::HttpServer::Reply::StatusType::bad_request;
		}
		else if( channelsForEachCBC.empty() )
		{
			errorLog << "HttpRequestHandler::handle_saveHistogramPicture - Error! no channels were specified to plot." << "\n";
			return httpserver::HttpServer::Reply::StatusType::bad_request;
		}
		else
		{
			std::vector< std::unique_ptr<TEfficiency> > histograms;

			// Use a const reference to make sure I don't create entries by querying.
			const cbcanalyser::FedSCurves& constCBCs=connectedCBCSCurves_;
			//
			// First run through and create all of the histograms
			//
			for( size_t cbcIndex=0; cbcIndex<channelsForEachCBC.size(); ++cbcIndex )
			{
				if( pDebugOutput ) (*pDebugOutput)  << "Trying CBC " << cbcIndex << "\n";
				try
				{
					const cbcanalyser::FedChannelSCurves& fedChannel=constCBCs.getFedChannelSCurves(cbcIndex);
					for( const auto channelNumber : channelsForEachCBC[cbcIndex] )
					{
						//if( pDebugOutput ) (*pDebugOutput) << "Adding CBC " << cbcIndex << " channel " << channelNumber << std::endl;
						const cbcanalyser::SCurve& sCurve=fedChannel.getStripSCurve(channelNumber);
						histograms.push_back( std::move( sCurve.createHistogram( "Efficiency CBC "+std::to_string(cbcIndex)+" channel "+std::to_string(channelNumber) ) ) );
						// Also fit the histogram
						cbcanalyser::SCurve::fitHistogram( histograms.back() );
					}
				}
				catch( std::runtime_error& exception )
				{
					errorLog << "HttpRequestHandler::handle_saveHistogramPicture - Got the error : " << exception.what() << std::endl;
				}
			}

			if( pDebugOutput ) (*pDebugOutput) << "Histograms created" << std::endl;

			std::unique_ptr<TCanvas> pCanvas( new TCanvas() );
			std::string drawOption="";
			for( const auto& pHistogram : histograms )
			{
				pHistogram->SetTitle("");
				pHistogram->Draw(drawOption.c_str());
				drawOption="same";
			}

			if( pDebugOutput ) (*pDebugOutput) << "Histograms drawn" << std::endl;

			// Add an error to the plot in case there was no data
			std::unique_ptr<TPaveText> pPaveText;
			if( histograms.empty() )
			{
				pPaveText.reset( new TPaveText(0.05,0.1,0.95,0.8) );
				pPaveText->AddText("There appears to be no data.");
				pPaveText->AddText("Have you taken a run yet?");
				pPaveText->Draw();
			}
			// Because of some bizarre root oddity, the only way I can find to remove the fit
			// parameters box is this. The painted graph isn't always available until the canvas
			// is updated, and this seems much faster after all the histograms have been plotted.
			pCanvas->Update();
			TList* pListOfPrimitives=pCanvas->GetListOfPrimitives();
			for( int index=0; index<pListOfPrimitives->GetSize(); ++index )
			{
				TObject* pPrimitive=pListOfPrimitives->At(index);
				if( pPrimitive->ClassName()==std::string("TEfficiency") )
				{
					TGraphAsymmErrors* pPaintedHistogram=static_cast<TEfficiency*>(pPrimitive)->GetPaintedGraph();
					pPaintedHistogram->SetMaximum(1.1); // Sometimes if the data doesn't go to maximum efficiency the top of the plot is cut off.
					TPaveStats* pStatBox=static_cast<TPaveStats*>(pPaintedHistogram->GetListOfFunctions()->FindObject("stats"));
					// Haven't figured out how to delete this, so clear the text and make the
					// box invisible.
					pStatBox->Clear();
					pStatBox->SetFillStyle(0);
					pStatBox->SetBorderSize(0);
					pStatBox->SetOptFit(0000);
					pStatBox->Clear("");
				}
			}
			if( pDebugOutput ) (*pDebugOutput) << "Text boxes removed" << std::endl;

			pCanvas->SaveAs( filename.c_str() );
			return httpserver::HttpServer::Reply::StatusType::ok;
		}
	}

	httpserver::HttpServer::Reply::StatusType HttpRequestHandler::handle_setThreshold( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput )
	{
		if( pDebugOutput ) (*pDebugOutput) << "setThreshold called" << "\n";
		// Look through the parameters and try and find the value to set to
		std::string valueAsString;
		for( const auto& paramValuePair : parameters ) if( paramValuePair.first=="value" ) valueAsString=paramValuePair.second;

		if( valueAsString.empty() )
		{
			errorLog << "HttpRequestHandler::handle_saveHistogramPicture - Error! no value was supplied. Add \"?value=<threshold>\" to the URL." << "\n";
			return httpserver::HttpServer::Reply::StatusType::bad_request;
		}
		else
		{
			std::stringstream stringConverter;
			stringConverter.str(valueAsString);
			float variable;
			stringConverter >> variable;
			if( pDebugOutput ) (*pDebugOutput) << "Setting threshold to " << variable << "; previous value was " << threshold_ << "\n";
			threshold_=variable;
		}

		return httpserver::HttpServer::Reply::StatusType::ok;
	}

	httpserver::HttpServer::Reply::StatusType HttpRequestHandler::handle_version( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput )
	{
		reply << "Version=" << "0.0" << "\n";
		return httpserver::HttpServer::Reply::StatusType::ok;
	}

	httpserver::HttpServer::Reply::StatusType HttpRequestHandler::handle_restoreFromRootFile( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput )
	{
		// Look through the parameters and try and find the filename to open
		std::string filename;
		for( const auto& paramValuePair : parameters ) if( paramValuePair.first=="filename" ) filename=paramValuePair.second;

		if( filename.empty() )
		{
			errorLog << "HttpRequestHandler::handle_restoreFromRootFile - Error! no filename was supplied, or it's empty." << "\n";
			return httpserver::HttpServer::Reply::StatusType::bad_request;
		}
		else
		{
			try
			{
				// Open a TFile and try loading the TEfficiency objects from the different subdirectories
				TFile outputFile( filename.c_str() );
				if( outputFile.IsZombie() ) throw std::runtime_error( "Unable to open file "+filename );
				connectedCBCSCurves_.restoreFromDirectory( &outputFile );
				if( pDebugOutput ) (*pDebugOutput)  << "State loaded from file " << filename << "\n";
				return httpserver::HttpServer::Reply::StatusType::ok;
			}
			catch( std::exception& error )
			{
				errorLog << "Error! " << error.what() << "\n";
				return httpserver::HttpServer::Reply::StatusType::internal_server_error;
			}
		}
	}

	httpserver::HttpServer::Reply::StatusType HttpRequestHandler::handle_occupancies( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput )
	{
		// At the moment I'm just going to return the occupancies for the first threshold
		// that is stored for each channel. Later I'll code up some option to specify
		// which threshold you want the occupancies for.
		reply << "{" << "\n";
		std::vector<size_t> channelIndices=connectedCBCSCurves_.getValidChannelIndices();
		for( std::vector<size_t>::const_iterator iIndexA=channelIndices.begin(); iIndexA!=channelIndices.end(); ++iIndexA )
		{
			reply << "   \"CBC " << std::setfill('0') << std::setw(2)  << *iIndexA << "\": {" << "\n";
			cbcanalyser::FedChannelSCurves& fedChannel=connectedCBCSCurves_.getFedChannelSCurves(*iIndexA);
			std::vector<size_t> stripIndices=fedChannel.getValidStripIndices();
			for( std::vector<size_t>::const_iterator iIndexB=stripIndices.begin(); iIndexB!=stripIndices.end(); ++iIndexB )
			{
				cbcanalyser::SCurve& sCurve=fedChannel.getStripSCurve(*iIndexB);
				std::vector<float> validThresholds=sCurve.getValidThresholds();
				if( !validThresholds.empty() )
				{
					cbcanalyser::SCurveEntry& firstEntry=sCurve.getEntry( validThresholds.front() );
					reply << "      \"Channel " << std::setfill('0') << std::setw(3)  << *iIndexB << "\": " << firstEntry.fraction();
					if( iIndexB+1 != stripIndices.end() ) reply << ",";
					reply << "\n";
				}
			}
			reply << "   }";
			if( iIndexA+1 != channelIndices.end() ) reply << ",";
			reply << "\n";
		}
		reply << "}" << "\n";
		return httpserver::HttpServer::Reply::StatusType::ok;
	}

	httpserver::HttpServer::Reply::StatusType HttpRequestHandler::handle_fitParameters( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput )
	{
		reply << "{" << "\n";
		std::vector<size_t> channelIndices=connectedCBCSCurves_.getValidChannelIndices();
		for( std::vector<size_t>::const_iterator iIndexA=channelIndices.begin(); iIndexA!=channelIndices.end(); ++iIndexA )
		{
			reply << "   \"CBC " << std::setfill('0') << std::setw(2)  << *iIndexA << "\": {" << "\n";
			cbcanalyser::FedChannelSCurves& fedChannel=connectedCBCSCurves_.getFedChannelSCurves(*iIndexA);
			std::vector<size_t> stripIndices=fedChannel.getValidStripIndices();
			for( std::vector<size_t>::const_iterator iIndexB=stripIndices.begin(); iIndexB!=stripIndices.end(); ++iIndexB )
			{
				cbcanalyser::SCurve& sCurve=fedChannel.getStripSCurve(*iIndexB);
				std::tuple<float,float,float,float,float> parameters=sCurve.fitParameters();
				reply << "      \"Channel " << std::setfill('0') << std::setw(3)  << *iIndexB
						<< "\": { \"chi2\": " << std::get<0>(parameters)
						<< ", \"ndf\":" << std::get<1>(parameters)
						<< ", \"maxEfficiency\":" << std::get<2>(parameters)
						<< ", \"standardDeviation\":" << std::get<3>(parameters)
						<< ", \"mean\":" << std::get<4>(parameters) << " }";
				if( iIndexB+1 != stripIndices.end() ) reply << ",";
				reply << "\n";
			}
			reply << "   }";
			if( iIndexA+1 != channelIndices.end() ) reply << ",";
			reply << "\n";
		}
		reply << "}" << "\n";
		return httpserver::HttpServer::Reply::StatusType::ok;
	}

	httpserver::HttpServer::Reply::StatusType HttpRequestHandler::handle_reset( std::ostream& reply, std::ostream& errorLog, const std::vector< std::pair<std::string,std::string> >& parameters, std::ostream* pDebugOutput )
	{
		if( pDebugOutput ) (*pDebugOutput) << "Reset called. Removing all data." << "\n";
		connectedCBCSCurves_=cbcanalyser::FedSCurves();
		return httpserver::HttpServer::Reply::StatusType::ok;
	}

	template<class T>
	std::vector<T> HttpRequestHandler::decodeStringToArray( const std::string& inputString )
	{
		std::vector<T> arrayEntries;
		if( inputString.empty() ) return arrayEntries;
		//if( inputString.front()!='[' ) throw std::runtime_error("decodeStringToArray - first character is not '['");
		//if( inputString.back()!=']' ) throw std::runtime_error("decodeStringToArray - last character is not ']'");

		size_t openCount=0; // The number of currently open brackets
		size_t currentElementStart=std::string::npos; // The index in the string where the current array element starts
		for( size_t index=0; index<inputString.size(); ++index )
		{
			if( inputString[index]=='[' )
			{
				++openCount;
				if( openCount==1 )
				{
					if( currentElementStart==std::string::npos ) currentElementStart=index+1;
					else throw std::runtime_error("decodeStringToArray - more than one root array");
				}
			}
			else if( inputString[index]==']' )
			{
				if( openCount==0 ) throw std::runtime_error("decodeStringToArray - arrays do not close properly (more ']' than '[')");
				--openCount;
			}
			else if( inputString[index]==',' && openCount==1 )
			{
				if( currentElementStart==std::string::npos ) throw std::runtime_error("decodeStringToArray - first character is not '['");
				// Copy everything from the end of the last entry into a new string
				// and add it to the array of entries.
				T convertedType=convertString<T>( inputString.substr(currentElementStart,index-currentElementStart) );
				arrayEntries.push_back( convertedType );
				currentElementStart=index+1;
			}
		}
		// Also need to add the very last entry
		std::string lastStringEntry=inputString.substr(currentElementStart,inputString.size()-currentElementStart-1);
		if( !lastStringEntry.empty() )
		{
			arrayEntries.push_back( convertString<T>( lastStringEntry ) );
		}

		if( openCount!=0 ) throw std::runtime_error("decodeStringToArray - arrays do not close properly (more '[' than ']')");
		return arrayEntries;
	}

	template<class T>
	T HttpRequestHandler::convertString( const std::string& inputString )
	{
		size_t idx;
		T result=convertString_impl<T>( inputString, idx );
		if( idx!=inputString.size() ) throw std::runtime_error( "convertString - couldn't convert the whole string '"+inputString+"'" );
		return result;
	}

	// These methods just use the function provided in <string>
	template<> std::string HttpRequestHandler::convertString_impl( const std::string& inputString, size_t& idx ) { idx=inputString.size(); return inputString; }
	template<> float HttpRequestHandler::convertString_impl( const std::string& inputString, size_t& idx ) { return std::stof( inputString, &idx ); }
	template<> double HttpRequestHandler::convertString_impl( const std::string& inputString, size_t& idx ) { return std::stod( inputString, &idx ); }
	template<> long double HttpRequestHandler::convertString_impl( const std::string& inputString, size_t& idx ) { return std::stold( inputString, &idx ); }
	template<> int HttpRequestHandler::convertString_impl( const std::string& inputString, size_t& idx ) { return std::stoi( inputString, &idx ); }
	template<> long HttpRequestHandler::convertString_impl( const std::string& inputString, size_t& idx ) { return std::stol( inputString, &idx ); }
	template<> unsigned long HttpRequestHandler::convertString_impl( const std::string& inputString, size_t& idx ) { return std::stoul( inputString, &idx ); }
	template<> long long HttpRequestHandler::convertString_impl( const std::string& inputString, size_t& idx ) { return std::stoll( inputString, &idx ); }
	template<> unsigned long long HttpRequestHandler::convertString_impl( const std::string& inputString, size_t& idx ) { return std::stoull( inputString, &idx ); }

	std::string HttpRequestHandler::urlDecode( std::string& inputString )
	{
		std::string returnValue;
		char ch;
		size_t index;
		unsigned int decodedCharacter;
		for( index=0; index<inputString.length(); ++index )
		{
			if( int(inputString[index])==37 )
			{
				sscanf(inputString.substr(index+1,2).c_str(), "%x", &decodedCharacter);
				ch=static_cast<char>(decodedCharacter);
				returnValue+=ch;
				index=index+2;
			}
			else returnValue+=inputString[index];
		}
		return returnValue;
	}

	void HttpRequestHandler::openDAQDumpFile( const std::string& filename )
	{
		std::ifstream inputFile( filename );
		if( !inputFile.is_open() ) throw std::runtime_error( "Unable to open the file \""+filename+"\"" );

		cbcanalyser::RawDataFileReader reader( inputFile );

		while( std::unique_ptr<cbcanalyser::RawDataEvent> pEvent=reader.nextEvent() )
		{
			for( size_t cbcIndex=0; cbcIndex<4; ++cbcIndex )
			{
				cbcanalyser::FedChannelSCurves& cbcSCurves=connectedCBCSCurves_.getFedChannelSCurves( cbcIndex );
				cbcanalyser::RawCBCEvent& cbcEvent=pEvent->cbc(cbcIndex);
				for( size_t index=0; index<cbcEvent.channelData().size(); ++index )
				{
					cbcanalyser::SCurve& scurveForStrip=cbcSCurves.getStripSCurve(index);

					// If there was a hit record an "on" event, otherwise record an "off" event
					// for the current threshold.
					if( cbcEvent.channelData()[index] ) ++scurveForStrip.getEntry( threshold_ ).eventsOn();
					else ++scurveForStrip.getEntry( threshold_ ).eventsOff();
				}
			}
//			std::cout << "\n"
//					<< "bunchCounter=" << pEvent->bunchCounter() << "\n"
//					<< "orbitCounter=" << pEvent->orbitCounter() << "\n"
//					<< "lumisection=" << pEvent->lumisection() << "\n"
//					<< "l1aCounter=" << pEvent->l1aCounter() << "\n"
//					<< "cbcCounter=" << pEvent->cbcCounter() << "\n";
			std::cout << "\t" << "Channel data: ";
			for( const auto& channel : pEvent->cbc(0).channelData() )
			{
				if( channel ) std::cout << "1";
				else std::cout << ".";
			}
			std::cout << std::endl;

		}

	}

} // end of the unnamed namespace
