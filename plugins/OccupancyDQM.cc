#include "SLHCUpgradeTracker/CBCAnalysis/plugins/OccupancyDQM.h"

#include <FWCore/Framework/interface/MakerMacros.h>
#include <FWCore/Framework/interface/Event.h>
#include <DataFormats/FEDRawData/interface/FEDRawDataCollection.h>
#include <EventFilter/SiStripRawToDigi/interface/SiStripFEDBuffer.h>
#include "SLHCUpgradeTracker/CBCAnalysis/interface/CBCChannelUnpacker.h"
#include "SLHCUpgradeTracker/CBCAnalysis/interface/SCurve.h"

namespace cbcanalyser
{
	DEFINE_FWK_MODULE(OccupancyDQM);
}

namespace // Use the unnamed namespace for things only used in this file. Definitions of things declared here are at the bottom.
{
	/** @brief Simple sentry class to lock a mutex and release it when it goes out of scope.
	 *
	 * In case something throws an exception and never calls release on the mutex.
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 09/Oct/2013
	 */
	class MutexLockSentry
	{
	public:
		/** @brief !!!The mutex should not be already locked!!! */
		MutexLockSentry( std::mutex& mutexToLock ) : mutex_(mutexToLock) { mutex_.lock(); isLocked_=true; }
		~MutexLockSentry() { if( isLocked_ ) mutex_.unlock(); }
		/** @brief Allows early releasing of the lock. */
		void unlock() { if( isLocked_ ) { isLocked_=false; mutex_.unlock(); } }
	protected:
		std::mutex& mutex_;
		bool isLocked_;
	};

	/** @brief Class to calculate the occupancy for the last rolling <n> events.
	 *
	 * <n> is set in the constructor.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 09/Oct/2013
	 */
	class RollingOccupancy
	{
	public:
		RollingOccupancy( size_t eventsToRecord );
		void addEvent( bool isOn ); ///< Adds a new event and forgets the event furthest in the past
		float occupancy() const; ///< The occupancy for the last <n> events.
		float occupancyError() const; ///< Simple poisson error.
		size_t eventsOn() const; ///< The number of recent events that the channel was on
		size_t eventsOff() const; ///< The number of recent events that the channel was off
	protected:
		std::deque<bool> mostRecentEvents_; ///< Keeps track of the state for individual events. Events furthest in the past are at the front.
		size_t eventsOn_; ///< Keeps track of the number of entries in mostRecentEvents_ that are true
	};

	class CBCChipRollingOccupancy
	{
	public:
		static void setDefaultEventsToRecord( size_t defaultEventsToRecord );
	public:
		/** @brief Constructor that creates RollingOccupancy instances to track the most recent <n> events,
		 * where <n> is whatever the last call to the static setDefaultEventsToRecord was.
		 */
		CBCChipRollingOccupancy();
		/** @brief Constructor that creates RollingOccupancy instances to track the most recent <eventsToRecord> events. */
		CBCChipRollingOccupancy( size_t eventsToRecord );
		void addEvent( const cbcanalyser::CBCChannelUnpacker& unpacker );
		const RollingOccupancy& stripOccupancy( size_t stripNumber ) const;
		size_t numberOfStrips() const;
	protected:
		std::vector<RollingOccupancy> stripOccupancies_;
		static size_t defaultEventsToRecord_;
	};

} // end of the unnamed namespace


//
// The pimple was only implicitly declared, so I still need to declare it explicitly
//
namespace cbcanalyser
{
	/** @brief Private members of the OccupancyDQM class, wrapped in a pimple idiom aka compiler firewall.
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 09/Oct/2013
	 */
	class OccupancyDQMPrivateMembers
	{
	public:
		/** @brief Nested maps of the rolling occupancies for all chips. These are referenced by FED number,
		 * then FED channel number. E.g. the chip on channel 4 of FED 2 can be retrieved with
		 * "allRollingOccupancies_[2][4]".
		 */
		std::map<size_t,std::map<size_t,::CBCChipRollingOccupancy> > allRollingOccupancies_;
	};

} // end of namespace cbcanalyser


cbcanalyser::OccupancyDQM::OccupancyDQM( const edm::ParameterSet& config )
	: server_(*this), pImple( new OccupancyDQMPrivateMembers )
{
	// For some reason I can't for the life of me understand, the server has to be started in
	// the analyze() method. If not the server and analyze() appear to have two separate copies
	// of the OccupancyDQM members, i.e. a change in one does not show up in the other. It's
	// almost as if XDAQ (or CMSSW) constructs this class and then takes a copy, but I've deleted
	// the copy/move constructors etcetera. Weird.
	// I've now moved hostname and port into members so that they're accessible from analyze().
//	std::string hostname=config.getUntrackedParameter<std::string>("commsServerHostname");
//	std::string port=config.getUntrackedParameter<std::string>("commsServerPort");
//	server_.start( hostname, port );
	hostname_=config.getUntrackedParameter<std::string>("commsServerHostname");
	port_=config.getUntrackedParameter<std::string>("commsServerPort");

	size_t eventsToRecord=config.getParameter<unsigned int>("eventsToRecord");
	CBCChipRollingOccupancy::setDefaultEventsToRecord( eventsToRecord );

	numberOfEvents_=0;
}

