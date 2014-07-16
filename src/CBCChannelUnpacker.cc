#include "SLHCUpgradeTracker/CBCAnalysis/interface/CBCChannelUnpacker.h"
#include <EventFilter/SiStripRawToDigi/interface/SiStripFEDBuffer.h>

cbcanalyser::CBCChannelUnpacker::CBCChannelUnpacker( const sistrip::FEDChannel& fedChannel )
	: hits_(256,false)
{
	sistrip::FEDZSChannelUnpacker unpacker=sistrip::FEDZSChannelUnpacker::zeroSuppressedModeUnpacker(fedChannel);

	hasData_=unpacker.hasData();

	while( unpacker.hasData() )
	{
		if( unpacker.adc()>0 ) // A "1" seems to be encoded with "243", and "0" either absent or with "0"
		{
			hits_[unpacker.sampleNumber()]=true;
		}
		unpacker++;
	}
}

const std::vector<bool>& cbcanalyser::CBCChannelUnpacker::hits() const
{
	return hits_;
}

bool cbcanalyser::CBCChannelUnpacker::hasData() const
{
	return hasData_;
}
