#include "SLHCUpgradeTracker/CBCAnalysis/interface/CBC2ChannelUnpacker.h"
#include <EventFilter/SiStripRawToDigi/interface/SiStripFEDBuffer.h>
#include <DataFormats/FEDRawData/interface/FEDRawData.h>

#include <iostream>
#include <iomanip>

cbcanalyser::CBC2ChannelUnpacker::CBC2ChannelUnpacker( const FEDRawData& rawData )
	: hasData_(false), hits_(254,false)
{
	const uint8_t* pData=rawData.data();
	for( size_t byte=0; byte<rawData.size(); ++byte )
	{
		std::cout << std::hex << std::setw(2) << (int)pData[byte] << " ";
		if( byte%16==15 ) std::cout << "\n";
	}

}

cbcanalyser::CBC2ChannelUnpacker::CBC2ChannelUnpacker( const sistrip::FEDChannel& fedChannel )
	: hasData_(false), hits_(254,false)
{
//	const uint8_t* pData=fedChannel.data();
//	for( uint16_t byte=0; byte<fedChannel.length(); ++byte )
//	{
//		std::cout << std::hex << std::setw(2) << (int)pData[byte] << " ";
//		if( byte%16==15 ) std::cout << "\n";
//	}
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

const std::vector<bool>& cbcanalyser::CBC2ChannelUnpacker::hits() const
{
	return hits_;
}

bool cbcanalyser::CBC2ChannelUnpacker::hasData() const
{
	return hasData_;
}