cbcanalyser::OccupancyDQM::~OccupancyDQM()
{
	server_.stop();
}

void cbcanalyser::OccupancyDQM::analyze( const edm::Event& event, const edm::EventSetup& setup )
{
	// See the note in the constructor about why the server has to be started from here. The
	// call checks to see if the server is already running, in which case it does nothing. So
	// it's harmless to call multiple times.
	server_.start( hostname_, port_ );

	++numberOfEvents_;

	edm::Handle<FEDRawDataCollection> hRawData;
	event.getByLabel( "rawDataCollector", hRawData );

	// I need to lock the mutex because the HTTP server thread could try and read while I'm
	// in the middle of modifying. I'm not worried about the server getting incorrect information
	// (since it's not mission critical) but the iterators it's looping on could change and cause
	// invalid memory access.
	// Use a sentry class to lock it in an exception safe way.
	MutexLockSentry mutexLock( serverMutex_ );


	for( size_t fedIndex=0; fedIndex<sistrip::CMS_FED_ID_MAX; ++fedIndex )
	{
		const FEDRawData& fedData=hRawData->FEDData(fedIndex);

		if( fedData.size()!=0 )
		{
			// Check to see if this FED is one of the ones allocated to the strip tracker
			if( fedIndex < sistrip::FED_ID_MIN || fedIndex > sistrip::FED_ID_MAX ) continue;

			try
			{
				sistrip::FEDBuffer myBuffer(fedData.data(),fedData.size());

				for ( uint16_t feIndex = 0; feIndex<sistrip::FEUNITS_PER_FED; ++feIndex )
				{
					if( !myBuffer.fePresent(feIndex) ) continue;

					for ( uint16_t channelInFe = 0; channelInFe < sistrip::FEDCH_PER_FEUNIT; ++channelInFe )
					{
						const uint16_t channelIndex=feIndex*sistrip::FEDCH_PER_FEUNIT+channelInFe;
						const sistrip::FEDChannel& channel=myBuffer.channel(channelIndex);

						cbcanalyser::CBCChannelUnpacker unpacker(channel);
						if( !unpacker.hasData() ) continue;

						pImple->allRollingOccupancies_[fedIndex][channelIndex].addEvent( unpacker );

					} // end of loop over FED channels
				}
			}
			catch( std::exception& error )
			{
				std::cerr << "Exception: "<< error.what() << std::endl;
			}

		} // end of "if FED has data"
	} // end of loop over FEDs

}

