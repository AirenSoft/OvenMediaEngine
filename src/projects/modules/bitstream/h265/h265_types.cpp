#include "h265_types.h"

bool operator==(uint8_t first, H265NALUnitType second)
{
	return first == static_cast<uint8_t>(second);
}

bool operator!=(uint8_t first, H265NALUnitType second)
{
	return first != static_cast<uint8_t>(second);
}