void cbcanalyser::OccupancyDQM::handleRequest( const httpserver::HttpServer::Request& request, httpserver::HttpServer::Reply& reply )
{
	// I don't really care what the request is (what URI or whatever), I'll just return the
	// same thing for every request. I should probably change this in the future but it's
	// okay to have something rough-n-ready for now.


	std::stringstream responseStream; // This will contain the data to send back in the reply
	responseStream << "<html>"
			<< "<body>"
			<< "<h1>CBC occupancies</h1><br>"
			<< "Strips run left to right, top to bottom. So strip 0 is top left; strip 15 top right; 16 second row far left etcetera."
			<< "<p>Total number of events=" << numberOfEvents_ << "</p>";
	// numberOfEvents_ is atomic so the above line should be fine without a mutex.

	// Use a sentry class to lock the thread in an exception safe way, in case something
	// changes while I'm traversing the map.
	::MutexLockSentry mutexLock( serverMutex_ );

	// Loop over the information for the FEDs
	for( const auto& fedNumberMapPair : pImple->allRollingOccupancies_ )
	{
		size_t fedNumber=fedNumberMapPair.first;
		// Loop over the information for the FED channels
		for( const auto& fedChannelNumberChipOccupancyPair : fedNumberMapPair.second )
		{
			size_t channelNumber=fedChannelNumberChipOccupancyPair.first;
			const ::CBCChipRollingOccupancy& chipOccupancy=fedChannelNumberChipOccupancyPair.second;
			responseStream << "<p>FED " << fedNumber << ", FED channel " << channelNumber << "</p>"
					<< "<table border=\"1\">";
			for( size_t stripIndex=0; stripIndex<chipOccupancy.numberOfStrips(); ++stripIndex )
			{
				const RollingOccupancy& stripOccupancy=chipOccupancy.stripOccupancy(stripIndex);
				float occupancy=stripOccupancy.occupancy();

				if( stripIndex%16 == 0 ) responseStream << "<tr>";
				// Make the cell colour a shade of green according to the occupancy, and put the contents as
				// the number of events on, events off, and percentage of events on.
				// To make the cell a shade of green, set the green RGB value to always max, and the other two
				// to max when occupancy is zero (=white) or zero when the occupancy is full (=green).
				responseStream << "<td align=\"center\" bgcolor=\"#"
						<< std::hex << static_cast<int>((1-occupancy)*255)  // The red RGB component of the cell background
						<< "ff"  // The green RGB component of the cell background. Always fully green.
						<< static_cast<int>((1-occupancy)*255)  // The blue RGB component of the cell background
						<< std::dec << "\">"
						<< stripOccupancy.eventsOn() << ":" << stripOccupancy.eventsOff() << "<br>"
						<< static_cast<int>(occupancy*100+0.5) << "&#37</td>"; // Multiply the occupancy by 100 to get a percentage. The +0.5 makes it round correctly.
				if( stripIndex%16 == 15 ) responseStream << "</tr>";

			} // end of loop over CBC strips

			responseStream << "</table>";

		} // end of loop over FED channels
	} // end of loop over FEDs

	mutexLock.unlock(); // Finished with the map so I can release the lock

	responseStream << "</body>"
			<< "</html>";

	reply.status=httpserver::HttpServer::Reply::StatusType::ok;
	reply.content=responseStream.str();
	reply.headers.resize( 2 );
	reply.headers[0].name="Content-Length";
	reply.headers[0].value=std::to_string( reply.content.size() );
	reply.headers[1].name="Content-Type";
	reply.headers[1].value="text/html";
}

//---------------------------------------------------------------------
//--------  Definitions of things in the unnamed namespace  -----------
//---------------------------------------------------------------------
namespace
{
	RollingOccupancy::RollingOccupancy( size_t eventsToRecord )
		: mostRecentEvents_(eventsToRecord,false), eventsOn_(0)
	{
		// No operation besides the initialiser list. Sets everything up
		// as though the last eventsToRecord number of events had the
		// channel off.
	}

	void RollingOccupancy::addEvent( bool isOn )
	{
		// First see if the number of eventsOn_ needs to change, i.e. whether
		// the event furthest in the past that I'm about to forget is different.
		if( mostRecentEvents_.front()!=isOn )
		{
			if( isOn ) ++eventsOn_;
			else --eventsOn_;
		}
		mostRecentEvents_.pop_front();
		mostRecentEvents_.push_back(isOn);
	}

	float RollingOccupancy::occupancy() const
	{
		return static_cast<float>(eventsOn_)/static_cast<float>(mostRecentEvents_.size());
	}

	float RollingOccupancy::occupancyError() const
	{
		return std::sqrt(eventsOn_)/static_cast<float>(mostRecentEvents_.size());
	}

	size_t RollingOccupancy::eventsOn() const
	{
		return eventsOn_;
	}

	size_t RollingOccupancy::eventsOff() const
	{
		return mostRecentEvents_.size()-eventsOn_;
	}



	size_t CBCChipRollingOccupancy::defaultEventsToRecord_=100;

	void CBCChipRollingOccupancy::setDefaultEventsToRecord( size_t defaultEventsToRecord )
	{
		defaultEventsToRecord_=defaultEventsToRecord;
	}

	CBCChipRollingOccupancy::CBCChipRollingOccupancy() : stripOccupancies_(128,defaultEventsToRecord_)
	{
		// No operation besides the initialiser list
	}

	CBCChipRollingOccupancy::CBCChipRollingOccupancy( size_t eventsToRecord ) : stripOccupancies_(128,eventsToRecord)
	{
		// No operation besides the initialiser list
	}

	void CBCChipRollingOccupancy::addEvent( const cbcanalyser::CBCChannelUnpacker& unpacker )
	{
		const std::vector<bool>& hits=unpacker.hits();

		if( stripOccupancies_.size()!=hits.size() ) throw std::logic_error( "CBCChipRollingOccupancy::addEvent was provided an unpacker with an unexpected number of strips" );

		for( size_t stripNumber=0; stripNumber<hits.size(); ++stripNumber )
		{
			stripOccupancies_[stripNumber].addEvent( hits[stripNumber] );
		}
	}

	const RollingOccupancy& CBCChipRollingOccupancy::stripOccupancy( size_t stripNumber ) const
	{
		return stripOccupancies_.at(stripNumber);
	}

	size_t CBCChipRollingOccupancy::numberOfStrips() const
	{
		return stripOccupancies_.size();
	}

} // end of the unnamed namespace
